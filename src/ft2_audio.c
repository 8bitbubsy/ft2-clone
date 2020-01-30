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
#include "ft2_module_loader.h"
#include "ft2_mix.h"
#include "ft2_audio.h"

#define INITIAL_DITHER_SEED 0x12345000

static int8_t pmpCountDiv, pmpChannels = 2;
static uint16_t smpBuffSize;
static int32_t masterVol, masterAmp, oldAudioFreq, speedVal, pmpLeft, randSeed = INITIAL_DITHER_SEED;
static uint32_t tickTimeLen, tickTimeLenFrac, oldSFrq, oldSFrqRev = 0xFFFFFFFF;
static float fAudioAmpMul;
static voice_t voice[MAX_VOICES * 2];
static void (*sendAudSamplesFunc)(uint8_t *, uint32_t, uint8_t); // "send mixed samples" routines

pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;

volatile bool pattQueueReading, pattQueueClearing, chQueueReading, chQueueClearing;

extern const uint32_t panningTab[257]; // defined at the bottom of this file

void resetOldRevFreqs(void)
{
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
	master    = CLAMP(master, 0, 256);

	// voiceVolume = (vol(0..2047) * amp(1..32) * pan(0..65536)) >> 4
	const float fAudioNorm = 1.0f / (((2047UL * 32 * 65536) / 16) / MAX_VOICES);

	if (bitDepth32Flag)
	{
		// 32-bit floating point (24-bit)
		fAudioAmpMul = fAudioNorm * (master / 256.0f);
	}
	else
	{
		// 16-bit integer
		masterVol = master;
	}

	// calculate channel amp

	if (masterAmp != ampFactor)
	{
		masterAmp = ampFactor;

		// make all channels update volume because of amp change
		for (uint32_t i = 0; i < song.antChn; i++)
			stm[i].status |= IS_Vol;
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

		/* - fractional part (scaled to 0..2^32-1) -
		** We have to resort to slow float->int conversion here because the result
		** can be above 2^31. In 64-bit mode, this will still use a fast SSE2 instruction.
		** Anyhow, this function won't be called that many times a second in worst case,
		** so it's not a big issue at all.
		*/
		dFrac *= UINT32_MAX;
		tickTimeLenFrac = (uint32_t)(dFrac + 0.5);
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

static inline void voiceUpdateVolumes(uint8_t i, uint8_t status)
{
	int32_t volL, volR;
	voice_t *v, *f;

	v = &voice[i];

	volL = v->SVol * masterAmp; // 0..2047 * 1..32 = 0..65504

	// (0..65504 * 0..65536) >> 4 = 0..268304384
	volR = ((uint32_t)volL * panningTab[v->SPan]) >> 4;
	volL = ((uint32_t)volL * panningTab[256 - v->SPan]) >> 4;

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
				f->SLVolIP = -f->SLVol2 / f->SVolIPLen;
				f->SRVolIP = -f->SRVol2 / f->SVolIPLen;

				f->isFadeOutVoice = true;
			}

			// make current voice fade in when it starts
			v->SLVol2 = 0;
			v->SRVol2 = 0;
		}

		// ramp volume changes

		/* FT2 has two internal volume ramping lengths:
		** IS_QuickVol: 5ms (audioFreq / 200)
		** Normal: The duration of a tick (speedVal) */

		if (volL == v->SLVol2 && volR == v->SRVol2)
		{
			v->SVolIPLen = 0; // there is no volume change
		}
		else
		{
			v->SVolIPLen = (status & IS_QuickVol) ? audio.quickVolSizeVal : speedVal;
			v->SLVolIP = (volL - v->SLVol2) / v->SVolIPLen;
			v->SRVolIP = (volR - v->SRVol2) / v->SVolIPLen;
		}
	}
}

static void voiceTrigger(uint8_t i, sampleTyp *s, int32_t position)
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
	voice_t *v;
	for (uint32_t i = 0; i < song.antChn; i++)
	{
		v = &voice[i];
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

	for (uint8_t i = 0; i < song.antChn; i++)
	{
		ch = &stm[i];
		v = &voice[i];

		status = ch->tmpStatus = ch->status; // ch->tmpStatus is used for audio/video sync queue
		if (status != 0)
		{
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
				v->SFrq = getFrequenceValue(ch->finalPeriod);

				if (v->SFrq != oldSFrq)
				{
					oldSFrq = v->SFrq;

					oldSFrqRev = 0xFFFFFFFF;
					if (oldSFrq != 0)
						oldSFrqRev /= oldSFrq;
				}

				v->SFrqRev = oldSFrqRev;
			}

			// sample trigger (note)
			if (status & IS_NyTon)
				voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
		}
	}
}

void resetDitherSeed(void)
{
	randSeed = INITIAL_DITHER_SEED;
}

// Delphi/Pascal LCG Random() (without limit). Suitable for 32-bit random numbers
static inline uint32_t random32(void)
{
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

	if (masterVol == 256) // max master volume
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			out32 = audio.mixBufferL[i] >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			out32 = audio.mixBufferR[i] >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;
		}
	}
	else
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			out32 = ((audio.mixBufferL[i] >> 8) * masterVol) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			out32 = ((audio.mixBufferR[i] >> 8) * masterVol) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;
		}
	}
}

static void sendSamples16BitMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t out32;
	uint32_t i, j;

	streamPointer16 = (int16_t *)stream;

	if (masterVol == 256) // max master volume
	{
		for (i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			out32 = audio.mixBufferL[i] >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			out32 = audio.mixBufferR[i] >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			for (j = 2; j < numAudioChannels; j++)
				*streamPointer16++ = 0;
		}
	}
	else
	{
		for (i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			out32 = ((audio.mixBufferL[i] >> 8) * masterVol) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			out32 = ((audio.mixBufferR[i] >> 8) * masterVol) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			for (j = 2; j < numAudioChannels; j++)
				*streamPointer16++ = 0;
		}
	}
}

static void sendSamples16BitDitherStereo(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t dither, out32;

	(void)numAudioChannels;

	streamPointer16 = (int16_t *)stream;

	if (masterVol == 256) // max master volume
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			dither = ((random32() % 3) - 1) << (8 - 1); // random 1.5-bit noise: -128, 0, 128
			out32 = (audio.mixBufferL[i] + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = (audio.mixBufferR[i] + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;
		}
	}
	else
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			dither = ((random32() % 3) - 1) << (8 - 1); // random 1.5-bit noise: -128, 0, 128
			out32 = (audio.mixBufferL[i] + dither) >> 8;
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = ((out32 * masterVol) + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = (audio.mixBufferR[i] + dither) >> 8;
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = ((out32 * masterVol) + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;
		}
	}
}

static void sendSamples16BitDitherMultiChan(uint8_t *stream, uint32_t sampleBlockLength, uint8_t numAudioChannels)
{
	int16_t *streamPointer16;
	int32_t dither, out32;

	streamPointer16 = (int16_t *)stream;

	if (masterVol == 256) // max master volume
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			dither = ((random32() % 3) - 1) << (8 - 1); // random 1.5-bit noise: -128, 0, 128
			out32 = (audio.mixBufferL[i] + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = (audio.mixBufferR[i] + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			for (uint32_t j = 2; j < numAudioChannels; j++)
				*streamPointer16++ = 0;
		}
	}
	else
	{
		for (uint32_t i = 0; i < sampleBlockLength; i++)
		{
			// left channel
			dither = ((random32() % 3) - 1) << (8 - 1); // random 1.5-bit noise: -128, 0, 128
			out32 = (audio.mixBufferL[i] + dither) >> 8;
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = ((out32 * masterVol) + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			// right channel
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = (audio.mixBufferR[i] + dither) >> 8;
			dither = ((random32() % 3) - 1) << (8 - 1);
			out32 = ((out32 * masterVol) + dither) >> 8;
			CLAMP16(out32);
			*streamPointer16++ = (int16_t)out32;

			for (uint32_t j = 2; j < numAudioChannels; j++)
				*streamPointer16++ = 0;
		}
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
	voice_t *v = voice;

	assert(sampleBlockLength <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.mixBufferL, 0, sampleBlockLength * sizeof (int32_t));
	memset(audio.mixBufferR, 0, sampleBlockLength * sizeof (int32_t));

	// mix channels
	for (uint32_t i = 0; i < song.antChn; i++)
	{
		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL)
			v->mixRoutine(v, sampleBlockLength);

		// mix fade-out voice
		v += MAX_VOICES;

		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL)
			v->mixRoutine(v, sampleBlockLength);

		v -= (MAX_VOICES - 1); // go to next normal channel
	}

	// normalize mix buffer and send to audio stream
	sendAudSamplesFunc(stream, sampleBlockLength, numAudioChannels);
}

// used for song-to-WAV renderer
uint32_t mixReplayerTickToBuffer(uint8_t *stream, uint8_t bitDepth)
{
	voice_t *v = voice;

	assert(speedVal <= MAX_WAV_RENDER_SAMPLES_PER_TICK);
	memset(audio.mixBufferL, 0, speedVal * sizeof (int32_t));
	memset(audio.mixBufferR, 0, speedVal * sizeof (int32_t));

	// mix channels
	for (uint32_t i = 0; i < song.antChn; i++)
	{
		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL)
			v->mixRoutine(v, speedVal);

		// mix fade-out voice
		v += MAX_VOICES;

		// call the mixing routine currently set for the voice
		if (v->mixRoutine != NULL)
			v->mixRoutine(v, speedVal);

		v -= (MAX_VOICES - 1); // go to next normal channel
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
			for (uint32_t i = 0; i < song.antChn; i++)
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

const uint32_t panningTab[257] = // panning table from FT2 code
{
		0, 4096, 5793, 7094, 8192, 9159,10033,10837,11585,12288,12953,13585,14189,14768,15326,15864,
	16384,16888,17378,17854,18318,18770,19212,19644,20066,20480,20886,21283,21674,22058,22435,22806,
	23170,23530,23884,24232,24576,24915,25249,25580,25905,26227,26545,26859,27170,27477,27780,28081,
	28378,28672,28963,29251,29537,29819,30099,30377,30652,30924,31194,31462,31727,31991,32252,32511,
	32768,33023,33276,33527,33776,34024,34270,34514,34756,34996,35235,35472,35708,35942,36175,36406,
	36636,36864,37091,37316,37540,37763,37985,38205,38424,38642,38858,39073,39287,39500,39712,39923,
	40132,40341,40548,40755,40960,41164,41368,41570,41771,41972,42171,42369,42567,42763,42959,43154,
	43348,43541,43733,43925,44115,44305,44494,44682,44869,45056,45242,45427,45611,45795,45977,46160,
	46341,46522,46702,46881,47059,47237,47415,47591,47767,47942,48117,48291,48465,48637,48809,48981,
	49152,49322,49492,49661,49830,49998,50166,50332,50499,50665,50830,50995,51159,51323,51486,51649,
	51811,51972,52134,52294,52454,52614,52773,52932,53090,53248,53405,53562,53719,53874,54030,54185,
	54340,54494,54647,54801,54954,55106,55258,55410,55561,55712,55862,56012,56162,56311,56459,56608,
	56756,56903,57051,57198,57344,57490,57636,57781,57926,58071,58215,58359,58503,58646,58789,58931,
	59073,59215,59357,59498,59639,59779,59919,60059,60199,60338,60477,60615,60753,60891,61029,61166,
	61303,61440,61576,61712,61848,61984,62119,62254,62388,62523,62657,62790,62924,63057,63190,63323,
	63455,63587,63719,63850,63982,64113,64243,64374,64504,64634,64763,64893,65022,65151,65279,65408,
	65536
};
