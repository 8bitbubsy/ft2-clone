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

#define STEREO_SAMPLE_HEIGHT (SAMPLE_AREA_HEIGHT/2)
#define SAMPLE_L_CENTER (SAMPLE_AREA_Y_CENTER - (STEREO_SAMPLE_HEIGHT/2))
#define SAMPLE_R_CENTER (SAMPLE_AREA_Y_CENTER + (STEREO_SAMPLE_HEIGHT/2))

#define MONO_SAMPLE_HEIGHT SAMPLE_AREA_HEIGHT
#define SAMPLE_CENTER SAMPLE_AREA_Y_CENTER

// this number may change when the audio input device is opened (must be 2^n)
#define SAMPLING_BUFFER_SIZE 2048

// (must be 2^n)
#if defined(_WIN32) || defined(__APPLE__)
#define PREVIEW_SAMPLES 2048
#else
#define PREVIEW_SAMPLES 8192
#endif

static bool sampleInStereo;
static volatile bool drawSamplingBufferFlag, outOfMemoryFlag, noMoreRoomFlag;
static int16_t previewBufL[2][PREVIEW_SAMPLES], previewBufR[2][PREVIEW_SAMPLES];
static int32_t samplesSampled, samplingBufferSize, currPreviewBufNum, oldPreviewSmpPos, currSampleLen;
static uint32_t samplingRate;
static sample_t *smpL, *smpR;
static SDL_AudioDeviceID recordDev;

static void SDLCALL stereoSamplingCallback(void *userdata, Uint8 *stream, int len)
{
	const int32_t samples = len >> 2;
	if (instr[editor.curInstr] == NULL || samples < 0 || samples > samplingBufferSize)
		return;

	if (!reallocateSmpData(smpL, smpL->length + samples, true) || !reallocateSmpData(smpR, smpR->length + samples, true))
	{
		drawSamplingBufferFlag = false;
		outOfMemoryFlag = true;
		return;
	}

	const int16_t *src16 = (int16_t *)stream;
	int16_t *dst16_L = (int16_t *)smpL->dataPtr + smpL->length;
	int16_t *dst16_R = (int16_t *)smpR->dataPtr + smpR->length;

	for (int32_t i = 0; i < samples; i++)
	{
		dst16_L[i] = *src16++;
		dst16_R[i] = *src16++;
	}

	smpL->length += samples;
	smpR->length += samples;

	if (smpL->length > MAX_SAMPLE_LEN) // length overflow
	{
		smpL->length -= samples;
		smpR->length -= samples;
		noMoreRoomFlag = true;
		return;
	}

	currSampleLen = smpL->length;

	// if we have gathared enough samples, fill the current display buffer

	samplesSampled += samples;
	if (samplesSampled >= PREVIEW_SAMPLES)
	{
		samplesSampled -= PREVIEW_SAMPLES;

		if (oldPreviewSmpPos > 0)
		{
			int16_t *dstL = previewBufL[currPreviewBufNum^1];
			int16_t *dstR = previewBufR[currPreviewBufNum^1];
			const int16_t *srcL = (int16_t *)smpL->dataPtr + oldPreviewSmpPos;
			const int16_t *srcR = (int16_t *)smpR->dataPtr + oldPreviewSmpPos;

			memcpy(dstL, srcL, PREVIEW_SAMPLES * sizeof (int16_t));
			memcpy(dstR, srcR, PREVIEW_SAMPLES * sizeof (int16_t));

			drawSamplingBufferFlag = true;
		}

		oldPreviewSmpPos = smpL->length - PREVIEW_SAMPLES;
	}

	(void)userdata;
}

static void SDLCALL monoSamplingCallback(void *userdata, Uint8 *stream, int len)
{
	const int32_t samples = len >> 1;
	if (instr[editor.curInstr] == NULL || samples < 0 || samples > samplingBufferSize)
		return;

	if (!reallocateSmpData(smpL, smpL->length + samples, true))
	{
		drawSamplingBufferFlag = false;
		outOfMemoryFlag = true;
		return;
	}

	const int16_t *src16 = (int16_t *)stream;
	int16_t *dst16 = (int16_t *)smpL->dataPtr + smpL->length;
	memcpy(dst16, src16, samples * sizeof (int16_t));

	smpL->length += samples;
	if (smpL->length > MAX_SAMPLE_LEN) // length overflow
	{
		smpL->length -= samples;
		noMoreRoomFlag = true;
		return;
	}

	// if we have gathared enough samples, fill the current display buffer

	samplesSampled += samples;
	if (samplesSampled >= PREVIEW_SAMPLES)
	{
		samplesSampled -= PREVIEW_SAMPLES;

		if (oldPreviewSmpPos > 0)
		{
			int16_t *dst = previewBufL[currPreviewBufNum^1];
			const int16_t *src = (int16_t *)smpL->dataPtr + oldPreviewSmpPos;
			memcpy(dst, src, PREVIEW_SAMPLES * sizeof (int16_t));

			drawSamplingBufferFlag = true;
		}

		oldPreviewSmpPos = smpL->length - PREVIEW_SAMPLES;
	}

	currSampleLen = smpL->length;

	(void)userdata;
}

void stopSampling(void)
{
	resumeAudio();
	mouseAnimOff();

	SDL_CloseAudioDevice(recordDev);

	if (smpL != NULL) fixSample(smpL);
	if (smpR != NULL) fixSample(smpR);

	editor.samplingAudioFlag = false;

	updateSampleEditorSample();
	editor.updateCurInstr = true;
}

static void getMinMax16(const int16_t *p, uint32_t position, uint32_t scanLen, int16_t *min16, int16_t *max16)
{
	int16_t minVal =  32767;
	int16_t maxVal = -32768;

	assert(position+scanLen <= PREVIEW_SAMPLES);

	const int16_t *ptr16 = (const int16_t *)p + position;
	for (uint32_t i = 0; i < scanLen; i++)
	{
		const int16_t smp16 = ptr16[i];
		if (smp16 < minVal) minVal = smp16;
		if (smp16 > maxVal) maxVal = smp16;
	}

	*min16 = minVal;
	*max16 = maxVal;
}

static int32_t scr2BufPos(int32_t x)
{
	const double dXScaleMul = PREVIEW_SAMPLES / (double)SAMPLE_AREA_WIDTH;
	return (int32_t)(x * dXScaleMul);
}

static void drawSamplingPreview(void)
{
	int16_t min, max;

	// clear sample data area
	memset(&video.frameBuffer[174 * SCREEN_W], 0, SAMPLE_AREA_WIDTH * SAMPLE_AREA_HEIGHT * sizeof (int32_t));

	if (sampleInStereo) // stereo sampling
	{
		const int16_t *smpDataL = previewBufL[currPreviewBufNum];
		const int16_t *smpDataR = previewBufR[currPreviewBufNum];

		int32_t oldMinL = SAMPLE_L_CENTER;
		int32_t oldMaxL = SAMPLE_L_CENTER;
		int32_t oldMinR = SAMPLE_R_CENTER;
		int32_t oldMaxR = SAMPLE_R_CENTER;

		hLine(0, SAMPLE_L_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP); // draw center line
		hLine(0, SAMPLE_R_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP); // draw center line

		for (int16_t x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			int32_t smpIdx = scr2BufPos(x+0);
			int32_t smpNum = scr2BufPos(x+1) - smpIdx;

			if (smpIdx+smpNum > PREVIEW_SAMPLES)
				smpNum = PREVIEW_SAMPLES-smpIdx;

			if (smpNum > 0)
			{
				// left channel
				getMinMax16(smpDataL, smpIdx, smpNum, &min, &max);
				min = SAMPLE_L_CENTER - ((min * STEREO_SAMPLE_HEIGHT) >> 16);
				max = SAMPLE_L_CENTER - ((max * STEREO_SAMPLE_HEIGHT) >> 16);

				if (x != 0)
				{
					if (min > oldMaxL) sampleLine(x-1, x, oldMaxL, min);
					if (max < oldMinL) sampleLine(x-1, x, oldMinL, max);
				}

				sampleLine(x, x, max, min);

				oldMinL = min;
				oldMaxL = max;

				// right channel
				getMinMax16(smpDataR, smpIdx, smpNum, &min, &max);
				min = SAMPLE_R_CENTER - ((min * STEREO_SAMPLE_HEIGHT) >> 16);
				max = SAMPLE_R_CENTER - ((max * STEREO_SAMPLE_HEIGHT) >> 16);

				if (x != 0)
				{
					if (min > oldMaxR) sampleLine(x-1, x, oldMaxR, min);
					if (max < oldMinR) sampleLine(x-1, x, oldMinR, max);
				}

				sampleLine(x, x, max, min);

				oldMinR = min;
				oldMaxR = max;
			}
		}
	}
	else // mono sampling
	{
		const int16_t *smpData = previewBufL[currPreviewBufNum];

		int32_t oldMin = SAMPLE_CENTER;
		int32_t oldMax = SAMPLE_CENTER;

		hLine(0, SAMPLE_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP); // draw center line

		for (int16_t x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			int32_t smpIdx = scr2BufPos(x+0);
			int32_t smpNum = scr2BufPos(x+1) - smpIdx;

			if (smpIdx+smpNum > PREVIEW_SAMPLES)
				smpNum = PREVIEW_SAMPLES-smpIdx;

			if (smpNum > 0)
			{
				getMinMax16(smpData, smpIdx, smpNum, &min, &max);
				min = SAMPLE_CENTER - ((min * MONO_SAMPLE_HEIGHT) >> 16);
				max = SAMPLE_CENTER - ((max * MONO_SAMPLE_HEIGHT) >> 16);

				if (x != 0)
				{
					if (min > oldMax) sampleLine(x-1, x, oldMax, min);
					if (max < oldMin) sampleLine(x-1, x, oldMin, max);
				}

				sampleLine(x, x, max, min);

				oldMin = min;
				oldMax = max;
			}
		}
	}
}

void handleSamplingUpdates(void)
{
	if (outOfMemoryFlag)
	{
		outOfMemoryFlag = false;
		stopSampling();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	if (noMoreRoomFlag)
	{
		noMoreRoomFlag = false;
		stopSampling();
		okBox(0, "System message", "Not more room in sample!", NULL);
		return;
	}

	if (drawSamplingBufferFlag)
	{
		drawSamplingBufferFlag = false;

		drawSamplingPreview();
		currPreviewBufNum ^= 1;
	}

	// clear and draw new sample length number
	fillRect(536, 362, 56, 10, PAL_DESKTOP);
	hexOut(536, 362, PAL_FORGRND, currSampleLen, 8);
}

void startSampling(void)
{
#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL < 5
	okBox(0, "System message", "This program needs to be compiled with SDL 2.0.5 or later to support audio sampling.", NULL);
	return;
#else
	SDL_AudioSpec want, have;

	if (editor.samplingAudioFlag || editor.curInstr == 0)
		return;

	int16_t result = okBox(5, "System request", "Stereo sampling will use the next sample slot. While ongoing, press any key to stop.", NULL);
	if (result == 0 || result == 3)
		return;

	sampleInStereo = (result == 2);
	mouseAnimOn();

	switch (config.audioInputFreq)
	{
		         case INPUT_FREQ_44KHZ: samplingRate = 44100; break;
		default: case INPUT_FREQ_48KHZ: samplingRate = 48000; break;
		         case INPUT_FREQ_96KHZ: samplingRate = 96000; break;
	}

	memset(&want, 0, sizeof (SDL_AudioSpec));
	want.freq = samplingRate;
	want.format = AUDIO_S16;
	want.channels = sampleInStereo ? 2 : 1;
	want.callback = sampleInStereo ? stereoSamplingCallback : monoSamplingCallback;
	want.samples = SAMPLING_BUFFER_SIZE;

	recordDev = SDL_OpenAudioDevice(audio.currInputDevice, true, &want, &have, 0);
	if (recordDev == 0)
	{
		okBox(0, "System message", "Couldn't open the input device! Try adjusting the input rate at the config screen.", NULL);
		return;
	}

	samplingRate = have.freq;
	samplingBufferSize = have.samples;

	pauseAudio();

	if (instr[editor.curInstr] == NULL && !allocateInstr(editor.curInstr))
	{
		stopSampling();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	if (sampleInStereo && editor.curSmp+1 >= MAX_SMP_PER_INST)
	{
		stopSampling();
		okBox(0, "System message", "Error: No free sample slot for the right channel!", NULL);
		return;
	}

	smpL = &instr[editor.curInstr]->smp[editor.curSmp];
	freeSample(editor.curInstr, editor.curSmp); // also sets pan to 128 and vol to 64
	setSampleC4Hz(smpL, samplingRate);
	smpL->flags |= SAMPLE_16BIT;

	if (sampleInStereo)
	{
		smpR = &instr[editor.curInstr]->smp[editor.curSmp+1];
		freeSample(editor.curInstr, editor.curSmp+1); // also sets pan to 128 and vol to 64
		setSampleC4Hz(smpR, samplingRate);
		smpR->flags |= SAMPLE_16BIT;

		strcpy(smpL->name, "Left sample");
		smpL->panning = 0;

		strcpy(smpR->name, "Right sample");
		smpR->panning = 255;
	}
	else
	{
		strcpy(smpL->name, "Mono sample");
	}

	memset(previewBufL, 0, sizeof (previewBufL));
	memset(previewBufR, 0, sizeof (previewBufR));
	currPreviewBufNum = 0;

	samplesSampled = currSampleLen = oldPreviewSmpPos = 0;
	noMoreRoomFlag = outOfMemoryFlag = drawSamplingBufferFlag = false;

	updateSampleEditorSample();
	updateSampleEditor();
	setSongModifiedFlag();

	editor.samplingAudioFlag = true;
	SDL_PauseAudioDevice(recordDev, false);
#endif
}
