// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h> // chdir() in UNICHAR_CHDIR()
#endif
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_sample_saver.h"
#include "ft2_mouse.h"
#include "ft2_diskop.h"
#include "ft2_keyboard.h"

static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };
static const char flatNote1Char[12]  = { 'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B' };
static const char flatNote2Char[12]  = { '-', 'b', '-', 'b', '-', '-', 'b', '-', 'b', '-', 'b', '-' };

static char smpEd_SysReqText[64];
static int8_t *smpCopyBuff;
static bool updateLoopsOnMouseUp, writeSampleFlag;
static int32_t smpEd_OldSmpPosLine = -1;
static int32_t smpEd_ViewSize, smpEd_ScrPos, smpCopySize, smpCopyBits;
static int32_t old_Rx1, old_Rx2, old_ViewSize, old_SmpScrPos;
static int32_t lastMouseX, lastMouseY, lastDrawX, lastDrawY, mouseXOffs, curSmpRepS, curSmpRepL;
static double dScrPosScaled, dPos2ScrMul, dScr2SmpPosMul;
static SDL_Thread *thread;

// globals
int32_t smpEd_Rx1 = 0, smpEd_Rx2 = 0;

sampleTyp *getCurSample(void)
{
	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return NULL;

	return &instr[editor.curInstr]->samp[editor.curSmp];
}

// adds wrapped samples after loop/end (for branchless mixer interpolation)
void fixSample(sampleTyp *s)
{
	uint8_t loopType;
	int16_t *ptr16;
	int32_t loopStart, loopLen, loopEnd, len;

	assert(s != NULL);
	if (s->origPek == NULL)
		return; // empty sample

	assert(s->pek != NULL);

	loopType = s->typ & 3;
	if (loopType == 0)
	{
		len = s->len;

		// no loop (don't mess with fixed, fixSpar of fixedPos)

		if (s->typ & 16)
		{
			if (len < 2)
				return;

			len /= 2;
			ptr16 = (int16_t *)s->pek;

			// write new values
			ptr16[-1] = 0;
			ptr16[len+0] = 0;
#ifndef LERPMIX
			ptr16[len+1] = 0;
#endif
		}
		else
		{
			if (len < 1)
				return;

			// write new values
			s->pek[-1] = 0;
			s->pek[len+0] = 0;
#ifndef LERPMIX
			s->pek[len+1] = 0;
#endif
		}

		return;
	}

	if (s->fixed)
		return; // already fixed

	if (loopType == 1)
	{
		// forward loop

		if (s->typ & 16)
		{
			// 16-bit sample

			if (s->repL < 2)
				return;

			loopStart = s->repS / 2;
			loopEnd = (s->repS + s->repL) / 2;

			ptr16 = (int16_t *)s->pek;

			// store old fix position and old values
			s->fixedPos = s->repS + s->repL;
			s->fixedSmp1 = ptr16[loopEnd+0];
#ifndef LERPMIX
			s->fixedSmp2 = ptr16[loopEnd+1];
#endif
			// write new values
			ptr16[loopEnd+0] = ptr16[loopStart+0];
#ifndef LERPMIX
			if (loopStart == 0 && loopEnd > 0)
				ptr16[-1] = ptr16[loopEnd-1];

			ptr16[loopEnd+1] = ptr16[loopStart+1];
#endif
		}
		else
		{
			// 8-bit sample

			if (s->repL < 1)
				return;

			loopStart = s->repS;
			loopEnd = s->repS + s->repL;

			// store old fix position and old values
			s->fixedPos = loopEnd;
			s->fixedSmp1 = s->pek[loopEnd+0];
#ifndef LERPMIX
			s->fixedSmp2 = s->pek[loopEnd+1];
#endif
			// write new values
			s->pek[loopEnd+0] = s->pek[loopStart+0];
#ifndef LERPMIX
			if (loopStart == 0 && loopEnd > 0)
				s->pek[-1] = s->pek[loopEnd-1];

			s->pek[loopEnd+1] = s->pek[loopStart+1];
#endif
		}
	}
	else
	{
		// pingpong loop

		if (s->typ & 16)
		{
			// 16-bit sample

			if (s->repL < 2)
				return;

			loopStart = s->repS / 2;
			loopLen = s->repL/ 2;

			loopEnd = loopStart + loopLen;
			ptr16 = (int16_t *)s->pek;

			// store old fix position and old values
			s->fixedPos = s->repS + s->repL;
			s->fixedSmp1 = ptr16[loopEnd+0];
#ifndef LERPMIX
			s->fixedSmp2 = ptr16[loopEnd+1];
#endif
			// write new values
			ptr16[loopEnd+0] = ptr16[loopEnd-1];
#ifndef LERPMIX
			if (loopStart == 0)
				ptr16[-1] = ptr16[0];

			if (loopLen >= 2)
				ptr16[loopEnd+1] = ptr16[loopEnd-2];
			else
				ptr16[loopEnd+1] = ptr16[loopStart+0];
#endif
		}
		else
		{
			// 8-bit sample

			if (s->repL < 1)
				return;

			loopStart = s->repS;
			loopLen = s->repL;

			loopEnd = loopStart + loopLen;

			// store old fix position and old values
			s->fixedPos = loopEnd;
			s->fixedSmp1 = s->pek[loopEnd+0];
#ifndef LERPMIX
			s->fixedSmp2 = s->pek[loopEnd+1];
#endif
			// write new values
			s->pek[loopEnd+0] = s->pek[loopEnd-1];
#ifndef LERPMIX
			if (loopStart == 0)
				s->pek[-1] = s->pek[0];

			if (loopLen >= 2)
				s->pek[loopEnd+1] = s->pek[loopEnd-2];
			else
				s->pek[loopEnd+1] = s->pek[loopStart+0];
#endif
		}
	}

	s->fixed = true;
}

// reverts wrapped samples after loop/end (for branchless mixer interpolation)
void restoreSample(sampleTyp *s)
{
	int16_t *ptr16;
	int32_t fixedPos16;

	assert(s != NULL);
	if (s->origPek == NULL || s->len == 0 || (s->typ & 3) == 0 || !s->fixed)
		return; // empty sample, no loop or not fixed

	assert(s->pek != NULL);
	s->fixed = false;

	// clear pre-start bytes
	s->pek[-4] = 0;
	s->pek[-3] = 0;
	s->pek[-2] = 0;
	s->pek[-1] = 0;

	if (s->typ & 16)
	{
		// 16-bit sample

		ptr16 = (int16_t *)s->pek;
		fixedPos16 = s->fixedPos >> 1;

		ptr16[fixedPos16+0] = s->fixedSmp1;
#ifndef LERPMIX
		ptr16[fixedPos16+1] = s->fixedSmp2;
#endif
	}
	else
	{
		// 8-bit sample

		s->pek[s->fixedPos+0] = (int8_t)s->fixedSmp1;
#ifndef LERPMIX
		s->pek[s->fixedPos+1] = (int8_t)s->fixedSmp2;
#endif
	}
}

inline int16_t getSampleValueNr(int8_t *ptr, uint8_t typ, int32_t pos)
{
	assert(pos >= 0);
	if (ptr == NULL)
		return 0;

	if (typ & 16)
	{
		assert(!(pos & 1));
		return *(int16_t *)&ptr[pos];
	}
	else
	{
		return ptr[pos];
	}
}

inline void putSampleValueNr(int8_t *ptr, uint8_t typ, int32_t pos, int16_t val)
{
	assert(pos >= 0);
	if (ptr == NULL)
		return;

	if (typ & 16)
	{
		assert(!(pos & 1));
		*(int16_t *)&ptr[pos] = val;
	}
	else
	{
		ptr[pos] = (int8_t)val;
	}
}

void clearCopyBuffer(void)
{
	if (smpCopyBuff != NULL)
	{
		free(smpCopyBuff);
		smpCopyBuff = NULL;
	}

	smpCopySize = 0;
	smpCopyBits = 8;
}

int32_t getSampleMiddleCRate(sampleTyp *s)
{
	int32_t realFineTune = (int32_t)s->fine >> 3; // the FT2 replayer is ASR'ing the finetune to the right by 3
	double dFTune = realFineTune * (1.0 / 16.0); // new range is -16..15

	double dFreq = 8363.0 * exp2((s->relTon + dFTune) * (1.0 / 12.0));
	return (int32_t)(dFreq + 0.5);
}

int32_t getSampleRangeStart(void)
{
	return smpEd_Rx1;
}

int32_t getSampleRangeEnd(void)
{
	return smpEd_Rx2;
}

int32_t getSampleRangeLength(void)
{
	return smpEd_Rx2 - smpEd_Rx1;
}

// for smpPos2Scr() / scr2SmpPos()
static void updateViewSize(void)
{
	if (smpEd_ViewSize == 0)
		dPos2ScrMul = 1.0;
	else
		dPos2ScrMul = (double)SAMPLE_AREA_WIDTH / smpEd_ViewSize;

	dScr2SmpPosMul = smpEd_ViewSize / (double)SAMPLE_AREA_WIDTH;
}

static void updateScrPos(void)
{
	dScrPosScaled = trunc(smpEd_ScrPos * dPos2ScrMul);
}

// sample pos -> screen x pos (if outside of visible area, will return <0 or >=SCREEN_W)
static int32_t smpPos2Scr(int32_t pos)
{
	double dPos;
	sampleTyp *s;

	if (smpEd_ViewSize <= 0)
		return -1;

	s = getCurSample();
	if (s == NULL)
		return -1;

	if (pos > s->len)
		pos = s->len;

	dPos = (pos * dPos2ScrMul) + 0.5; // rounding is needed here (+ 0.5)
	dPos -= dScrPosScaled;

	// this is important, or else the result can mess up in some cases
	dPos = CLAMP(dPos, INT32_MIN, INT32_MAX);
	pos = (int32_t)dPos;

	return pos;
}

// screen x pos -> sample pos
static int32_t scr2SmpPos(int32_t x)
{
	double dPos;
	sampleTyp *s;

	if (smpEd_ViewSize <= 0)
		return 0;

	s = getCurSample();
	if (s == NULL)
		return 0;

	if (x < 0)
		x = 0;

	dPos = (dScrPosScaled + x) * dScr2SmpPosMul;
	x = (int32_t)dPos;

	if (x > s->len)
		x = s->len;

	if (s->typ & 16)
		x &= 0xFFFFFFFE;

	return x;
}

static void fixRepeatGadgets(void)
{
	int32_t repS, repE;
	sampleTyp *s;

	s = getCurSample();
	if (s == NULL || s->len <= 0 || s->pek == NULL || (s->typ & 3) == 0)
	{
		hideSprite(SPRITE_LEFT_LOOP_PIN);
		hideSprite(SPRITE_RIGHT_LOOP_PIN);

		if (editor.ui.sampleEditorShown)
		{
			hexOutBg(536, 375, PAL_FORGRND, PAL_DESKTOP, 0, 8);
			hexOutBg(536, 387, PAL_FORGRND, PAL_DESKTOP, 0, 8);
		}
		return;
	}

	// draw sample loop points

	repS = smpPos2Scr(curSmpRepS);
	repE = smpPos2Scr(curSmpRepS+curSmpRepL);

	// do -8 test because part of the loop sprite sticks out on the left/right

	if (repS >= -8 && repS <= SAMPLE_AREA_WIDTH+8)
		setSpritePos(SPRITE_LEFT_LOOP_PIN, (int16_t)(repS - 8), 174);
	else
		hideSprite(SPRITE_LEFT_LOOP_PIN);

	if (repE >= -8)
	{
		if (repE <= SAMPLE_AREA_WIDTH+8)
			setSpritePos(SPRITE_RIGHT_LOOP_PIN, (int16_t)(repE - 8), 174);
		else
			hideSprite(SPRITE_RIGHT_LOOP_PIN);
	}
	else
	{
		hideSprite(SPRITE_RIGHT_LOOP_PIN);
	}

	if (editor.ui.sampleEditorShown)
	{
		hexOutBg(536, 375, PAL_FORGRND, PAL_DESKTOP, curSmpRepS, 8);
		hexOutBg(536, 387, PAL_FORGRND, PAL_DESKTOP, curSmpRepL, 8);
	}
}

static void fixSampleDrag(void)
{
	sampleTyp *s = getCurSample();
	if (s == NULL)
	{
		setScrollBarPageLength(SB_SAMP_SCROLL, 0);
		setScrollBarEnd(SB_SAMP_SCROLL, 0);
		setScrollBarPos(SB_SAMP_SCROLL, 0, false);
		return;
	}

	setScrollBarPageLength(SB_SAMP_SCROLL, smpEd_ViewSize);
	setScrollBarEnd(SB_SAMP_SCROLL, instr[editor.curInstr]->samp[editor.curSmp].len);
	setScrollBarPos(SB_SAMP_SCROLL, smpEd_ScrPos, false);
}

static bool getCopyBuffer(int32_t size)
{
	if (smpCopyBuff != NULL)
		free(smpCopyBuff);

	if (size > MAX_SAMPLE_LEN)
		size = MAX_SAMPLE_LEN;

	smpCopyBuff = (int8_t *)malloc(size);
	if (smpCopyBuff == NULL)
	{
		smpCopySize = 0;
		return false;
	}

	smpCopySize = size;
	return true;
}

static int32_t SDLCALL copySampleThread(void *ptr)
{
	bool error;
	int8_t *p;
	int16_t destIns, destSmp, sourceIns, sourceSmp;
	sampleTyp *src, *dst;

	(void)ptr;

	error = false;

	destIns = editor.curInstr;
	destSmp = editor.curSmp;
	sourceIns = editor.srcInstr;
	sourceSmp = editor.srcSmp;

	pauseAudio();

	if (instr[destIns] == NULL)
		error = !allocateInstr(destIns);

	if (!error)
	{
		freeSample(destIns, destSmp);

		src = &instr[sourceIns]->samp[sourceSmp];
		dst = &instr[destIns]->samp[destSmp];

		if (instr[sourceIns] != NULL && src->origPek != NULL)
		{
			p = (int8_t *)malloc(src->len + LOOP_FIX_LEN);
			if (p != NULL)
			{
				memcpy(dst, src, sizeof (sampleTyp));
				memcpy(p, src->origPek, src->len + LOOP_FIX_LEN);
				dst->origPek = p;
				dst->pek = dst->origPek + SMP_DAT_OFFSET;
			}
			else error = true;
		}
	}

	resumeAudio();

	if (error)
		okBoxThreadSafe(0, "System message", "Not enough memory!");

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return true;
}

void copySmp(void) // copy sample from srcInstr->srcSmp to curInstr->curSmp
{
	if (editor.curInstr == 0 || (editor.curInstr == editor.srcInstr && editor.curSmp == editor.srcSmp))
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(copySampleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

void xchgSmp(void) // dstSmp <-> srcSmp
{
	sampleTyp *src, *dst, dstTmp;

	if (editor.curInstr == 0 ||
		(editor.curInstr == editor.srcInstr && editor.curSmp == editor.srcSmp) ||
		instr[editor.curInstr] == NULL)
	{
		return;
	}

	src = &instr[editor.curInstr]->samp[editor.srcSmp];
	dst = &instr[editor.curInstr]->samp[editor.curSmp];

	lockMixerCallback();
	dstTmp = *dst;
	*dst = *src;
	*src = dstTmp;
	unlockMixerCallback();

	updateNewSample();
	setSongModifiedFlag();
}

static void writeRange(void)
{
	int32_t start, end, rangeLen;
	uint32_t *ptr32;

	// very first sample (rx1=0,rx2=0) is the "no range" special case
	if (!editor.ui.sampleEditorShown || smpEd_ViewSize == 0 || (smpEd_Rx1 == 0 && smpEd_Rx2 == 0))
		return;

	// test if range is outside of view (passed it by scrolling)
	start = smpPos2Scr(smpEd_Rx1);
	if (start >= SAMPLE_AREA_WIDTH)
		return;

	// test if range is outside of view (passed it by scrolling)
	end = smpPos2Scr(smpEd_Rx2);
	if (end < 0)
		return;

	start = CLAMP(start, 0, SAMPLE_AREA_WIDTH - 1);
	end = CLAMP(end, 0, SAMPLE_AREA_WIDTH - 1);
	rangeLen = (end + 1) - start;

	assert(start+rangeLen <= SCREEN_W);

	ptr32 = &video.frameBuffer[(174 * SCREEN_W) + start];
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++)
	{
		for (int32_t x = 0; x < rangeLen; x++)
			ptr32[x] = video.palette[(ptr32[x] >> 24) ^ 2]; // ">> 24" to get palette, XOR 2 to switch between mark/normal palette

		ptr32 += SCREEN_W;
	}
}

static int8_t getScaledSample(sampleTyp *s, int32_t index)
{
	int8_t *ptr8, sample;
	int16_t *ptr16;
	int32_t tmp32;

	if (s->pek == NULL || index < 0 || index >= s->len)
		return 0; // return center value if overflown (e.g. sample is shorter than screen width)

	if (s->typ & 16)
	{
		ptr16 = (int16_t *)s->pek;

		assert(!(index & 1));

		// restore fixed mixer interpolation sample(s)
		if (s->fixed)
		{
			if (index == s->fixedPos)
				tmp32 = s->fixedSmp1;
#ifndef LERPMIX
			else if (index == s->fixedPos+2)
				tmp32 = s->fixedSmp2;
#endif
			else
				tmp32 = ptr16[index >> 1];
		}
		else
		{
			tmp32 = ptr16[index >> 1];
		}

		sample = (int8_t)((tmp32 * SAMPLE_AREA_HEIGHT) >> 16);
	}
	else
	{
		ptr8 = s->pek;

		// restore fixed mixer interpolation sample(s)
		if (s->fixed)
		{
			if (index == s->fixedPos)
				tmp32 = s->fixedSmp1;
#ifndef LERPMIX
			else if (index == s->fixedPos+1)
				tmp32 = s->fixedSmp2;
#endif
			else
				tmp32 = ptr8[index];
		}
		else
		{
			tmp32 = ptr8[index];
		}

		sample = (int8_t)((tmp32 * SAMPLE_AREA_HEIGHT) >> 8);
	}

	return sample;
}

static void sampleLine(int16_t x1, int16_t x2, int16_t y1, int16_t y2)
{
	int16_t d, x, y, sx, sy, dx, dy;
	uint16_t ax, ay;
	int32_t pitch;
	uint32_t pal1, pal2, pixVal, *dst32;

	// get coefficients
	dx = x2 - x1;
	ax = ABS(dx) * 2;
	sx = SGN(dx);
	dy = y2 - y1;
	ay = ABS(dy) * 2;
	sy = SGN(dy);
	x  = x1;
	y  = y1;

	pal1   = video.palette[PAL_DESKTOP];
	pal2   = video.palette[PAL_FORGRND];
	pixVal = video.palette[PAL_PATTEXT];
	pitch  = sy * SCREEN_W;

	dst32 = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		d = ay - (ax / 2);

		while (true)
		{
			// invert certain colors
			if (*dst32 != pal2)
			{
				if (*dst32 == pal1)
					*dst32 = pal2;
				else
					*dst32 = pixVal;
			}

			if (x == x2)
				break;

			if (d >= 0)
			{
				d -= ax;
				dst32 += pitch;
			}

			x += sx;
			d += ay;
			dst32 += sx;
		}
	}
	else
	{
		d = ax - (ay / 2);

		while (true)
		{
			// invert certain colors
			if (*dst32 != pal2)
			{
				if (*dst32 == pal1)
					*dst32 = pal2;
				else
					*dst32 = pixVal;
			}

			if (y == y2)
				break;

			if (d >= 0)
			{
				d -= ay;
				dst32 += sx;
			}

			y += sy;
			d += ax;
			dst32 += pitch;
		}
	}
}

static void getMinMax16(const void *p, uint32_t scanLen, int16_t *min16, int16_t *max16)
{
#if defined __APPLE__ || defined _WIN32 || defined __amd64__ || (defined __i386__ && defined __SSE2__)
	if (cpu.hasSSE2)
	{
		/* Taken with permission from the OpenMPT project (and slightly modified).
		**
		** SSE2 implementation for min/max finder, packs 8*int16 in a 128-bit XMM register.
		** scanLen = How many samples to process
		*/
		const int16_t *p16;
		uint32_t scanLen8;
		const __m128i *v;
		__m128i minVal, maxVal, minVal2, maxVal2, curVals;

		// Put minimum / maximum in 8 packed int16 values
		minVal = _mm_set1_epi16(32767);
		maxVal = _mm_set1_epi16(-32768);

		scanLen8 = scanLen / 8;
		if (scanLen8 > 0)
		{
			v = (__m128i *)p;
			p = (const __m128i *)p + scanLen8;

			while (scanLen8--)
			{
				curVals = _mm_loadu_si128(v++);
				minVal = _mm_min_epi16(minVal, curVals);
				maxVal = _mm_max_epi16(maxVal, curVals);
			}

			/* Now we have 8 minima and maxima each.
			** Move the upper 4 values to the lower half and compute the minima/maxima of that. */
			minVal2 = _mm_unpackhi_epi64(minVal, minVal);
			maxVal2 = _mm_unpackhi_epi64(maxVal, maxVal);
			minVal = _mm_min_epi16(minVal, minVal2);
			maxVal = _mm_max_epi16(maxVal, maxVal2);

			/* Now we have 4 minima and maxima each.
			** Move the upper 2 values to the lower half and compute the minima/maxima of that. */
			minVal2 = _mm_shuffle_epi32(minVal, _MM_SHUFFLE(1, 1, 1, 1));
			maxVal2 = _mm_shuffle_epi32(maxVal, _MM_SHUFFLE(1, 1, 1, 1));
			minVal = _mm_min_epi16(minVal, minVal2);
			maxVal = _mm_max_epi16(maxVal, maxVal2);

			// Compute the minima/maxima of the both remaining values
			minVal2 = _mm_shufflelo_epi16(minVal, _MM_SHUFFLE(1, 1, 1, 1));
			maxVal2 = _mm_shufflelo_epi16(maxVal, _MM_SHUFFLE(1, 1, 1, 1));
			minVal = _mm_min_epi16(minVal, minVal2);
			maxVal = _mm_max_epi16(maxVal, maxVal2);
		}

		p16 = (const int16_t *)p;
		while (scanLen-- & 7)
		{
			curVals = _mm_set1_epi16(*p16++);
			minVal = _mm_min_epi16(minVal, curVals);
			maxVal = _mm_max_epi16(maxVal, curVals);
		}

		*min16 = (int16_t)_mm_cvtsi128_si32(minVal);
		*max16 = (int16_t)_mm_cvtsi128_si32(maxVal);
	}
	else
#endif
	{
		// non-SSE version (really slow for big samples, especially when scrolling!)
		int16_t smp16, minVal, maxVal, *ptr16;

		minVal = 32767;
		maxVal = -32768;

		ptr16 = (int16_t *)p;
		for (uint32_t i = 0; i < scanLen; i++)
		{
			smp16 = ptr16[i];
			if (smp16 < minVal) minVal = smp16;
			if (smp16 > maxVal) maxVal = smp16;
		}

		*min16 = minVal;
		*max16 = maxVal;
	}
}

static void getMinMax8(const void *p, uint32_t scanLen, int8_t *min8, int8_t *max8)
{
#if defined __APPLE__ || defined _WIN32 || defined __amd64__ || (defined __i386__ && defined __SSE2__)
	if (cpu.hasSSE2)
	{
		/* Taken with permission from the OpenMPT project (and slightly modified).
		**
		** SSE2 implementation for min/max finder, packs 16*int8 in a 128-bit XMM register.
		** scanLen = How many samples to process
		*/
		const int8_t *p8;
		uint32_t scanLen16;
		const __m128i *v;
		__m128i xorVal, minVal, maxVal, minVal2, maxVal2, curVals;

		// Put minimum / maximum in 8 packed int16 values (-1 and 0 because unsigned)
		minVal = _mm_set1_epi8(-1);
		maxVal = _mm_set1_epi8(0);

		// For signed <-> unsigned conversion (_mm_min_epi8/_mm_max_epi8 is SSE4)
		xorVal = _mm_set1_epi8(0x80);

		scanLen16 = scanLen / 16;
		if (scanLen16 > 0)
		{
			v = (__m128i *)p;
			p = (const __m128i *)p + scanLen16;

			while (scanLen16--)
			{
				curVals = _mm_loadu_si128(v++);
				curVals = _mm_xor_si128(curVals, xorVal);
				minVal = _mm_min_epu8(minVal, curVals);
				maxVal = _mm_max_epu8(maxVal, curVals);
			}

			/* Now we have 16 minima and maxima each.
			** Move the upper 8 values to the lower half and compute the minima/maxima of that. */
			minVal2 = _mm_unpackhi_epi64(minVal, minVal);
			maxVal2 = _mm_unpackhi_epi64(maxVal, maxVal);
			minVal = _mm_min_epu8(minVal, minVal2);
			maxVal = _mm_max_epu8(maxVal, maxVal2);

			/* Now we have 8 minima and maxima each.
			** Move the upper 4 values to the lower half and compute the minima/maxima of that. */
			minVal2 = _mm_shuffle_epi32(minVal, _MM_SHUFFLE(1, 1, 1, 1));
			maxVal2 = _mm_shuffle_epi32(maxVal, _MM_SHUFFLE(1, 1, 1, 1));
			minVal = _mm_min_epu8(minVal, minVal2);
			maxVal = _mm_max_epu8(maxVal, maxVal2);

			/* Now we have 4 minima and maxima each.
			** Move the upper 2 values to the lower half and compute the minima/maxima of that. */
			minVal2 = _mm_srai_epi32(minVal, 16);
			maxVal2 = _mm_srai_epi32(maxVal, 16);
			minVal = _mm_min_epu8(minVal, minVal2);
			maxVal = _mm_max_epu8(maxVal, maxVal2);

			// Compute the minima/maxima of the both remaining values
			minVal2 = _mm_srai_epi16(minVal, 8);
			maxVal2 = _mm_srai_epi16(maxVal, 8);
			minVal = _mm_min_epu8(minVal, minVal2);
			maxVal = _mm_max_epu8(maxVal, maxVal2);
		}

		p8 = (const int8_t *)p;
		while (scanLen-- & 15)
		{
			curVals = _mm_set1_epi8(*p8++ ^ 0x80);
			minVal = _mm_min_epu8(minVal, curVals);
			maxVal = _mm_max_epu8(maxVal, curVals);
		}

		*min8 = (int8_t)(_mm_cvtsi128_si32(minVal) ^ 0x80);
		*max8 = (int8_t)(_mm_cvtsi128_si32(maxVal) ^ 0x80);
	}
	else
#endif
	{
		// non-SSE version (really slow for big samples, especially when scrolling!)
		int8_t smp8, minVal, maxVal, *ptr8;

		minVal = 127;
		maxVal = -128;

		ptr8 = (int8_t *)p;
		for (uint32_t i = 0; i < scanLen; i++)
		{
			smp8 = ptr8[i];
			if (smp8 < minVal) minVal = smp8;
			if (smp8 > maxVal) maxVal = smp8;
		}

		*min8 = minVal;
		*max8 = maxVal;
	}
}

static void getSampleDataPeak(sampleTyp *s, int32_t index, int32_t numBytes, int16_t *outMin, int16_t *outMax)
{
	int8_t min8, max8;
	int16_t min16, max16;

	if (numBytes == 0 || s->pek == NULL || s->len <= 0)
	{
		*outMin = SAMPLE_AREA_Y_CENTER;
		*outMax = SAMPLE_AREA_Y_CENTER;
		return;
	}

	if (s->typ & 16)
	{
		// 16-bit sample

		assert(!(index & 1));

		getMinMax16((int16_t *)&s->pek[index], numBytes >> 1, &min16, &max16);

		*outMin = SAMPLE_AREA_Y_CENTER - ((min16 * SAMPLE_AREA_HEIGHT) >> 16);
		*outMax = SAMPLE_AREA_Y_CENTER - ((max16 * SAMPLE_AREA_HEIGHT) >> 16);
	}
	else
	{
		// 8-bit sample

		getMinMax8(&s->pek[index], numBytes, &min8, &max8);

		*outMin = SAMPLE_AREA_Y_CENTER - ((min8 * SAMPLE_AREA_HEIGHT) >> 8);
		*outMax = SAMPLE_AREA_Y_CENTER - ((max8 * SAMPLE_AREA_HEIGHT) >> 8);
	}
}

static void writeWaveform(void)
{
	int16_t x, y1, y2, min, max, oldMin, oldMax;
	int32_t smpIdx, smpNum, smpNumMin;
	uint32_t viewSizeSamples;
	sampleTyp *s;

	// clear sample data area
	memset(&video.frameBuffer[174 * SCREEN_W], 0, SAMPLE_AREA_WIDTH * SAMPLE_AREA_HEIGHT * sizeof (int32_t));

	// draw center line
	hLine(0, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);

	if (instr[editor.curInstr] == NULL || smpEd_ViewSize == 0)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->pek == NULL || s->len == 0)
		return;

	y1 = SAMPLE_AREA_Y_CENTER - getScaledSample(s, scr2SmpPos(0));

	viewSizeSamples = smpEd_ViewSize;
	if (s->typ & 16)
		viewSizeSamples /= 2;

	if (viewSizeSamples <= SAMPLE_AREA_WIDTH)
	{
		// 1:1 or zoomed in
		for (x = 1; x < SAMPLE_AREA_WIDTH; x++)
		{
			y2 = SAMPLE_AREA_Y_CENTER - getScaledSample(s, scr2SmpPos(x));
			sampleLine(x - 1, x, y1, y2);
			y1 = y2;
		}
	}
	else
	{
		// zoomed out

		oldMin = y1;
		oldMax = y1;

		smpNumMin = (s->typ & 16) ? 2 : 1;
		for (x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			smpIdx = scr2SmpPos(x);
			smpNum = scr2SmpPos(x+1) - smpIdx;

			// prevent look-up overflow (yes, this can happen near the end of the sample)
			if (smpIdx+smpNum > s->len)
				smpNum = s->len - smpNum;

			if (smpNum < smpNumMin)
				smpNum = smpNumMin;

			getSampleDataPeak(s, smpIdx, smpNum, &min, &max);

			if (x != 0)
			{
				if (min > oldMax) sampleLine(x - 1, x, oldMax, min);
				if (max < oldMin) sampleLine(x - 1, x, oldMin, max);
			}

			sampleLine(x, x, max, min);

			oldMin = min;
			oldMax = max;
		}
	}
}

void writeSample(bool forceSmpRedraw)
{
	int32_t tmpRx1, tmpRx2;
	sampleTyp *s;

	// update sample loop points for visuals

	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->samp[0];
	else
		s = &instr[editor.curInstr]->samp[editor.curSmp];

	curSmpRepS = s->repS;
	curSmpRepL = s->repL;

	// exchange range variables if x1 is after x2
	if (smpEd_Rx1 > smpEd_Rx2)
	{
		tmpRx2 = smpEd_Rx2;
		smpEd_Rx2 = smpEd_Rx1;
		smpEd_Rx1 = tmpRx2;
	}

	// clamp range
	smpEd_Rx1 = CLAMP(smpEd_Rx1, 0, s->len);
	smpEd_Rx2 = CLAMP(smpEd_Rx2, 0, s->len);

	// sanitize sample scroll position
	if (smpEd_ScrPos+smpEd_ViewSize > s->len)
	{
		smpEd_ScrPos = s->len - smpEd_ViewSize;
		updateScrPos();
	}
 
	if (smpEd_ScrPos < 0)
	{
		smpEd_ScrPos = 0;
		updateScrPos();

		if (smpEd_ViewSize > s->len)
		{
			smpEd_ViewSize = s->len;
			updateViewSize();
		}
	}

	// handle updating
	if (editor.ui.sampleEditorShown)
	{
		// check if we need to redraw sample data
		if (forceSmpRedraw || (old_SmpScrPos != smpEd_ScrPos || old_ViewSize != smpEd_ViewSize))
		{
			if (editor.ui.sampleEditorShown)
				writeWaveform();

			old_SmpScrPos = smpEd_ScrPos;
			old_ViewSize = smpEd_ViewSize;

			if (editor.ui.sampleEditorShown)
				writeRange(); // range was overwritten, draw it again

			smpEd_OldSmpPosLine = -1;

			old_Rx1 = smpEd_Rx1;
			old_Rx2 = smpEd_Rx2;
		}

		// check if we need to write new range
		if (old_Rx1 != smpEd_Rx1 || old_Rx2 != smpEd_Rx2)
		{
			tmpRx1 = smpEd_Rx1;
			tmpRx2 = smpEd_Rx2;

			// remove old range
			smpEd_Rx1 = old_Rx1;
			smpEd_Rx2 = old_Rx2;

			if (editor.ui.sampleEditorShown)
				writeRange();

			// write new range
			smpEd_Rx1 = tmpRx1;
			smpEd_Rx2 = tmpRx2;

			if (editor.ui.sampleEditorShown)
				writeRange();

			old_Rx1 = smpEd_Rx1;
			old_Rx2 = smpEd_Rx2;
		}

		fixRepeatGadgets();
	}

	if (editor.ui.sampleEditorShown)
		fixSampleDrag();

	updateSampleEditor();
}

static void setSampleRange(int32_t start, int32_t end)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL)
	{
		smpEd_Rx1 = 0;
		smpEd_Rx2 = 0;
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];

	if (start < 0)
		start = 0;

	if (end < 0)
		end = 0;

	// kludge so that you can mark the last sample of what we see
	// XXX: This doesn't seem to work properly!
	if (end == SCREEN_W-1)
	{
		if (smpEd_ViewSize < SAMPLE_AREA_WIDTH) // zoomed in
			end += 2;
		else if (smpEd_ScrPos+smpEd_ViewSize >= s->len)
			end++;
	}

	smpEd_Rx1 = scr2SmpPos(start);
	smpEd_Rx2 = scr2SmpPos(end);

	// 2-byte align if sample is 16-bit
	if (s->typ & 16)
	{
		smpEd_Rx1 &= 0xFFFFFFFE;
		smpEd_Rx2 &= 0xFFFFFFFE;
	}
}

void updateSampleEditorSample(void)
{
	smpEd_Rx1 = 0;
	smpEd_Rx2 = 0;

	smpEd_ScrPos = 0;
	updateScrPos();

	if (instr[editor.curInstr] == NULL)
		smpEd_ViewSize = 0;
	else
		smpEd_ViewSize = instr[editor.curInstr]->samp[editor.curSmp].len;

	updateViewSize();

	writeSample(true);
}

void updateSampleEditor(void)
{
	char noteChar1, noteChar2, octaChar;
	uint8_t note, typ;
	int32_t sampleLen;

	if (!editor.ui.sampleEditorShown)
		return;

	if (instr[editor.curInstr] == NULL)
	{
		typ = 0;
		sampleLen = 0;
	}
	else
	{
		typ = instr[editor.curInstr]->samp[editor.curSmp].typ;
		sampleLen = instr[editor.curInstr]->samp[editor.curSmp].len;
	}

	// sample bit depth radio buttons
	uncheckRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);
	if (typ & 16)
		radioButtons[RB_SAMPLE_16BIT].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_SAMPLE_8BIT].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);

	// sample loop radio buttons
	uncheckRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);
	if (typ & 3)
	{
		if (typ & 2)
			radioButtons[RB_SAMPLE_PINGPONG_LOOP].state = RADIOBUTTON_CHECKED;
		else
			radioButtons[RB_SAMPLE_FORWARD_LOOP].state = RADIOBUTTON_CHECKED;
	}
	else
	{
		radioButtons[RB_SAMPLE_NO_LOOP].state = RADIOBUTTON_CHECKED;
	}
	showRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);

	// draw sample play note

	note = (editor.smpEd_NoteNr - 1) % 12;
	if (config.ptnAcc == 0)
	{
		noteChar1 = sharpNote1Char[note];
		noteChar2 = sharpNote2Char[note];
	}
	else
	{
		noteChar1 = flatNote1Char[note];
		noteChar2 = flatNote2Char[note];
	}

	octaChar = '0' + ((editor.smpEd_NoteNr - 1) / 12);

	charOutBg(7,  369, PAL_FORGRND, PAL_BCKGRND, noteChar1);
	charOutBg(15, 369, PAL_FORGRND, PAL_BCKGRND, noteChar2);
	charOutBg(23, 369, PAL_FORGRND, PAL_BCKGRND, octaChar);

	// draw sample display/length

	hexOutBg(536, 350, PAL_FORGRND, PAL_DESKTOP, smpEd_ViewSize, 8);
	hexOutBg(536, 362, PAL_FORGRND, PAL_DESKTOP, sampleLen, 8);
}

void sampPlayNoteUp(void)
{
	if (editor.smpEd_NoteNr < 96)
	{
		editor.smpEd_NoteNr++;
		updateSampleEditor();
	}
}

void sampPlayNoteDown(void)
{
	if (editor.smpEd_NoteNr > 1)
	{
		editor.smpEd_NoteNr--;
		updateSampleEditor();
	}
}

void scrollSampleDataLeft(void)
{
	int32_t scrollAmount, sampleLen;

	if (instr[editor.curInstr] == NULL)
		sampleLen = 0;
	else
		sampleLen = instr[editor.curInstr]->samp[editor.curSmp].len;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	if (mouse.rightButtonPressed)
	{
		scrollAmount = smpEd_ViewSize / 14; // rounded from 16 (70Hz)
		if (scrollAmount < 1)
			scrollAmount = 1;
	}
	else
	{
		scrollAmount = smpEd_ViewSize / 27; // rounded from 32 (70Hz)
		if (scrollAmount < 1)
			scrollAmount = 1;
	}

	smpEd_ScrPos -= scrollAmount;
	if (smpEd_ScrPos < 0)
		smpEd_ScrPos = 0;

	updateScrPos();
}

void scrollSampleDataRight(void)
{
	int32_t scrollAmount, sampleLen;

	if (instr[editor.curInstr] == NULL)
		sampleLen = 0;
	else
		sampleLen = instr[editor.curInstr]->samp[editor.curSmp].len;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	if (mouse.rightButtonPressed)
	{
		scrollAmount = smpEd_ViewSize / 14; // was 16 (70Hz->60Hz)
		if (scrollAmount < 1)
			scrollAmount = 1;
	}
	else
	{
		scrollAmount = smpEd_ViewSize / 27; // was 32 (70Hz->60Hz)
		if (scrollAmount < 1)
			scrollAmount = 1;
	}

	smpEd_ScrPos += scrollAmount;
	if (smpEd_ScrPos+smpEd_ViewSize > sampleLen)
		smpEd_ScrPos = sampleLen - smpEd_ViewSize;

	updateScrPos();
}

void scrollSampleData(uint32_t pos)
{
	int32_t sampleLen;

	if (instr[editor.curInstr] == NULL)
		sampleLen = 0;
	else
		sampleLen = instr[editor.curInstr]->samp[editor.curSmp].len;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	smpEd_ScrPos = (int32_t)pos;
	updateScrPos();
}

void sampPlayWave(void)
{
	playSample(editor.cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0);
}

void sampPlayDisplay(void)
{
	playRange(editor.cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0, smpEd_ScrPos, smpEd_ViewSize);
}

void sampPlayRange(void)
{
	playRange(editor.cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0, smpEd_Rx1, smpEd_Rx2 - smpEd_Rx1);
}

void showRange(void)
{
	sampleTyp *s;

	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->pek == NULL)
		return;

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		smpEd_ViewSize = smpEd_Rx2 - smpEd_Rx1;

		if (s->typ & 16)
		{
			if (smpEd_ViewSize < 4)
				smpEd_ViewSize = 4;
		}
		else if (smpEd_ViewSize < 2)
		{
			smpEd_ViewSize = 2;
		}

		updateViewSize();

		smpEd_ScrPos = smpEd_Rx1;
		updateScrPos();
	}
	else
	{
		okBox(0, "System message", "Cannot show empty range!");
	}
}

void rangeAll(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	smpEd_Rx1 = smpEd_ScrPos;
	smpEd_Rx2 = smpEd_ScrPos + smpEd_ViewSize;
}

static void zoomSampleDataIn(int32_t step, int16_t x)
{
	int32_t tmp32, minViewSize;
	int64_t newScrPos64;
	sampleTyp *s;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];

	minViewSize = (s->typ & 16) ? 4 : 2;
	if (old_ViewSize <= minViewSize)
		return;

	if (step < 1)
		step = 1;

	smpEd_ViewSize = old_ViewSize - (step * 2);
	if (smpEd_ViewSize < minViewSize)
		smpEd_ViewSize = minViewSize;

	updateViewSize();

	tmp32 = (x - (SAMPLE_AREA_WIDTH / 2)) * step;
	tmp32 += SAMPLE_AREA_WIDTH/4; // rounding bias
	tmp32 /= SAMPLE_AREA_WIDTH/2;

	step += tmp32;

	newScrPos64 = old_SmpScrPos + step;
	if (newScrPos64+smpEd_ViewSize > s->len)
		newScrPos64 = s->len - smpEd_ViewSize;

	smpEd_ScrPos = newScrPos64 & 0xFFFFFFFF;
	updateScrPos();
}

static void zoomSampleDataOut(int32_t step, int16_t x)
{
	int32_t tmp32;
	int64_t newViewSize64;
	sampleTyp *s;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];

	if (old_ViewSize == s->len)
		return;

	if (step < 1)
		step = 1;

	newViewSize64 = (int64_t)old_ViewSize + (step * 2);
	if (newViewSize64 > s->len)
	{
		smpEd_ViewSize = s->len;
		smpEd_ScrPos = 0;
	}
	else
	{
		tmp32 = (x - (SAMPLE_AREA_WIDTH / 2)) * step;
		tmp32 += SAMPLE_AREA_WIDTH/4; // rounding bias
		tmp32 /= SAMPLE_AREA_WIDTH/2;

		step += tmp32;

		smpEd_ViewSize = newViewSize64 & 0xFFFFFFFF;

		smpEd_ScrPos = old_SmpScrPos - step;
		if (smpEd_ScrPos < 0)
			smpEd_ScrPos = 0;

		if ((smpEd_ScrPos + smpEd_ViewSize) > s->len)
			smpEd_ScrPos = s->len - smpEd_ViewSize;
	}

	updateViewSize();
	updateScrPos();
}

void mouseZoomSampleDataIn(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	zoomSampleDataIn((old_ViewSize + 5) / 10, mouse.x);
}

void mouseZoomSampleDataOut(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	zoomSampleDataOut((old_ViewSize + 5) / 10, mouse.x);
}

void zoomOut(void)
{
	sampleTyp *s;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];

	if (old_ViewSize == s->len)
		return;

	int32_t tmp32 = old_ViewSize;
	tmp32++; // rounding bias
	tmp32 >>= 1;

	smpEd_ScrPos = old_SmpScrPos - tmp32;
	if (smpEd_ScrPos < 0)
		smpEd_ScrPos = 0;

	smpEd_ViewSize = old_ViewSize * 2;
	if (smpEd_ViewSize < old_ViewSize)
	{
		smpEd_ViewSize = s->len;
		smpEd_ScrPos = 0;
	}
	else if (smpEd_ViewSize+smpEd_ScrPos > s->len)
	{
		smpEd_ViewSize = s->len - smpEd_ScrPos;
	}

	updateViewSize();
	updateScrPos();
}

void showAll(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	smpEd_ScrPos = 0;
	updateScrPos();

	smpEd_ViewSize = instr[editor.curInstr]->samp[editor.curSmp].len;
	updateViewSize();
}

void saveRange(void)
{
	UNICHAR *filenameU;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
	{
		return;
	}

	if (smpEd_Rx1 >= smpEd_Rx2)
	{
		okBox(0, "System message", "No range specified!");
		return;
	}

	smpEd_SysReqText[0] = '\0';
	if (inputBox(1, "Enter filename:", smpEd_SysReqText, sizeof (smpEd_SysReqText) - 1) != 1)
		return;

	if (smpEd_SysReqText[0] == '\0')
	{
		okBox(0, "System message", "Filename can't be empty!");
		return;
	}

	if (smpEd_SysReqText[0] == '.')
	{
		okBox(0, "System message", "The very first character in the filename can't be '.' (dot)!");
		return;
	}

	if (strpbrk(smpEd_SysReqText, "\\/:*?\"<>|") != NULL)
	{
		okBox(0, "System message", "The filename can't contain the following characters: \\ / : * ? \" < > |");
		return;
	}

	switch (editor.sampleSaveMode)
	{
		         case SMP_SAVE_MODE_RAW: changeFilenameExt(smpEd_SysReqText, ".raw", sizeof (smpEd_SysReqText) - 1); break;
		         case SMP_SAVE_MODE_IFF: changeFilenameExt(smpEd_SysReqText, ".iff", sizeof (smpEd_SysReqText) - 1); break;
		default: case SMP_SAVE_MODE_WAV: changeFilenameExt(smpEd_SysReqText, ".wav", sizeof (smpEd_SysReqText) - 1); break;
	}

	filenameU = cp437ToUnichar(smpEd_SysReqText);
	if (filenameU == NULL)
	{
		okBox(0, "System message", "Error converting string locale!");
		return;
	}

	saveSample(filenameU, SAVE_RANGE);
	free(filenameU);
}

static bool cutRange(bool cropMode, int32_t r1, int32_t r2)
{
	int8_t *newPtr;
	int32_t len, repE;
	sampleTyp *s = getCurSample();

	if (s == NULL)
		return false;

	assert(!(s->typ & 16) || (!(r1 & 1) && !(r2 & 1) && !(s->len & 1)));

	if (!cropMode)
	{
		if (editor.curInstr == 0 || s->pek == NULL || s->len == 0)
			return false;

		pauseAudio();
		restoreSample(s);

		if (config.smpCutToBuffer)
		{
			if (!getCopyBuffer(r2 - r1))
			{
				fixSample(s);
				resumeAudio();

				okBoxThreadSafe(0, "System message", "Not enough memory!");
				return false;
			}

			memcpy(smpCopyBuff, &s->pek[r1], r2 - r1);
			smpCopyBits = (s->typ & 16) ? 16 : 8;
		}
	}

	memmove(&s->pek[r1], &s->pek[r2], s->len - r2);

	len = s->len - r2 + r1;
	if (len > 0)
	{
		newPtr = (int8_t *)realloc(s->origPek, len + LOOP_FIX_LEN);
		if (newPtr == NULL)
		{
			freeSample(editor.curInstr, editor.curSmp);
			editor.updateCurSmp = true;

			if (!cropMode)
				resumeAudio();

			okBoxThreadSafe(0, "System message", "Not enough memory!");
			return false;
		}

		s->origPek = newPtr;
		s->pek = s->origPek + SMP_DAT_OFFSET;

		s->len = len;

		repE = s->repS + s->repL;
		if (s->repS > r1)
		{
			s->repS -= r2 - r1;
			if (s->repS < r1)
				s->repS = r1;
		}

		if (repE > r1)
		{
			repE -= r2 - r1;
			if (repE < r1)
				repE = r1;
		}

		s->repL = repE - s->repS;
		if (s->repL < 0)
			s->repL = 0;

		if (s->repS+s->repL > len)
			s->repL = len - s->repS;

		// 2-byte align loop points if sample is 16-bit
		if (s->typ & 16)
		{
			s->repL &= 0xFFFFFFFE;
			s->repS &= 0xFFFFFFFE;
		}

		if (s->repL == 0)
		{
			s->repS = 0;
			s->typ &= ~3; // disable loop
		}

		if (!cropMode)
			fixSample(s);
	}
	else
	{
		freeSample(editor.curInstr, editor.curSmp);
		editor.updateCurSmp = true;
	}

	if (!cropMode)
	{
		resumeAudio();
		setSongModifiedFlag();

		setMouseBusy(false);

		smpEd_Rx2 = r1;
		writeSampleFlag = true;
	}

	return true;
}

static int32_t SDLCALL sampCutThread(void *ptr)
{
	(void)ptr;

	if (!cutRange(false, smpEd_Rx1, smpEd_Rx2))
		okBoxThreadSafe(0, "System message", "Not enough memory! (Disable \"cut to buffer\")");
	else
		writeSampleFlag = true;

	return true;
}

void sampCut(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0 || smpEd_Rx2 == 0 || smpEd_Rx2 < smpEd_Rx1)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampCutThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampCopyThread(void *ptr)
{
	sampleTyp *s = getCurSample();

	(void)ptr;

	assert(s != NULL && (!(s->typ & 16) || (!(smpEd_Rx1 & 1) && !(smpEd_Rx2 & 1))));

	if (!getCopyBuffer(smpEd_Rx2 - smpEd_Rx1))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	restoreSample(s);
	memcpy(smpCopyBuff, &s->pek[smpEd_Rx1], smpEd_Rx2 - smpEd_Rx1);
	fixSample(s);

	smpCopyBits = (s->typ & 16) ? 16 : 8;
	setMouseBusy(false);

	return true;
}

void sampCopy(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->origPek == NULL || s->len <= 0 || smpEd_Rx2 == 0 || smpEd_Rx2 < smpEd_Rx1)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampCopyThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static void pasteOverwrite(sampleTyp *s)
{
	int8_t *p = (int8_t *)malloc(smpCopySize + LOOP_FIX_LEN);
	if (p == NULL)
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return;
	}

	pauseAudio();

	if (s->origPek != NULL)
		free(s->origPek);

	memset(s, 0, sizeof (sampleTyp));

	s->origPek = p;
	s->pek = p + SMP_DAT_OFFSET;

	memcpy(s->pek, smpCopyBuff, smpCopySize);

	s->len = smpCopySize;
	s->vol = 64;
	s->pan = 128;
	s->typ = (smpCopyBits == 16) ? 16 : 0;

	fixSample(s);
	resumeAudio();

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);
}

static void pasteCopiedData(int8_t *pek, int32_t offset, int32_t length, bool smpIs16Bit)
{
	if (smpIs16Bit)
	{
		// destination sample = 16-bit

		if (smpCopyBits == 16)
		{
			// src/dst = equal bits, copy directly
			memcpy(&pek[offset], smpCopyBuff, length);
		}
		else
		{
			// convert copy data to 16-bit then paste
			int16_t *ptr16 = (int16_t *)&pek[offset];
			int32_t len32 = length >> 1;

			for (int32_t i = 0; i < len32; i++)
				ptr16[i] = smpCopyBuff[i] << 8;
		}
	}
	else
	{
		// destination sample = 8-bit

		if (smpCopyBits == 8)
		{
			// src/dst = equal bits, copy directly
			memcpy(&pek[offset], smpCopyBuff, length);
		}
		else
		{
			// convert copy data to 8-bit then paste
			int8_t *ptr8 = (int8_t *)&pek[offset];
			int16_t *ptr16 = (int16_t *)smpCopyBuff;

			for (int32_t i = 0; i < length; i++)
				ptr8[i] = ptr16[i] >> 8;
		}
	}
}

static int32_t SDLCALL sampPasteThread(void *ptr)
{
	int8_t *p;
	int32_t newLength, realCopyLen;
	sampleTyp *s;

	(void)ptr;

	if (instr[editor.curInstr] == NULL && !allocateInstr(editor.curInstr))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	s = getCurSample();

	if (smpEd_Rx2 == 0 || s == NULL || s->pek == NULL)
	{
		pasteOverwrite(s);
		return true;
	}

	bool smpIs16Bit = (s->typ >> 4) & 1;

	assert(!smpIs16Bit || (!(smpEd_Rx1 & 1) && !(smpEd_Rx2 & 1) && !(s->len & 1)));

	if (s->len+smpCopySize > MAX_SAMPLE_LEN)
	{
		okBoxThreadSafe(0, "System message", "Not enough room in sample!");
		return true;
	}

	realCopyLen = smpCopySize;

	if (smpIs16Bit)
	{
		// destination sample is 16-bit

		if (smpCopyBits == 8) // copy buffer is 8-bit, multiply length by 2
			realCopyLen <<= 1;
	}
	else
	{
		// destination sample is 8-bit

		if (smpCopyBits == 16) // copy buffer is 16-bit, divide length by 2
			realCopyLen >>= 1;
	}

	newLength = s->len + realCopyLen - (smpEd_Rx2 - smpEd_Rx1);
	if (newLength <= 0)
		return true;

	if (newLength > MAX_SAMPLE_LEN)
	{
		okBoxThreadSafe(0, "System message", "Not enough room in sample!");
		return true;
	}

	p = (int8_t *)malloc(newLength + LOOP_FIX_LEN);
	if (p == NULL)
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	int8_t *newPek = p + SMP_DAT_OFFSET;

	pauseAudio();
	restoreSample(s);

	// paste left part of original sample
	if (smpEd_Rx1 > 0)
		memcpy(newPek, s->pek, smpEd_Rx1);

	// paste copied data
	pasteCopiedData(newPek, smpEd_Rx1, realCopyLen, smpIs16Bit);

	// paste right part of original sample
	if (smpEd_Rx2 < s->len)
		memmove(&newPek[smpEd_Rx1+realCopyLen], &s->pek[smpEd_Rx2], s->len - smpEd_Rx2);

	free(s->origPek);

	// adjust loop points if necessary
	if (smpEd_Rx2-smpEd_Rx1 != realCopyLen)
	{
		int32_t loopAdjust = realCopyLen - (smpEd_Rx1 - smpEd_Rx2);

		if (s->repS > smpEd_Rx2)
		{
			s->repS += loopAdjust;
			s->repL -= loopAdjust;
		}

		if (s->repS+s->repL > smpEd_Rx2)
			s->repL += loopAdjust;

		if (s->repS > newLength)
		{
			s->repS = 0;
			s->repL = 0;
		}

		if (s->repS+s->repL > newLength)
			s->repL = newLength - s->repS;

		// align loop points if sample is 16-bit
		if (smpIs16Bit)
		{
			s->repL &= 0xFFFFFFFE;
			s->repS &= 0xFFFFFFFE;
		}
	}

	s->len = newLength;
	s->origPek = p;
	s->pek = s->origPek + SMP_DAT_OFFSET;

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	// set new range
	smpEd_Rx2 = smpEd_Rx1 + realCopyLen;

	// align sample marking points if sample is 16-bit
	if (smpIs16Bit)
	{
		smpEd_Rx1 &= 0xFFFFFFFE;
		smpEd_Rx2 &= 0xFFFFFFFE;
	}

	writeSampleFlag = true;
	return true;
}

void sampPaste(void)
{
	if (editor.curInstr == 0 || smpEd_Rx2 < smpEd_Rx1 || smpCopyBuff == NULL || smpCopySize == 0)
		return;

	if (smpEd_Rx2 == 0) // no sample data marked, overwrite sample with copy buffer
	{
		sampleTyp *s = getCurSample();
		if (s != NULL && s->pek != NULL)
		{
			if (okBox(2, "System request", "The current sample is not empty. Do you really want to overwrite it?") != 1)
				return;
		}
	}

	mouseAnimOn();
	thread = SDL_CreateThread(sampPasteThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampCropThread(void *ptr)
{
	int32_t r1, r2;
	sampleTyp *s = getCurSample();

	(void)ptr;

	assert(!(s->typ & 16) || (!(smpEd_Rx1 & 1) && !(smpEd_Rx2 & 1) && !(s->len & 1)));

	r1 = smpEd_Rx1;
	r2 = smpEd_Rx2;

	pauseAudio();
	restoreSample(s);

	if (!cutRange(true, 0, r1) || !cutRange(true, r2 - r1, s->len))
	{
		fixSample(s);
		resumeAudio();
		return true;
	}
	
	fixSample(s);
	resumeAudio();

	r1 = 0;
	r2 = s->len;

	if (s->typ & 16)
		r2 &= 0xFFFFFFFE;

	setSongModifiedFlag();
	setMouseBusy(false);

	smpEd_Rx1 = r1;
	smpEd_Rx2 = r2;
	writeSampleFlag = true;

	return true;
}

void sampCrop(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0 || smpEd_Rx1 >= smpEd_Rx2)
		return;

	if (smpEd_Rx1 == 0 && smpEd_Rx2 >= s->len)
		return; // no need to crop (the whole sample is marked)

	mouseAnimOn();
	thread = SDL_CreateThread(sampCropThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

void sampXFade(void)
{
	bool is16Bit;
	uint8_t t;
	int16_t c ,d;
	int32_t tmp32, i, x1, x2, y1, y2, a, b, d1, d2, d3, dist;
	double dR, dS1, dS2, dS3, dS4;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	assert(!(s->typ & 16) || (!(smpEd_Rx1 & 1) && !(smpEd_Rx2 & 1) && !(s->len & 1)));

	t = s->typ;

	// check if the sample has the loop flag enabled
	if ((t & 3) == 0)
	{
		okBox(0, "System message", "X-Fade can only be used on a loop-enabled sample!");
		return;
	}

	// check if we selected a range
	if (smpEd_Rx2 == 0)
	{
		okBox(0, "System message", "No range selected! Make a small range that includes loop start or loop end.");
		return;
	}

	// check if we selected a valid range length
	if (smpEd_Rx2-smpEd_Rx1 <= 2)
	{
		okBox(0, "System message", "Invalid range!");
		return;
	}

	x1 = smpEd_Rx1;
	x2 = smpEd_Rx2;

	is16Bit = (t & 16) ? true : false;

	if ((t & 3) >= 2)
	{
		// pingpong loop

		y1 = s->repS;
		if (x1 <= y1)
		{
			// first loop point

			if (x2 <= y1 || x2 >= s->repS+s->repL)
			{
				okBox(0, "System message", "Not enough sample data outside loop!");
				return;
			}

			d1 = y1 - x1;
			if (x2-y1 > d1)
				d1 = x2 - y1;

			d2 = y1 - x1;
			d3 = x2 - y1;

			if (d1 < 1 || d2 < 1 || d3 < 1)
			{
				okBox(0, "System message", "Not enough sample data outside loop!");
				return;
			}

			if (y1-d1 < 0 || y1+d1 >= s->len)
			{
				okBox(0, "System message", "Invalid range!");
				return;
			}

			dist = 1;
			if (is16Bit)
				dist++;

			pauseAudio();
			restoreSample(s);

			i = 0;
			while (i < d1)
			{
				a = getSampleValueNr(s->pek, t, y1 + dist * (-i - 1));
				b = getSampleValueNr(s->pek, t, y1 + dist * i);

				dS1 = 1.0 - i / (double)d2; dS2 = 2.0 - dS1;
				dS3 = 1.0 - i / (double)d3; dS4 = 2.0 - dS3;

				tmp32 = (int32_t)round((a * dS2 + b * dS1) / (dS1 + dS2));
				c = (int16_t)tmp32;

				tmp32 = (int32_t)round((b * dS4 + a * dS3) / (dS3 + dS4));
				d = (int16_t)tmp32;

				if (i < d2) putSampleValueNr(s->pek, t, y1 + dist * (-i - 1), c);
				if (i < d3) putSampleValueNr(s->pek, t, y1 + dist * i, d);

				i += dist;
			}

			fixSample(s);
			resumeAudio();
		}
		else
		{
			// last loop point

			y1 += s->repL;
			if (x1 >= y1 || x2 <= y1 || x2 >= s->len)
			{
				okBox(0, "System message", "Not enough sample data outside loop!");
				return;
			}

			d1 = y1 - x1;
			if (x2-y1 > d1)
				d1 = x2 - y1;

			d2 = y1 - x1;
			d3 = x2 - y1;

			if (d1 < 1 || d2 < 1 || d3 < 1)
			{
				okBox(0, "System message", "Not enough sample data outside loop!");
				return;
			}

			if (y1-d1 < 0 || y1+d1 >= s->len)
			{
				okBox(0, "System message", "Invalid range!");
				return;
			}

			dist = is16Bit ? 2 : 1;

			pauseAudio();
			restoreSample(s);

			i = 0;
			while (i < d1)
			{
				a = getSampleValueNr(s->pek, t, y1 - i - dist);
				b = getSampleValueNr(s->pek, t, y1 + i);

				dS1 = 1.0 - i / (double)d2; dS2 = 2.0 - dS1;
				dS3 = 1.0 - i / (double)d3; dS4 = 2.0 - dS3;

				tmp32 = (int32_t)round((a * dS2 + b * dS1) / (dS1 + dS2));
				c = (int16_t)tmp32;

				tmp32 = (int32_t)round((b * dS4 + a * dS3) / (dS3 + dS4));
				d = (int16_t)tmp32;

				if (i < d2) putSampleValueNr(s->pek, t, y1 - i - dist, c);
				if (i < d3) putSampleValueNr(s->pek, t, y1 + i, d);

				i += dist;
			}

			fixSample(s);
			resumeAudio();
		}
	}
	else
	{
		// standard loop

		if (x1 > s->repS)
		{
			x1 -= s->repL;
			x2 -= s->repL;
		}

		if (x1 < 0 || x2 <= x1 || x2 >= s->len)
		{
			okBox(0, "System message", "Not enough sample data outside loop!");
			return;
		}

		i = (x2 - x1 + 1) / 2;
		y1 = s->repS - i;
		y2 = s->repS + s->repL - i;

		if (t & 16)
		{
			y1 &= 0xFFFFFFFE;
			y2 &= 0xFFFFFFFE;
		}

		if (y1 < 0 || y2+(x2-x1) >= s->len)
		{
			okBox(0, "System message", "Invalid range!");
			return;
		}

		d1 = x2 - x1;
		d2 = s->repS - y1;
		d3 = x2 - x1 - d2;

		if (y1+(x2-x1) <= s->repS || d1 == 0 || d3 == 0 || d1 > s->repL)
		{
			okBox(0, "System message", "Not enough sample data outside loop!");
			return;
		}

		dR = (s->repS - i) / (double)(x2 - x1);
		dist = is16Bit ? 2 : 1;

		pauseAudio();
		restoreSample(s);

		i = 0;
		while (i < x2-x1)
		{
			a = getSampleValueNr(s->pek, t, y1 + i);
			b = getSampleValueNr(s->pek, t, y2 + i);

			dS2 = i / (double)d1;
			dS1 = 1.0 - dS2;

			if (y1+i < s->repS)
			{
				dS3 = 1.0 - (1.0 - dR) * i / d2;
				dS4 = dR * i / d2;

				tmp32 = (int32_t)round((a * dS3 + b * dS4) / (dS3 + dS4));
				c = (int16_t)tmp32;

				tmp32 = (int32_t)round((a * dS2 + b * dS1) / (dS1 + dS2));
				d = (int16_t)tmp32;
			}
			else
			{
				dS3 = 1.0 - (1.0 - dR) * (d1 - i) / d3;
				dS4 = dR * (d1 - i) / d3;

				tmp32 = (int32_t)round((a * dS2 + b * dS1) / (dS1 + dS2));
				c = (int16_t)tmp32;

				tmp32 = (int32_t)round((a * dS4 + b * dS3) / (dS3 + dS4));
				d = (int16_t)tmp32;
			}

			putSampleValueNr(s->pek, t, y1 + i, c);
			putSampleValueNr(s->pek, t, y2 + i, d);

			i += dist;
		}

		fixSample(s);
		resumeAudio();
	}

	writeSample(true);
	setSongModifiedFlag();
}

void rbSampleNoLoop(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	lockMixerCallback();
	restoreSample(s);

	s->typ &= ~3;

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

void rbSampleForwardLoop(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	lockMixerCallback();
	restoreSample(s);

	s->typ = (s->typ & ~3) | 1;

	if (s->repL+s->repS == 0)
	{
		s->repS = 0;
		s->repL = s->len;
	}

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

void rbSamplePingpongLoop(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	lockMixerCallback();
	restoreSample(s);

	s->typ = (s->typ & ~3) | 2;

	if (s->repL+s->repS == 0)
	{
		s->repS = 0;
		s->repL = s->len;
	}

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

static int32_t SDLCALL convSmp8Bit(void *ptr)
{
	int8_t *dst8, *newPtr;
	int16_t *src16;
	int32_t i, newLen;
	sampleTyp *s = getCurSample();

	(void)ptr;

	pauseAudio();
	restoreSample(s);

	src16 = (int16_t *)s->pek;
	dst8 = s->pek;

	newLen = s->len >> 1;
	for (i = 0; i < newLen; i++)
		dst8[i] = src16[i] >> 8;

	assert(s->origPek != NULL);

	newPtr = (int8_t *)realloc(s->origPek, newLen + LOOP_FIX_LEN);
	if (newPtr != NULL)
	{
		s->origPek = newPtr;
		s->pek = s->origPek + SMP_DAT_OFFSET;
	}

	s->repL >>= 1;
	s->repS >>= 1;
	s->len >>= 1;
	s->typ &= ~16; // remove 16-bit flag

	fixSample(s);
	resumeAudio();

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return true;
}

void rbSample8bit(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (okBox(2, "System request", "Convert sampledata?") == 1)
	{
		mouseAnimOn();
		thread = SDL_CreateThread(convSmp8Bit, NULL, NULL);
		if (thread == NULL)
		{
			okBox(0, "System message", "Couldn't create thread!");
			return;
		}

		SDL_DetachThread(thread);
		return;
	}
	else
	{
		lockMixerCallback();
		restoreSample(s);

		s->typ &= ~16; // remove 16-bit flag

		fixSample(s);
		unlockMixerCallback();

		updateSampleEditorSample();
		updateSampleEditor();
		setSongModifiedFlag();
	}
}

static int32_t SDLCALL convSmp16Bit(void *ptr)
{
	int8_t *src8, *newPtr;
	int16_t smp16, *dst16;
	int32_t i;
	sampleTyp *s = getCurSample();

	(void)ptr;

	pauseAudio();
	restoreSample(s);

	assert(s->origPek != NULL);

	newPtr = (int8_t *)realloc(s->origPek, (s->len * 2) + LOOP_FIX_LEN);
	if (newPtr == NULL)
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}
	else
	{
		s->origPek = newPtr;
		s->pek = s->origPek + SMP_DAT_OFFSET;
	}

	src8 = s->pek;
	dst16 = (int16_t *)s->pek;

	for (i = s->len-1; i >= 0; i--)
	{
		smp16 = src8[i] << 8;
		dst16[i] = smp16;
	}

	s->len <<= 1;
	s->repL <<= 1;
	s->repS <<= 1;
	s->typ |= 16; // add 16-bit flag

	fixSample(s);
	resumeAudio();

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return true;
}

void rbSample16bit(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (okBox(2, "System request", "Convert sampledata?") == 1)
	{
		mouseAnimOn();
		thread = SDL_CreateThread(convSmp16Bit, NULL, NULL);
		if (thread == NULL)
		{
			okBox(0, "System message", "Couldn't create thread!");
			return;
		}

		SDL_DetachThread(thread);
		return;
	}
	else
	{
		lockMixerCallback();
		restoreSample(s);

		s->typ |= 16; // add 16-bit flag

		// make sure stuff is 2-byte aligned for 16-bit mode
		s->repS &= 0xFFFFFFFE;
		s->repL &= 0xFFFFFFFE;
		s->len &= 0xFFFFFFFE;

		fixSample(s);
		unlockMixerCallback();

		updateSampleEditorSample();
		updateSampleEditor();
		setSongModifiedFlag();
	}
}

void clearSample(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (okBox(1, "System request", "Clear sample?") != 1)
		return;

	freeSample(editor.curInstr, editor.curSmp);
	updateNewSample();
	setSongModifiedFlag();
}

void sampMin(void)
{
	int8_t *newPtr;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	bool hasLoop = s->typ & 3;
	int32_t loopEnd = s->repS + s->repL;

	if (!hasLoop || (loopEnd >= s->len || loopEnd == 0))
	{
		okBox(0, "System message", "Sample is already minimized.");
	}
	else
	{
		lockMixerCallback();

		s->len = loopEnd;

		newPtr = (int8_t *)realloc(s->origPek, s->len + LOOP_FIX_LEN);
		if (newPtr != NULL)
		{
			s->origPek = newPtr;
			s->pek = s->origPek + SMP_DAT_OFFSET;
		}

		unlockMixerCallback();

		updateSampleEditorSample();
		updateSampleEditor();
		setSongModifiedFlag();
	}
}

void sampRepeatUp(void)
{
	int32_t repS, repL, addVal, lenSub;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (s->typ & 16)
	{
		lenSub = 4;
		addVal = 2;
	}
	else
	{
		lenSub = 2;
		addVal = 1;
	}

	repS = curSmpRepS;
	repL = curSmpRepL;

	if (repS < s->len-lenSub)
		repS += addVal;

	if (repS+repL > s->len)
		repL = s->len - repS;

	curSmpRepS = (s->typ & 16) ? (int32_t)(repS & 0xFFFFFFFE) : repS;
	curSmpRepL = (s->typ & 16) ? (int32_t)(repL & 0xFFFFFFFE) : repL;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

void sampRepeatDown(void)
{
	int32_t repS;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (s->typ & 16)
		repS = curSmpRepS - 2;
	else
		repS = curSmpRepS - 1;

	if (repS < 0)
		repS = 0;

	curSmpRepS = (s->typ & 16) ? (int32_t)(repS & 0xFFFFFFFE) : repS;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

void sampReplenUp(void)
{
	int32_t repL;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (s->typ & 16)
		repL = curSmpRepL + 2;
	else
		repL = curSmpRepL + 1;

	if (curSmpRepS+repL > s->len)
		repL = s->len - curSmpRepS;

	curSmpRepL = (s->typ & 16) ? (int32_t)(repL & 0xFFFFFFFE) : repL;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

void sampReplenDown(void)
{
	int32_t repL;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	if (s->typ & 16)
		repL = curSmpRepL - 2;
	else
		repL = curSmpRepL - 1;

	if (repL < 0)
		repL = 0;

	curSmpRepL = (s->typ & 16) ? (int32_t)(repL & 0xFFFFFFFE) : repL;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

void hideSampleEditor(void)
{
	hidePushButton(PB_SAMP_SCROLL_LEFT);
	hidePushButton(PB_SAMP_SCROLL_RIGHT);
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
	hidePushButton(PB_SAMP_XFADE);
	hidePushButton(PB_SAMP_EXIT);
	hidePushButton(PB_SAMP_CLEAR);
	hidePushButton(PB_SAMP_MIN);
	hidePushButton(PB_SAMP_REPEAT_UP);
	hidePushButton(PB_SAMP_REPEAT_DOWN);
	hidePushButton(PB_SAMP_REPLEN_UP);
	hidePushButton(PB_SAMP_REPLEN_DOWN);

	hideRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);
	hideRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);

	hideScrollBar(SB_SAMP_SCROLL);

	editor.ui.sampleEditorShown = false;

	hideSprite(SPRITE_LEFT_LOOP_PIN);
	hideSprite(SPRITE_RIGHT_LOOP_PIN);
}

void exitSampleEditor(void)
{
	hideSampleEditor();

	if (editor.ui.sampleEditorExtShown)
		hideSampleEditorExt();

	showPatternEditor();
}

void showSampleEditor(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideInstEditor();
	hidePatternEditor();
	editor.ui.sampleEditorShown = true;

	drawFramework(0,   329, 632, 17, FRAMEWORK_TYPE1);
	drawFramework(0,   346, 115, 54, FRAMEWORK_TYPE1);
	drawFramework(115, 346, 133, 54, FRAMEWORK_TYPE1);
	drawFramework(248, 346,  49, 54, FRAMEWORK_TYPE1);
	drawFramework(297, 346,  56, 54, FRAMEWORK_TYPE1);
	drawFramework(353, 346,  74, 54, FRAMEWORK_TYPE1);
	drawFramework(427, 346, 205, 54, FRAMEWORK_TYPE1);
	drawFramework(2,   366,  34, 15, FRAMEWORK_TYPE2);

	textOutShadow(5,   352, PAL_FORGRND, PAL_DSKTOP2, "Play:");
	textOutShadow(371, 352, PAL_FORGRND, PAL_DSKTOP2, "No loop");
	textOutShadow(371, 369, PAL_FORGRND, PAL_DSKTOP2, "Forward");
	textOutShadow(371, 386, PAL_FORGRND, PAL_DSKTOP2, "Pingpong");
	textOutShadow(446, 369, PAL_FORGRND, PAL_DSKTOP2, "8-bit");
	textOutShadow(445, 385, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
	textOutShadow(488, 349, PAL_FORGRND, PAL_DSKTOP2, "Display");
	textOutShadow(488, 361, PAL_FORGRND, PAL_DSKTOP2, "Length");
	textOutShadow(488, 375, PAL_FORGRND, PAL_DSKTOP2, "Repeat");
	textOutShadow(488, 387, PAL_FORGRND, PAL_DSKTOP2, "Replen.");

	showPushButton(PB_SAMP_SCROLL_LEFT);
	showPushButton(PB_SAMP_SCROLL_RIGHT);
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
	showPushButton(PB_SAMP_XFADE);
	showPushButton(PB_SAMP_EXIT);
	showPushButton(PB_SAMP_CLEAR);
	showPushButton(PB_SAMP_MIN);
	showPushButton(PB_SAMP_REPEAT_UP);
	showPushButton(PB_SAMP_REPEAT_DOWN);
	showPushButton(PB_SAMP_REPLEN_UP);
	showPushButton(PB_SAMP_REPLEN_DOWN);

	showRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);
	showRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);

	showScrollBar(SB_SAMP_SCROLL);

	// clear two lines that are never written to when the sampler is open
	hLine(0, 173, SAMPLE_AREA_WIDTH, PAL_BCKGRND);
	hLine(0, 328, SAMPLE_AREA_WIDTH, PAL_BCKGRND);

	updateSampleEditor();
	writeSample(true);
}

void toggleSampleEditor(void)
{
	hideInstEditor();

	if (editor.ui.sampleEditorShown)
	{
		exitSampleEditor();
	}
	else
	{
		hidePatternEditor();
		showSampleEditor();
	}
}

static void writeSmpXORLine(int32_t x)
{
	uint32_t *ptr32;

	if (x < 0 || x >= SCREEN_W)
		return;

	ptr32 = &video.frameBuffer[(174 * SCREEN_W) + x];
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++)
	{
		*ptr32 = video.palette[(*ptr32 >> 24) ^ 1]; // ">> 24" to get palette, XOR 1 to switch between normal/inverted mode
		ptr32 += SCREEN_W;
	}
}

static void writeSamplePosLine(void)
{
	uint8_t ins, smp;
	int32_t smpPos, scrPos;
	lastChInstr_t *c;

	assert(editor.curSmpChannel < MAX_VOICES);

	c = &lastChInstr[editor.curSmpChannel];
	if (c->instrNr == 130) // "Play Wave/Range/Display" in Smp. Ed.
	{
		ins = editor.curPlayInstr;
		smp = editor.curPlaySmp;
	}
	else
	{
		ins = c->instrNr;
		smp = c->sampleNr;
	}

	if (editor.curInstr == ins && editor.curSmp == smp)
	{
		smpPos = getSamplePosition(editor.curSmpChannel);
		if (smpPos != -1)
		{
			// convert sample position to screen position
			scrPos = smpPos2Scr(smpPos);
			if (scrPos != -1)
			{
				if (scrPos != smpEd_OldSmpPosLine)
				{
					writeSmpXORLine(smpEd_OldSmpPosLine); // remove old line
					writeSmpXORLine(scrPos); // write new line
				}

				smpEd_OldSmpPosLine = scrPos;
				return;
			}
		}
	}

	if (smpEd_OldSmpPosLine != -1)
		writeSmpXORLine(smpEd_OldSmpPosLine);

	smpEd_OldSmpPosLine = -1;
}

void handleSamplerRedrawing(void)
{
	// update sample editor

	if (!editor.ui.sampleEditorShown || editor.samplingAudioFlag)
		return;

	if (writeSampleFlag)
	{
		writeSampleFlag = false;
		writeSample(true);
	}
	else if (smpEd_Rx1 != old_Rx1 || smpEd_Rx2 != old_Rx2 || smpEd_ScrPos != old_SmpScrPos || smpEd_ViewSize != old_ViewSize)
	{
		writeSample(false);
	}

	writeSamplePosLine();
}

static void setLeftLoopPinPos(int32_t x)
{
	int32_t repS, repL, newPos;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	newPos = scr2SmpPos(x) - curSmpRepS;

	repS = curSmpRepS + newPos;
	repL = curSmpRepL - newPos;

	if (repS < 0)
	{
		repL += repS;
		repS = 0;
	}

	if (repL < 0)
	{
		repL = 0;
		repS = curSmpRepS + curSmpRepL;
	}

	if (s->typ & 16)
	{
		repS &= 0xFFFFFFFE;
		repL &= 0xFFFFFFFE;
	}

	curSmpRepS = repS;
	curSmpRepL = repL;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

static void setRightLoopPinPos(int32_t x)
{
	int32_t repL;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	repL = scr2SmpPos(x) - curSmpRepS;
	if (repL < 0)
		repL = 0;

	if (repL+curSmpRepS > s->len)
		repL = s->len - curSmpRepS;

	if (repL < 0)
		repL = 0;

	if (s->typ & 16)
		repL &= 0xFFFFFFFE;

	curSmpRepL = repL;

	fixRepeatGadgets();
	updateLoopsOnMouseUp = true;
}

static void editSampleData(bool mouseButtonHeld)
{
	int8_t *ptr8;
	int16_t *ptr16;
	int32_t mx, my, tmp32, p, vl, tvl, r, rl, rvl, start, end;
	double dVal;
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	// ported directly from FT2 and slightly modified

	mx = mouse.x;
	my = mouse.y;

	if (!mouseButtonHeld)
	{
		pauseAudio();
		restoreSample(s);
		editor.editSampleFlag = true;

		lastDrawX = scr2SmpPos(mx);
		if (s->typ & 16)
			lastDrawX >>= 1;

		if (my == 250) // center
		{
			lastDrawY = 128;
		}
		else
		{
			dVal = (my - 174) * (256.0 / SAMPLE_AREA_HEIGHT);
			lastDrawY = (int32_t)(dVal + 0.5);
			lastDrawY = CLAMP(lastDrawY, 0, 255);
			lastDrawY ^= 0xFF;
		}

		lastMouseX = mx;
		lastMouseY = my;
	}
	else if (mx == lastMouseX && my == lastMouseY)
	{
		return; // don't continue if we didn't move the mouse
	}

	// kludge so that you can edit the very end of the sample
	if (mx == SCREEN_W-1 && smpEd_ScrPos+smpEd_ViewSize >= s->len)
		mx++;

	if (mx != lastMouseX)
	{
		p = scr2SmpPos(mx);
		if (s->typ & 16)
			p >>= 1;
	}
	else
	{
		p = lastDrawX;
	}

	if (!keyb.leftShiftPressed && my != lastMouseY)
	{
		if (my == 250) // center
		{
			vl = 128;
		}
		else
		{
			dVal = (my - 174) * (256.0 / SAMPLE_AREA_HEIGHT);
			vl = (int32_t)(dVal + 0.5);
			vl = CLAMP(vl, 0, 255);
			vl ^= 0xFF;
		}
	}
	else
	{
		vl = lastDrawY;
	}

	lastMouseX = mx;
	lastMouseY = my;

	r = p;
	rvl = vl;

	// swap x/y if needed
	if (p > lastDrawX)
	{
		// swap x
		tmp32 = p;
		p = lastDrawX;
		lastDrawX = tmp32;

		// swap y
		tmp32 = lastDrawY;
		lastDrawY = vl;
		vl = tmp32;
	}

	if (s->typ & 16)
	{
		// 16-bit

		ptr16 = (int16_t *)s->pek;

		start = p;
		end = lastDrawX+1;

		if (start < 0)
			start = 0;

		tmp32 = s->len >> 1;
		if (end > tmp32)
			end = tmp32;

		if (p == lastDrawX)
		{
			int16_t smpVal = (int16_t)((vl << 8) ^ 0x8000);
			for (rl = start; rl < end; rl++)
				ptr16[rl] = smpVal;
		}
		else
		{
			int32_t y = lastDrawY - vl;
			uint32_t x = lastDrawX - p;

			uint32_t xMul = 0xFFFFFFFF;
			if (x != 0)
				xMul /= x;

			int32_t i = 0;
			for (rl = start; rl < end; rl++)
			{
				tvl = y * i;
				tvl = ((int64_t)tvl * xMul) >> 32; // tvl /= x;
				tvl += vl;
				tvl <<= 8;
				tvl ^= 0x8000;

				ptr16[rl] = (int16_t)tvl;
				i++;
			}
		}
	}
	else
	{
		// 8-bit

		ptr8 = s->pek;

		start = p;
		if (start < 0)
			start = 0;

		end = lastDrawX+1;
		if (end > s->len)
			end = s->len;

		if (p == lastDrawX)
		{
			int8_t smpVal = (int8_t)(vl ^ 0x80);
			for (rl = start; rl < end; rl++)
				ptr8[rl] = smpVal;
		}
		else
		{
			int32_t y = lastDrawY - vl;
			uint32_t x = lastDrawX - p;

			uint32_t xMul = 0xFFFFFFFF;
			if (x != 0)
				xMul /= x;

			int32_t i = 0;
			for (rl = start; rl < end; rl++)
			{
				tvl = y * i;
				tvl = ((int64_t)tvl * xMul) >> 32; // tvl /= x;
				tvl += vl;
				tvl ^= 0x80;

				ptr8[rl] = (int8_t)tvl;
				i++;
			}
		}
	}

	lastDrawY = rvl;
	lastDrawX = r;

	writeSample(true);
}

void handleSampleDataMouseDown(bool mouseButtonHeld)
{
	int32_t tmp, leftLoopPinPos, rightLoopPinPos;

	if (editor.curInstr == 0)
		return;

	if (!mouseButtonHeld)
	{
		editor.ui.rightLoopPinMoving  = false;
		editor.ui.leftLoopPinMoving = false;
		editor.ui.sampleDataOrLoopDrag = -1;

		mouseXOffs = 0;
		lastMouseX = mouse.x;
		lastMouseY = mouse.y;

		mouse.lastUsedObjectType = OBJECT_SMPDATA;

		if (mouse.leftButtonPressed)
		{
			// move loop pins
			if (mouse.y < 183)
			{
				leftLoopPinPos = getSpritePosX(SPRITE_LEFT_LOOP_PIN);
				if (mouse.x >= leftLoopPinPos && mouse.x <= leftLoopPinPos+16)
				{
					mouseXOffs = (leftLoopPinPos + 8) - mouse.x;

					editor.ui.sampleDataOrLoopDrag = true;

					setLeftLoopPinState(true);
					lastMouseX = mouse.x;

					editor.ui.leftLoopPinMoving = true;
					return;
				}
			}
			else if (mouse.y > 318)
			{
				rightLoopPinPos = getSpritePosX(SPRITE_RIGHT_LOOP_PIN);
				if (mouse.x >= rightLoopPinPos && mouse.x <= rightLoopPinPos+16)
				{
					mouseXOffs = (rightLoopPinPos + 8) - mouse.x;

					editor.ui.sampleDataOrLoopDrag = true;

					setRightLoopPinState(true);
					lastMouseX = mouse.x;

					editor.ui.rightLoopPinMoving = true;
					return;
				}
			}

			// mark data
			editor.ui.sampleDataOrLoopDrag = mouse.x;
			lastMouseX = editor.ui.sampleDataOrLoopDrag;
			setSampleRange(editor.ui.sampleDataOrLoopDrag, editor.ui.sampleDataOrLoopDrag);
		}
		else if (mouse.rightButtonPressed)
		{
			// edit data
			editor.ui.sampleDataOrLoopDrag = true;
			editSampleData(false);
		}

		return;
	}

	if (mouse.rightButtonPressed)
	{
		editSampleData(true);
		return;
	}

	if (mouse.x != lastMouseX)
	{
		if (mouse.leftButtonPressed)
		{
			if (editor.ui.leftLoopPinMoving)
			{
				lastMouseX = mouse.x;
				setLeftLoopPinPos(mouseXOffs + lastMouseX);
			}
			else if (editor.ui.rightLoopPinMoving)
			{
				lastMouseX = mouse.x;
				setRightLoopPinPos(mouseXOffs + lastMouseX);
			}
			else if (editor.ui.sampleDataOrLoopDrag >= 0)
			{
				// mark data

				lastMouseX = mouse.x;
				tmp = lastMouseX;

				if (lastMouseX  > editor.ui.sampleDataOrLoopDrag)
					setSampleRange(editor.ui.sampleDataOrLoopDrag, tmp);
				else if (lastMouseX == editor.ui.sampleDataOrLoopDrag)
					setSampleRange(editor.ui.sampleDataOrLoopDrag, editor.ui.sampleDataOrLoopDrag);
				else if (lastMouseX  < editor.ui.sampleDataOrLoopDrag)
					setSampleRange(tmp, editor.ui.sampleDataOrLoopDrag);
			}
		}
	}
}

// SAMPLE EDITOR EXTENSION

void handleSampleEditorExtRedrawing(void)
{
	hexOutBg(35,  96,  PAL_FORGRND, PAL_DESKTOP, smpEd_Rx1, 8);
	hexOutBg(99,  96,  PAL_FORGRND, PAL_DESKTOP, smpEd_Rx2, 8);
	hexOutBg(99,  110, PAL_FORGRND, PAL_DESKTOP, smpEd_Rx2 - smpEd_Rx1, 8);
	hexOutBg(99,  124, PAL_FORGRND, PAL_DESKTOP, smpCopySize, 8);
	hexOutBg(226, 96,  PAL_FORGRND, PAL_DESKTOP, editor.srcInstr, 2);
	hexOutBg(274, 96,  PAL_FORGRND, PAL_DESKTOP, editor.srcSmp, 2);
	hexOutBg(226, 109, PAL_FORGRND, PAL_DESKTOP, editor.curInstr, 2);
	hexOutBg(274, 109, PAL_FORGRND, PAL_DESKTOP, editor.curSmp, 2);
}

void drawSampleEditorExt(void)
{
	drawFramework(0,    92, 158, 44, FRAMEWORK_TYPE1);
	drawFramework(0,   136, 158, 37, FRAMEWORK_TYPE1);
	drawFramework(158,  92, 133, 81, FRAMEWORK_TYPE1);

	textOutShadow( 4,  96, PAL_FORGRND, PAL_DSKTOP2, "Rng.:");
	charOutShadow(91,  95, PAL_FORGRND, PAL_DSKTOP2, '-');
	textOutShadow( 4, 109, PAL_FORGRND, PAL_DSKTOP2, "Range size");
	textOutShadow( 4, 123, PAL_FORGRND, PAL_DSKTOP2, "Copy buf. size");

	textOutShadow(162,  95, PAL_FORGRND, PAL_DSKTOP2, "Src.instr.");
	textOutShadow(245,  96, PAL_FORGRND, PAL_DSKTOP2, "smp.");
	textOutShadow(162, 109, PAL_FORGRND, PAL_DSKTOP2, "Dest.instr.");
	textOutShadow(245, 109, PAL_FORGRND, PAL_DSKTOP2, "smp.");

	showPushButton(PB_SAMP_EXT_CLEAR_COPYBUF);
	showPushButton(PB_SAMP_EXT_CONV);
	showPushButton(PB_SAMP_EXT_ECHO);
	showPushButton(PB_SAMP_EXT_BACKWARDS);
	showPushButton(PB_SAMP_EXT_CONV_W);
	showPushButton(PB_SAMP_EXT_MORPH);
	showPushButton(PB_SAMP_EXT_COPY_INS);
	showPushButton(PB_SAMP_EXT_COPY_SMP);
	showPushButton(PB_SAMP_EXT_XCHG_INS);
	showPushButton(PB_SAMP_EXT_XCHG_SMP);
	showPushButton(PB_SAMP_EXT_RESAMPLE);
	showPushButton(PB_SAMP_EXT_MIX_SAMPLE);
}

void showSampleEditorExt(void)
{
	hideTopScreen();
	showTopScreen(false);

	if (editor.ui.extended)
		exitPatternEditorExtended();

	if (!editor.ui.sampleEditorShown)
		showSampleEditor();

	editor.ui.sampleEditorExtShown = true;
	editor.ui.scopesShown = false;
	drawSampleEditorExt();
}

void hideSampleEditorExt(void)
{
	editor.ui.sampleEditorExtShown = false;

	hidePushButton(PB_SAMP_EXT_CLEAR_COPYBUF);
	hidePushButton(PB_SAMP_EXT_CONV);
	hidePushButton(PB_SAMP_EXT_ECHO);
	hidePushButton(PB_SAMP_EXT_BACKWARDS);
	hidePushButton(PB_SAMP_EXT_CONV_W);
	hidePushButton(PB_SAMP_EXT_MORPH);
	hidePushButton(PB_SAMP_EXT_COPY_INS);
	hidePushButton(PB_SAMP_EXT_COPY_SMP);
	hidePushButton(PB_SAMP_EXT_XCHG_INS);
	hidePushButton(PB_SAMP_EXT_XCHG_SMP);
	hidePushButton(PB_SAMP_EXT_RESAMPLE);
	hidePushButton(PB_SAMP_EXT_MIX_SAMPLE);

	editor.ui.scopesShown = true;
	drawScopeFramework();
}

void toggleSampleEditorExt(void)
{
	if (editor.ui.sampleEditorExtShown)
		hideSampleEditorExt();
	else
		showSampleEditorExt();
}

static int32_t SDLCALL sampleBackwardsThread(void *ptr)
{
	int8_t tmp8, *ptrStart, *ptrEnd;
	int16_t tmp16, *ptrStart16, *ptrEnd16;
	sampleTyp *s = getCurSample();

	(void)ptr;

	if (s->typ & 16)
	{
		if (smpEd_Rx1 >= smpEd_Rx2)
		{
			ptrStart16 = (int16_t *)s->pek;
			ptrEnd16 = (int16_t *)&s->pek[s->len-2];
		}
		else
		{
			ptrStart16 = (int16_t *)&s->pek[smpEd_Rx1];
			ptrEnd16 = (int16_t *)&s->pek[smpEd_Rx2-2];
		}

		pauseAudio();
		restoreSample(s);

		while (ptrStart16 < ptrEnd16)
		{
			tmp16 = *ptrStart16;
			*ptrStart16++ = *ptrEnd16;
			*ptrEnd16-- = tmp16;
		}

		fixSample(s);
		resumeAudio();
	}
	else
	{
		if (smpEd_Rx1 >= smpEd_Rx2)
		{
			ptrStart = s->pek;
			ptrEnd = &s->pek[s->len-1];
		}
		else
		{
			ptrStart = &s->pek[smpEd_Rx1];
			ptrEnd = &s->pek[smpEd_Rx2-1];
		}

		pauseAudio();
		restoreSample(s);

		while (ptrStart < ptrEnd)
		{
			tmp8 = *ptrStart;
			*ptrStart++ = *ptrEnd;
			*ptrEnd-- = tmp8;
		}

		fixSample(s);
		resumeAudio();
	}

	setSongModifiedFlag();
	setMouseBusy(false);
	writeSampleFlag = true;

	return true;
}

void sampleBackwards(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len < 2)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampleBackwardsThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampleConvThread(void *ptr)
{
	int8_t *ptr8;
	int16_t *ptr16;
	int32_t i, len;
	sampleTyp *s = getCurSample();

	(void)ptr;

	pauseAudio();
	restoreSample(s);

	if (s->typ & 16)
	{
		len = s->len / 2;
		ptr16 = (int16_t *)s->pek;

		for (i = 0; i < len; i++)
			ptr16[i] ^= 0x8000;
	}
	else
	{
		len = s->len;
		ptr8 = s->pek;

		for (i = 0; i < len; i++)
			ptr8[i] ^= 0x80;
	}

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);
	writeSampleFlag = true;

	return true;
}

void sampleConv(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampleConvThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampleConvWThread(void *ptr)
{
	int8_t *ptr8, tmp;
	int32_t len;
	sampleTyp *s = getCurSample();

	(void)ptr;

	pauseAudio();
	restoreSample(s);

	len = s->len / 2;
	ptr8 = s->pek;

	for (int32_t i = 0; i < len; i++)
	{
		tmp = ptr8[0];
		ptr8[0] = ptr8[1];
		ptr8[1] = tmp;

		ptr8 += 2;
	}

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);
	writeSampleFlag = true;

	return true;
}

void sampleConvW(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampleConvWThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL fixDCThread(void *ptr)
{
	int8_t *ptr8;
	int16_t *ptr16;
	int32_t i, len, smpSub, smp32;
	int64_t averageDC;
	sampleTyp *s = getCurSample();

	(void)ptr;

	averageDC = 0;

	if (s->typ & 16)
	{
		if (smpEd_Rx1 >= smpEd_Rx2)
		{
			assert(!(s->len & 1));

			ptr16 = (int16_t *)s->pek;
			len = s->len >> 1;
		}
		else
		{
			assert(!(smpEd_Rx1 & 1));
			assert(!(smpEd_Rx2 & 1));

			ptr16 = (int16_t *)&s->pek[smpEd_Rx1];
			len = (smpEd_Rx2 - smpEd_Rx1) >> 1;
		}

		if (len < 0 || len > s->len>>1)
		{
			setMouseBusy(false);
			return true;
		}

		pauseAudio();
		restoreSample(s);

		for (i = 0; i < len; i++)
			averageDC += ptr16[i];
		averageDC /= len;

		smpSub = (int32_t)averageDC;
		for (i = 0; i < len; i++)
		{
			smp32 = ptr16[i] - smpSub;
			CLAMP16(smp32);
			ptr16[i] = (int16_t)smp32;
		}

		fixSample(s);
		resumeAudio();
	}
	else
	{
		if (smpEd_Rx1 >= smpEd_Rx2)
		{
			ptr8 = s->pek;
			len = s->len;
		}
		else
		{
			ptr8 = &s->pek[smpEd_Rx1];
			len = smpEd_Rx2 - smpEd_Rx1;
		}

		if (len < 0 || len > s->len)
		{
			setMouseBusy(false);
			return true;
		}

		pauseAudio();
		restoreSample(s);

		for (i = 0; i < len; i++)
			averageDC += ptr8[i];
		averageDC /= len;

		smpSub = (int32_t)averageDC;
		for (i = 0; i < len; i++)
		{
			smp32 = ptr8[i] - smpSub;
			CLAMP8(smp32);
			ptr8[i] = (int8_t)smp32;
		}

		fixSample(s);
		resumeAudio();
	}

	writeSampleFlag = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return true;
}

void fixDC(void)
{
	sampleTyp *s = getCurSample();

	if (s == NULL || s->pek == NULL || s->len <= 0)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(fixDCThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

void smpEdStop(void)
{
	// safely kills all voices
	lockMixerCallback();
	unlockMixerCallback();
}

void testSmpEdMouseUp(void) // used for setting new loop points
{
	sampleTyp *s;

	if (updateLoopsOnMouseUp)
	{
		updateLoopsOnMouseUp = false;

		s = getCurSample();
		if (s == NULL)
			return;

		if (s->repS != curSmpRepS || s->repL != curSmpRepL)
		{
			lockMixerCallback();
			restoreSample(s);

			setSongModifiedFlag();

			s->repS = curSmpRepS;
			s->repL = curSmpRepL;

			fixSample(s);
			unlockMixerCallback();

			writeSample(true);
		}
	}
}
