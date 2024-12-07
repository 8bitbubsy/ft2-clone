// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "scopes/ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_wav_renderer.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_audioselector.h"
#include "mixer/ft2_mix.h"
#include "mixer/ft2_silence_mix.h"

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

static int32_t smpShiftValue;
static uint32_t oldAudioFreq, tickTimeLenInt;
static uint64_t tickTimeLenFrac;
static float fAudioNormalizeMul, fSqrtPanningTable[256+1];
static voice_t voice[MAX_CHANNELS * 2];

// globalized
audio_t audio;
pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;
chSync_t chSync;
pattSync_t pattSync;
volatile bool pattQueueClearing, chQueueClearing;

void resetCachedMixerVars(void)
{
	channel_t *ch = channel;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++)
		ch->oldFinalPeriod = -1;

	voice_t *v = voice;
	for (int32_t i = 0; i < MAX_CHANNELS*2; i++, v++)
		v->oldDelta = 0;
}

void stopVoice(int32_t i)
{
	voice_t *v;

	v = &voice[i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;

	// clear "fade out" voice too

	v = &voice[MAX_CHANNELS + i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;
}

bool setNewAudioSettings(void) // only call this from the main input/video thread
{
	pauseAudio();

	if (!setupAudio(CONFIG_HIDE_ERRORS))
	{
		// set back old known working settings

		config.audioFreq = audio.lastWorkingAudioFreq;
		config.specialFlags &= ~(BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
		config.specialFlags |= audio.lastWorkingAudioBits;

		if (audio.lastWorkingAudioDeviceName != NULL)
		{
			if (audio.currOutputDevice != NULL)
			{
				free(audio.currOutputDevice);
				audio.currOutputDevice = NULL;
			}

			audio.currOutputDevice = strdup(audio.lastWorkingAudioDeviceName);
		}

		// also update config audio radio buttons if we're on that screen at the moment
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
			setConfigAudioRadioButtonStates();

		// if it didn't work to use the old settings again, then something is seriously wrong...
		if (!setupAudio(CONFIG_HIDE_ERRORS))
			okBox(0, "System message", "Couldn't find a working audio mode... You'll get no sound / replayer timer!", NULL);

		resumeAudio();
		return false;
	}

	resumeAudio();

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);
	return true;
}

// amp = 1..32, masterVol = 0..256
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag)
{
	amp = CLAMP(amp, 1, 32);
	masterVol = CLAMP(masterVol, 0, 256);

	double dAmp = (amp * masterVol) / (32.0 * 256.0);
	if (!bitDepth32Flag)
		dAmp *= 32768.0;

	fAudioNormalizeMul = (float)dAmp;
}

void decreaseMasterVol(void)
{
	if (config.masterVol >= 16)
		config.masterVol -= 16;
	else
		config.masterVol = 0;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void increaseMasterVol(void)
{
	if (config.masterVol < (256-16))
		config.masterVol += 16;
	else
		config.masterVol = 256;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void setNewAudioFreq(uint32_t freq) // for song-to-WAV rendering
{
	if (freq == 0)
		return;

	oldAudioFreq = audio.freq;
	audio.freq = freq;

	const bool mustRecalcTables = audio.freq != oldAudioFreq;
	if (mustRecalcTables)
		calcReplayerVars(audio.freq);
}

void setBackOldAudioFreq(void) // for song-to-WAV rendering
{
	const bool mustRecalcTables = audio.freq != oldAudioFreq;

	audio.freq = oldAudioFreq;

	if (mustRecalcTables)
		calcReplayerVars(audio.freq);
}

void setMixerBPM(int32_t bpm)
{
	if (bpm < MIN_BPM || bpm > MAX_BPM)
		return;

	int32_t i = bpm - MIN_BPM;

	audio.samplesPerTickInt = audio.samplesPerTickIntTab[i];
	audio.samplesPerTickFrac = audio.samplesPerTickFracTab[i];
	audio.fSamplesPerTickIntMul = (float)(1.0 / (double)audio.samplesPerTickInt);

	// for audio/video sync timestamp
	tickTimeLenInt = audio.tickTimeIntTab[i];
	tickTimeLenFrac = audio.tickTimeFracTab[i];
}

void audioSetVolRamp(bool volRamp)
{
	lockMixerCallback();
	audio.volumeRampingFlag = volRamp;
	unlockMixerCallback();
}

void audioSetInterpolationType(uint8_t interpolationType)
{
	lockMixerCallback();
	audio.interpolationType = interpolationType;

	audio.sincInterpolation = false;

	// set sinc LUT pointers
	if (config.interpolation == INTERPOLATION_SINC8)
	{
		fKaiserSinc = fKaiserSinc_8;
		fDownSample1 = fDownSample1_8;
		fDownSample2 = fDownSample2_8;

		audio.sincInterpolation = true;
	}
	else if (config.interpolation == INTERPOLATION_SINC16)
	{
		fKaiserSinc = fKaiserSinc_16;
		fDownSample1 = fDownSample1_16;
		fDownSample2 = fDownSample2_16;

		audio.sincInterpolation = true;
	}

	unlockMixerCallback();
}

void calcPanningTable(void)
{
	// same formula as FT2's panning table (with 0.0 .. 1.0 scale)
	for (int32_t i = 0; i <= 256; i++)
		fSqrtPanningTable[i] = (float)sqrt(i / 256.0);
}

static void voiceUpdateVolumes(int32_t i, uint8_t status)
{
	voice_t *v = &voice[i];

	v->fTargetVolumeL = v->fVolume * fSqrtPanningTable[256-v->panning];
	v->fTargetVolumeR = v->fVolume * fSqrtPanningTable[    v->panning];

	if (!audio.volumeRampingFlag)
	{
		// volume ramping is disabled, set volume directly
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
		return;
	}

	// now we need to handle volume ramping

	const bool voiceSampleTrigger = !!(status & IS_Trigger);

	if (voiceSampleTrigger)
	{
		// sample is about to start, ramp out/in at the same time

		if (v->fCurrVolumeL > 0.0f || v->fCurrVolumeR > 0.0f)
		{
			// setup fadeout voice

			voice_t *f = &voice[MAX_CHANNELS+i];

			*f = *v; // copy current voice to new fadeout-ramp voice

			const float fVolumeLDiff = 0.0f - f->fCurrVolumeL;
			const float fVolumeRDiff = 0.0f - f->fCurrVolumeR;

			f->volumeRampLength = audio.quickVolRampSamples; // 5ms
			f->fVolumeLDelta = fVolumeLDiff * audio.fQuickVolRampSamplesMul;
			f->fVolumeRDelta = fVolumeRDiff * audio.fQuickVolRampSamplesMul;

			f->isFadeOutVoice = true;
		}

		// make current voice fade in from zero when it starts
		v->fCurrVolumeL = v->fCurrVolumeR = 0.0f;
	}

	if (!voiceSampleTrigger && v->fTargetVolumeL == v->fCurrVolumeL && v->fTargetVolumeR == v->fCurrVolumeR)
	{
		v->volumeRampLength = 0; // no ramp needed for now
	}
	else
	{
		const float fVolumeLDiff = v->fTargetVolumeL - v->fCurrVolumeL;
		const float fVolumeRDiff = v->fTargetVolumeR - v->fCurrVolumeR;

		float fRampLengthMul;
		if (status & IS_QuickVol) // duration of 5ms
		{
			v->volumeRampLength = audio.quickVolRampSamples;
			fRampLengthMul = audio.fQuickVolRampSamplesMul;
		}
		else // duration of a tick
		{
			v->volumeRampLength = audio.samplesPerTickInt;
			fRampLengthMul = audio.fSamplesPerTickIntMul;
		}

		v->fVolumeLDelta = fVolumeLDiff * fRampLengthMul;
		v->fVolumeRDelta = fVolumeRDiff * fRampLengthMul;
	}
}

static void voiceTrigger(int32_t ch, sample_t *s, int32_t position)
{
	voice_t *v = &voice[ch];

	int32_t length = s->length;
	int32_t loopStart = s->loopStart;
	int32_t loopLength = s->loopLength;
	int32_t loopEnd = s->loopStart + s->loopLength;
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (s->dataPtr == NULL || length < 1)
	{
		v->active = false; // shut down voice (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sample16Bit)
	{
		v->base16 = (const int16_t *)s->dataPtr;
		v->revBase16 = &v->base16[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps16 = s->leftEdgeTapSamples16 + MAX_LEFT_TAPS;
	}
	else
	{
		v->base8 = s->dataPtr;
		v->revBase8 = &v->base8[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps8 = s->leftEdgeTapSamples8 + MAX_LEFT_TAPS;
	}

	v->hasLooped = false; // for sinc interpolation special case
	v->samplingBackwards = false;
	v->loopType = loopType;
	v->sampleEnd = (loopType == LOOP_OFF) ? length : loopEnd;
	v->loopStart = loopStart;
	v->loopLength = loopLength;
	v->position = position;
	v->positionFrac = 0;

	// if position overflows, shut down voice (f.ex. through 9xx command)
	if (v->position >= v->sampleEnd)
	{
		v->active = false;
		return;
	}

	v->mixFuncOffset = ((int32_t)sample16Bit * 18) + (audio.interpolationType * 3) + loopType;
	v->active = true;
}

void resetRampVolumes(void)
{
	voice_t *v = voice;
	for (int32_t i = 0; i < song.numChannels; i++, v++)
	{
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
	}
}

void updateVoices(void)
{
	channel_t *ch = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, ch++, v++)
	{
		const uint8_t status = ch->tmpStatus = ch->status; // (tmpStatus is used for audio/video sync queue)
		if (status == 0)
			continue;

		ch->status = 0;

		if (status & IS_Vol)
		{
			v->fVolume = ch->fFinalVol;

			// set scope volume
			const int32_t scopeVolume = (int32_t)((SCOPE_HEIGHT * ch->fFinalVol) + 0.5f); // rounded
			v->scopeVolume = (uint8_t)scopeVolume;
		}

		if (status & IS_Pan)
			v->panning = ch->finalPan;

		if (status & (IS_Vol + IS_Pan))
			voiceUpdateVolumes(i, status);

		if (status & IS_Period)
		{
			// use cached values when possible
			if (ch->finalPeriod != ch->oldFinalPeriod)
			{
				ch->oldFinalPeriod = ch->finalPeriod;

				const double dHz = dPeriod2Hz(ch->finalPeriod);

				// set voice delta
				const uint64_t delta = v->oldDelta = (int64_t)((dHz * audio.dHz2MixDeltaMul) + 0.5); // Hz -> fixed-point delta (rounded)

				if (audio.sincInterpolation) // decide which sinc LUT to use according to the resampling ratio
				{
					if (delta <= sincDownsample1Ratio)
						v->fSincLUT = fKaiserSinc;
					else if (delta <= sincDownsample2Ratio)
						v->fSincLUT = fDownSample1;
					else
						v->fSincLUT = fDownSample2;
				}

				// set scope delta
				const double dHz2ScopeDeltaMul = SCOPE_FRAC_SCALE / (double)SCOPE_HZ;
				v->scopeDelta = (int64_t)((dHz * dHz2ScopeDeltaMul) + 0.5); // Hz -> fixed-point delta (rounded)
			}

			v->delta = v->oldDelta;
		}

		if (status & IS_Trigger)
			voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
	}
}

static void sendSamples16BitStereo(void *stream, uint32_t sampleBlockLength)
{
	int16_t *streamPtr16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// TODO: This could use dithering (a proper implementation, that is...)

		int32_t L = (int32_t)(audio.fMixBufferL[i] * fAudioNormalizeMul);
		int32_t R = (int32_t)(audio.fMixBufferR[i] * fAudioNormalizeMul);

		CLAMP16(L);
		CLAMP16(R);

		*streamPtr16++ = (int16_t)L;
		*streamPtr16++ = (int16_t)R;

		// clear what we read from the mixing buffer
		audio.fMixBufferL[i] = 0.0f;
		audio.fMixBufferR[i] = 0.0f;
	}
}

static void sendSamples32BitFloatStereo(void *stream, uint32_t sampleBlockLength)
{
	float *fStreamPtr32 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		const float fL = audio.fMixBufferL[i] * fAudioNormalizeMul;
		const float fR = audio.fMixBufferR[i] * fAudioNormalizeMul;

		*fStreamPtr32++ = CLAMP(fL, -1.0f, 1.0f);
		*fStreamPtr32++ = CLAMP(fR, -1.0f, 1.0f);

		// clear what we read from the mixing buffer
		audio.fMixBufferL[i] = 0.0f;
		audio.fMixBufferR[i] = 0.0f;
	}
}

static void doChannelMixing(int32_t bufferPosition, int32_t samplesToMix)
{
	voice_t *v = voice; // normal voices
	voice_t *r = &voice[MAX_CHANNELS]; // volume ramp fadeout-voices

	const int32_t mixOffsetBias = 3 * NUM_INTERPOLATORS * 2; // 3 = loop types (off/fwd/bidi), 2 = bit depths (8-bit/16-bit)

	for (int32_t i = 0; i < song.numChannels; i++, v++, r++)
	{
		if (v->active)
		{
			const bool volRampFlag = (v->volumeRampLength > 0);
			if (!volRampFlag && v->fCurrVolumeL == 0.0f && v->fCurrVolumeR == 0.0f)
				silenceMixRoutine(v, samplesToMix);
			else
				mixFuncTab[((int32_t)volRampFlag * mixOffsetBias) + v->mixFuncOffset](v, bufferPosition, samplesToMix);
		}

		if (r->active) // volume ramp fadeout-voice
			mixFuncTab[mixOffsetBias + r->mixFuncOffset](r, bufferPosition, samplesToMix);
	}
}

// used for song-to-WAV renderer
void mixReplayerTickToBuffer(uint32_t samplesToMix, void *stream, uint8_t bitDepth)
{
	doChannelMixing(0, samplesToMix);

	// normalize mix buffer and send to audio stream
	if (bitDepth == 16)
		sendSamples16BitStereo(stream, samplesToMix);
	else
		sendSamples32BitFloatStereo(stream, samplesToMix);
}

int32_t pattQueueReadSize(void)
{
	while (pattQueueClearing);

	if (pattSync.writePos > pattSync.readPos)
		return pattSync.writePos - pattSync.readPos;
	else if (pattSync.writePos < pattSync.readPos)
		return pattSync.writePos - pattSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t pattQueueWriteSize(void)
{
	int32_t size;

	if (pattSync.writePos > pattSync.readPos)
	{
		size = pattSync.readPos - pattSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (pattSync.writePos < pattSync.readPos)
	{
		pattQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes a minute
		** or two at max BPM between each time (when queue size is default, 4095)
		*/
		pattSync.data[0].timestamp = 0;
		pattSync.readPos = 0;
		pattSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		pattQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool pattQueuePush(pattSyncData_t t)
{
	if (!pattQueueWriteSize())
		return false;

	assert(pattSync.writePos <= SYNC_QUEUE_LEN);
	pattSync.data[pattSync.writePos] = t;
	pattSync.writePos = (pattSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool pattQueuePop(void)
{
	if (!pattQueueReadSize())
		return false;

	pattSync.readPos = (pattSync.readPos + 1) & SYNC_QUEUE_LEN;
	assert(pattSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

pattSyncData_t *pattQueuePeek(void)
{
	if (!pattQueueReadSize())
		return NULL;

	assert(pattSync.readPos <= SYNC_QUEUE_LEN);
	return &pattSync.data[pattSync.readPos];
}

uint64_t getPattQueueTimestamp(void)
{
	if (!pattQueueReadSize())
		return 0;

	assert(pattSync.readPos <= SYNC_QUEUE_LEN);
	return pattSync.data[pattSync.readPos].timestamp;
}

int32_t chQueueReadSize(void)
{
	while (chQueueClearing);

	if (chSync.writePos > chSync.readPos)
		return chSync.writePos - chSync.readPos;
	else if (chSync.writePos < chSync.readPos)
		return chSync.writePos - chSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t chQueueWriteSize(void)
{
	int32_t size;

	if (chSync.writePos > chSync.readPos)
	{
		size = chSync.readPos - chSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (chSync.writePos < chSync.readPos)
	{
		chQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes several
		** minutes between each time (when queue size is default, 16384)
		*/
		chSync.data[0].timestamp = 0;
		chSync.readPos = 0;
		chSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		chQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool chQueuePush(chSyncData_t t)
{
	if (!chQueueWriteSize())
		return false;

	assert(chSync.writePos <= SYNC_QUEUE_LEN);
	chSync.data[chSync.writePos] = t;
	chSync.writePos = (chSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool chQueuePop(void)
{
	if (!chQueueReadSize())
		return false;

	chSync.readPos = (chSync.readPos + 1) & SYNC_QUEUE_LEN;
	assert(chSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

chSyncData_t *chQueuePeek(void)
{
	if (!chQueueReadSize())
		return NULL;

	assert(chSync.readPos <= SYNC_QUEUE_LEN);
	return &chSync.data[chSync.readPos];
}

uint64_t getChQueueTimestamp(void)
{
	if (!chQueueReadSize())
		return 0;

	assert(chSync.readPos <= SYNC_QUEUE_LEN);
	return chSync.data[chSync.readPos].timestamp;
}

void lockAudio(void)
{
	if (audio.dev != 0)
		SDL_LockAudioDevice(audio.dev);

	audio.locked = true;
}

void unlockAudio(void)
{
	if (audio.dev != 0)
		SDL_UnlockAudioDevice(audio.dev);

	audio.locked = false;
}

void resetSyncQueues(void)
{
	pattSync.data[0].timestamp = 0;
	pattSync.readPos = 0;
	pattSync.writePos = 0;
	
	chSync.data[0].timestamp = 0;
	chSync.writePos = 0;
	chSync.readPos = 0;
}

void lockMixerCallback(void) // lock audio + clear voices/scopes (for short operations)
{
	if (!audio.locked)
		lockAudio();

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
}

void unlockMixerCallback(void)
{
	stopVoices(); // VERY important! prevents potential crashes by purging pointers
	
	if (audio.locked)
		unlockAudio();
}

void pauseAudio(void) // lock audio + clear voices/scopes + render silence (for long operations)
{
	if (audioPaused)
	{
		stopVoices(); // VERY important! prevents potential crashes by purging pointers
		return;
	}

	if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, true);

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
	audioPaused = true;
}

void resumeAudio(void) // unlock audio
{
	if (!audioPaused)
		return;

	if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, false);

	audioPaused = false;
}

static void fillVisualsSyncBuffer(void)
{
	pattSyncData_t pattSyncData;
	chSyncData_t chSyncData;

	if (audio.resetSyncTickTimeFlag)
	{
		audio.resetSyncTickTimeFlag = false;

		audio.tickTime64 = SDL_GetPerformanceCounter() + audio.audLatencyPerfValInt;
		audio.tickTime64Frac = audio.audLatencyPerfValFrac;
	}

	if (songPlaying)
	{
		// push pattern variables to sync queue
		pattSyncData.tick = song.curReplayerTick;
		pattSyncData.row = song.curReplayerRow;
		pattSyncData.pattNum = song.curReplayerPattNum;
		pattSyncData.songPos = song.curReplayerSongPos;
		pattSyncData.BPM = (uint8_t)song.BPM;
		pattSyncData.speed = (uint8_t)song.speed;
		pattSyncData.globalVolume = (uint8_t)song.globalVolume;
		pattSyncData.timestamp = audio.tickTime64;
		pattQueuePush(pattSyncData);
	}

	// push channel variables to sync queue

	syncedChannel_t *c = chSyncData.channels;
	channel_t *s = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, c++, s++, v++)
	{
		c->scopeVolume = v->scopeVolume;
		c->scopeDelta = v->scopeDelta;
		c->instrNum = s->instrNum;
		c->smpNum = s->smpNum;
		c->status = s->tmpStatus;
		c->smpStartPos = s->smpStartPos;

		c->pianoNoteNum = 255; // no piano key
		if (songPlaying && (c->status & IS_Period) && !s->keyOff)
		{
			const int32_t note = getPianoKey(s->finalPeriod, s->finetune, s->relativeNote);
			if (note >= 0 && note <= 95)
				c->pianoNoteNum = (uint8_t)note;
		}
	}

	chSyncData.timestamp = audio.tickTime64;
	chQueuePush(chSyncData);

	audio.tickTime64 += tickTimeLenInt;

	audio.tickTime64Frac += tickTimeLenFrac;
	if (audio.tickTime64Frac >= TICK_TIME_FRAC_SCALE)
	{
		audio.tickTime64Frac &= TICK_TIME_FRAC_MASK;
		audio.tickTime64++;
	}
}

static void SDLCALL audioCallback(void *userdata, Uint8 *stream, int len)
{
	if (editor.wavIsRendering)
		return;

	len >>= smpShiftValue; // bytes -> samples
	if (len <= 0)
		return;

	int32_t bufferPosition = 0;

	uint32_t samplesLeft = len;
	while (samplesLeft > 0)
	{
		if (audio.tickSampleCounter == 0) // new replayer tick
		{
			replayerBusy = true;
			if (!musicPaused) // important, don't remove this check! (also used for safety)
			{
				if (audio.volumeRampingFlag)
					resetRampVolumes();

				tickReplayer();
				updateVoices();
				fillVisualsSyncBuffer();
			}
			replayerBusy = false;

			audio.tickSampleCounter = audio.samplesPerTickInt;

			audio.tickSampleCounterFrac += audio.samplesPerTickFrac;
			if (audio.tickSampleCounterFrac >= BPM_FRAC_SCALE)
			{
				audio.tickSampleCounterFrac &= BPM_FRAC_MASK;
				audio.tickSampleCounter++;
			}
		}

		uint32_t samplesToMix = samplesLeft;
		if (samplesToMix > audio.tickSampleCounter)
			samplesToMix = audio.tickSampleCounter;

		doChannelMixing(bufferPosition, samplesToMix);
		bufferPosition += samplesToMix;
		
		audio.tickSampleCounter -= samplesToMix;
		samplesLeft -= samplesToMix;
	}

	if (config.specialFlags & BITDEPTH_16)
		sendSamples16BitStereo(stream, len);
	else
		sendSamples32BitFloatStereo(stream, len);

	(void)userdata;
}

static bool setupAudioBuffers(void)
{
	const int32_t maxAudioFreq = MAX(MAX_AUDIO_FREQ, MAX_WAV_RENDER_FREQ);
	int32_t maxSamplesPerTick = (int32_t)ceil(maxAudioFreq / (MIN_BPM / 2.5)) + 1;

	audio.fMixBufferL = (float *)calloc(maxSamplesPerTick, sizeof (float));
	audio.fMixBufferR = (float *)calloc(maxSamplesPerTick, sizeof (float));

	if (audio.fMixBufferL == NULL || audio.fMixBufferR == NULL)
		return false;

	return true;
}

static void freeAudioBuffers(void)
{
	if (audio.fMixBufferL != NULL)
	{
		free(audio.fMixBufferL);
		audio.fMixBufferL = NULL;
	}

	if (audio.fMixBufferR != NULL)
	{
		free(audio.fMixBufferR);
		audio.fMixBufferR = NULL;
	}
}

static void calcAudioLatencyVars(int32_t audioBufferSize, int32_t audioFreq)
{
	double dInt;

	if (audioFreq == 0)
		return;

	const double dAudioLatencySecs = audioBufferSize / (double)audioFreq;

	double dFrac = modf(dAudioLatencySecs * editor.dPerfFreq, &dInt);

	audio.audLatencyPerfValInt = (uint32_t)dInt;
	audio.audLatencyPerfValFrac = (uint64_t)((dFrac * TICK_TIME_FRAC_SCALE) + 0.5); // rounded
}

static void setLastWorkingAudioDevName(void)
{
	if (audio.lastWorkingAudioDeviceName != NULL)
	{
		free(audio.lastWorkingAudioDeviceName);
		audio.lastWorkingAudioDeviceName = NULL;
	}

	if (audio.currOutputDevice != NULL)
		audio.lastWorkingAudioDeviceName = strdup(audio.currOutputDevice);
}

bool setupAudio(bool showErrorMsg)
{
	SDL_AudioSpec want, have;

	closeAudio();

	if (config.audioFreq < MIN_AUDIO_FREQ || config.audioFreq > MAX_AUDIO_FREQ)
		config.audioFreq = DEFAULT_AUDIO_FREQ;

	// get audio buffer size from config special flags

	uint16_t configAudioBufSize = 1024;
	if (config.specialFlags & BUFFSIZE_512)
		configAudioBufSize = 512;
	else if (config.specialFlags & BUFFSIZE_2048)
		configAudioBufSize = 2048;

	audio.wantFreq = config.audioFreq;
	audio.wantSamples = configAudioBufSize;

	// set up audio device
	memset(&want, 0, sizeof (want));
	want.freq = config.audioFreq;
	want.format = (config.specialFlags & BITDEPTH_32) ? AUDIO_F32 : AUDIO_S16;
	want.channels = 2;
	want.callback = audioCallback;
	want.samples  = configAudioBufSize;

	char *device = audio.currOutputDevice;
	if (device != NULL && strcmp(device, DEFAULT_AUDIO_DEV_STR) == 0)
		device = NULL; // force default device

	audio.dev = SDL_OpenAudioDevice(device, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (audio.dev == 0)
	{
		audio.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
		if (audio.currOutputDevice != NULL)
		{
			free(audio.currOutputDevice);
			audio.currOutputDevice = NULL;
		}
		audio.currOutputDevice = strdup(DEFAULT_AUDIO_DEV_STR);

		if (audio.dev == 0)
		{
			if (showErrorMsg)
				showErrorMsgBox("Couldn't open audio device:\n\"%s\"\n\nDo you have an audio device enabled and plugged in?", SDL_GetError());

			return false;
		}
	}

	// test if the received audio format is compatible
	if (have.format != AUDIO_S16 && have.format != AUDIO_F32)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program only supports 16-bit or 32-bit float audio streams. Sorry!");

		closeAudio();
		return false;
	}

	// test if the received audio stream is compatible

	if (have.channels != 2)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program only supports stereo audio streams. Sorry!");

		closeAudio();
		return false;
	}

	/*
	if (have.freq != 44100 && have.freq != 48000 && have.freq != 96000)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program doesn't support an audio output rate of %dHz. Sorry!", have.freq);

		closeAudio();
		return false;
	}
	*/

	if (!setupAudioBuffers())
	{
		if (showErrorMsg)
			showErrorMsgBox("Not enough memory!");

		closeAudio();
		return false;
	}

	// set new bit depth flag

	int8_t newBitDepth = 16;
	config.specialFlags &= ~BITDEPTH_32;
	config.specialFlags |=  BITDEPTH_16;

	if (have.format == AUDIO_F32)
	{
		newBitDepth = 24;
		config.specialFlags &= ~BITDEPTH_16;
		config.specialFlags |=  BITDEPTH_32;
	}

	audio.haveFreq = have.freq;
	audio.haveSamples = have.samples;
	config.audioFreq = audio.freq = have.freq;

	calcAudioLatencyVars(have.samples, have.freq);
	smpShiftValue = (newBitDepth == 16) ? 2 : 3;

	// make a copy of the new known working audio settings

	audio.lastWorkingAudioFreq = config.audioFreq;
	audio.lastWorkingAudioBits = config.specialFlags & (BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
	setLastWorkingAudioDevName();

	// update config audio radio buttons if we're on that screen at the moment
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		showConfigScreen();

	updateWavRendererSettings();
	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// don't call stopVoices() in this routine
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		stopVoice(i);

	stopAllScopes();

	// zero tick sample counter so that it will instantly initiate a tick
	audio.tickSampleCounterFrac  = audio.tickSampleCounter = 0;

	calcReplayerVars(audio.freq);

	if (song.BPM == 0)
		song.BPM = 125;

	setMixerBPM(song.BPM); // this is important

	audio.resetSyncTickTimeFlag = true;

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);

	return true;
}

void closeAudio(void)
{
	if (audio.dev > 0)
	{
		SDL_PauseAudioDevice(audio.dev, true);
		SDL_CloseAudioDevice(audio.dev);
		audio.dev = 0;
	}

	freeAudioBuffers();
}
