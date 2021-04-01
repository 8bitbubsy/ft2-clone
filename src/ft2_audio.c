// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_wav_renderer.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
// --------------------------------
#include "mixer/ft2_mix.h"
#include "mixer/ft2_center_mix.h"
#include "mixer/ft2_silence_mix.h"
// --------------------------------

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#define INITIAL_DITHER_SEED 0x12345000

static int8_t pmpCountDiv, pmpChannels = 2;
static uint16_t smpBuffSize;
static int32_t randSeed = INITIAL_DITHER_SEED;
static uint32_t oldAudioFreq, tickTimeLen, tickTimeLenFrac;
static double dAudioNormalizeMul, dPrngStateL, dPrngStateR, dPanningTab[256+1];
static voice_t voice[MAX_VOICES * 2];
static void (*sendAudSamplesFunc)(uint8_t *, uint32_t, uint8_t); // "send mixed samples" routines

// globalized
audio_t audio;
pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;
chSync_t chSync;
pattSync_t pattSync;
volatile bool pattQueueClearing, chQueueClearing;

void resetCachedMixerVars(void)
{
	stmTyp *ch = stm;
	for (int32_t i = 0; i < MAX_VOICES; i++, ch++)
		ch->oldFinalPeriod = -1;

	voice_t *v = voice;
	for (int32_t i = 0; i < MAX_VOICES*2; i++, v++)
	{
		v->dOldHz = 0.0;
		v->oldDelta = 0;
	}
}

void stopVoice(int32_t i)
{
	voice_t *v;

	v = &voice[i];
	memset(v, 0, sizeof (voice_t));
	v->pan = 128;

	// clear "fade out" voice too

	v = &voice[MAX_VOICES + i];
	memset(v, 0, sizeof (voice_t));
	v->pan = 128;
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
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
			setConfigIORadioButtonStates();

		// if it didn't work to use the old settings again, then something is seriously wrong...
		if (!setupAudio(CONFIG_HIDE_ERRORS))
			okBox(0, "System message", "Couldn't find a working audio mode... You'll get no sound / replayer timer!");

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

	dAudioNormalizeMul = dAmp;
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

void P_SetSpeed(uint16_t bpm)
{
	if (bpm == 0)
		return;

	// non-FT2 check for security
	if (bpm > MAX_BPM)
		return;

	audio.dSamplesPerTick = audio.dSamplesPerTickTab[bpm];
	audio.samplesPerTick = (int32_t)(audio.dSamplesPerTick + 0.5);

	// get tick time length for audio/video sync timestamp
	const uint64_t tickTimeLen64 = audio.tickTimeTab[bpm];
	tickTimeLen = tickTimeLen64 >> 32;
	tickTimeLenFrac = tickTimeLen64 & UINT32_MAX;

	// used for calculating volume ramp length for "ticks" ramps
	audio.dRampTickMul = audio.dRampTickMulTab[bpm];
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
	unlockMixerCallback();
}

void calcPanningTable(void)
{
	// same formula as FT2's panning table (with 0.0f..1.0f range)
	for (int32_t i = 0; i <= 256; i++)
		dPanningTab[i] = sqrt(i * (1.0 / 256.0));
}

static void voiceUpdateVolumes(int32_t i, uint8_t status)
{
	double dDestVolL, dDestVolR;

	voice_t *v = &voice[i];

	const double dVolL = v->dVol * dPanningTab[256-v->pan];
	const double dVolR = v->dVol * dPanningTab[    v->pan];

	if (!audio.volumeRampingFlag)
	{
		// volume ramping is disabled
		v->dVolL = dVolL;
		v->dVolR = dVolR;
		v->volRampSamples = 0;
		return;
	}

	v->dDestVolL = dVolL;
	v->dDestVolR = dVolR;

	if (status & IS_NyTon)
	{
		// sample is about to start, ramp out/in at the same time

		// setup "fade out" voice (only if current voice volume > 0)
		if (v->dVolL > 0.0 || v->dVolR > 0.0)
		{
			voice_t *f = &voice[MAX_VOICES+i];

			*f = *v; // copy voice

			f->volRampSamples = audio.quickVolRampSamples;

			dDestVolL = -f->dVolL;
			dDestVolR = -f->dVolR;

			f->dVolDeltaL = dDestVolL * audio.dRampQuickVolMul;
			f->dVolDeltaR = dDestVolR * audio.dRampQuickVolMul;

			f->isFadeOutVoice = true;
		}

		// make current voice fade in from zero when it starts
		v->dVolL = 0.0;
		v->dVolR = 0.0;
	}

	// ramp volume changes

	/* FT2 has two internal volume ramping lengths:
	** IS_QuickVol: 5ms
	** Normal: The duration of a tick (samplesPerTick)
	*/

	// if destination volume and current volume is the same (and we have no sample trigger), don't do ramp
	if (dVolL == v->dVolL && dVolR == v->dVolR && !(status & IS_NyTon))
	{
		// there is no volume change
		v->volRampSamples = 0;
	}
	else
	{
		dDestVolL = dVolL - v->dVolL;
		dDestVolR = dVolR - v->dVolR;

		if (status & IS_QuickVol)
		{
			v->volRampSamples = audio.quickVolRampSamples;
			v->dVolDeltaL = dDestVolL * audio.dRampQuickVolMul;
			v->dVolDeltaR = dDestVolR * audio.dRampQuickVolMul;
		}
		else
		{
			v->volRampSamples = audio.samplesPerTick;
			v->dVolDeltaL = dDestVolL * audio.dRampTickMul;
			v->dVolDeltaR = dDestVolR * audio.dRampTickMul;
		}
	}
}

static void voiceTrigger(int32_t ch, sampleTyp *s, int32_t position)
{
	voice_t *v = &voice[ch];

	int32_t length = s->len;
	int32_t loopStart = s->repS;
	int32_t loopLength = s->repL;
	int32_t loopEnd = s->repS + s->repL;
	uint8_t loopType = s->typ & 3;
	const bool sampleIs16Bit = (s->typ >> 4) & 1;

	if (sampleIs16Bit)
	{
		assert(!(length & 1));
		assert(!(loopStart & 1));
		assert(!(loopLength & 1));
		assert(!(loopEnd & 1));

		length >>= 1;
		loopStart >>= 1;
		loopLength >>= 1;
		loopEnd >>= 1;
	}

	if (s->pek == NULL || length < 1)
	{
		v->active = false; // shut down voice (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sampleIs16Bit)
	{
		v->base16 = (const int16_t *)s->pek;
		v->revBase16 = &v->base16[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps16 = s->leftEdgeTapSamples16 + SINC_LEFT_TAPS;
	}
	else
	{
		v->base8 = s->pek;
		v->revBase8 = &v->base8[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps8 = s->leftEdgeTapSamples8 + SINC_LEFT_TAPS;
	}

	v->hasLooped = false; // for sinc interpolation special case
	v->backwards = false;
	v->loopType = loopType;
	v->end = (loopType > 0) ? loopEnd : length;
	v->loopStart = loopStart;
	v->loopLength = loopLength;
	v->pos = position;
	v->posFrac = 0;

	// if position overflows, shut down voice (f.ex. through 9xx command)
	if (v->pos >= v->end)
	{
		v->active = false;
		return;
	}

	v->mixFuncOffset = (sampleIs16Bit * 9) + (audio.interpolationType * 3) + loopType;
	v->active = true;
}

void resetRampVolumes(void)
{
	voice_t *v = voice;
	for (int32_t i = 0; i < song.antChn; i++, v++)
	{
		v->dVolL = v->dDestVolL;
		v->dVolR = v->dDestVolR;
		v->volRampSamples = 0;
	}
}

void updateVoices(void)
{
	stmTyp *ch = stm;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.antChn; i++, ch++, v++)
	{
		const uint8_t status = ch->tmpStatus = ch->status; // (tmpStatus is used for audio/video sync queue)
		if (status == 0)
			continue;

		ch->status = 0;

		if (status & IS_Vol)
			v->dVol = ch->dFinalVol;

		if (status & IS_Pan)
			v->pan = ch->finalPan;

		if (status & (IS_Vol + IS_Pan))
			voiceUpdateVolumes(i, status);

		if (status & IS_Period)
		{
			// use cached values when possible
			if (ch->finalPeriod != ch->oldFinalPeriod)
			{
				ch->oldFinalPeriod = ch->finalPeriod;

				if (ch->finalPeriod == 0) // in FT2, period 0 -> delta 0
				{
					v->dOldHz = 0.0;
					v->oldDelta = 0;
					v->dSincLUT = gKaiserSinc;
				}
				else
				{
					const double dHz = dPeriod2Hz(ch->finalPeriod);
					const uint64_t delta64 = (int64_t)((dHz * audio.dHz2MixDeltaMul) + 0.5); // Hz -> rounded 32.32 fixed-point

					v->dOldHz = dHz;
					v->oldDelta = delta64;

					// decide which sinc LUT to use according to resampling ratio
					if (delta64 <= 0x130000000) // 1.1875 (32.32fp)
						v->dSincLUT = gKaiserSinc;
					else if (delta64 <= 0x180000000) // 1.5 (32.32fp)
						v->dSincLUT = gDownSample1;
					else
						v->dSincLUT = gDownSample2;
				}
			}

			v->delta = v->oldDelta;
			v->dHz = v->dOldHz;
		}

		if (status & IS_NyTon)
			voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
	}
}

void resetAudioDither(void)
{
	randSeed = INITIAL_DITHER_SEED;
	dPrngStateL = 0.0;
	dPrngStateR = 0.0;
}

static inline int32_t random32(void)
{
	// LCG 32-bit random
	randSeed *= 134775813;
	randSeed++;

	return (int32_t)randSeed;
}

static void sendSamples16BitDitherStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int32_t out32;
	double dOut, dPrng;

	int16_t *streamPointer16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel - 1-bit triangular dithering
		dPrng = random32() * (0.5 / INT32_MAX); // -0.5 .. 0.5
		dOut = ((audio.dMixBufferL[i] * dAudioNormalizeMul) + dPrng) - dPrngStateL;
		dPrngStateL = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel - 1-bit triangular dithering
		dPrng = random32() * (0.5 / INT32_MAX); // -0.5 .. 0.5
		dOut = ((audio.dMixBufferR[i] * dAudioNormalizeMul) + dPrng) - dPrngStateR;
		dPrngStateR = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;
	}

	(void)numAudioChannels;
}

static void sendSamples16BitDitherMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int32_t out32;
	double dOut, dPrng;

	int16_t *streamPointer16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel - 1-bit triangular dithering
		dPrng = random32() * (0.5 / INT32_MAX); // -0.5..0.5
		dOut = ((audio.dMixBufferL[i] * dAudioNormalizeMul) + dPrng) - dPrngStateL;
		dPrngStateL = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel - 1-bit triangular dithering
		dPrng = random32() * (0.5 / INT32_MAX); // -0.5..0.5
		dOut = ((audio.dMixBufferR[i] * dAudioNormalizeMul) + dPrng) - dPrngStateR;
		dPrngStateR = dPrng;
		out32 = (int32_t)dOut;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// send zeroes to the rest of the channels
		for (uint32_t j = 2; j < numAudioChannels; j++)
			*streamPointer16++ = 0;
	}
}

static void sendSamples32BitStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	double dOut;

	float *fStreamPointer32 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		dOut = audio.dMixBufferL[i] * dAudioNormalizeMul;
		dOut = CLAMP(dOut, -1.0, 1.0);
		*fStreamPointer32++ = (float)dOut;

		// right channel
		dOut = audio.dMixBufferR[i] * dAudioNormalizeMul;
		dOut = CLAMP(dOut, -1.0, 1.0);
		*fStreamPointer32++ = (float)dOut;
	}

	(void)numAudioChannels;
}

static void sendSamples32BitMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	double dOut;

	float *fStreamPointer32 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		dOut = audio.dMixBufferL[i] * dAudioNormalizeMul;
		dOut = CLAMP(dOut, -1.0, 1.0);
		*fStreamPointer32++ = (float)dOut;

		// right channel
		dOut = audio.dMixBufferR[i] * dAudioNormalizeMul;
		dOut = CLAMP(dOut, -1.0, 1.0);
		*fStreamPointer32++ = (float)dOut;

		// send zeroes to the rest of the channels
		for (uint32_t j = 2; j < numAudioChannels; j++)
			*fStreamPointer32++ = 0.0f;
	}
}

static void doChannelMixing(int32_t bufferPosition, int32_t samplesToMix)
{
	voice_t *v = voice; // normal voices
	voice_t *r = &voice[MAX_VOICES]; // volume ramp fadeout-voices

	for (int32_t i = 0; i < song.antChn; i++, v++, r++)
	{
		if (v->active)
		{
			bool centerMixFlag;

			const bool volRampFlag = v->volRampSamples > 0;
			if (volRampFlag)
			{
				centerMixFlag = (v->dDestVolL == v->dDestVolR) && (v->dVolDeltaL == v->dVolDeltaR);
			}
			else // no volume ramping active
			{
				if (v->dVolL == 0.0 && v->dVolR == 0.0)
				{
					silenceMixRoutine(v, samplesToMix);
					continue;
				}

				centerMixFlag = v->dVolL == v->dVolR;
			}

			mixFuncTab[(centerMixFlag * 36) + (volRampFlag * 18) + v->mixFuncOffset](v, bufferPosition, samplesToMix);
		}

		if (r->active) // volume ramp fadeout-voice
		{
			const bool centerMixFlag = (r->dDestVolL == r->dDestVolR) && (r->dVolDeltaL == r->dVolDeltaR);
			mixFuncTab[(centerMixFlag * 36) + 18 + r->mixFuncOffset](r, bufferPosition, samplesToMix);
		}
	}
}

// used for song-to-WAV renderer
void mixReplayerTickToBuffer(uint32_t samplesToMix, uint8_t *stream, uint8_t bitDepth)
{
	assert(samplesToMix <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.dMixBufferL, 0, samplesToMix * sizeof (double));
	memset(audio.dMixBufferR, 0, samplesToMix * sizeof (double));

	doChannelMixing(0, samplesToMix);

	// normalize mix buffer and send to audio stream
	if (bitDepth == 16)
		sendSamples16BitDitherStereo(stream, samplesToMix, 2);
	else
		sendSamples32BitStereo(stream, samplesToMix, 2);
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
		pattSyncData.timer = song.curReplayerTimer;
		pattSyncData.patternPos = song.curReplayerPattPos;
		pattSyncData.pattern = song.curReplayerPattNr;
		pattSyncData.songPos = song.curReplayerSongPos;
		pattSyncData.speed = song.speed;
		pattSyncData.tempo = (uint8_t)song.tempo;
		pattSyncData.globalVol = (uint8_t)song.globVol;
		pattSyncData.timestamp = audio.tickTime64;
		pattQueuePush(pattSyncData);
	}

	// push channel variables to sync queue

	syncedChannel_t *c = chSyncData.channels;
	stmTyp *s = stm;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.antChn; i++, c++, s++, v++)
	{
		c->vol = s->finalVol;
		c->dHz = v->dHz;
		c->instrNr = s->instrNr;
		c->sampleNr = s->sampleNr;
		c->status = s->tmpStatus;
		c->smpStartPos = (uint16_t)s->smpStartPos;

		c->pianoNoteNr = 255; // no piano key
		if (songPlaying && (c->status & IS_Period) && s->envSustainActive)
		{
			const int32_t note = getPianoKey(s->finalPeriod, s->fineTune, s->relTonNr);
			if (note >= 0 && note <= 95)
				c->pianoNoteNr = (uint8_t)note;
		}
	}

	chSyncData.timestamp = audio.tickTime64;
	chQueuePush(chSyncData);

	audio.tickTime64 += tickTimeLen;
	audio.tickTime64Frac += tickTimeLenFrac;
	if (audio.tickTime64Frac > UINT32_MAX)
	{
		audio.tickTime64Frac &= UINT32_MAX;
		audio.tickTime64++;
	}
}

static void SDLCALL audioCallback(void *userdata, Uint8 *stream, int len)
{
	if (editor.wavIsRendering)
		return;

	len /= pmpCountDiv; // bytes -> samples
	if (len <= 0)
		return;

	assert(len <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.dMixBufferL, 0, len * sizeof (double));
	memset(audio.dMixBufferR, 0, len * sizeof (double));

	int32_t bufferPosition = 0;

	int32_t samplesLeft = len;
	while (samplesLeft > 0)
	{
		if (audio.dTickSampleCounter <= 0.0)
		{
			// new replayer tick

			replayerBusy = true;

			if (audio.volumeRampingFlag)
				resetRampVolumes();

			tickReplayer();
			updateVoices();
			fillVisualsSyncBuffer();

			audio.dTickSampleCounter += audio.dSamplesPerTick;

			replayerBusy = false;
		}

		const int32_t remainingTick = (int32_t)ceil(audio.dTickSampleCounter);

		int32_t samplesToMix = samplesLeft;
		if (samplesToMix > remainingTick)
			samplesToMix = remainingTick;

		doChannelMixing(bufferPosition, samplesToMix);

		bufferPosition += samplesToMix;
		samplesLeft -= samplesToMix;
		audio.dTickSampleCounter -= samplesToMix;
	}

	// normalize mix buffer and send to audio stream
	sendAudSamplesFunc(stream, len, pmpChannels);

	(void)userdata;
}

static bool setupAudioBuffers(void)
{
	const uint32_t sampleSize = sizeof (double);

	audio.dMixBufferLUnaligned = (double *)MALLOC_PAD(MAX_WAV_RENDER_SAMPLES_PER_TICK * sampleSize, 256);
	audio.dMixBufferRUnaligned = (double *)MALLOC_PAD(MAX_WAV_RENDER_SAMPLES_PER_TICK * sampleSize, 256);

	if (audio.dMixBufferLUnaligned == NULL || audio.dMixBufferRUnaligned == NULL)
		return false;

	// make aligned main pointers
	audio.dMixBufferL = (double *)ALIGN_PTR(audio.dMixBufferLUnaligned, 256);
	audio.dMixBufferR = (double *)ALIGN_PTR(audio.dMixBufferRUnaligned, 256);

	return true;
}

static void freeAudioBuffers(void)
{
	if (audio.dMixBufferLUnaligned != NULL)
	{
		free(audio.dMixBufferLUnaligned);
		audio.dMixBufferLUnaligned = NULL;
	}

	if (audio.dMixBufferRUnaligned != NULL)
	{
		free(audio.dMixBufferRUnaligned);
		audio.dMixBufferRUnaligned = NULL;
	}

	audio.dMixBufferL = NULL;
	audio.dMixBufferR = NULL;
}

void updateSendAudSamplesRoutine(bool lockMixer)
{
	if (lockMixer)
		lockMixerCallback();

	if (config.specialFlags & BITDEPTH_16)
	{
		if (pmpChannels > 2)
			sendAudSamplesFunc = sendSamples16BitDitherMultiChan;
		else
			sendAudSamplesFunc = sendSamples16BitDitherStereo;
	}
	else
	{
		if (pmpChannels > 2)
			sendAudSamplesFunc = sendSamples32BitMultiChan;
		else
			sendAudSamplesFunc = sendSamples32BitStereo;
	}

	if (lockMixer)
		unlockMixerCallback();
}

static void calcAudioLatencyVars(int32_t audioBufferSize, int32_t audioFreq)
{
	double dInt;

	if (audioFreq == 0)
		return;

	const double dAudioLatencySecs = audioBufferSize / (double)audioFreq;

	double dFrac = modf(dAudioLatencySecs * editor.dPerfFreq, &dInt);

	// integer part
	audio.audLatencyPerfValInt = (int32_t)dInt;

	// fractional part (scaled to 0..2^32-1)
	dFrac *= UINT32_MAX+1.0;
	audio.audLatencyPerfValFrac = (uint32_t)dFrac;

	audio.dAudioLatencyMs = dAudioLatencySecs * 1000.0;
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
		config.audioFreq = 48000; // set default rate

	// get audio buffer size from config special flags

	uint16_t configAudioBufSize = 1024;
	if (config.specialFlags & BUFFSIZE_512)
		configAudioBufSize = 512;
	else if (config.specialFlags & BUFFSIZE_2048)
		configAudioBufSize = 2048;

	audio.wantFreq = config.audioFreq;
	audio.wantSamples = configAudioBufSize;
	audio.wantChannels = 2;

	// set up audio device
	memset(&want, 0, sizeof (want));

	// these three may change after opening a device, but our mixer is dealing with it
	want.freq = config.audioFreq;
	want.format = (config.specialFlags & BITDEPTH_32) ? AUDIO_F32 : AUDIO_S16;
	want.channels = 2;
	// -------------------------------------------------------------------------------
	want.callback = audioCallback;
	want.samples  = configAudioBufSize;

	audio.dev = SDL_OpenAudioDevice(audio.currOutputDevice, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE); // prevent SDL2 from resampling
	if (audio.dev == 0)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\n\"%s\"\n\nDo you have any audio device enabled and plugged in?", SDL_GetError());

		return false;
	}

	// test if the received audio format is compatible
	if (have.format != AUDIO_S16 && have.format != AUDIO_F32)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThe program doesn't support an SDL_AudioFormat of '%d' (not 16-bit or 32-bit float).",
				(uint32_t)have.format);

		closeAudio();
		return false;
	}

	// test if the received audio rate is compatible

	if (have.freq != 44100 && have.freq != 48000 && have.freq != 96000 && have.freq != 192000)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThe program doesn't support an audio output rate of %dHz. Sorry!", have.freq);

		closeAudio();
		return false;
	}

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
	audio.haveChannels = have.channels;

	// set a few variables

	config.audioFreq = have.freq;
	audio.freq = have.freq;
	smpBuffSize = have.samples;

	calcAudioLatencyVars(have.samples, have.freq);

	pmpChannels = have.channels;
	pmpCountDiv = pmpChannels * ((newBitDepth == 16) ? sizeof (int16_t) : sizeof (float));

	// make a copy of the new known working audio settings

	audio.lastWorkingAudioFreq = config.audioFreq;
	audio.lastWorkingAudioBits = config.specialFlags & (BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
	setLastWorkingAudioDevName();

	// update config audio radio buttons if we're on that screen at the moment
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
		showConfigScreen();

	updateWavRendererSettings();
	setAudioAmp(config.boostLevel, config.masterVol, (config.specialFlags & BITDEPTH_32) ? true : false);

	// don't call stopVoices() in this routine
	for (int32_t i = 0; i < MAX_VOICES; i++)
		stopVoice(i);

	stopAllScopes();

	audio.dTickSampleCounter = 0.0; // zero tick sample counter so that it will instantly initiate a tick

	calcReplayerVars(audio.freq);

	if (song.speed == 0)
		song.speed = 125;

	P_SetSpeed(song.speed); // this is important

	updateSendAudSamplesRoutine(false);
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
