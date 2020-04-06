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
#include "ft2_mix.h"
#include "ft2_tables.h"

#define INITIAL_DITHER_SEED 0x12345000

static int8_t pmpCountDiv, pmpChannels = 2;
static uint16_t smpBuffSize;
static int32_t masterVol, oldAudioFreq, speedVal, pmpLeft, randSeed = INITIAL_DITHER_SEED;
static int32_t prngStateL, prngStateR, oldPeriod;
static uint32_t tickTimeLen, tickTimeLenFrac, oldSFrq, oldSFrqRev;
static float fAudioAmpMul;
static voice_t voice[MAX_VOICES * 2];
static void (*sendAudSamplesFunc)(uint8_t *, uint32_t, uint8_t); // "send mixed samples" routines

pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;

volatile bool pattQueueReading, pattQueueClearing, chQueueReading, chQueueClearing;

void resetCachedMixerVars(void)
{
	oldPeriod = -1;
	oldSFrq = 0;
	oldSFrqRev = 0xFFFFFFFF;
}

void stopVoice(uint8_t i)
{
	voice_t *v;

	v = &voice[i];
	memset(v, 0, sizeof (voice_t));
	v->SPan = 128;

	// clear "fade out" voice too

	v = &voice[MAX_VOICES + i];
	memset(v, 0, sizeof (voice_t));
	v->SPan = 128;
}

bool setNewAudioSettings(void) // only call this from the main input/video thread
{
	uint32_t stringLen;

	pauseAudio();

	if (!setupAudio(CONFIG_HIDE_ERRORS))
	{
		// set back old known working settings

		config.audioFreq = audio.lastWorkingAudioFreq;
		config.specialFlags &= ~(BITDEPTH_16 + BITDEPTH_24 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
		config.specialFlags |= audio.lastWorkingAudioBits;

		if (audio.lastWorkingAudioDeviceName != NULL)
		{
			if (audio.currOutputDevice != NULL)
			{
				free(audio.currOutputDevice);
				audio.currOutputDevice = NULL;
			}

			stringLen = (uint32_t)strlen(audio.lastWorkingAudioDeviceName);

			audio.currOutputDevice = (char *)malloc(stringLen + 2);
			if (audio.currOutputDevice != NULL)
			{
				strcpy(audio.currOutputDevice, audio.lastWorkingAudioDeviceName);
				audio.currOutputDevice[stringLen + 1] = '\0'; // UTF-8 needs double null termination
			}
		}

		// also update config audio radio buttons if we're on that screen at the moment
		if (editor.ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
			setConfigIORadioButtonStates();

		// if it didn't work to use the old settings again, then something is seriously wrong...
		if (!setupAudio(CONFIG_HIDE_ERRORS))
			okBox(0, "System message", "Couldn't find a working audio mode... You'll get no sound / replayer timer!");

		resumeAudio();
		return false;
	}

	resumeAudio();
	return true;
}

// ampFactor = 1..32, masterVol = 0..256
void setAudioAmp(int16_t ampFactor, int16_t master, bool bitDepth32Flag)
{
	ampFactor = CLAMP(ampFactor, 1, 32);
	master = CLAMP(master, 0, 256);

	// voiceVolume = (vol(0..65535) * pan(0..65536)) >> 4
	const float fAudioNorm = 1.0f / (((65535UL * 65536) / 16) / MAX_VOICES);

	if (bitDepth32Flag)
	{
		// 32-bit floating point (24-bit)
		fAudioAmpMul = fAudioNorm * (master / 256.0f) * (ampFactor / 32.0f);
	}
	else
	{
		// 16-bit integer
		masterVol = master * ampFactor * 2048;
	}
}

void setNewAudioFreq(uint32_t freq) // for song to WAV rendering
{
	if (freq == 0)
		return;

	oldAudioFreq = audio.freq;
	audio.freq = freq;

	calcReplayRate(audio.freq);
}

void setBackOldAudioFreq(void) // for song to WAV rendering
{
	audio.freq = oldAudioFreq;
	calcReplayRate(audio.freq);
}

void setSpeed(uint16_t bpm)
{
	double dInt, dFrac;

	if (bpm == 0)
		return;

	speedVal = ((audio.freq + audio.freq) + (audio.freq >> 1)) / bpm; // (audio.freq * 2.5) / BPM
	if (speedVal > 0) // calculate tick time length for audio/video sync timestamp
	{
		// number of samples per tick -> tick length for performance counter
		dFrac = modf(speedVal * audio.dSpeedValMul, &dInt);

		/* - integer part -
		** Cast to int32_t so that the compiler will use fast SSE2 float->int instructions.
		** This result won't be above 2^31, so this is safe.
		*/
		tickTimeLen = (int32_t)dInt;

		// - fractional part (scaled to 0..2^32-1) -
		dFrac *= UINT32_MAX;
		tickTimeLenFrac = (uint32_t)(dFrac + 0.5);

		audio.rampSpeedValMul = 0xFFFFFFFF / speedVal;
	}
}

void audioSetVolRamp(bool volRamp)
{
	lockMixerCallback();
	audio.volumeRampingFlag = volRamp;
	unlockMixerCallback();
}

void audioSetInterpolation(bool interpolation)
{
	lockMixerCallback();
	audio.interpolationFlag = interpolation;
	unlockMixerCallback();
}

static void voiceUpdateVolumes(int32_t i, uint8_t status)
{
	int32_t volL, volR, destVolL, destVolR;
	uint32_t vol;
	voice_t *v, *f;

	v = &voice[i];

	vol = v->SVol; // 0..65535

	// (0..65535 * 0..65536) >> 4 = 0..268431360
	volR = (vol * panningTab[    v->SPan]) >> 4;
	volL = (vol * panningTab[256-v->SPan]) >> 4;

	if (!audio.volumeRampingFlag)
	{
		v->SLVol2 = volL;
		v->SRVol2 = volR;
	}
	else
	{
		v->SLVol1 = volL;
		v->SRVol1 = volR;

		if (status & IS_NyTon)
		{
			// sample is about to start, ramp out/in at the same time

			// setup "fade out" voice (only if current voice volume>0)
			if (v->SLVol2 > 0 || v->SRVol2 > 0)
			{
				f = &voice[MAX_VOICES + i];

				*f = *v; // copy voice

				f->SVolIPLen = audio.quickVolSizeVal;

				destVolL = -f->SLVol2;
				destVolR = -f->SRVol2;

				f->SLVolIP = ((int64_t)destVolL * audio.rampQuickVolMul) >> 32;
				f->SRVolIP = ((int64_t)destVolR * audio.rampQuickVolMul) >> 32;

				f->isFadeOutVoice = true;
			}

			// make current voice fade in when it starts
			v->SLVol2 = 0;
			v->SRVol2 = 0;
		}

		// ramp volume changes

		/* FT2 has two internal volume ramping lengths:
		** IS_QuickVol: 5ms (audioFreq / 200)
		** Normal: The duration of a tick (speedVal)
		*/

		if (volL == v->SLVol2 && volR == v->SRVol2)
		{
			v->SVolIPLen = 0; // there is no volume change
		}
		else
		{
			destVolL = volL - v->SLVol2;
			destVolR = volR - v->SRVol2;

			if (status & IS_QuickVol)
			{
				v->SVolIPLen = audio.quickVolSizeVal;
				v->SLVolIP = ((int64_t)destVolL * audio.rampQuickVolMul) >> 32;
				v->SRVolIP = ((int64_t)destVolR * audio.rampQuickVolMul) >> 32;
			}
			else
			{
				v->SVolIPLen = speedVal;
				v->SLVolIP = ((int64_t)destVolL * audio.rampSpeedValMul) >> 32;
				v->SRVolIP = ((int64_t)destVolR * audio.rampSpeedValMul) >> 32;
			}
		}
	}
}

static void voiceTrigger(int32_t i, sampleTyp *s, int32_t position)
{
	bool sampleIs16Bit;
	uint8_t loopType;
	int32_t oldSLen, length, loopBegin, loopLength;
	voice_t *v;

	v = &voice[i];

	length = s->len;
	loopBegin = s->repS;
	loopLength = s->repL;
	loopType = s->typ & 3;
	sampleIs16Bit = (s->typ >> 4) & 1;

	if (sampleIs16Bit)
	{
		assert(!(length & 1));
		assert(!(loopBegin & 1));
		assert(!(loopLength & 1));

		length >>= 1;
		loopBegin >>= 1;
		loopLength >>= 1;
	}

	if (s->pek == NULL || length < 1)
	{
		v->mixRoutine = NULL; // shut down voice (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sampleIs16Bit)
	{
		v->SBase16 = (const int16_t *)s->pek;
		v->SRevBase16 = &v->SBase16[loopBegin + (loopBegin + loopLength)]; // for pingpong loops
	}
	else
	{
		v->SBase8 = s->pek;
		v->SRevBase8 = &v->SBase8[loopBegin + (loopBegin + loopLength)]; // for pingpong loops
	}

	v->backwards = false;
	v->SLen = (loopType > 0) ? (loopBegin + loopLength) : length;
	v->SRepS = loopBegin;
	v->SRepL = loopLength;
	v->SPos = position;
	v->SPosDec = 0; // position fraction

	// if 9xx position overflows, shut down voice
	oldSLen = (loopType > 0) ? (loopBegin + loopLength) : length;
	if (v->SPos >= oldSLen)
	{
		v->mixRoutine = NULL;
		return;
	}

	v->mixRoutine = mixRoutineTable[(sampleIs16Bit * 12) + (audio.volumeRampingFlag * 6) + (audio.interpolationFlag * 3) + loopType];
}

void mix_SaveIPVolumes(void) // for volume ramping
{
	voice_t *v = voice;
	for (uint32_t i = 0; i < MAX_VOICES; i++, v++)
	{
		v->SLVol2 = v->SLVol1;
		v->SRVol2 = v->SRVol1;
		v->SVolIPLen = 0;
	}
}

void mix_UpdateChannelVolPanFrq(void)
{
	uint8_t status;
	stmTyp *ch;
	voice_t *v;

	ch = stm;
	v = voice;

	for (int32_t i = 0; i < song.antChn; i++, ch++, v++)
	{
		status = ch->tmpStatus = ch->status; // ch->tmpStatus is used for audio/video sync queue
		if (status == 0)
			continue;

		ch->status = 0;

		// volume change
		if (status & IS_Vol)
			v->SVol = ch->finalVol;

		// panning change
		if (status & IS_Pan)
			v->SPan = ch->finalPan;

		// update mixing volumes if vol/pan change
		if (status & (IS_Vol + IS_Pan))
			voiceUpdateVolumes(i, status);

		// frequency change
		if (status & IS_Period)
		{
			if (ch->finalPeriod != oldPeriod) // this value will very often be the same as before
			{
				oldPeriod = ch->finalPeriod;

				oldSFrq = getFrequenceValue(ch->finalPeriod);

				oldSFrqRev = 0xFFFFFFFF;
				if (oldSFrq != 0)
					oldSFrqRev /= oldSFrq;
			}

			v->SFrq = oldSFrq;
			v->SFrqRev = oldSFrqRev;
		}

		// sample trigger (note)
		if (status & IS_NyTon)
			voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
	}
}

void resetAudioDither(void)
{
	randSeed = INITIAL_DITHER_SEED;
	prngStateL = 0;
	prngStateR = 0;
}

static inline uint32_t random32(void)
{
	// LCG 32-bit random
	randSeed *= 134775813;
	randSeed++;

	return randSeed;
}

static void sendSamples16BitStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t out32;

	(void)numAudioChannels;

	streamPointer16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		out32 = ((int64_t)audio.mixBufferL[i] * masterVol) >> 32;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel
		out32 = ((int64_t)audio.mixBufferR[i] * masterVol) >> 32;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;
	}
}

static void sendSamples16BitMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t out32;
	uint32_t i, j;

	streamPointer16 = (int16_t *)stream;
	for (i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		out32 = ((int64_t)audio.mixBufferL[i] * masterVol) >> 32;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel
		out32 = ((int64_t)audio.mixBufferR[i] * masterVol) >> 32;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		for (j = 2; j < numAudioChannels; j++)
			*streamPointer16++ = 0;
	}
}

static void sendSamples16BitDitherStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t prng, out32;

	(void)numAudioChannels;

	streamPointer16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel - 1-bit triangular dithering
		prng = random32();
		out32 = ((((int64_t)audio.mixBufferL[i] * masterVol) + prng) - prngStateL) >> 32;
		prngStateL = prng;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel - 1-bit triangular dithering
		prng = random32();
		out32 = ((((int64_t)audio.mixBufferR[i] * masterVol) + prng) - prngStateR) >> 32;
		prngStateR = prng;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;
	}
}

static void sendSamples16BitDitherMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t prng, out32;

	streamPointer16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel - 1-bit triangular dithering
		prng = random32();
		out32 = ((((int64_t)audio.mixBufferL[i] * masterVol) + prng) - prngStateL) >> 32;
		prngStateL = prng;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		// right channel - 1-bit triangular dithering
		prng = random32();
		out32 = ((((int64_t)audio.mixBufferR[i] * masterVol) + prng) - prngStateR) >> 32;
		prngStateR = prng;
		CLAMP16(out32);
		*streamPointer16++ = (int16_t)out32;

		for (uint32_t j = 2; j < numAudioChannels; j++)
			*streamPointer16++ = 0;
	}
}

static void sendSamples24BitStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	float fOut, *fStreamPointer24;

	(void)numAudioChannels;

	fStreamPointer24 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		fOut = audio.mixBufferL[i] * fAudioAmpMul;
		fOut = CLAMP(fOut, -1.0f, 1.0f);
		*fStreamPointer24++ = fOut;

		// right channel
		fOut = audio.mixBufferR[i] * fAudioAmpMul;
		fOut = CLAMP(fOut, -1.0f, 1.0f);
		*fStreamPointer24++ = fOut;
	}
}

static void sendSamples24BitMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	float fOut, *fStreamPointer24;

	fStreamPointer24 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		// left channel
		fOut = audio.mixBufferL[i] * fAudioAmpMul;
		fOut = CLAMP(fOut, -1.0f, 1.0f);
		*fStreamPointer24++ = fOut;

		// right channel
		fOut = audio.mixBufferR[i] * fAudioAmpMul;
		fOut = CLAMP(fOut, -1.0f, 1.0f);
		*fStreamPointer24++ = fOut;

		for (uint32_t j = 2; j < numAudioChannels; j++)
			*fStreamPointer24++ = 0.0f;
	}
}

static void mixAudio(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	voice_t *v, *r;

	assert(sampleBlockLength <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.mixBufferL, 0, sampleBlockLength * sizeof (int32_t));
	memset(audio.mixBufferR, 0, sampleBlockLength * sizeof (int32_t));

	// mix channels

	v = voice; // normal voices
	r = &voice[MAX_VOICES]; // volume ramp voices

	for (int32_t i = 0; i < song.antChn; i++, v++, r++)
	{
		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL) v->mixRoutine(v, sampleBlockLength); // mix normal voice
		if (r->mixRoutine != NULL) r->mixRoutine(r, sampleBlockLength); // mix volume ramp voice
	}

	// normalize mix buffer and send to audio stream
	sendAudSamplesFunc(stream, sampleBlockLength, numAudioChannels);
}

// used for song-to-WAV renderer
uint32_t mixReplayerTickToBuffer(uint8_t *stream, uint8_t bitDepth)
{
	voice_t *v, *r;

	assert(speedVal <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.mixBufferL, 0, speedVal * sizeof (int32_t));
	memset(audio.mixBufferR, 0, speedVal * sizeof (int32_t));

	// mix channels
	v = voice; // normal voices
	r = &voice[MAX_VOICES]; // volume ramp voices

	for (int32_t i = 0; i < song.antChn; i++, v++, r++)
	{
		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL) v->mixRoutine(v, speedVal); // mix normal voice
		if (r->mixRoutine != NULL) r->mixRoutine(r, speedVal); // mix volume ramp voice
	}

	// normalize mix buffer and send to audio stream
	if (bitDepth == 16)
	{
		if (config.specialFlags2 & DITHERED_AUDIO)
			sendSamples16BitDitherStereo(stream, speedVal, 2);
		else
			sendSamples16BitStereo(stream, speedVal, 2);
	}
	else
	{
		sendSamples24BitStereo(stream, speedVal, 2);
	}

	return speedVal;
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
		** though that is barely noticable on normal buffer sizes, and it takes several
		** minutes between each time (when queue size is default, 16384) */
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
		** minutes between each time (when queue size is default, 16384) */
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

static void resetSyncQueues(void)
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

static void SDLCALL mixCallback(void *userdata, Uint8 *stream, int len)
{
	int32_t a, b;
	pattSyncData_t pattSyncData;
	chSyncData_t chSyncData;
	syncedChannel_t *c;
	stmTyp *s;

	(void)userdata;

	assert(len < 65536); // limitation in mixer
	assert(pmpCountDiv > 0);

	a = len / pmpCountDiv;
	if (a <= 0)
		return;

	while (a > 0)
	{
		if (pmpLeft == 0)
		{
			// replayer tick

			replayerBusy = true;

			if (audio.volumeRampingFlag)
				mix_SaveIPVolumes();

			mainPlayer();
			mix_UpdateChannelVolPanFrq();

			// AUDIO/VIDEO SYNC

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
				pattSyncData.speed = (uint8_t)song.speed;
				pattSyncData.tempo = (uint8_t)song.tempo;
				pattSyncData.globalVol = (uint8_t)song.globVol;
				pattSyncData.timestamp = audio.tickTime64;
				pattQueuePush(pattSyncData);
			}

			// push channel variables to sync queue
			for (int32_t i = 0; i < song.antChn; i++)
			{
				c = &chSyncData.channels[i];
				s = &stm[i];

				c->voiceDelta = voice[i].SFrq;
				c->finalPeriod = s->finalPeriod;
				c->fineTune = s->fineTune;
				c->relTonNr = s->relTonNr;
				c->instrNr = s->instrNr;
				c->sampleNr = s->sampleNr;
				c->envSustainActive = s->envSustainActive;
				c->status = s->tmpStatus;
				c->finalVol = s->finalVol;
				c->smpStartPos = s->smpStartPos;
			}

			chSyncData.timestamp = audio.tickTime64;
			chQueuePush(chSyncData);

			audio.tickTime64 += tickTimeLen;
			audio.tickTime64Frac += tickTimeLenFrac;
			if (audio.tickTime64Frac > 0xFFFFFFFF)
			{
				audio.tickTime64Frac &= 0xFFFFFFFF;
				audio.tickTime64++;
			}

			pmpLeft = speedVal;
			replayerBusy = false;
		}

		b = a;
		if (b > pmpLeft)
			b = pmpLeft;

		mixAudio(stream, b, pmpChannels);
		stream += b * pmpCountDiv;

		a -= b;
		pmpLeft -= b;
	}
}

static bool setupAudioBuffers(void)
{
	const uint32_t sampleSize = sizeof (int32_t);

	audio.mixBufferLUnaligned = (int32_t *)MALLOC_PAD(MAX_WAV_RENDER_SAMPLES_PER_TICK * sampleSize, 256);
	audio.mixBufferRUnaligned = (int32_t *)MALLOC_PAD(MAX_WAV_RENDER_SAMPLES_PER_TICK * sampleSize, 256);

	if (audio.mixBufferLUnaligned == NULL || audio.mixBufferRUnaligned == NULL)
		return false;

	// make aligned main pointers
	audio.mixBufferL = (int32_t *)ALIGN_PTR(audio.mixBufferLUnaligned, 256);
	audio.mixBufferR = (int32_t *)ALIGN_PTR(audio.mixBufferRUnaligned, 256);

	return true;
}

static void freeAudioBuffers(void)
{
	if (audio.mixBufferLUnaligned != NULL)
	{
		free(audio.mixBufferLUnaligned);
		audio.mixBufferLUnaligned = NULL;
	}

	if (audio.mixBufferRUnaligned != NULL)
	{
		free(audio.mixBufferRUnaligned);
		audio.mixBufferRUnaligned = NULL;
	}

	audio.mixBufferL = NULL;
	audio.mixBufferR = NULL;
}

void updateSendAudSamplesRoutine(bool lockMixer)
{
	if (lockMixer)
		lockMixerCallback();

	// force dither off if somehow set with 24-bit float (illegal)
	if ((config.specialFlags2 & DITHERED_AUDIO) && (config.specialFlags & BITDEPTH_24))
		config.specialFlags2 &= ~DITHERED_AUDIO;

	if (config.specialFlags2 & DITHERED_AUDIO)
	{
		if (config.specialFlags & BITDEPTH_16)
		{
			if (pmpChannels > 2)
				sendAudSamplesFunc = sendSamples16BitDitherMultiChan;
			else
				sendAudSamplesFunc = sendSamples16BitDitherStereo;
		}
	}
	else
	{
		if (config.specialFlags & BITDEPTH_16)
		{
			if (pmpChannels > 2)
				sendAudSamplesFunc = sendSamples16BitMultiChan;
			else
				sendAudSamplesFunc = sendSamples16BitStereo;
		}
		else
		{
			if (pmpChannels > 2)
				sendAudSamplesFunc = sendSamples24BitMultiChan;
			else
				sendAudSamplesFunc = sendSamples24BitStereo;
		}
	}

	if (lockMixer)
		unlockMixerCallback();
}

static void calcAudioLatencyVars(uint16_t haveSamples, int32_t haveFreq)
{
	double dHaveFreq, dAudioLatencySecs, dInt, dFrac;

	dHaveFreq = haveFreq;
	if (dHaveFreq == 0.0)
		return; // panic!

	dAudioLatencySecs = haveSamples / dHaveFreq;

	// XXX: haveSamples and haveFreq better not be bogus values...
	dFrac = modf(dAudioLatencySecs * editor.dPerfFreq, &dInt);

	// integer part
	audio.audLatencyPerfValInt = (uint32_t)dInt;

	// fractional part (scaled to 0..2^32-1)
	dFrac *= UINT32_MAX;
	audio.audLatencyPerfValFrac = (uint32_t)(dFrac + 0.5);

	audio.dAudioLatencyMs = dAudioLatencySecs * 1000.0;
}

static void setLastWorkingAudioDevName(void)
{
	uint32_t stringLen;

	if (audio.lastWorkingAudioDeviceName != NULL)
	{
		free(audio.lastWorkingAudioDeviceName);
		audio.lastWorkingAudioDeviceName = NULL;
	}

	if (audio.currOutputDevice != NULL)
	{
		stringLen = (uint32_t)strlen(audio.currOutputDevice);

		audio.lastWorkingAudioDeviceName = (char *)malloc(stringLen + 2);
		if (audio.lastWorkingAudioDeviceName != NULL)
		{
			if (stringLen > 0)
				strcpy(audio.lastWorkingAudioDeviceName, audio.currOutputDevice);
			audio.lastWorkingAudioDeviceName[stringLen + 1] = '\0'; // UTF-8 needs double null termination
		}
	}
}

bool setupAudio(bool showErrorMsg)
{
	int8_t newBitDepth;
	uint16_t configAudioBufSize;
	SDL_AudioSpec want, have;

	closeAudio();

	if (config.audioFreq < MIN_AUDIO_FREQ || config.audioFreq > MAX_AUDIO_FREQ)
	{
		// set default rate
#ifdef __APPLE__
		config.audioFreq = 44100;
#else
		config.audioFreq = 48000;
#endif
	}

	// get audio buffer size from config special flags

	configAudioBufSize = 1024;
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
	want.format = (config.specialFlags & BITDEPTH_24) ? AUDIO_F32 : AUDIO_S16;
	want.channels = 2;
	// -------------------------------------------------------------------------------
	want.callback = mixCallback;
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
			showErrorMsgBox("Couldn't open audio device:\nThe program doesn't support an SDL_AudioFormat of '%d' (not 16-bit or 24-bit float).",
				(uint32_t)have.format);

		closeAudio();
		return false;
	}

	// test if the received audio rate is compatible
	if (have.freq != 44100 && have.freq != 48000 && have.freq != 96000)
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

	newBitDepth = 16;
	config.specialFlags &= ~BITDEPTH_24;
	config.specialFlags |=  BITDEPTH_16;

	if (have.format == AUDIO_F32)
	{
		newBitDepth = 24;
		config.specialFlags &= ~BITDEPTH_16;
		config.specialFlags |=  BITDEPTH_24;
	}

	audio.haveFreq = have.freq;
	audio.haveSamples = have.samples;
	audio.haveChannels = have.channels;

	// set a few variables

	config.audioFreq = have.freq;
	audio.freq = have.freq;
	smpBuffSize = have.samples;

	calcAudioLatencyVars(have.samples, have.freq);

	if ((config.specialFlags2 & DITHERED_AUDIO) && newBitDepth == 24)
		config.specialFlags2 &= ~DITHERED_AUDIO;

	pmpChannels = have.channels;
	pmpCountDiv = pmpChannels * ((newBitDepth == 16) ? sizeof (int16_t) : sizeof (float));

	// make a copy of the new known working audio settings

	audio.lastWorkingAudioFreq = config.audioFreq;
	audio.lastWorkingAudioBits = config.specialFlags & (BITDEPTH_16 + BITDEPTH_24 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
	setLastWorkingAudioDevName();

	// update config audio radio buttons if we're on that screen at the moment
	if (editor.ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
		showConfigScreen();

	updateWavRendererSettings();
	setAudioAmp(config.boostLevel, config.masterVol, (config.specialFlags & BITDEPTH_24) ? true : false);

	// don't call stopVoices() in this routine
	for (uint8_t i = 0; i < MAX_VOICES; i++)
		stopVoice(i);

	stopAllScopes();

	pmpLeft = 0; // reset sample counter

	calcReplayRate(audio.freq);

	if (song.speed == 0)
		song.speed = 125;

	setSpeed(song.speed); // this is important

	updateSendAudSamplesRoutine(false);
	audio.resetSyncTickTimeFlag = true;

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
