// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_sample_ed.h"
#include "ft2_video.h"
#include "ft2_sampling.h"
#include "ft2_structs.h"

// these may very well change after opening the audio input device
#define SAMPLING_BUFFER_SIZE 2048

static bool sampleInStereo;
static volatile bool drawSamplingBufferFlag, outOfMemoryFlag, noMoreRoomFlag;
static int16_t *currWriteBuf;
static int16_t displayBuffer1[SAMPLING_BUFFER_SIZE * 2], displayBuffer2[SAMPLING_BUFFER_SIZE * 2];
static int32_t bytesSampled, samplingBufferBytes;
static uint32_t samplingRate;
static volatile int32_t currSampleLen;
static SDL_AudioDeviceID recordDev;
static int16_t rightChSmpSlot = -1;

static void SDLCALL samplingCallback(void *userdata, Uint8 *stream, int len)
{
	if (instr[editor.curInstr] == NULL || len < 0 || len > samplingBufferBytes)
		return;

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	int8_t *newPtr = (int8_t *)realloc(s->origPek, (s->len + len) + LOOP_FIX_LEN);
	if (newPtr == NULL)
	{
		drawSamplingBufferFlag = false;
		outOfMemoryFlag = true;
		return;
	}

	s->origPek = newPtr;
	s->pek = s->origPek + SMP_DAT_OFFSET;

	memcpy(&s->pek[s->len], stream, len);

	s->len += len;
	if (s->len > MAX_SAMPLE_LEN) // length overflow
	{
		s->len -= len;
		noMoreRoomFlag = true;
		return;
	}

	bytesSampled += len;
	if (bytesSampled >= samplingBufferBytes)
	{
		bytesSampled -= samplingBufferBytes;

		currSampleLen = s->len - samplingBufferBytes;

		// fill display buffer
		memcpy(currWriteBuf, &s->pek[currSampleLen], samplingBufferBytes);

		// swap write buffer (double-buffering)
		if (currWriteBuf == displayBuffer1)
			currWriteBuf = displayBuffer2;
		else
			currWriteBuf = displayBuffer1;

		drawSamplingBufferFlag = true;
	}

	(void)userdata;
}

void stopSampling(void)
{
	int8_t *newPtr;
	int16_t *dst16, *src16;
	int32_t i, len;

	resumeAudio();
	mouseAnimOff();

	SDL_CloseAudioDevice(recordDev);
	editor.samplingAudioFlag = false;

	sampleTyp *currSmp = NULL;
	sampleTyp *nextSmp = NULL;

	if (instr[editor.curInstr] != NULL)
		currSmp = &instr[editor.curInstr]->samp[editor.curSmp];

	if (sampleInStereo)
	{
		// read right channel data
		
		if (currSmp->pek != NULL && rightChSmpSlot != -1)
		{
			nextSmp = &instr[editor.curInstr]->samp[rightChSmpSlot];

			nextSmp->origPek = (int8_t *)malloc((currSmp->len >> 1) + LOOP_FIX_LEN);
			if (nextSmp->origPek != NULL)
			{
				nextSmp->pek = nextSmp->origPek + SMP_DAT_OFFSET;
				nextSmp->len = currSmp->len >> 1;

				src16 = (int16_t *)currSmp->pek;
				dst16 = (int16_t *)nextSmp->pek;

				len = nextSmp->len >> 1;
				for (i = 0; i < len; i++)
					dst16[i] = src16[(i << 1) + 1];
			}
			else
			{
				freeSample(editor.curInstr, rightChSmpSlot);
			}

			currSmp->len >>= 1;

			// read left channel data by skipping every other sample

			dst16 = (int16_t *)currSmp->pek;

			len = currSmp->len >> 1;
			for (i = 0; i < len; i++)
				dst16[i] = dst16[i << 1];
		}
	}

	if (currSmp->origPek != NULL)
	{
		newPtr = (int8_t *)realloc(currSmp->origPek, currSmp->len + LOOP_FIX_LEN);
		if (newPtr != NULL)
		{
			currSmp->origPek = newPtr;
			currSmp->pek = currSmp->origPek + SMP_DAT_OFFSET;
		}

		fixSample(currSmp);
	}
	else
	{
		freeSample(editor.curInstr, editor.curSmp);
	}

	if (nextSmp != NULL && nextSmp->origPek != NULL)
		fixSample(nextSmp);

	updateSampleEditorSample();
	editor.updateCurInstr = true;
}

static uint8_t getDispBuffPeakMono(const int16_t *smpData, int32_t smpNum)
{
	uint32_t max = 0;
	for (int32_t i = 0; i < smpNum; i++)
	{
		const int16_t smp16 = smpData[i];

		const uint32_t smpAbs = ABS(smp16);
		if (smpAbs > max)
			max = smpAbs;
	}

	max = (max * SAMPLE_AREA_HEIGHT) >> 16;
	if (max > 76)
		max = 76;

	return (uint8_t)max;
}

static uint8_t getDispBuffPeakLeft(const int16_t *smpData, int32_t smpNum)
{
	smpNum <<= 1;

	uint32_t max = 0;
	for (int32_t i = 0; i < smpNum; i += 2)
	{
		const int16_t smp16 = smpData[i];

		const uint32_t smpAbs = ABS(smp16);
		if (smpAbs > max)
			max = smpAbs;
	}

	max = (max * SAMPLE_AREA_HEIGHT) >> (16 + 1);
	if (max > 38)
		max = 38;

	return (uint8_t)max;
}

static uint8_t getDispBuffPeakRight(const int16_t *smpData, int32_t smpNum)
{
	smpNum <<= 1;

	uint32_t max = 0;
	for (int32_t i = 0; i < smpNum; i += 2)
	{
		const int16_t smp16 = smpData[i];

		const uint32_t smpAbs = ABS(smp16);
		if (smpAbs > max)
			max = smpAbs;
	}

	max = (max * SAMPLE_AREA_HEIGHT) >> (16 + 1);
	if (max > 38)
		max = 38;

	return (uint8_t)max;
}

static inline int32_t scrPos2SmpBufPos(int32_t x) // x = 0..SAMPLE_AREA_WIDTH
{
	return (x * ((SAMPLING_BUFFER_SIZE << 16) / SAMPLE_AREA_WIDTH)) >> 16;
}

static void drawSamplingPreview(void)
{
	uint8_t smpAbs;
	int16_t *readBuf;
	uint16_t x;
	int32_t smpIdx, smpNum;
	uint32_t *centerPtrL, *centerPtrR;

	const uint32_t pixVal = video.palette[PAL_PATTEXT];

	// select buffer currently not being written to (double-buffering)
	if (currWriteBuf == displayBuffer1)
		readBuf = displayBuffer2;
	else
		readBuf = displayBuffer1;

	if (sampleInStereo)
	{
		// stereo sampling

		const uint16_t centerL = SAMPLE_AREA_Y_CENTER - (SAMPLE_AREA_HEIGHT / 4);
		const uint16_t centerR = SAMPLE_AREA_Y_CENTER + (SAMPLE_AREA_HEIGHT / 4);

		centerPtrL = &video.frameBuffer[centerL*SCREEN_W];
		centerPtrR = &video.frameBuffer[centerR*SCREEN_W];

		for (x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			smpIdx = scrPos2SmpBufPos(x);
			smpNum = scrPos2SmpBufPos(x+1) - smpIdx;

			if (smpIdx+smpNum >= SAMPLING_BUFFER_SIZE)
				smpNum = SAMPLING_BUFFER_SIZE - smpIdx;

			// left channel samples
			smpAbs = getDispBuffPeakLeft(&readBuf[(smpIdx * 2) + 0], smpNum);
			if (smpAbs == 0)
				centerPtrL[x] = pixVal;
			else
				vLine(x, centerL - smpAbs, (smpAbs * 2) + 1, PAL_PATTEXT);

			// right channel samples
			smpAbs = getDispBuffPeakRight(&readBuf[(smpIdx * 2) + 1], smpNum);
			if (smpAbs == 0)
				centerPtrR[x] = pixVal;
			else
				vLine(x, centerR - smpAbs, (smpAbs * 2) + 1, PAL_PATTEXT);
		}
	}
	else
	{
		// mono sampling

		centerPtrL = &video.frameBuffer[SAMPLE_AREA_Y_CENTER * SCREEN_W];

		for (x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			smpIdx = scrPos2SmpBufPos(x);
			smpNum = scrPos2SmpBufPos(x+1) - smpIdx;

			if (smpIdx+smpNum >= SAMPLING_BUFFER_SIZE)
				smpNum = SAMPLING_BUFFER_SIZE - smpIdx;

			smpAbs = getDispBuffPeakMono(&readBuf[smpIdx], smpNum);
			if (smpAbs == 0)
				centerPtrL[x] = pixVal;
			else
				vLine(x, SAMPLE_AREA_Y_CENTER - smpAbs, (smpAbs * 2) + 1, PAL_PATTEXT);
		}
	}
}

void handleSamplingUpdates(void)
{
	if (outOfMemoryFlag)
	{
		outOfMemoryFlag = false;
		stopSampling();
		okBox(0, "System message", "Not enough memory!");
		return;
	}

	if (noMoreRoomFlag)
	{
		noMoreRoomFlag = false;
		stopSampling();
		okBox(0, "System message", "Not more room in sample!");
		return;
	}

	if (drawSamplingBufferFlag)
	{
		drawSamplingBufferFlag = false;

		// clear sample data area
		memset(&video.frameBuffer[174 * SCREEN_W], 0, SAMPLE_AREA_WIDTH * SAMPLE_AREA_HEIGHT * sizeof (int32_t));

		drawSamplingPreview();

		// clear and draw new sample length number
		fillRect(536, 362, 56, 10, PAL_DESKTOP);

		if (sampleInStereo)
			hexOut(536, 362, PAL_FORGRND, currSampleLen >> 1, 8);
		else
			hexOut(536, 362, PAL_FORGRND, currSampleLen, 8);
	}
}

void startSampling(void)
{
#if SDL_PATCHLEVEL < 5
	okBox(0, "System message", "This program needs to be compiled with SDL 2.0.5 or later to support audio sampling.");
	return;
#else
	SDL_AudioSpec want, have;

	if (editor.samplingAudioFlag || editor.curInstr == 0)
		return;

	int16_t result = okBox(9, "System request", "Stereo sampling will use the next sample slot. While ongoing, press any key to stop.");
	if (result == 0 || result == 3)
		return;

	sampleInStereo = (result == 2);
	samplingBufferBytes = sampleInStereo ? (SAMPLING_BUFFER_SIZE * 4) : (SAMPLING_BUFFER_SIZE * 2);

	mouseAnimOn();

	switch (config.audioInputFreq)
	{
		case INPUT_FREQ_96KHZ: samplingRate = 96000; break;
		case INPUT_FREQ_44KHZ: samplingRate = 44100; break;
		default: samplingRate = 48000; break;
	}

	memset(&want, 0, sizeof (SDL_AudioSpec));
	want.freq = samplingRate;
	want.format = AUDIO_S16;
	want.channels = 1 + sampleInStereo;
	want.callback = samplingCallback;
	want.userdata = NULL;
	want.samples = SAMPLING_BUFFER_SIZE;

	recordDev = SDL_OpenAudioDevice(audio.currInputDevice, true, &want, &have, 0);
	if (recordDev == 0)
	{
		okBox(0, "System message", "Couldn't open the input device! Try adjusting the input rate at the config screen.");
		return;
	}

	pauseAudio();

	if (instr[editor.curInstr] == NULL && !allocateInstr(editor.curInstr))
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!");
		return;
	}

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	// wipe current sample and prepare it
	freeSample(editor.curInstr, editor.curSmp);
	s->typ |= 16; // we always sample in 16-bit

	tuneSample(s, samplingRate, audio.linearPeriodsFlag); // tune sample (relTone/finetune) to the sampling frequency we obtained

	if (sampleInStereo)
	{
		strcpy(s->name, "Left sample");
		s->pan = 0;

		if (editor.curSmp+1 < MAX_SMP_PER_INST)
			rightChSmpSlot = editor.curSmp+1;
		else
			rightChSmpSlot = -1;

		if (rightChSmpSlot != -1)
		{
			// wipe current sample and prepare it
			freeSample(editor.curInstr, rightChSmpSlot);
			sampleTyp *nextSmp = &instr[editor.curInstr]->samp[rightChSmpSlot];

			strcpy(nextSmp->name, "Right sample");
			nextSmp->typ |= 16; // we always sample in 16-bit
			nextSmp->pan = 255;

			tuneSample(nextSmp, samplingRate, audio.linearPeriodsFlag); // tune sample (relTone/finetune) to the sampling frequency we obtained
		}
	}
	else
	{
		strcpy(s->name, "Mono-mixed sample");
	}

	updateSampleEditorSample();
	updateSampleEditor();
	setSongModifiedFlag();

	currWriteBuf = displayBuffer1;
	memset(displayBuffer1, 0, sizeof (displayBuffer1));
	memset(displayBuffer2, 0, sizeof (displayBuffer2));

	editor.samplingAudioFlag = true;
	bytesSampled = 0;
	currSampleLen = 0;

	SDL_PauseAudioDevice(recordDev, false);
#endif
}
