// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_sample_ed.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"

#define RESONANCE_RANGE 99
#define RESONANCE_MIN 0.01 /* prevent massive blow-up */

enum
{
	REMOVE_SAMPLE_MARK = 0,
	KEEP_SAMPLE_MARK   = 1
};

static struct
{
	bool filled, keepSampleMark;
	uint8_t flags, undoInstr, undoSmp;
	uint32_t length, loopStart, loopLength;
	int8_t *smpData8;
	int16_t *smpData16;
} sampleUndo;

typedef struct
{
	double a1, a2, a3, b1, b2;
	double inTmp[2], outTmp[2];
} resoFilter_t;

enum
{
	FILTER_LOWPASS  = 0,
	FILTER_HIGHPASS = 1
};

static bool normalization;
static uint8_t lastFilterType;
static int32_t lastLpCutoff = 2000, lastHpCutoff = 200, filterResonance, smpCycles = 1, lastWaveLength = 64, lastAmp = 75;

void clearSampleUndo(void)
{
	if (sampleUndo.smpData8 != NULL)
	{
		free(sampleUndo.smpData8);
		sampleUndo.smpData8 = NULL;
	}

	if (sampleUndo.smpData16 != NULL)
	{
		free(sampleUndo.smpData16);
		sampleUndo.smpData16 = NULL;
	}

	sampleUndo.filled = false;
	sampleUndo.keepSampleMark = false;
}

static void fillSampleUndo(bool keepSampleMark)
{
	sampleUndo.filled = false;

	sample_t *s = getCurSample();
	if (s != NULL && s->length > 0)
	{
		pauseAudio();
		unfixSample(s);

		clearSampleUndo();

		sampleUndo.undoInstr = editor.curInstr;
		sampleUndo.undoSmp = editor.curSmp;
		sampleUndo.flags = s->flags;
		sampleUndo.length = s->length;
		sampleUndo.loopStart = s->loopStart;
		sampleUndo.loopLength = s->loopLength;
		sampleUndo.keepSampleMark = keepSampleMark;

		if (s->flags & SAMPLE_16BIT)
		{
			sampleUndo.smpData16 = (int16_t *)malloc(s->length * sizeof (int16_t));
			if (sampleUndo.smpData16 != NULL)
			{
				memcpy(sampleUndo.smpData16, s->dataPtr, s->length * sizeof (int16_t));
				sampleUndo.filled = true;
			}
		}
		else
		{
			sampleUndo.smpData8 = (int8_t *)malloc(s->length * sizeof (int8_t));
			if (sampleUndo.smpData8 != NULL)
			{
				memcpy(sampleUndo.smpData8, s->dataPtr, s->length * sizeof (int8_t));
				sampleUndo.filled = true;
			}
		}

		fixSample(s);
		resumeAudio();
	}
}

static sample_t *setupNewSample(uint32_t length)
{
	pauseAudio();

	if (instr[editor.curInstr] == NULL)
		allocateInstr(editor.curInstr);

	if (instr[editor.curInstr] == NULL)
		goto Error;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (!reallocateSmpData(s, length, true))
		goto Error;

	s->isFixed = false;
	s->length = length;
	s->loopLength = s->loopStart = 0;
	s->flags = SAMPLE_16BIT;

	resumeAudio();
	return s;

Error:
	resumeAudio();
	return NULL;
}

void cbSfxNormalization(void)
{
	normalization ^= 1;
}

static void drawSampleCycles(void)
{
	const int16_t x = 54;

	fillRect(x, 352, 7*3, 8, PAL_DESKTOP);

	char str[16];
	sprintf(str, "%03d", smpCycles);
	textOut(x, 352, PAL_FORGRND, str);
}

void pbSfxCyclesUp(void)
{
	if (smpCycles < 256)
	{
		smpCycles++;
		drawSampleCycles();
	}
}

void pbSfxCyclesDown(void)
{
	if (smpCycles > 1)
	{
		smpCycles--;
		drawSampleCycles();
	}
}

void pbSfxTriangle(void)
{
	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastWaveLength);

	if (inputBox(1, "Enter new waveform length:", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastWaveLength = (int32_t)atoi(lengthStr);
	if (lastWaveLength <= 1 || lastWaveLength > 65536)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 2..65535", NULL);
		return;
	}

	fillSampleUndo(REMOVE_SAMPLE_MARK);
	
	int32_t newLength = lastWaveLength * smpCycles;

	pauseAudio();

	sample_t *s = setupNewSample(newLength);
	if (s == NULL)
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	const double delta = 4.0 / lastWaveLength;
	double phase = 0.0;

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
	{
		double t = phase;
		if (t > 3.0)
			t -= 4.0;
		else if (t >= 1.0)
			t = 2.0 - t;

		*ptr16++ = (int16_t)(t * INT16_MAX);
		phase = fmod(phase + delta, 4.0);
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FORWARD;
	fixSample(s);
	resumeAudio();

	updateSampleEditorSample();
}

void pbSfxSaw(void)
{
	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastWaveLength);

	if (inputBox(1, "Enter new waveform length:", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastWaveLength = (int32_t)atoi(lengthStr);
	if (lastWaveLength <= 1 || lastWaveLength > 65536)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 2..65535", NULL);
		return;
	}

	fillSampleUndo(REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;

	pauseAudio();

	sample_t *s = setupNewSample(newLength);
	if (s == NULL)
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	uint64_t point64 = 0;
	uint64_t delta64 = ((uint64_t)(INT16_MAX*2) << 32ULL) / lastWaveLength;

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = (int16_t)(point64 >> 32);
		point64 += delta64;
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FORWARD;
	fixSample(s);
	resumeAudio();

	updateSampleEditorSample();
}

void pbSfxSine(void)
{
	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastWaveLength);

	if (inputBox(1, "Enter new waveform length:", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastWaveLength = (int32_t)atoi(lengthStr);
	if (lastWaveLength <= 1 || lastWaveLength > 65536)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 2..65535", NULL);
		return;
	}

	fillSampleUndo(REMOVE_SAMPLE_MARK);

	int32_t newLength = lastWaveLength * smpCycles;

	pauseAudio();

	sample_t *s = setupNewSample(newLength);
	if (s == NULL)
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	const double delta = 2.0 * M_PI / lastWaveLength;
	double phase = 0.0;

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (int32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = (int16_t)(INT16_MAX * sin(phase));
		phase += delta;
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FORWARD;
	fixSample(s);
	resumeAudio();

	updateSampleEditorSample();
}

void pbSfxSquare(void)
{
	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastWaveLength);

	if (inputBox(1, "Enter new waveform length:", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastWaveLength = (int32_t)atoi(lengthStr);
	if (lastWaveLength <= 1 || lastWaveLength > 65536)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 2..65535", NULL);
		return;
	}

	fillSampleUndo(REMOVE_SAMPLE_MARK);

	uint32_t newLength = lastWaveLength * smpCycles;

	pauseAudio();

	sample_t *s = setupNewSample(newLength);
	if (s == NULL)
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	const uint32_t halfWaveLength = lastWaveLength / 2;

	int16_t currValue = INT16_MAX;
	uint32_t counter = 0;

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (uint32_t i = 0; i < newLength; i++)
	{
		*ptr16++ = currValue;
		if (++counter >= halfWaveLength)
		{
			counter = 0;
			currValue = -currValue;
		}
	}

	s->loopLength = newLength;
	s->flags |= LOOP_FORWARD;
	fixSample(s);
	resumeAudio();

	updateSampleEditorSample();
}

void drawFilterResonance(void)
{
	const int16_t x = 172;

	fillRect(x, 352, 18, 12, PAL_DESKTOP);

	if (filterResonance <= 0)
	{
		textOut(x, 352, PAL_FORGRND, "off");
	}
	else
	{
		char str[16];
		sprintf(str, "%02d", filterResonance);
		textOut(x+3, 352, PAL_FORGRND, str);
	}
}

void pbSfxResoUp(void)
{
	if (filterResonance < RESONANCE_RANGE)
	{
		filterResonance++;
		drawFilterResonance();
	}
}

void pbSfxResoDown(void)
{
	if (filterResonance > 0)
	{
		filterResonance--;
		drawFilterResonance();
	}
}

#define CUTOFF_EPSILON (1E-4)

static void setupResoLpFilter(sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	// 12dB/oct resonant low-pass filter

	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq/2.0)
			cutoff = (sampleFreq/2.0) - CUTOFF_EPSILON;

		cutoff /= sampleFreq;
	}

	double r = sqrt(2.0);
	if (resonance > 0)
	{
		r = pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0));
		if (r < RESONANCE_MIN)
			r = RESONANCE_MIN;
	}

	const double c = 1.0 / tan(PI * cutoff);

	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = 2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (1.0 - c*c) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;

	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0; // clear filter history
}

static void setupResoHpFilter(sample_t *s, resoFilter_t *f, double cutoff, uint32_t resonance, bool absoluteCutoff)
{
	// 12dB/oct resonant high-pass filter

	if (!absoluteCutoff)
	{
		const double sampleFreq = getSampleC4Rate(s);
		if (cutoff >= sampleFreq/2.0)
			cutoff = (sampleFreq/2.0) - CUTOFF_EPSILON;

		cutoff /= sampleFreq;
	}

	double r = sqrt(2.0);
	if (resonance > 0)
	{
		r = pow(10.0, (resonance * -24.0) / (RESONANCE_RANGE * 20.0));
		if (r < RESONANCE_MIN)
			r = RESONANCE_MIN;
	}

	const double c = tan(PI * cutoff);

	f->a1 = 1.0 / (1.0 + r * c + c * c);
	f->a2 = -2.0 * f->a1;
	f->a3 = f->a1;
	f->b1 = 2.0 * (c*c - 1.0) * f->a1;
	f->b2 = (1.0 - r * c + c * c) * f->a1;

	f->inTmp[0] = f->inTmp[1] = f->outTmp[0] = f->outTmp[1] = 0.0; // clear filter history
}

static bool applyResoFilter(sample_t *s, resoFilter_t *f)
{
	int32_t x1, x2;
	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			return true;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}
	
	const int32_t len = x2 - x1;

	if (!normalization)
	{
		pauseAudio();
		unfixSample(s);

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f->a1*ptr16[i]) + (f->a2*f->inTmp[0]) + (f->a3*f->inTmp[1]) - (f->b1*f->outTmp[0]) - (f->b2*f->outTmp[1]);

				f->inTmp[1] = f->inTmp[0];
				f->inTmp[0] = ptr16[i];

				f->outTmp[1] = f->outTmp[0];
				f->outTmp[0] = out;

				ptr16[i] = (int16_t)CLAMP(out, INT16_MIN, INT16_MAX);
			}
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f->a1*ptr8[i]) + (f->a2*f->inTmp[0]) + (f->a3*f->inTmp[1]) - (f->b1*f->outTmp[0]) - (f->b2*f->outTmp[1]);

				f->inTmp[1] = f->inTmp[0];
				f->inTmp[0] = ptr8[i];

				f->outTmp[1] = f->outTmp[0];
				f->outTmp[0] = out;

				ptr8[i] = (int8_t)CLAMP(out, INT8_MIN, INT8_MAX);
			}
		}

		fixSample(s);
		resumeAudio();
	}
	else // normalize peak, no clipping
	{
		double *dSmp = (double *)malloc(len * sizeof (double));
		if (dSmp == NULL)
		{
			okBox(0, "System message", "Not enough memory!", NULL);
			return false;
		}

		pauseAudio();
		unfixSample(s);

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				dSmp[i] = (double)ptr16[i];
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				dSmp[i] = (double)ptr8[i];
		}

		double peak = 0.0;
		for (int32_t i = 0; i < len; i++)
		{
			const double out = (f->a1*dSmp[i]) + (f->a2*f->inTmp[0]) + (f->a3*f->inTmp[1]) - (f->b1*f->outTmp[0]) - (f->b2*f->outTmp[1]);

			f->inTmp[1] = f->inTmp[0];
			f->inTmp[0] = dSmp[i];

			f->outTmp[1] = f->outTmp[0];
			f->outTmp[0] = out;

			dSmp[i] = out;

			const double outAbs = fabs(out);
			if (outAbs > peak)
				peak = outAbs;
		}

		if (s->flags & SAMPLE_16BIT)
		{
			const double scale = INT16_MAX / peak;

			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				ptr16[i] = (int16_t)(dSmp[i] * scale);
		}
		else
		{
			const double scale = INT8_MAX / peak;

			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
				ptr8[i] = (int8_t)(dSmp[i] * scale);
		}

		free(dSmp);

		fixSample(s);
		resumeAudio();
	}

	return true;
}

void pbSfxLowPass(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastLpCutoff);

	lastFilterType = FILTER_LOWPASS;
	if (inputBox(6, "Enter low-pass filter cutoff (in Hz):", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastLpCutoff = (int32_t)atoi(lengthStr);
	if (lastLpCutoff < 1 || lastLpCutoff > 99999)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 1..99999", NULL);
		return;
	}

	setupResoLpFilter(s, &f, lastLpCutoff, filterResonance, false);
	fillSampleUndo(KEEP_SAMPLE_MARK);
	applyResoFilter(s, &f);
	writeSample(true);
}

void pbSfxHighPass(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	char lengthStr[5+1];
	memset(lengthStr, '\0', sizeof (lengthStr));
	snprintf(lengthStr, sizeof (lengthStr), "%d", lastHpCutoff);

	lastFilterType = FILTER_HIGHPASS;
	if (inputBox(6, "Enter high-pass filter cutoff (in Hz):", lengthStr, sizeof (lengthStr)-1) != 1)
		return;

	if (lengthStr[0] == '\0')
		return;

	lastHpCutoff = (int32_t)atoi(lengthStr);
	if (lastHpCutoff < 1 || lastHpCutoff > 99999)
	{
		okBox(0, "System message", "Illegal range! Allowed range is 1..99999", NULL);
		return;
	}

	setupResoHpFilter(s, &f, lastHpCutoff, filterResonance, false);
	fillSampleUndo(KEEP_SAMPLE_MARK);
	applyResoFilter(s, &f);
	writeSample(true);
}

void sfxPreviewFilter(uint32_t cutoff)
{
	sample_t oldSample;
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length == 0 || cutoff < 1 || cutoff > 99999)
		return;

	int32_t x1, x2;
	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			return;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}

	const int32_t len = x2 - x1;

	pauseAudio();
	unfixSample(s);
	memcpy(&oldSample, s, sizeof (sample_t));

	if (lastFilterType == FILTER_LOWPASS)
		setupResoLpFilter(s, &f, cutoff, filterResonance, false);
	else
		setupResoHpFilter(s, &f, cutoff, filterResonance, false);

	// prepare new sample
	int8_t *sampleData;
	if (s->flags & SAMPLE_16BIT)
	{
		sampleData = (int8_t *)malloc((len * sizeof (int16_t)) + SAMPLE_PAD_LENGTH);
		if (sampleData == NULL)
			goto Error;

		memcpy(sampleData + SMP_DAT_OFFSET, (int16_t *)s->dataPtr + x1, len * sizeof (int16_t));
	}
	else
	{
		sampleData = (int8_t *)malloc((len * sizeof (int8_t)) + SAMPLE_PAD_LENGTH);
		if (sampleData == NULL)
			goto Error;

		memcpy(sampleData + SMP_DAT_OFFSET, (int8_t *)s->dataPtr + x1, len * sizeof (int8_t));
	}

	s->origDataPtr = sampleData;
	s->length = len;
	s->dataPtr = s->origDataPtr + SMP_DAT_OFFSET;
	s->loopStart = s->loopLength = 0;
	fixSample(s);

	const int32_t oldX1 = smpEd_Rx1;
	const int32_t oldX2 = smpEd_Rx2;
	smpEd_Rx1 = smpEd_Rx2 = 0;
	applyResoFilter(s, &f);
	smpEd_Rx1 = oldX1;
	smpEd_Rx2 = oldX2;

	// set up preview sample on channel 0
	channel_t *ch = &channel[0];
	uint8_t note = editor.smpEd_NoteNr;
	ch->smpNum = editor.curSmp;
	ch->instrNum = editor.curInstr;
	ch->copyOfInstrAndNote = (ch->instrNum << 8) | note;
	ch->efx = 0;
	ch->smpStartPos = 0;
	resumeAudio();
	triggerNote(note, 0, 0, ch);
	resetVolumes(ch);
	triggerInstrument(ch);
	ch->realVol = ch->outVol = ch->oldVol = 64;
	updateVolPanAutoVib(ch);

	while (ch->status & IS_Trigger); // wait for sample to latch in mixer
	SDL_Delay(1500); // wait 1.5 seconds

	// we're done, stop voice and free temporary data
	pauseAudio();
	free(sampleData);

Error:
	// set back old sample
	memcpy(s, &oldSample, sizeof (sample_t));
	fixSample(s);
	resumeAudio();
}

void pbSfxSubBass(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	setupResoHpFilter(s, &f, 0.001, 0, true);
	fillSampleUndo(KEEP_SAMPLE_MARK);
	applyResoFilter(s, &f);
	writeSample(true);
}

void pbSfxAddBass(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			return;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}
	
	const int32_t len = x2 - x1;

	setupResoLpFilter(s, &f, 0.015, 0, true);

	double *dSmp = (double *)malloc(len * sizeof (double));
	if (dSmp == NULL)
	{
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	fillSampleUndo(KEEP_SAMPLE_MARK);

	pauseAudio();
	unfixSample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr16[i];
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr8[i];
	}

	if (!normalization)
	{
		for (int32_t i = 0; i < len; i++)
		{
			double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

			f.inTmp[1] = f.inTmp[0];
			f.inTmp[0] = dSmp[i];

			f.outTmp[1] = f.outTmp[0];
			f.outTmp[0] = out;

			dSmp[i] = out;
		}

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = ptr16[i] + (dSmp[i] * 0.25);
				out = CLAMP(out, INT16_MIN, INT16_MAX);

				ptr16[i] = (int16_t)out;
			}
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = ptr8[i] + (dSmp[i] * 0.25);
				out = CLAMP(out, INT8_MIN, INT8_MAX);

				ptr8[i] = (int8_t)out;
			}
		}
	}
	else
	{
		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;

			double peak = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

				f.inTmp[1] = f.inTmp[0];
				f.inTmp[0] = dSmp[i];

				f.outTmp[1] = f.outTmp[0];
				f.outTmp[0] = out;

				dSmp[i] = out;
				double bass = ptr16[i] + (out * 0.25);

				const double outAbs = fabs(bass);
				if (outAbs > peak)
					peak = outAbs;
			}

			double scale = INT16_MAX / peak;
			for (int32_t i = 0; i < len; i++)
				ptr16[i] = (int16_t)((ptr16[i] + (dSmp[i] * 0.25)) * scale);
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;

			double peak = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

				f.inTmp[1] = f.inTmp[0];
				f.inTmp[0] = dSmp[i];

				f.outTmp[1] = f.outTmp[0];
				f.outTmp[0] = out;

				dSmp[i] = out;
				double bass = ptr8[i] + (out * 0.25);

				const double outAbs = fabs(bass);
				if (outAbs > peak)
					peak = outAbs;
			}

			double scale = INT8_MAX / peak;
			for (int32_t i = 0; i < len; i++)
				ptr8[i] = (int8_t)((ptr8[i] + (dSmp[i] * 0.25)) * scale);
		}
	}

	free(dSmp);

	fixSample(s);
	resumeAudio();

	writeSample(true);
}

void pbSfxSubTreble(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	setupResoLpFilter(s, &f, 0.33, 0, true);
	fillSampleUndo(KEEP_SAMPLE_MARK);
	applyResoFilter(s, &f);
	writeSample(true);
}

void pbSfxAddTreble(void)
{
	resoFilter_t f;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			return;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}
	
	const int32_t len = x2 - x1;

	setupResoHpFilter(s, &f, 0.27, 0, true);

	double *dSmp = (double *)malloc(len * sizeof (double));
	if (dSmp == NULL)
	{
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	fillSampleUndo(KEEP_SAMPLE_MARK);

	pauseAudio();
	unfixSample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr16[i];
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
			dSmp[i] = (double)ptr8[i];
	}

	if (!normalization)
	{
		for (int32_t i = 0; i < len; i++)
		{
			double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

			f.inTmp[1] = f.inTmp[0];
			f.inTmp[0] = dSmp[i];

			f.outTmp[1] = f.outTmp[0];
			f.outTmp[0] = out;

			dSmp[i] = out;
		}

		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = ptr16[i] - (dSmp[i] * 0.25);
				out = CLAMP(out, INT16_MIN, INT16_MAX);

				ptr16[i] = (int16_t)out;
			}
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
			for (int32_t i = 0; i < len; i++)
			{
				double out = ptr8[i] - (dSmp[i] * 0.25);
				out = CLAMP(out, INT8_MIN, INT8_MAX);

				ptr8[i] = (int8_t)out;
			}
		}
	}
	else
	{
		if (s->flags & SAMPLE_16BIT)
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + x1;

			double peak = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

				f.inTmp[1] = f.inTmp[0];
				f.inTmp[0] = dSmp[i];

				f.outTmp[1] = f.outTmp[0];
				f.outTmp[0] = out;

				dSmp[i] = out;
				double treble = ptr16[i] - (out * 0.25);

				const double outAbs = fabs(treble);
				if (outAbs > peak)
					peak = outAbs;
			}

			double scale = INT16_MAX / peak;
			for (int32_t i = 0; i < len; i++)
				ptr16[i] = (int16_t)((ptr16[i] - (dSmp[i] * 0.25)) * scale);
		}
		else
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + x1;

			double peak = 0.0;
			for (int32_t i = 0; i < len; i++)
			{
				double out = (f.a1*dSmp[i]) + (f.a2*f.inTmp[0]) + (f.a3*f.inTmp[1]) - (f.b1*f.outTmp[0]) - (f.b2*f.outTmp[1]);

				f.inTmp[1] = f.inTmp[0];
				f.inTmp[0] = dSmp[i];

				f.outTmp[1] = f.outTmp[0];
				f.outTmp[0] = out;

				dSmp[i] = out;
				double treble = ptr8[i] - (out * 0.25);

				const double outAbs = fabs(treble);
				if (outAbs > peak)
					peak = outAbs;
			}

			double scale = INT8_MAX / peak;
			for (int32_t i = 0; i < len; i++)
				ptr8[i] = (int8_t)((ptr8[i] - (dSmp[i] * 0.25)) * scale);
		}
	}

	free(dSmp);

	fixSample(s);
	resumeAudio();

	writeSample(true);
}

void pbSfxSetAmp(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	int32_t x1, x2;
	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			return;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}
	
	const int32_t len = x2 - x1;

	char ampStr[3+1];
	memset(ampStr, '\0', sizeof (ampStr));
	snprintf(ampStr, sizeof (ampStr), "%d", lastAmp);

	if (inputBox(1, "Change sample amplitude (in percentage, 0..999):", ampStr, sizeof (ampStr)-1) != 1)
		return;

	if (ampStr[0] == '\0')
		return;

	lastAmp = (int32_t)atoi(ampStr);

	fillSampleUndo(KEEP_SAMPLE_MARK);

	pauseAudio();
	unfixSample(s);

	const int32_t mul = (int32_t)round((1 << 22UL) * (lastAmp / 100.0));

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr16[i] * (int32_t)mul) >> 22;
			sample = CLAMP(sample, INT16_MIN, INT16_MAX);
			ptr16[i] = (int16_t)sample;
		}
	}
	else
	{
		int8_t *ptr8 = (int8_t *)s->dataPtr + x1;
		for (int32_t i = 0; i < len; i++)
		{
			int32_t sample = ((int64_t)ptr8[i] * (int32_t)mul) >> 22;
			sample = CLAMP(sample, INT8_MIN, INT8_MAX);
			ptr8[i] = (int8_t)sample;
		}
	}

	fixSample(s);
	resumeAudio();

	writeSample(true);
}

void pbSfxUndo(void)
{
	if (!sampleUndo.filled || sampleUndo.undoInstr != editor.curInstr || sampleUndo.undoSmp != editor.curSmp)
		return;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL)
		return;

	pauseAudio();

	freeSmpData(s);
	s->flags = sampleUndo.flags;
	s->length = sampleUndo.length;
	s->loopStart = sampleUndo.loopStart;
	s->loopLength = sampleUndo.loopLength;

	if (allocateSmpData(s, s->length, !!(s->flags & SAMPLE_16BIT)))
	{
		if (s->flags & SAMPLE_16BIT)
			memcpy(s->dataPtr, sampleUndo.smpData16, s->length * sizeof (int16_t));
		else
			memcpy(s->dataPtr, sampleUndo.smpData8, s->length * sizeof (int8_t));

		fixSample(s);
		resumeAudio();
	}
	else
	{
		resumeAudio();
		okBox(0, "System message", "Not enough memory!", NULL);
	}

	int32_t oldRx1 = smpEd_Rx1;
	int32_t oldRx2 = smpEd_Rx2;

	updateSampleEditorSample();

	if (sampleUndo.keepSampleMark && oldRx1 < oldRx2)
	{
		smpEd_Rx1 = oldRx1;
		smpEd_Rx2 = oldRx2;
		writeSample(false); // redraw sample mark only
	}

	sampleUndo.keepSampleMark = false;
	sampleUndo.filled = false;
}

void hideSampleEffectsScreen(void)
{
	ui.sampleEditorEffectsShown = false;

	hideCheckBox(CB_SAMPFX_NORMALIZATION);
	hidePushButton(PB_SAMPFX_CYCLES_UP);
	hidePushButton(PB_SAMPFX_CYCLES_DOWN);
	hidePushButton(PB_SAMPFX_TRIANGLE);
	hidePushButton(PB_SAMPFX_SAW);
	hidePushButton(PB_SAMPFX_SINE);
	hidePushButton(PB_SAMPFX_SQUARE);
	hidePushButton(PB_SAMPFX_RESO_UP);
	hidePushButton(PB_SAMPFX_RESO_DOWN);
	hidePushButton(PB_SAMPFX_LOWPASS);
	hidePushButton(PB_SAMPFX_HIGHPASS);
	hidePushButton(PB_SAMPFX_SUB_BASS);
	hidePushButton(PB_SAMPFX_SUB_TREBLE);
	hidePushButton(PB_SAMPFX_ADD_BASS);
	hidePushButton(PB_SAMPFX_ADD_TREBLE);
	hidePushButton(PB_SAMPFX_SET_AMP);
	hidePushButton(PB_SAMPFX_UNDO);
	hidePushButton(PB_SAMPFX_XFADE);
	hidePushButton(PB_SAMPFX_BACK);

	drawFramework(0,   346, 115, 54, FRAMEWORK_TYPE1);
	drawFramework(115, 346, 133, 54, FRAMEWORK_TYPE1);
	drawFramework(248, 346,  49, 54, FRAMEWORK_TYPE1);
	drawFramework(297, 346,  56, 54, FRAMEWORK_TYPE1);

	showPushButton(PB_SAMP_PNOTE_UP);
	showPushButton(PB_SAMP_PNOTE_DOWN);
	showPushButton(PB_SAMP_STOP);
	showPushButton(PB_SAMP_PWAVE);
	showPushButton(PB_SAMP_PRANGE);
	showPushButton(PB_SAMP_PDISPLAY);
	showPushButton(PB_SAMP_SHOW_RANGE);
	showPushButton(PB_SAMP_RANGE_ALL);
	showPushButton(PB_SAMP_CLR_RANGE);
	showPushButton(PB_SAMP_ZOOM_OUT);
	showPushButton(PB_SAMP_SHOW_ALL);
	showPushButton(PB_SAMP_SAVE_RNG);
	showPushButton(PB_SAMP_CUT);
	showPushButton(PB_SAMP_COPY);
	showPushButton(PB_SAMP_PASTE);
	showPushButton(PB_SAMP_CROP);
	showPushButton(PB_SAMP_VOLUME);
	showPushButton(PB_SAMP_EFFECTS);

	drawFramework(2, 366,  34, 15, FRAMEWORK_TYPE2);
	textOutShadow(5, 352, PAL_FORGRND, PAL_DSKTOP2, "Play:");
	updateSampleEditor();
}

void pbEffects(void)
{
	hidePushButton(PB_SAMP_PNOTE_UP);
	hidePushButton(PB_SAMP_PNOTE_DOWN);
	hidePushButton(PB_SAMP_STOP);
	hidePushButton(PB_SAMP_PWAVE);
	hidePushButton(PB_SAMP_PRANGE);
	hidePushButton(PB_SAMP_PDISPLAY);
	hidePushButton(PB_SAMP_SHOW_RANGE);
	hidePushButton(PB_SAMP_RANGE_ALL);
	hidePushButton(PB_SAMP_CLR_RANGE);
	hidePushButton(PB_SAMP_ZOOM_OUT);
	hidePushButton(PB_SAMP_SHOW_ALL);
	hidePushButton(PB_SAMP_SAVE_RNG);
	hidePushButton(PB_SAMP_CUT);
	hidePushButton(PB_SAMP_COPY);
	hidePushButton(PB_SAMP_PASTE);
	hidePushButton(PB_SAMP_CROP);
	hidePushButton(PB_SAMP_VOLUME);
	hidePushButton(PB_SAMP_EFFECTS);

	drawFramework(0,   346, 116, 54, FRAMEWORK_TYPE1);
	drawFramework(116, 346, 114, 54, FRAMEWORK_TYPE1);
	drawFramework(230, 346,  67, 54, FRAMEWORK_TYPE1);
	drawFramework(297, 346,  56, 54, FRAMEWORK_TYPE1);

	checkBoxes[CB_SAMPFX_NORMALIZATION].checked = normalization ? true : false;
	showCheckBox(CB_SAMPFX_NORMALIZATION);
	showPushButton(PB_SAMPFX_CYCLES_UP);
	showPushButton(PB_SAMPFX_CYCLES_DOWN);
	showPushButton(PB_SAMPFX_TRIANGLE);
	showPushButton(PB_SAMPFX_SAW);
	showPushButton(PB_SAMPFX_SINE);
	showPushButton(PB_SAMPFX_SQUARE);
	showPushButton(PB_SAMPFX_RESO_UP);
	showPushButton(PB_SAMPFX_RESO_DOWN);
	showPushButton(PB_SAMPFX_LOWPASS);
	showPushButton(PB_SAMPFX_HIGHPASS);
	showPushButton(PB_SAMPFX_SUB_BASS);
	showPushButton(PB_SAMPFX_SUB_TREBLE);
	showPushButton(PB_SAMPFX_ADD_BASS);
	showPushButton(PB_SAMPFX_ADD_TREBLE);
	showPushButton(PB_SAMPFX_SET_AMP);
	showPushButton(PB_SAMPFX_UNDO);
	showPushButton(PB_SAMPFX_XFADE);
	showPushButton(PB_SAMPFX_BACK);

	textOutShadow(4,   352, PAL_FORGRND, PAL_DSKTOP2, "Cycles:");
	drawSampleCycles();

	textOutShadow(121, 352, PAL_FORGRND, PAL_DSKTOP2, "Reson.:");
	drawFilterResonance();

	textOutShadow(135, 386, PAL_FORGRND, PAL_DSKTOP2, "Normalization");

	textOutShadow(235, 352, PAL_FORGRND, PAL_DSKTOP2, "Bass");
	textOutShadow(235, 369, PAL_FORGRND, PAL_DSKTOP2, "Treb.");

	ui.sampleEditorEffectsShown = true;
}
