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
#if defined _WIN32 || defined __amd64__ || (defined __i386__ && defined __SSE2__)
#include <emmintrin.h>
#endif
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "scopes/ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_sample_saver.h"
#include "ft2_mouse.h"
#include "ft2_diskop.h"
#include "ft2_keyboard.h"
#include "ft2_structs.h"
#include "ft2_random.h"
#include "ft2_replayer.h"
#include "ft2_smpfx.h"
#include "mixer/ft2_mix_interpolation.h" // SINC_TAPS, SINC_NEGATIVE_TAPS

static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };
static const char flatNote1Char[12]  = { 'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B' };
static const char flatNote2Char[12]  = { '-', 'b', '-', 'b', '-', '-', 'b', '-', 'b', '-', 'b', '-' };

static char smpEd_SysReqText[64];
static int8_t *smpCopyBuff;
static bool updateLoopsOnMouseUp, writeSampleFlag, smpCopyDidCopyWholeSample;
static int32_t smpEd_OldSmpPosLine = -1;
static int32_t smpEd_ViewSize, smpEd_ScrPos, smpCopySize, smpCopyBits;
static int32_t old_Rx1, old_Rx2, old_ViewSize, old_SmpScrPos;
static int32_t lastMouseX, lastMouseY, lastDrawX, lastDrawY, mouseXOffs, curSmpLoopStart, curSmpLoopLength;
static double dScrPosScaled, dPos2ScrMul, dScr2SmpPosMul;
static sample_t smpCopySample;
static SDL_Thread *thread;

// globals
int32_t smpEd_Rx1 = 0, smpEd_Rx2 = 0;

// allocs sample with proper alignment and padding for branchless resampling interpolation
bool allocateSmpData(sample_t *s, int32_t length, bool sample16Bit)
{
	if (sample16Bit)
		length <<= 1;

	s->origDataPtr = (int8_t *)malloc(length + SAMPLE_PAD_LENGTH);
	if (s->origDataPtr == NULL)
	{
		s->dataPtr = NULL;
		return false;
	}

	s->dataPtr = s->origDataPtr + SMP_DAT_OFFSET;
	return true;
}

bool allocateSmpDataPtr(smpPtr_t *sp, int32_t length, bool sample16Bit)
{
	if (sample16Bit)
		length <<= 1;

	int8_t *newPtr = (int8_t *)malloc(length + SAMPLE_PAD_LENGTH);
	if (newPtr == NULL)
		return false;

	sp->origPtr = newPtr;

	sp->ptr = sp->origPtr + SMP_DAT_OFFSET;
	return true;
}

// reallocs sample with proper alignment and padding for branchless resampling interpolation
bool reallocateSmpData(sample_t *s, int32_t length, bool sample16Bit)
{
	if (s->origDataPtr == NULL)
		return allocateSmpData(s, length, sample16Bit);

	if (sample16Bit)
		length <<= 1;

	int8_t *newPtr = (int8_t *)realloc(s->origDataPtr, length + SAMPLE_PAD_LENGTH);
	if (newPtr == NULL)
		return false;

	s->origDataPtr = newPtr;
	s->dataPtr = s->origDataPtr + SMP_DAT_OFFSET;

	return true;
}

// reallocs sample with proper alignment and padding for branchless resampling interpolation
bool reallocateSmpDataPtr(smpPtr_t *sp, int32_t length, bool sample16Bit)
{
	if (sp->origPtr == NULL)
		return allocateSmpDataPtr(sp, length, sample16Bit);

	if (sample16Bit)
		length <<= 1;

	int8_t *newPtr = (int8_t *)realloc(sp->origPtr, length + SAMPLE_PAD_LENGTH);
	if (newPtr == NULL)
		return false;

	sp->origPtr = newPtr;
	sp->ptr = sp->origPtr + SMP_DAT_OFFSET;

	return true;
}

void setSmpDataPtr(sample_t *s, smpPtr_t *sp)
{
	s->origDataPtr = sp->origPtr;
	s->dataPtr = sp->ptr;
}

void freeSmpDataPtr(smpPtr_t *sp)
{
	if (sp->origPtr != NULL)
	{
		free(sp->origPtr);
		sp->origPtr = NULL;
	}

	sp->ptr = NULL;
}

void freeSmpData(sample_t *s)
{
	if (s->origDataPtr != NULL)
	{
		free(s->origDataPtr);
		s->origDataPtr = NULL;
	}

	s->dataPtr = NULL;
	s->isFixed = false;
}

bool cloneSample(sample_t *src, sample_t *dst)
{
	smpPtr_t sp;

	freeSmpData(dst);

	if (src == NULL)
	{
		memset(dst, 0, sizeof (sample_t));
	}
	else
	{
		memcpy(dst, src, sizeof (sample_t));

		// zero out stuff that wasn't supposed to be cloned
		dst->origDataPtr = dst->dataPtr = NULL;
		dst->isFixed = false;
		dst->fixedPos = 0;

		// if source sample isn't empty, allocate room and copy it over (and fix it)
		if (src->length > 0 && src->dataPtr != NULL)
		{
			bool sample16Bit = !!(src->flags & SAMPLE_16BIT);
			if (!allocateSmpDataPtr(&sp, src->length, sample16Bit))
			{
				dst->length = 0;
				return false;
			}

			setSmpDataPtr(dst, &sp);
			memcpy(dst->dataPtr, src->dataPtr, src->length << sample16Bit);
			fixSample(dst);
		}
	}

	return true;
}

sample_t *getCurSample(void)
{
	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return NULL;

	return &instr[editor.curInstr]->smp[editor.curSmp];
}

void sanitizeSample(sample_t *s)
{
	if (s == NULL)
		return;

	// if a sample has both forward loop and pingpong loop set, it means pingpong loop (FT2 mixer behavior)
	if (GET_LOOPTYPE(s->flags) == (LOOP_FWD | LOOP_BIDI))
		s->flags &= ~LOOP_FWD; // remove forward loop flag

	if (s->volume > 64)
		s->volume = 64;

	s->relativeNote = CLAMP(s->relativeNote, -48, 71);
	s->length = CLAMP(s->length, 0, MAX_SAMPLE_LEN);

	if (s->loopStart < 0 || s->loopLength <= 0 || s->loopStart+s->loopLength > s->length)
	{
		s->loopStart = 0;
		s->loopLength = 0;
		DISABLE_LOOP(s->flags);
	}
}

static int32_t myMod(int32_t a, int32_t b) // works on negative numbers!
{
	int32_t c = a % b;
	return (c < 0) ? (c + b) : c;
}

// modifies samples before index 0, and after loop/end (for branchless mixer interpolation (kinda))
void fixSample(sample_t *s)
{
	int32_t pos;
	bool backwards;

	assert(s != NULL);
	if (s->dataPtr == NULL || s->length <= 0)
	{
		s->isFixed = false;
		s->fixedPos = 0;
		return; // empty sample
	}

	const bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	int16_t *ptr16 = (int16_t *)s->dataPtr;
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	int32_t length = s->length;
	int32_t loopStart = s->loopStart;
	int32_t loopLength = s->loopLength;
	int32_t loopEnd = s->loopStart + s->loopLength;

	// treat loop as disabled if loopLen == 0 (FT2 does this)
	if (loopType != 0 && loopLength <= 0)
	{
		loopType = 0;
		loopStart = loopLength = loopEnd = 0;
	}

	/* All negative taps should be equal to the first sample point when at sampling
	** position #0 (on sample trigger), until an eventual loop cycle, where we do
	** a special left edge case with replaced tap data.
	** The sample pointer is offset and has allocated data before it, so this is
	** safe.
	*/
	if (sample16Bit)
	{
		for (int32_t i = 0; i < MAX_LEFT_TAPS; i++)
			ptr16[i-MAX_LEFT_TAPS] = ptr16[0];
	}
	else
	{
		for (int32_t i = 0; i < MAX_LEFT_TAPS; i++)
			s->dataPtr[i-MAX_LEFT_TAPS] = s->dataPtr[0];
	}

	if (loopType == LOOP_OFF) // no loop
	{
		if (sample16Bit)
		{
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
				ptr16[length+i] = ptr16[length-1];
		}
		else
		{
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
				s->dataPtr[length+i] = s->dataPtr[length-1];
		}

		s->fixedPos = 0; // this value is not used for non-looping samples, set to zero
		s->isFixed = false; // no fixed samples inside actual sample data
		return;
	}

	s->fixedPos = loopEnd;
	s->isFixed = true;

	if (loopType == LOOP_FWD) // forward loop
	{
		if (sample16Bit)
		{
			// left edge (we need MAX_TAPS amount of taps starting from the center tap)
			for (int32_t i = -MAX_LEFT_TAPS; i < MAX_TAPS; i++)
			{
				pos = loopStart + myMod(i, loopLength);
				s->leftEdgeTapSamples16[MAX_LEFT_TAPS+i] = ptr16[pos];
			}

			// right edge (change actual sample data since data after loop is never used)
			pos = loopStart;
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			{
				s->fixedSmp[i] = ptr16[loopEnd+i];
				ptr16[loopEnd+i] = ptr16[pos];

				if (++pos >= loopEnd)
					pos -= loopLength;
			}
		}
		else // 8-bit
		{
			// left edge (we need MAX_TAPS amount of taps starting from the center tap)
			for (int32_t i = -MAX_LEFT_TAPS; i < MAX_TAPS; i++)
			{
				pos = loopStart + myMod(i, loopLength);
				s->leftEdgeTapSamples8[MAX_LEFT_TAPS+i] = s->dataPtr[pos];
			}

			// right edge (change actual sample data since data after loop is never used)
			pos = loopStart;
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			{
				s->fixedSmp[i] = s->dataPtr[loopEnd+i];
				s->dataPtr[loopEnd+i] = s->dataPtr[pos];

				if (++pos >= loopEnd)
					pos -= loopLength;
			}
		}
	}
	else // pingpong loop
	{
		if (sample16Bit)
		{
			// left edge (positive taps, we need MAX_TAPS amount of taps starting from the center tap)
			pos = loopStart;
			backwards = false;
			for (int32_t i = 0; i < MAX_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd) // forwards
				{
					pos = loopEnd-1;
					backwards = true;
				}

				s->leftEdgeTapSamples16[MAX_LEFT_TAPS+i] = ptr16[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}

			// left edge (negative taps)
			for (int32_t i = 0; i < MAX_LEFT_TAPS; i++)
				s->leftEdgeTapSamples16[(MAX_LEFT_TAPS-1)-i] = s->leftEdgeTapSamples16[MAX_LEFT_TAPS+1+i];

			// right edge (change actual sample data since data after loop is never used)
			pos = loopEnd-1;
			backwards = true;
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd) // forwards
				{
					pos = loopEnd-1;
					backwards = true;
				}

				s->fixedSmp[i] = ptr16[loopEnd+i];
				ptr16[loopEnd+i] = ptr16[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}
		}
		else // 8-bit
		{
			// left edge (positive taps, we need MAX_TAPS amount of taps starting from the center tap)
			pos = loopStart;
			backwards = false;
			for (int32_t i = 0; i < MAX_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd) // forwards
				{
					pos = loopEnd-1;
					backwards = true;
				}

				s->leftEdgeTapSamples8[MAX_LEFT_TAPS+i] = s->dataPtr[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}

			// left edge (negative taps)
			for (int32_t i = 0; i < MAX_LEFT_TAPS; i++)
				s->leftEdgeTapSamples8[(MAX_LEFT_TAPS-1)-i] = s->leftEdgeTapSamples8[MAX_LEFT_TAPS+1+i];

			// right edge (change actual sample data since data after loop is never used)
			pos = loopEnd-1;
			backwards = true;
			for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			{
				if (backwards)
				{
					if (pos < loopStart)
					{
						pos = loopStart;
						backwards = false;
					}
				}
				else if (pos >= loopEnd) // forwards
				{
					pos = loopEnd-1;
					backwards = true;
				}

				s->fixedSmp[i] = s->dataPtr[loopEnd+i];
				s->dataPtr[loopEnd+i] = s->dataPtr[pos];

				if (backwards)
					pos--;
				else
					pos++;
			}
		}
	}
}

// restores interpolation tap samples after loop/end
void unfixSample(sample_t *s)
{
	assert(s != NULL);
	if (s->dataPtr == NULL || !s->isFixed)
		return; // empty sample or not fixed (f.ex. no loop)

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + s->fixedPos;
		for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			ptr16[i] = s->fixedSmp[i];
	}
	else // 8-bit
	{
		int8_t *ptr8 = s->dataPtr + s->fixedPos;
		for (int32_t i = 0; i < MAX_RIGHT_TAPS; i++)
			ptr8[i] = (int8_t)s->fixedSmp[i];
	}

	s->isFixed = false;
}

double getSampleValue(int8_t *smpData, int32_t position, bool sample16Bit)
{
	if (smpData == NULL)
		return 0;

	if (sample16Bit)
	{
		position <<= 1;
		return *((int16_t *)&smpData[position]);
	}
	else
	{
		return smpData[position];
	}
}

void putSampleValue(int8_t *smpData, int32_t position, double dSample, bool sample16Bit)
{
	DROUND(dSample);
	int32_t sample = (int32_t)dSample;

	if (sample16Bit)
	{
		CLAMP16(sample);
		*((int16_t *)&smpData[position<<1]) = (int16_t)sample;
	}
	else
	{
		CLAMP8(sample);
		smpData[position] = (int8_t)sample;
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
	smpCopyDidCopyWholeSample = false;
}

int32_t getSampleMiddleCRate(sample_t *s)
{
	return (int32_t)(getSampleC4Rate(s) + 0.5); // rounded
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

	dScr2SmpPosMul = smpEd_ViewSize * (1.0 / SAMPLE_AREA_WIDTH);
}

static void updateScrPos(void)
{
	dScrPosScaled = floor(smpEd_ScrPos * dPos2ScrMul);
}

// sample pos -> screen x pos (if outside of visible area, will return <0 or >=SCREEN_W)
static int32_t smpPos2Scr(int32_t pos)
{
	if (smpEd_ViewSize <= 0)
		return -1;

	sample_t *s = getCurSample();
	if (s == NULL)
		return -1;

	if (pos > s->length)
		pos = s->length;

	double dPos = (pos * dPos2ScrMul) + 0.5; // pre-rounding bias is needed here
	dPos -= dScrPosScaled;

	// this is important, or else the result can mess up in some cases
	dPos = CLAMP(dPos, INT32_MIN, INT32_MAX);
	pos = (int32_t)dPos;

	return pos;
}

// screen x pos -> sample pos
static int32_t scr2SmpPos(int32_t x)
{
	if (smpEd_ViewSize <= 0)
		return 0;

	sample_t *s = getCurSample();
	if (s == NULL)
		return 0;

	if (x < 0)
		x = 0;

	double dPos = (dScrPosScaled + x) * dScr2SmpPosMul;

	x = (int32_t)dPos;
	if (x > s->length)
		x = s->length;

	return x;
}

static void hideLoopPinSprites(void)
{
	hideSprite(SPRITE_LEFT_LOOP_PIN);
	hideSprite(SPRITE_RIGHT_LOOP_PIN);
}

static void fixLoopGadgets(void)
{
	if (!ui.sampleEditorShown)
	{
		hideLoopPinSprites();
		return;
	}

	sample_t *s = getCurSample();

	bool showLoopPins = true;
	if (s == NULL || s->dataPtr == NULL || s->length <= 0 || GET_LOOPTYPE(s->flags) == LOOP_OFF)
		showLoopPins = false;

	// draw Repeat/Replen. numbers
	hexOutBg(536, 375, PAL_FORGRND, PAL_DESKTOP, curSmpLoopStart, 8);
	hexOutBg(536, 387, PAL_FORGRND, PAL_DESKTOP, curSmpLoopLength, 8);

	if (!showLoopPins)
	{
		hideLoopPinSprites();
	}
	else
	{
		// draw sample loop points

		const int32_t loopStart = smpPos2Scr(curSmpLoopStart);
		const int32_t loopEnd = smpPos2Scr(curSmpLoopStart+curSmpLoopLength);

		// do -8 test because part of the loop sprite sticks out on the left/right

		if (loopStart >= -8 && loopStart <= SAMPLE_AREA_WIDTH+8)
			setSpritePos(SPRITE_LEFT_LOOP_PIN, (int16_t)(loopStart - 8), 174);
		else
			hideSprite(SPRITE_LEFT_LOOP_PIN);

		if (loopEnd >= -8)
		{
			if (loopEnd <= SAMPLE_AREA_WIDTH+8)
				setSpritePos(SPRITE_RIGHT_LOOP_PIN, (int16_t)(loopEnd - 8), 174);
			else
				hideSprite(SPRITE_RIGHT_LOOP_PIN);
		}
		else
		{
			hideSprite(SPRITE_RIGHT_LOOP_PIN);
		}
	}
}

static void fixSampleScrollbar(void)
{
	sample_t *s = getCurSample();
	if (s == NULL)
	{
		setScrollBarPageLength(SB_SAMP_SCROLL, 0);
		setScrollBarEnd(SB_SAMP_SCROLL, 0);
		setScrollBarPos(SB_SAMP_SCROLL, 0, false);
		return;
	}

	setScrollBarPageLength(SB_SAMP_SCROLL, smpEd_ViewSize);
	setScrollBarEnd(SB_SAMP_SCROLL, instr[editor.curInstr]->smp[editor.curSmp].length);
	setScrollBarPos(SB_SAMP_SCROLL, smpEd_ScrPos, false);
}

static bool getCopyBuffer(int32_t size, bool sample16Bit)
{
	if (smpCopyBuff != NULL)
		free(smpCopyBuff);

	if (size > MAX_SAMPLE_LEN)
		size = MAX_SAMPLE_LEN;

	smpCopyBuff = (int8_t *)malloc(size << sample16Bit);
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
	pauseAudio();

	sample_t *src;
	if (instr[editor.srcInstr] == NULL)
		src = NULL;
	else
		src = &instr[editor.srcInstr]->smp[editor.srcSmp];

	if (instr[editor.curInstr] == NULL && !allocateInstr(editor.curInstr))
		goto error;

	sample_t *dst = &instr[editor.curInstr]->smp[editor.curSmp];
	if (!cloneSample(src, dst))
		goto error;

	resumeAudio();

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return true;

error:
	resumeAudio();
	okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
	return true;

	(void)ptr;
}

void copySmp(void) // copy sample from srcInstr->srcSmp to curInstr->curSmp
{
	if (editor.curInstr == 0 || (editor.curInstr == editor.srcInstr && editor.curSmp == editor.srcSmp))
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(copySampleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

void xchgSmp(void) // dstSmp <-> srcSmp
{
	if (editor.curInstr == 0 ||
		(editor.curInstr == editor.srcInstr && editor.curSmp == editor.srcSmp) ||
		instr[editor.curInstr] == NULL)
	{
		return;
	}

	sample_t *src = &instr[editor.curInstr]->smp[editor.srcSmp];
	sample_t *dst = &instr[editor.curInstr]->smp[editor.curSmp];

	lockMixerCallback();
	const sample_t dstTmp = *dst;
	*dst = *src;
	*src = dstTmp;
	unlockMixerCallback();

	updateNewSample();
	setSongModifiedFlag();
}

static void writeRange(void)
{
	// very first sample (rx1=0,rx2=0) is the "no range" special case
	if (!ui.sampleEditorShown || smpEd_ViewSize == 0 || (smpEd_Rx1 == 0 && smpEd_Rx2 == 0))
		return;

	// test if range is outside of view (passed it by scrolling)
	int32_t start = smpPos2Scr(smpEd_Rx1);
	if (start >= SAMPLE_AREA_WIDTH)
		return;

	// test if range is outside of view (passed it by scrolling)
	int32_t end = smpPos2Scr(smpEd_Rx2);
	if (end < 0)
		return;

	start = CLAMP(start, 0, SAMPLE_AREA_WIDTH-1);
	end = CLAMP(end, 0, SAMPLE_AREA_WIDTH-1);

	int32_t rangeLen = (end + 1) - start;
	assert(start+rangeLen <= SCREEN_W);

	uint32_t *ptr32 = &video.frameBuffer[(174 * SCREEN_W) + start];
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++)
	{
		for (int32_t x = 0; x < rangeLen; x++)
			ptr32[x] = video.palette[(ptr32[x] >> 24) ^ 2]; // ">> 24" to get palette, XOR 2 to switch between mark/normal palette

		ptr32 += SCREEN_W;
	}
}

static int32_t getScaledSample(sample_t *s, int32_t index) // for sample data viewer
{
	int32_t tmp32, sample;

	const int32_t loopEnd = s->loopStart + s->loopLength;

	if (s->dataPtr == NULL || index < 0 || index >= s->length)
		return 0;

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr;

		// don't read fixed mixer interpolation samples, read the prestine ones instead
		if (index >= s->fixedPos && index < s->fixedPos+MAX_RIGHT_TAPS && s->length > loopEnd && s->isFixed)
			tmp32 = s->fixedSmp[index-s->fixedPos];
		else
			tmp32 = ptr16[index];

		sample = (int8_t)((tmp32 * SAMPLE_AREA_HEIGHT) >> 16);
	}
	else // 8-bit
	{
		if (index >= s->fixedPos && index < s->fixedPos+MAX_RIGHT_TAPS && s->length > loopEnd && s->isFixed)
			tmp32 = s->fixedSmp[index-s->fixedPos];
		else
			tmp32 = s->dataPtr[index];

		sample = (int8_t)((tmp32 * SAMPLE_AREA_HEIGHT) >> 8);
	}

	return SAMPLE_AREA_Y_CENTER-sample;
}

void sampleLine(int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	int32_t d;
	const int32_t dx = x2 - x1;
	const int32_t ax = ABS(dx) * 2;
	const int32_t sx = SGN(dx);
	const int32_t dy = y2 - y1;
	const int32_t ay = ABS(dy) * 2;
	const int32_t sy = SGN(dy);
	int32_t x  = x1;
	int32_t y  = y1;
	const uint32_t pal1 = video.palette[PAL_DESKTOP];
	const uint32_t pal2 = video.palette[PAL_FORGRND];
	const uint32_t pixVal = video.palette[PAL_PATTEXT];
	const int32_t pitch = sy * SCREEN_W;
	uint32_t *dst32 = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		d = ay - (ax >> 1);

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
		d = ax - (ay >> 1);

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
#if defined _WIN32 || defined __amd64__ || (defined __i386__ && defined __SSE2__)
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
		// non-SSE version (really slow for big samples while zoomed out)
		int16_t minVal =  32767;
		int16_t maxVal = -32768;

		const int16_t *ptr16 = (const int16_t *)p;
		for (uint32_t i = 0; i < scanLen; i++)
		{
			const int16_t smp16 = ptr16[i];
			if (smp16 < minVal) minVal = smp16;
			if (smp16 > maxVal) maxVal = smp16;
		}

		*min16 = minVal;
		*max16 = maxVal;
	}
}

static void getMinMax8(const void *p, uint32_t scanLen, int8_t *min8, int8_t *max8)
{
#if defined _WIN32 || defined __amd64__ || (defined __i386__ && defined __SSE2__)
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
		// non-SSE version (really slow for big samples while zoomed out)
		int8_t minVal =  127;
		int8_t maxVal = -128;

		const int8_t *ptr8 = (const int8_t *)p;
		for (uint32_t i = 0; i < scanLen; i++)
		{
			const int8_t smp8 = ptr8[i];
			if (smp8 < minVal) minVal = smp8;
			if (smp8 > maxVal) maxVal = smp8;
		}

		*min8 = minVal;
		*max8 = maxVal;
	}
}

// for scanning sample data peak where loopEnd+MAX_RIGHT_TAPS is within scan range (fixed interpolation tap samples)
static void getSpecialMinMax16(sample_t *s, int32_t index, int32_t scanEnd, int16_t *min16, int16_t *max16)
{
	int16_t minVal2, maxVal2;

	const int16_t *ptr16 = (const int16_t *)s->dataPtr;

	int16_t minVal =  32767;
	int16_t maxVal = -32768;

	// read samples before fixed samples (if needed)
	if (index < s->fixedPos)
	{
		getMinMax16(&ptr16[index], s->fixedPos-index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	// read fixed samples (we are guaranteed to be within the fixed samples here)
	const int32_t tapIndex = index-s->fixedPos;
	const int32_t scanLength = MAX_RIGHT_TAPS-tapIndex;

	int32_t tmpScanEnd = index+scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const int16_t *smpReadPtr = s->fixedSmp + tapIndex;
	for (; index < tmpScanEnd; index++)
	{
		const int16_t smp16 = *smpReadPtr++;
		if (smp16 < minVal) minVal = smp16;
		if (smp16 > maxVal) maxVal = smp16;
	}

	// read samples after fixed samples (if needed)
	if (index < scanEnd)
	{
		getMinMax16(&ptr16[index], scanEnd-index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*min16 = minVal;
	*max16 = maxVal;
}

// for scanning sample data peak where loopEnd+MAX_RIGHT_TAPS is within scan range (fixed interpolation tap samples)
static void getSpecialMinMax8(sample_t *s, int32_t index, int32_t scanEnd, int8_t *min8, int8_t *max8)
{
	int8_t minVal2, maxVal2;

	const int8_t *ptr8 = (const int8_t *)s->dataPtr;

	int8_t minVal =  127;
	int8_t maxVal = -128;

	// read samples before fixed samples (if needed)
	if (index < s->fixedPos)
	{
		getMinMax8(&ptr8[index], s->fixedPos-index, &minVal, &maxVal);
		index = s->fixedPos;
	}

	// read fixed samples (we are guaranteed to be within the fixed samples here)
	const int32_t tapIndex = index-s->fixedPos;
	const int32_t scanLength = MAX_RIGHT_TAPS-tapIndex;

	int32_t tmpScanEnd = index+scanLength;
	if (tmpScanEnd > scanEnd)
		tmpScanEnd = scanEnd;

	const int16_t *smpReadPtr = (const int16_t *)s->fixedSmp + tapIndex;
	for (; index < tmpScanEnd; index++)
	{
		const int8_t smp8 = (int8_t)(*smpReadPtr++);
		if (smp8 < minVal) minVal = smp8;
		if (smp8 > maxVal) maxVal = smp8;
	}

	// read samples after fixed samples (if needed)
	if (index < scanEnd)
	{
		getMinMax8(&ptr8[index], scanEnd-index, &minVal2, &maxVal2);
		if (minVal2 < minVal) minVal = minVal2;
		if (maxVal2 > maxVal) maxVal = maxVal2;
	}

	*min8 = minVal;
	*max8 = maxVal;
}

static void getSampleDataPeak(sample_t *s, int32_t index, int32_t length, int16_t *outMin, int16_t *outMax)
{
	int8_t min8, max8;
	int16_t min16, max16;

	if (length == 0 || s->dataPtr == NULL || s->length <= 0)
	{
		*outMin = SAMPLE_AREA_Y_CENTER;
		*outMax = SAMPLE_AREA_Y_CENTER;
		return;
	}

	if (s->isFixed && s->length > s->loopLength+s->loopStart)
	{
		const int32_t scanEnd = index + length;

		/* If the scan area is including the fixed samples (for branchless mixer interpolation),
		** do a special procedure to scan the original non-touched samples when needed.
		*/
		const bool insideRange = index >= s->fixedPos && index < s->fixedPos+MAX_RIGHT_TAPS;
		if (insideRange || (index < s->fixedPos && scanEnd >= s->fixedPos))
		{
			if (s->flags & SAMPLE_16BIT)
			{
				getSpecialMinMax16(s, index, scanEnd, &min16, &max16);
				*outMin = SAMPLE_AREA_Y_CENTER - ((min16 * SAMPLE_AREA_HEIGHT) >> 16);
				*outMax = SAMPLE_AREA_Y_CENTER - ((max16 * SAMPLE_AREA_HEIGHT) >> 16);
			}
			else // 8-bit
			{
				getSpecialMinMax8(s, index, scanEnd, &min8, &max8);
				*outMin = SAMPLE_AREA_Y_CENTER - ((min8 * SAMPLE_AREA_HEIGHT) >> 8);
				*outMax = SAMPLE_AREA_Y_CENTER - ((max8 * SAMPLE_AREA_HEIGHT) >> 8);
			}

			return;
		}
	}

	if (s->flags & SAMPLE_16BIT)
	{
		const int16_t *smpPtr16 = (int16_t *)s->dataPtr;
		getMinMax16(&smpPtr16[index], length, &min16, &max16);
		*outMin = SAMPLE_AREA_Y_CENTER - ((min16 * SAMPLE_AREA_HEIGHT) >> 16);
		*outMax = SAMPLE_AREA_Y_CENTER - ((max16 * SAMPLE_AREA_HEIGHT) >> 16);
	}
	else // 8-bit
	{
		getMinMax8(&s->dataPtr[index], length, &min8, &max8);
		*outMin = SAMPLE_AREA_Y_CENTER - ((min8 * SAMPLE_AREA_HEIGHT) >> 8);
		*outMax = SAMPLE_AREA_Y_CENTER - ((max8 * SAMPLE_AREA_HEIGHT) >> 8);
	}
}

static void writeWaveform(void)
{
	// clear sample data area
	memset(&video.frameBuffer[174 * SCREEN_W], 0, SAMPLE_AREA_WIDTH * SAMPLE_AREA_HEIGHT * sizeof (int32_t));

	// draw center line
	hLine(0, SAMPLE_AREA_Y_CENTER, SAMPLE_AREA_WIDTH, PAL_DESKTOP);

	if (instr[editor.curInstr] == NULL || smpEd_ViewSize == 0)
		return;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->dataPtr == NULL || s->length <= 0)
		return;

	if (smpEd_ViewSize <= SAMPLE_AREA_WIDTH) // zoomed in (or 1:1)
	{
		for (int32_t x = 0; x <= SAMPLE_AREA_WIDTH; x++)
		{
			int32_t currSmpPos = scr2SmpPos(x+0);
			int32_t nextSmpPos = scr2SmpPos(x+1);

			if (currSmpPos >= s->length) currSmpPos = s->length-1;
			if (nextSmpPos >= s->length) nextSmpPos = s->length-1;

			int32_t x1 = smpPos2Scr(currSmpPos);
			int32_t x2 = smpPos2Scr(nextSmpPos);
			int32_t y1 = getScaledSample(s, currSmpPos);
			int32_t y2 = getScaledSample(s, nextSmpPos);

			x1 = CLAMP(x1, 0, SAMPLE_AREA_WIDTH-1);
			x2 = CLAMP(x2, 0, SAMPLE_AREA_WIDTH-1);

			// kludge: sometimes the last point wouldn't reach the end of the sample window
			if (x == SAMPLE_AREA_WIDTH)
				x2 = SAMPLE_AREA_WIDTH-1;

			sampleLine(x1, x2, y1, y2);
		}
	}
	else // zoomed out
	{
		const int32_t firstSamplePoint = getScaledSample(s, scr2SmpPos(0));

		int32_t oldMin = firstSamplePoint;
		int32_t oldMax = firstSamplePoint;

		for (int16_t x = 0; x < SAMPLE_AREA_WIDTH; x++)
		{
			int32_t smpIdx = scr2SmpPos(x+0);
			int32_t smpNum = scr2SmpPos(x+1) - smpIdx;

			// prevent look-up overflow (yes, this can happen near the end of the sample)
			if (smpIdx+smpNum > s->length)
				smpNum = s->length - smpIdx;

			if (smpNum > 0)
			{
				int16_t min, max;
				getSampleDataPeak(s, smpIdx, smpNum, &min, &max);

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

void writeSample(bool forceSmpRedraw)
{
	int32_t tmpRx1, tmpRx2;
	sample_t *s;

	// update sample loop points for visuals

	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->smp[0];
	else
		s = &instr[editor.curInstr]->smp[editor.curSmp];

	curSmpLoopStart = s->loopStart;
	curSmpLoopLength = s->loopLength;

	// exchange range variables if x1 is after x2
	if (smpEd_Rx1 > smpEd_Rx2)
	{
		tmpRx2 = smpEd_Rx2;
		smpEd_Rx2 = smpEd_Rx1;
		smpEd_Rx1 = tmpRx2;
	}

	// clamp range
	smpEd_Rx1 = CLAMP(smpEd_Rx1, 0, s->length);
	smpEd_Rx2 = CLAMP(smpEd_Rx2, 0, s->length);

	// sanitize sample scroll position
	if (smpEd_ScrPos+smpEd_ViewSize > s->length)
	{
		smpEd_ScrPos = s->length - smpEd_ViewSize;
		updateScrPos();
	}
 
	if (smpEd_ScrPos < 0)
	{
		smpEd_ScrPos = 0;
		updateScrPos();

		if (smpEd_ViewSize > s->length)
		{
			smpEd_ViewSize = s->length;
			updateViewSize();
		}
	}

	// handle updating
	if (ui.sampleEditorShown)
	{
		// check if we need to redraw sample data
		if (forceSmpRedraw || (old_SmpScrPos != smpEd_ScrPos || old_ViewSize != smpEd_ViewSize))
		{
			if (ui.sampleEditorShown)
				writeWaveform();

			old_SmpScrPos = smpEd_ScrPos;
			old_ViewSize = smpEd_ViewSize;

			if (ui.sampleEditorShown)
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

			if (ui.sampleEditorShown)
				writeRange();

			// write new range
			smpEd_Rx1 = tmpRx1;
			smpEd_Rx2 = tmpRx2;

			if (ui.sampleEditorShown)
				writeRange();

			old_Rx1 = smpEd_Rx1;
			old_Rx2 = smpEd_Rx2;
		}

		fixLoopGadgets();
	}

	if (ui.sampleEditorShown)
		fixSampleScrollbar();

	updateSampleEditor();
}

static void setSampleRange(int32_t start, int32_t end)
{
	if (instr[editor.curInstr] == NULL)
	{
		smpEd_Rx1 = 0;
		smpEd_Rx2 = 0;
		return;
	}

	if (start < 0)
		start = 0;

	if (end < 0)
		end = 0;

	smpEd_Rx1 = scr2SmpPos(start);
	smpEd_Rx2 = scr2SmpPos(end);
}

void updateSampleEditorSample(void)
{
	smpEd_Rx1 = smpEd_Rx2 = 0;

	smpEd_ScrPos = 0;
	updateScrPos();

	if (instr[editor.curInstr] == NULL)
		smpEd_ViewSize = 0;
	else
		smpEd_ViewSize = instr[editor.curInstr]->smp[editor.curSmp].length;

	updateViewSize();

	writeSample(true);
}

void updateSampleEditor(void)
{
	char noteChar1, noteChar2;
	uint8_t flags;
	int32_t sampleLength;

	if (!ui.sampleEditorShown)
		return;

	if (instr[editor.curInstr] == NULL)
	{
		flags = 0;
		sampleLength = 0;
	}
	else
	{
		flags = instr[editor.curInstr]->smp[editor.curSmp].flags;
		sampleLength = instr[editor.curInstr]->smp[editor.curSmp].length;
	}

	// sample bit depth radio buttons
	uncheckRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);
	if (flags & SAMPLE_16BIT)
		radioButtons[RB_SAMPLE_16BIT].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_SAMPLE_8BIT].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_SAMPLE_DEPTH);

	// sample loop radio buttons
	uncheckRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);

	uint8_t loopType = GET_LOOPTYPE(flags);
	if (loopType == LOOP_OFF)
		radioButtons[RB_SAMPLE_NO_LOOP].state = RADIOBUTTON_CHECKED;
	else if (loopType == LOOP_FWD)
		radioButtons[RB_SAMPLE_FORWARD_LOOP].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_SAMPLE_PINGPONG_LOOP].state = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(RB_GROUP_SAMPLE_LOOP);

	if (!ui.sampleEditorEffectsShown)
	{
		// draw sample play note

		const uint32_t noteNr = editor.smpEd_NoteNr - 1;

		const uint32_t note   = noteNr % 12;
		const uint32_t octave = noteNr / 12;

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

		charOutBg(7,  369, PAL_FORGRND, PAL_BCKGRND, noteChar1);
		charOutBg(15, 369, PAL_FORGRND, PAL_BCKGRND, noteChar2);
		charOutBg(23, 369, PAL_FORGRND, PAL_BCKGRND, (char)('0' + octave));
	}

	// draw sample display/length

	hexOutBg(536, 350, PAL_FORGRND, PAL_DESKTOP, smpEd_ViewSize, 8);
	hexOutBg(536, 362, PAL_FORGRND, PAL_DESKTOP, sampleLength, 8);
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
	int32_t sampleLen;

	if (instr[editor.curInstr] == NULL)
		sampleLen = 0;
	else
		sampleLen = instr[editor.curInstr]->smp[editor.curSmp].length;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	int32_t scrollAmount = (uint32_t)smpEd_ViewSize / 32;
	if (scrollAmount < 1)
		scrollAmount = 1;

	smpEd_ScrPos -= scrollAmount;
	if (smpEd_ScrPos < 0)
		smpEd_ScrPos = 0;

	updateScrPos();
}

void scrollSampleDataRight(void)
{
	int32_t sampleLen;

	if (instr[editor.curInstr] == NULL)
		sampleLen = 0;
	else
		sampleLen = instr[editor.curInstr]->smp[editor.curSmp].length;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	int32_t scrollAmount = (uint32_t)smpEd_ViewSize / 32;
	if (scrollAmount < 1)
		scrollAmount = 1;

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
		sampleLen = instr[editor.curInstr]->smp[editor.curSmp].length;

	if (smpEd_ViewSize == 0 || smpEd_ViewSize == sampleLen)
		return;

	smpEd_ScrPos = pos;
	updateScrPos();
}

void sampPlayWave(void)
{
	playSample(cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0);
}

void sampPlayDisplay(void)
{
	playRange(cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0, smpEd_ScrPos, smpEd_ViewSize);
}

void sampPlayRange(void)
{
	playRange(cursor.ch, editor.curInstr, editor.curSmp, editor.smpEd_NoteNr, 0, 0, smpEd_Rx1, smpEd_Rx2 - smpEd_Rx1);
}

void showRange(void)
{
	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->dataPtr == NULL)
		return;

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		smpEd_ViewSize = smpEd_Rx2 - smpEd_Rx1;
		if (smpEd_ViewSize < 2)
			smpEd_ViewSize = 2;

		updateViewSize();

		smpEd_ScrPos = smpEd_Rx1;
		updateScrPos();
	}
	else
	{
		okBox(0, "System message", "Cannot show empty range!", NULL);
	}
}

void rangeAll(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	smpEd_Rx1 = smpEd_ScrPos;
	smpEd_Rx2 = smpEd_ScrPos + smpEd_ViewSize;
}

static void zoomSampleDataIn(int32_t step, int32_t x)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (old_ViewSize <= 2)
		return;

	if (step < 1)
		step = 1;

	smpEd_ViewSize = old_ViewSize - (step * 2);
	if (smpEd_ViewSize < 2)
		smpEd_ViewSize = 2;

	updateViewSize();

	int32_t tmp32 = (x - (SAMPLE_AREA_WIDTH / 2)) * step;
	tmp32 += SAMPLE_AREA_WIDTH/4; // rounding bias
	tmp32 /= SAMPLE_AREA_WIDTH/2;

	step += tmp32;

	int64_t newScrPos64 = old_SmpScrPos + step;
	if (newScrPos64+smpEd_ViewSize > s->length)
		newScrPos64 = s->length - smpEd_ViewSize;

	smpEd_ScrPos = (uint32_t)newScrPos64;
	updateScrPos();
}

static void zoomSampleDataOut(int32_t step, int32_t x)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (old_ViewSize == s->length)
		return;

	if (step < 1)
		step = 1;

	int64_t newViewSize64 = (int64_t)old_ViewSize + (step * 2);
	if (newViewSize64 > s->length)
	{
		smpEd_ViewSize = s->length;
		smpEd_ScrPos = 0;
	}
	else
	{
		int32_t tmp32 = (x - (SAMPLE_AREA_WIDTH / 2)) * step;
		tmp32 += SAMPLE_AREA_WIDTH/4; // rounding bias
		tmp32 /= SAMPLE_AREA_WIDTH/2;

		step += tmp32;

		smpEd_ViewSize = newViewSize64 & 0xFFFFFFFF;

		smpEd_ScrPos = old_SmpScrPos - step;
		if (smpEd_ScrPos < 0)
			smpEd_ScrPos = 0;

		if (smpEd_ScrPos+smpEd_ViewSize > s->length)
			smpEd_ScrPos = s->length - smpEd_ViewSize;
	}

	updateViewSize();
	updateScrPos();
}

void mouseZoomSampleDataIn(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	zoomSampleDataIn((old_ViewSize+5) / 10, mouse.x);
}

void mouseZoomSampleDataOut(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	zoomSampleDataOut((old_ViewSize+5) / 10, mouse.x);
}

void zoomOut(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (old_ViewSize == s->length)
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
		smpEd_ViewSize = s->length;
		smpEd_ScrPos = 0;
	}
	else if (smpEd_ViewSize+smpEd_ScrPos > s->length)
	{
		smpEd_ViewSize = s->length - smpEd_ScrPos;
	}

	updateViewSize();
	updateScrPos();
}

void showAll(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	smpEd_ScrPos = 0;
	updateScrPos();

	smpEd_ViewSize = instr[editor.curInstr]->smp[editor.curSmp].length;
	updateViewSize();
}

void saveRange(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	if (smpEd_Rx1 == smpEd_Rx2)
	{
		okBox(0, "System message", "No range specified!", NULL);
		return;
	}

	smpEd_SysReqText[0] = '\0';
	if (inputBox(1, "Enter filename:", smpEd_SysReqText, sizeof (smpEd_SysReqText) - 1) != 1)
		return;

	if (smpEd_SysReqText[0] == '\0')
	{
		okBox(0, "System message", "Filename can't be empty!", NULL);
		return;
	}

	if (smpEd_SysReqText[0] == '.')
	{
		okBox(0, "System message", "The very first character in the filename can't be '.' (dot)!", NULL);
		return;
	}

	if (strpbrk(smpEd_SysReqText, "\\/:*?\"<>|") != NULL)
	{
		okBox(0, "System message", "The filename can't contain the following characters: \\ / : * ? \" < > |", NULL);
		return;
	}

	switch (editor.sampleSaveMode)
	{
		         case SMP_SAVE_MODE_RAW: changeFilenameExt(smpEd_SysReqText, ".raw", sizeof (smpEd_SysReqText) - 1); break;
		         case SMP_SAVE_MODE_IFF: changeFilenameExt(smpEd_SysReqText, ".iff", sizeof (smpEd_SysReqText) - 1); break;
		default: case SMP_SAVE_MODE_WAV: changeFilenameExt(smpEd_SysReqText, ".wav", sizeof (smpEd_SysReqText) - 1); break;
	}

	UNICHAR *filenameU = cp850ToUnichar(smpEd_SysReqText);
	if (filenameU == NULL)
	{
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	if (fileExistsAnsi(smpEd_SysReqText))
	{
		char buf[256];
		createFileOverwriteText(smpEd_SysReqText, buf);
		if (okBox(2, "System request", buf, NULL) != 1)
			return;
	}

	saveSample(filenameU, SAVE_RANGE);
	free(filenameU);
}

static bool cutRange(bool cropMode, int32_t r1, int32_t r2)
{
	sample_t *s = getCurSample();
	if (s == NULL)
		return false;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (!cropMode)
	{
		if (editor.curInstr == 0 || s->dataPtr == NULL || s->length == 0)
			return false;

		pauseAudio();
		unfixSample(s);

		if (config.smpCutToBuffer)
		{
			if (!getCopyBuffer(r2-r1, sample16Bit))
			{
				fixSample(s);
				resumeAudio();

				okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
				return false;
			}

			memcpy(smpCopyBuff, &s->dataPtr[r1 << sample16Bit], (r2-r1) << sample16Bit);
			smpCopyBits = sample16Bit ? 16 : 8;
		}
	}

	memmove(&s->dataPtr[r1 << sample16Bit], &s->dataPtr[r2 << sample16Bit], (s->length-r2) << sample16Bit);

	int32_t length = s->length - r2+r1;
	if (length > 0)
	{
		if (!reallocateSmpData(s, length, sample16Bit))
		{
			freeSample(editor.curInstr, editor.curSmp);
			editor.updateCurSmp = true;

			if (!cropMode)
				resumeAudio();

			okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
			return false;
		}

		s->length = length;

		int32_t loopEnd = s->loopStart + s->loopLength;
		if (s->loopStart > r1)
		{
			s->loopStart -= r2-r1;
			if (s->loopStart < r1)
				s->loopStart = r1;
		}

		if (loopEnd > r1)
		{
			loopEnd -= r2-r1;
			if (loopEnd < r1)
				loopEnd = r1;
		}

		s->loopLength = loopEnd - s->loopStart;
		if (s->loopLength < 0)
			s->loopLength = 0;

		if (s->loopStart+s->loopLength > length)
			s->loopLength = length - s->loopStart;

		if (s->loopLength <= 0)
		{
			s->loopStart = 0;
			DISABLE_LOOP(s->flags);
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
	if (!cutRange(false, smpEd_Rx1, smpEd_Rx2))
		okBoxThreadSafe(0, "System message", "Not enough memory! (Disable \"cut to buffer\")", NULL);
	else
		writeSampleFlag = true;

	return true;

	(void)ptr;
}

void sampCut(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0 || smpEd_Rx2 == 0 || smpEd_Rx2 < smpEd_Rx1)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampCutThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampCopyThread(void *ptr)
{
	sample_t *s = getCurSample();

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (!getCopyBuffer(smpEd_Rx2- smpEd_Rx1, sample16Bit))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return true;
	}

	unfixSample(s);
	memcpy(smpCopyBuff, &s->dataPtr[smpEd_Rx1 << sample16Bit], (smpEd_Rx2-smpEd_Rx1) << sample16Bit);
	fixSample(s);

	setMouseBusy(false);

	// copy sample information (in case we paste over an empty sample)
	if (smpEd_Rx1 == 0 && smpEd_Rx2 == s->length)
	{
		smpCopySample = *s;
		smpCopyDidCopyWholeSample = true;
	}

	smpCopyBits = sample16Bit? 16 : 8;
	return true;

	(void)ptr;
}

void sampCopy(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0 || smpEd_Rx2 == 0 || smpEd_Rx2 < smpEd_Rx1)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampCopyThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void pasteOverwrite(sample_t *s)
{
	bool sample16Bit = (smpCopyBits == 16);

	if (!reallocateSmpData(s, smpCopySize, sample16Bit))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return;
	}

	pauseAudio();

	memcpy(s->dataPtr, smpCopyBuff, smpCopySize << sample16Bit);

	if (smpCopyDidCopyWholeSample)
	{
		sample_t *src = &smpCopySample;
		memcpy(s->name, src->name, 23);
		s->length = src->length;
		s->loopStart = src->loopStart;
		s->loopLength = src->loopLength;
		s->volume = src->volume;
		s->panning = src->panning;
		s->finetune = src->finetune;
		s->relativeNote = src->relativeNote;
		s->flags = src->flags;
	}
	else
	{
		s->name[0] = '\0';
		s->length = smpCopySize;
		s->loopStart = 0;
		s->loopLength = 0;
		s->volume = 64;
		s->panning = 128;
		s->finetune = 0;
		s->relativeNote = 0;
		s->flags = (smpCopyBits == 16) ? SAMPLE_16BIT : 0;
	}

	s->isFixed = false;

	fixSample(s);
	resumeAudio();

	editor.updateCurSmp = true;
	setSongModifiedFlag();
	setMouseBusy(false);
}

static void pasteCopiedData(int8_t *dataPtr, int32_t offset, int32_t length, bool sample16Bit)
{
	if (sample16Bit) // destination sample is 16-bits
	{
		if (smpCopyBits == 16)
		{
			// src/dst bits are equal, do direct copy
			memcpy(&dataPtr[offset<<1], smpCopyBuff, length * sizeof (int16_t));
		}
		else
		{
			// convert copied data to 16-bit then paste
			int16_t *ptr16 = (int16_t *)dataPtr + offset;
			for (int32_t i = 0; i < length; i++)
				ptr16[i] = smpCopyBuff[i] << 8;
		}
	}
	else // destination sample is 8-bits
	{
		if (smpCopyBits == 8)
		{
			// src/dst bits are equal, do direct copy
			memcpy(&dataPtr[offset], smpCopyBuff, length * sizeof (int8_t));
		}
		else
		{
			// convert copied data to 8-bit then paste
			int8_t *ptr8 = (int8_t *)&dataPtr[offset];
			int16_t *ptr16 = (int16_t *)smpCopyBuff;

			for (int32_t i = 0; i < length; i++)
				ptr8[i] = ptr16[i] >> 8;
		}
	}
}

static int32_t SDLCALL sampPasteThread(void *ptr)
{
	smpPtr_t sp;

	if (instr[editor.curInstr] == NULL && !allocateInstr(editor.curInstr))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return true;
	}

	sample_t *s = getCurSample();
	if (smpEd_Rx2 == 0 || s == NULL || s->dataPtr == NULL)
	{
		pasteOverwrite(s);
		return true;
	}

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (s->length+smpCopySize > MAX_SAMPLE_LEN)
	{
		okBoxThreadSafe(0, "System message", "Not enough room in sample!", NULL);
		return true;
	}

	int32_t newLength = s->length + smpCopySize - (smpEd_Rx2 - smpEd_Rx1);
	if (newLength <= 0)
		return true;

	if (newLength > MAX_SAMPLE_LEN)
	{
		okBoxThreadSafe(0, "System message", "Not enough room in sample!", NULL);
		return true;
	}

	if (!allocateSmpDataPtr(&sp, newLength, sample16Bit))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return true;
	}

	pauseAudio();
	unfixSample(s);

	// paste left part of original sample
	if (smpEd_Rx1 > 0)
		memcpy(sp.ptr, s->dataPtr, smpEd_Rx1 << sample16Bit);

	// paste copied data
	pasteCopiedData(sp.ptr, smpEd_Rx1, smpCopySize, sample16Bit);

	// paste right part of original sample
	if (smpEd_Rx2 < s->length)
		memmove(&sp.ptr[(smpEd_Rx1+smpCopySize) << sample16Bit], &s->dataPtr[smpEd_Rx2 << sample16Bit], (s->length-smpEd_Rx2) << sample16Bit);

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	// adjust loop points if necessary
	if (smpEd_Rx2-smpEd_Rx1 != smpCopySize)
	{
		int32_t loopAdjust = smpCopySize - (smpEd_Rx1 - smpEd_Rx2);

		if (s->loopStart > smpEd_Rx2)
		{
			s->loopStart += loopAdjust;
			s->loopLength -= loopAdjust;
		}

		if (s->loopStart+s->loopLength > smpEd_Rx2)
			s->loopLength += loopAdjust;

		if (s->loopStart > newLength)
		{
			s->loopStart = 0;
			s->loopLength = 0;
		}

		if (s->loopStart+s->loopLength > newLength)
			s->loopLength = newLength - s->loopStart;
	}

	s->length = newLength;

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	// set new range
	smpEd_Rx2 = smpEd_Rx1 + smpCopySize;

	writeSampleFlag = true;
	return true;

	(void)ptr;
}

void sampPaste(void)
{
	if (editor.curInstr == 0 || smpEd_Rx2 < smpEd_Rx1 || smpCopyBuff == NULL || smpCopySize == 0)
		return;

	if (smpEd_Rx2 == 0) // no sample data marked, overwrite sample with copy buffer
	{
		sample_t *s = getCurSample();
		if (s != NULL && s->dataPtr != NULL && s->length > 0)
		{
			if (okBox(2, "System request", "The current sample is not empty. Do you really want to overwrite it?", NULL) != 1)
				return;
		}
	}

	mouseAnimOn();
	thread = SDL_CreateThread(sampPasteThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampCropThread(void *ptr)
{
	sample_t *s = getCurSample();

	int32_t r1 = smpEd_Rx1;
	int32_t r2 = smpEd_Rx2;

	pauseAudio();
	unfixSample(s);

	if (!cutRange(true, 0, r1) || !cutRange(true, r2-r1, s->length))
	{
		fixSample(s);
		resumeAudio();
		return true;
	}
	
	fixSample(s);
	resumeAudio();

	r1 = 0;
	r2 = s->length;

	setSongModifiedFlag();
	setMouseBusy(false);

	smpEd_Rx1 = r1;
	smpEd_Rx2 = r2;

	writeSampleFlag = true;
	return true;

	(void)ptr;
}

void sampCrop(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0 || smpEd_Rx1 == smpEd_Rx2)
		return;

	if (smpEd_Rx1 == 0 && smpEd_Rx2 == s->length)
		return; // nothing to crop (the whole sample is marked)

	mouseAnimOn();
	thread = SDL_CreateThread(sampCropThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

void sampXFade(void)
{
	int32_t y1, y2, d1, d2, d3;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	// check if the sample has the loop flag enabled
	if (GET_LOOPTYPE(s->flags) == LOOP_OFF)
	{
		okBox(0, "System message", "X-Fade can only be used on a loop-enabled sample!", NULL);
		return;
	}

	// check if we selected a range
	if (smpEd_Rx2 == 0)
	{
		okBox(0, "System message", "No range selected! Make a small range that includes loop start or loop end.", NULL);
		return;
	}

	// check if we selected a valid range length
	if (smpEd_Rx2-smpEd_Rx1 <= 2)
	{
		okBox(0, "System message", "Invalid range!", NULL);
		return;
	}

	int32_t x1 = smpEd_Rx1;
	int32_t x2 = smpEd_Rx2;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (GET_LOOPTYPE(s->flags) == LOOP_BIDI)
	{
		y1 = s->loopStart;
		if (x1 <= y1) // first loop point
		{
			if (x2 <= y1 || x2 >= s->loopStart+s->loopLength)
			{
				okBox(0, "System message", "Error: No loop point found inside marked data.", NULL);
				return;
			}

			d1 = y1 - x1;
			if (x2-y1 > d1)
				d1 = x2 - y1;

			d2 = y1 - x1;
			d3 = x2 - y1;

			if (d1 < 1 || d2 < 1 || d3 < 1)
			{
				okBox(0, "System message", "Invalid range! Try to mark more data.", NULL);
				return;
			}

			if (y1-d1 < 0 || y1+d1 >= s->length)
			{
				okBox(0, "System message", "Not enough sample data outside loop!", NULL);
				return;
			}

			const double dD2Mul = 1.0 / d2;
			const double dD3Mul = 1.0 / d3;

			pauseAudio();
			unfixSample(s);

			for (int32_t i = 0; i < d1; i++)
			{
				const int32_t aIdx = y1-i-1;
				const int32_t bIdx = y1+i;
				const double dI = i;

				const double dA = getSampleValue(s->dataPtr, aIdx, sample16Bit);
				const double dB = getSampleValue(s->dataPtr, bIdx, sample16Bit);

				if (i < d2)
				{
					const double dS1 = 1.0 - (dI * dD2Mul);
					const double dS2 = 2.0 - dS1;
					double dSample = (dA * dS2 + dB * dS1) / (dS1 + dS2);
					putSampleValue(s->dataPtr, aIdx, dSample, sample16Bit);
				}

				if (i < d3)
				{
					const double dS1 = 1.0 - (dI * dD3Mul);
					const double dS2 = 2.0 - dS1;
					double dSample = (dB * dS2 + dA * dS1) / (dS1 + dS2);
					putSampleValue(s->dataPtr, bIdx, dSample, sample16Bit);
				}
			}

			fixSample(s);
			resumeAudio();
		}
		else // last loop point
		{
			y1 += s->loopLength;
			if (x1 >= y1 || x2 <= y1 || x2 >= s->length)
			{
				okBox(0, "System message", "Error: No loop point found inside marked data.", NULL);
				return;
			}

			d1 = y1 - x1;
			if (x2-y1 > d1)
				d1 = x2 - y1;

			d2 = y1 - x1;
			d3 = x2 - y1;

			if (d1 < 1 || d2 < 1 || d3 < 1)
			{
				okBox(0, "System message", "Invalid range! Try to mark more data.", NULL);
				return;
			}

			if (y1-d1 < 0 || y1+d1 >= s->length)
			{
				okBox(0, "System message", "Not enough sample data outside loop!", NULL);
				return;
			}

			const double dD2Mul = 1.0 / d2;
			const double dD3Mul = 1.0 / d3;

			pauseAudio();
			unfixSample(s);

			for (int32_t i = 0; i < d1; i++)
			{
				const int32_t aIdx = y1-i-1;
				const int32_t bIdx = y1+i;
				const double dI = i;

				const double dA = getSampleValue(s->dataPtr, aIdx, sample16Bit);
				const double dB = getSampleValue(s->dataPtr, bIdx, sample16Bit);

				if (i < d2)
				{
					const double dS1 = 1.0 - (dI * dD2Mul);
					const double dS2 = 2.0 - dS1;
					double dSample = (dA * dS2 + dB * dS1) / (dS1 + dS2);
					putSampleValue(s->dataPtr, aIdx, dSample, sample16Bit);
				}

				if (i < d3)
				{
					const double dS1 = 1.0 - (dI * dD3Mul);
					const double dS2 = 2.0 - dS1;
					double dSample = (dB * dS2 + dA * dS1) / (dS1 + dS2);
					putSampleValue(s->dataPtr, bIdx, dSample, sample16Bit);
				}
			}

			fixSample(s);
			resumeAudio();
		}
	}
	else // forward loop
	{
		if (x1 > s->loopStart)
		{
			x1 -= s->loopLength;
			x2 -= s->loopLength;
		}

		if (x1 < 0 || x2 <= x1 || x2 >= s->length)
		{
			okBox(0, "System message", "Invalid range!", NULL);
			return;
		}

		const int32_t length = x2 - x1;

		int32_t x = (length + 1) >> 1;
		y1 = s->loopStart - x;
		y2 = s->loopStart+s->loopLength - x;

		if (y1 < 0 || y2+length >= s->length)
		{
			okBox(0, "System message", "Not enough sample data outside loop!", NULL);
			return;
		}

		d1 = length;
		d2 = s->loopStart - y1;
		d3 = length - d2;

		if (y1+length <= s->loopStart || d1 == 0 || d3 == 0 || d1 > s->loopLength)
		{
			okBox(0, "System message", "Invalid range!", NULL);
			return;
		}

		const double dR = (s->loopStart - x) / (double)length;
		const double dD1 = d1;
		const double dD1Mul = 1.0 / d1;
		const double dD2Mul = 1.0 / d2;
		const double dD3Mul = 1.0 / d3;

		pauseAudio();
		unfixSample(s);

		for (int32_t i = 0; i < length; i++)
		{
			const int32_t aIdx = y1+i;
			const int32_t bIdx = y2+i;
			const double dI = i;

			const double dA = getSampleValue(s->dataPtr, aIdx, sample16Bit);
			const double dB = getSampleValue(s->dataPtr, bIdx, sample16Bit);
			const double dS2 = dI * dD1Mul;
			const double dS1 = 1.0 - dS2;

			double dC, dD;
			if (y1+i < s->loopStart)
			{
				const double dS3 = 1.0 - (1.0 - dR) * dI * dD2Mul;
				const double dS4 = dR * dI * dD2Mul;
				
				dC = (dA * dS3 + dB * dS4) / (dS3 + dS4);
				dD = (dA * dS2 + dB * dS1) / (dS1 + dS2);
			}
			else
			{
				const double dS3 = 1.0 - (1.0 - dR) * (dD1 - dI) * dD3Mul;
				const double dS4 = dR * (dD1 - dI) * dD3Mul;

				dC = (dA * dS2 + dB * dS1) / (dS1 + dS2);
				dD = (dA * dS4 + dB * dS3) / (dS3 + dS4);
			}

			putSampleValue(s->dataPtr, aIdx, dC, sample16Bit);
			putSampleValue(s->dataPtr, bIdx, dD, sample16Bit);
		}

		fixSample(s);
		resumeAudio();
	}

	writeSample(true);
	setSongModifiedFlag();
}

void rbSampleNoLoop(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	lockMixerCallback();
	unfixSample(s);

	DISABLE_LOOP(s->flags);

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

void rbSampleForwardLoop(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	lockMixerCallback();
	unfixSample(s);

	DISABLE_LOOP(s->flags);
	s->flags |= LOOP_FWD;

	if (s->loopStart+s->loopLength == 0)
	{
		s->loopStart = 0;
		s->loopLength = s->length;
	}

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

void rbSamplePingpongLoop(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	lockMixerCallback();
	unfixSample(s);

	DISABLE_LOOP(s->flags);
	s->flags |= LOOP_BIDI;

	if (s->loopStart+s->loopLength == 0)
	{
		s->loopStart = 0;
		s->loopLength = s->length;
	}

	fixSample(s);
	unlockMixerCallback();

	updateSampleEditor();
	writeSample(true);
	setSongModifiedFlag();
}

static int32_t SDLCALL convSmp8Bit(void *ptr)
{
	sample_t *s = getCurSample();
	assert(s->dataPtr != NULL);

	pauseAudio();
	unfixSample(s);

	const int16_t *src16 = (const int16_t *)s->dataPtr;
	for (int32_t i = 0; i < s->length; i++)
		s->dataPtr[i] = src16[i] >> 8;

	reallocateSmpData(s, s->length, false);

	s->flags &= ~SAMPLE_16BIT; // remove 16-bit flag

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	editor.updateCurSmp = true;
	return true;

	(void)ptr;
}

void rbSample8bit(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	if (okBox(2, "System request", "Pre-convert sample data?", NULL) == 1)
	{
		mouseAnimOn();
		thread = SDL_CreateThread(convSmp8Bit, NULL, NULL);
		if (thread == NULL)
		{
			okBox(0, "System message", "Couldn't create thread!", NULL);
			return;
		}

		SDL_DetachThread(thread);
		return;
	}
	else
	{
		lockMixerCallback();
		unfixSample(s);

		s->flags &= ~SAMPLE_16BIT; // remove 16-bit flag
		s->length <<= 1;
		// no need to call reallocateSmpData, number of bytes allocated is the same

		fixSample(s);
		unlockMixerCallback();

		updateSampleEditorSample();
		updateSampleEditor();
		setSongModifiedFlag();
	}
}

static int32_t SDLCALL convSmp16Bit(void *ptr)
{
	sample_t *s = getCurSample();

	pauseAudio();
	unfixSample(s);

	if (!reallocateSmpData(s, s->length, true))
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
		return true;
	}

	int16_t *dst16 = (int16_t *)s->dataPtr;
	for (int32_t i = s->length-1; i >= 0; i--)
		dst16[i] = s->dataPtr[i] << 8;

	s->flags |= SAMPLE_16BIT;

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	editor.updateCurSmp = true;
	return true;

	(void)ptr;
}

void rbSample16bit(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	if (okBox(2, "System request", "Pre-convert sample data?", NULL) == 1)
	{
		mouseAnimOn();
		thread = SDL_CreateThread(convSmp16Bit, NULL, NULL);
		if (thread == NULL)
		{
			okBox(0, "System message", "Couldn't create thread!", NULL);
			return;
		}

		SDL_DetachThread(thread);
		return;
	}
	else
	{
		lockMixerCallback();
		unfixSample(s);

		s->flags |= SAMPLE_16BIT;
		s->length >>= 1;
		// no need to call reallocateSmpData, number of bytes allocated is the same

		fixSample(s);
		unlockMixerCallback();

		updateSampleEditorSample();
		updateSampleEditor();
		setSongModifiedFlag();
	}
}

void clearSample(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	if (okBox(1, "System request", "Clear sample?", NULL) != 1)
		return;

	freeSample(editor.curInstr, editor.curSmp);
	updateNewSample();
	setSongModifiedFlag();
}

void sampMinimize(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	const bool hasLoop = GET_LOOPTYPE(s->flags) != LOOP_OFF;
	if (!hasLoop)
	{
		okBox(0, "System message", "Only a looped sample can be minimized!", NULL);
		return;
	}

	if (s->loopStart+s->loopLength >= s->length)
	{
		okBox(0, "System message", "This sample is already minimized.", NULL);
		return;
	}

	if (okBox(1, "System request", "Minimize sample?", NULL) != 1)
		return;
	
	lockMixerCallback();

	s->length = s->loopStart + s->loopLength;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	reallocateSmpData(s, s->length, sample16Bit);
	// note: we don't need to make a call to fixSample()

	unlockMixerCallback();

	updateSampleEditorSample();
	updateSampleEditor();
	setSongModifiedFlag();
}

void sampRepeatUp(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t loopStart = curSmpLoopStart;
	int32_t loopLength = curSmpLoopLength;

	if (loopStart < s->length-2)
		loopStart++;

	if (loopStart+loopLength > s->length)
		loopLength = s->length - loopStart;

	curSmpLoopStart = loopStart;
	curSmpLoopLength = loopLength;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

void sampRepeatDown(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t loopStart = curSmpLoopStart - 1;
	if (loopStart < 0)
		loopStart = 0;

	curSmpLoopStart = loopStart;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

void sampReplenUp(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t loopLength = curSmpLoopLength + 1;
	if (curSmpLoopStart+loopLength > s->length)
		loopLength = s->length - curSmpLoopStart;

	curSmpLoopLength = loopLength;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

void sampReplenDown(void)
{
	int32_t loopLength;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	loopLength = curSmpLoopLength - 1;
	if (loopLength < 0)
		loopLength = 0;

	curSmpLoopLength = loopLength;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

void hideSampleEditor(void)
{
	hideSampleEffectsScreen();

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
	hidePushButton(PB_SAMP_EFFECTS);
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

	ui.sampleEditorShown = false;

	hideSprite(SPRITE_LEFT_LOOP_PIN);
	hideSprite(SPRITE_RIGHT_LOOP_PIN);
}

void exitSampleEditor(void)
{
	hideSampleEditor();

	if (ui.sampleEditorExtShown)
		hideSampleEditorExt();

	showPatternEditor();
}

void showSampleEditor(void)
{
	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideInstEditor();
	hidePatternEditor();
	ui.sampleEditorShown = true;

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
	textOutShadow(445, 384, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
	textOutShadow(488, 350, PAL_FORGRND, PAL_DSKTOP2, "Display");
	textOutShadow(488, 362, PAL_FORGRND, PAL_DSKTOP2, "Length");
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
	showPushButton(PB_SAMP_EFFECTS);
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

	// clear two lines in the sample data view that are never written to when the sampler is open
	hLine(0, 173, SAMPLE_AREA_WIDTH, PAL_BCKGRND);
	hLine(0, 328, SAMPLE_AREA_WIDTH, PAL_BCKGRND);

	updateSampleEditor();
	writeSample(true);

	if (ui.sampleEditorEffectsShown)
		pbEffects();
}

void toggleSampleEditor(void)
{
	hideInstEditor();

	if (ui.sampleEditorShown)
	{
		exitSampleEditor();
	}
	else
	{
		hidePatternEditor();
		showSampleEditor();
	}
}

static void invertSamplePosLine(int32_t x)
{
	if (x < 0 || x >= SCREEN_W)
		return;

	uint32_t *ptr32 = &video.frameBuffer[(174 * SCREEN_W) + x];
	for (int32_t y = 0; y < SAMPLE_AREA_HEIGHT; y++, ptr32 += SCREEN_W)
		*ptr32 = video.palette[(*ptr32 >> 24) ^ 1]; // ">> 24" to get palette, XOR 1 to switch between normal/inverted mode
}

static void writeSamplePosLine(void)
{
	uint8_t ins, smp;

	assert(editor.curSmpChannel < MAX_CHANNELS);
	lastChInstr_t *c = &lastChInstr[editor.curSmpChannel];

	if (c->instrNum == 130) // "Play Wave/Range/Display" in Smp. Ed.
	{
		ins = editor.curPlayInstr;
		smp = editor.curPlaySmp;
	}
	else
	{
		ins = c->instrNum;
		smp = c->smpNum;
	}

	if (editor.curInstr == ins && editor.curSmp == smp)
	{
		const int32_t smpPos = getSamplePositionFromScopes(editor.curSmpChannel);
		if (smpPos != -1)
		{
			// convert sample position to screen position
			const int32_t scrPos = smpPos2Scr(smpPos);
			if (scrPos != -1)
			{
				if (scrPos != smpEd_OldSmpPosLine)
				{
					invertSamplePosLine(smpEd_OldSmpPosLine); // remove old line
					invertSamplePosLine(scrPos); // write new line
				}

				smpEd_OldSmpPosLine = scrPos;
				return;
			}
		}
	}

	if (smpEd_OldSmpPosLine != -1)
		invertSamplePosLine(smpEd_OldSmpPosLine);

	smpEd_OldSmpPosLine = -1;
}

void handleSamplerRedrawing(void)
{
	// update sample editor

	if (!ui.sampleEditorShown || editor.samplingAudioFlag)
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
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t newPos = scr2SmpPos(x) - curSmpLoopStart;
	int32_t loopStart = curSmpLoopStart + newPos;
	int32_t loopLength = curSmpLoopLength - newPos;

	if (loopStart < 0)
	{
		loopLength += loopStart;
		loopStart = 0;
	}

	if (loopLength < 0)
	{
		loopLength = 0;
		loopStart = curSmpLoopStart + curSmpLoopLength;
	}

	curSmpLoopStart = loopStart;
	curSmpLoopLength = loopLength;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

static void setRightLoopPinPos(int32_t x)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t loopLength = scr2SmpPos(x) - curSmpLoopStart;
	if (loopLength < 0)
		loopLength = 0;

	if (loopLength+curSmpLoopStart > s->length)
		loopLength = s->length - curSmpLoopStart;

	if (loopLength < 0)
		loopLength = 0;

	curSmpLoopLength = loopLength;

	fixLoopGadgets();
	updateLoopsOnMouseUp = true;
}

static int32_t mouseYToSampleY(int32_t my)
{
	my -= 174; // 0..SAMPLE_AREA_HEIGHT-1

	const double dTmp = my * (256.0 / SAMPLE_AREA_HEIGHT);
	const int32_t tmp32 = (const int32_t)(dTmp + 0.5); // rounded

	return 255 - CLAMP(tmp32, 0, 255);
}

static void editSampleData(bool mouseButtonHeld)
{
	int8_t *ptr8;
	int16_t *ptr16;
	int32_t tmp32, p, vl, tvl, r, rl, rvl, start, end;

	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	int32_t mx = mouse.x;
	if (mx > SCREEN_W)
		mx = SCREEN_W;

	int32_t my = mouse.y;

	if (!mouseButtonHeld)
	{
		pauseAudio();
		unfixSample(s);
		editor.editSampleFlag = true;

		lastDrawX = scr2SmpPos(mx);

		lastDrawY = mouseYToSampleY(my);

		lastMouseX = mx;
		lastMouseY = my;
	}
	else if (mx == lastMouseX && my == lastMouseY)
	{
		return; // don't continue if we didn't move the mouse
	}

	if (mx != lastMouseX)
		p = scr2SmpPos(mx);
	else
		p = lastDrawX;

	if (!keyb.leftShiftPressed && my != lastMouseY)
		vl = mouseYToSampleY(my);
	else
		vl = lastDrawY;

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

	if (s->flags & SAMPLE_16BIT)
	{
		ptr16 = (int16_t *)s->dataPtr;

		start = p;
		if (start < 0)
			start = 0;

		end = lastDrawX+1;
		if (end > s->length)
			end = s->length;

		if (p == lastDrawX)
		{
			const int16_t smpVal = (int16_t)((vl << 8) ^ 0x8000);
			for (rl = start; rl < end; rl++)
				ptr16[rl] = smpVal;
		}
		else
		{
			int32_t y = lastDrawY - vl;
			int32_t x = lastDrawX - p;

			if (x != 0)
			{
				double dMul = 1.0 / x;
				int32_t i = 0;

				for (rl = start; rl < end; rl++)
				{
					tvl = y * i;
					tvl = (int32_t)(tvl * dMul); // tvl /= x
					tvl += vl;
					tvl <<= 8;
					tvl ^= 0x8000;

					ptr16[rl] = (int16_t)tvl;
					i++;
				}
			}
		}
	}
	else // 8-bit
	{
		ptr8 = s->dataPtr;

		start = p;
		if (start < 0)
			start = 0;

		end = lastDrawX+1;
		if (end > s->length)
			end = s->length;

		if (p == lastDrawX)
		{
			const int8_t smpVal = (int8_t)(vl ^ 0x80);
			for (rl = start; rl < end; rl++)
				ptr8[rl] = smpVal;
		}
		else
		{
			int32_t y = lastDrawY - vl;
			int32_t x = lastDrawX - p;

			if (x != 0)
			{
				double dMul = 1.0 / x;
				int32_t i = 0;

				for (rl = start; rl < end; rl++)
				{
					tvl = y * i;
					tvl = (int32_t)(tvl * dMul); // tvl /= x
					tvl += vl;
					tvl ^= 0x80;

					ptr8[rl] = (int8_t)tvl;
					i++;
				}
			}
		}
	}

	lastDrawY = rvl;
	lastDrawX = r;

	writeSample(true);
}

void handleSampleDataMouseDown(bool mouseButtonHeld)
{
	if (editor.curInstr == 0)
		return;

	int32_t mx = CLAMP(mouse.x, 0, SCREEN_W+8); // allow some pixels outside of the screen
	int32_t my = CLAMP(mouse.y, 0, SCREEN_H-1);

	if (!mouseButtonHeld)
	{
		ui.rightLoopPinMoving  = false;
		ui.leftLoopPinMoving = false;
		ui.sampleDataOrLoopDrag = -1;

		mouseXOffs = 0;
		lastMouseX = mx;
		lastMouseY = my;

		mouse.lastUsedObjectType = OBJECT_SMPDATA;

		if (mouse.leftButtonPressed)
		{
			// move loop pins
			if (my < 183)
			{
				const int32_t leftLoopPinPos = getSpritePosX(SPRITE_LEFT_LOOP_PIN);
				if (mx >= leftLoopPinPos && mx <= leftLoopPinPos+16)
				{
					mouseXOffs = (leftLoopPinPos + 8) - mx;

					ui.sampleDataOrLoopDrag = true;

					setLeftLoopPinState(true);
					lastMouseX = mx;

					ui.leftLoopPinMoving = true;
					return;
				}
			}
			else if (my > 318)
			{
				const int32_t rightLoopPinPos = getSpritePosX(SPRITE_RIGHT_LOOP_PIN);
				if (mx >= rightLoopPinPos && mx <= rightLoopPinPos+16)
				{
					mouseXOffs = (rightLoopPinPos + 8) - mx;

					ui.sampleDataOrLoopDrag = true;

					setRightLoopPinState(true);
					lastMouseX = mx;

					ui.rightLoopPinMoving = true;
					return;
				}
			}

			// mark data
			lastMouseX = mx;
			ui.sampleDataOrLoopDrag = mx;

			setSampleRange(mx, mx);
		}
		else if (mouse.rightButtonPressed)
		{
			// edit data
			ui.sampleDataOrLoopDrag = true;
			editSampleData(false);
		}

		return;
	}

	if (mouse.rightButtonPressed)
	{
		editSampleData(true);
		return;
	}

	if (mx != lastMouseX)
	{
		if (mouse.leftButtonPressed)
		{
			if (ui.leftLoopPinMoving)
			{
				lastMouseX = mx;
				setLeftLoopPinPos(mouseXOffs + mx);
			}
			else if (ui.rightLoopPinMoving)
			{
				lastMouseX = mx;
				setRightLoopPinPos(mouseXOffs + mx);
			}
			else if (ui.sampleDataOrLoopDrag >= 0)
			{
				// mark data

				lastMouseX = mx;

				/* Edge-case hack for fullscreen sample marking where the width
				** of the image fills the whole screen (or close).
				*/
				if (video.fullscreen && video.renderW >= video.displayW-5 && mx == SCREEN_W-1)
					mx = SCREEN_W;

				if (mx > ui.sampleDataOrLoopDrag)
					setSampleRange(ui.sampleDataOrLoopDrag, mx);
				else if (mx == ui.sampleDataOrLoopDrag)
					setSampleRange(ui.sampleDataOrLoopDrag, ui.sampleDataOrLoopDrag);
				else if (mx < ui.sampleDataOrLoopDrag)
					setSampleRange(mx, ui.sampleDataOrLoopDrag);
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
	textOutShadow( 4, 110, PAL_FORGRND, PAL_DSKTOP2, "Range size");
	textOutShadow( 4, 124, PAL_FORGRND, PAL_DSKTOP2, "Copy buf. size");

	textOutShadow(162,  96, PAL_FORGRND, PAL_DSKTOP2, "Src.instr.");
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

	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	if (!ui.sampleEditorShown)
		showSampleEditor();

	ui.sampleEditorExtShown = true;
	ui.scopesShown = false;
	drawSampleEditorExt();
}

void hideSampleEditorExt(void)
{
	ui.sampleEditorExtShown = false;

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

	ui.scopesShown = true;
	drawScopeFramework();
}

void toggleSampleEditorExt(void)
{
	if (ui.sampleEditorExtShown)
		hideSampleEditorExt();
	else
		showSampleEditorExt();
}

static int32_t SDLCALL sampleBackwardsThread(void *ptr)
{
	int8_t tmp8, *ptrStart, *ptrEnd;
	int16_t tmp16, *ptrStart16, *ptrEnd16;

	const bool sampleDataMarked = (smpEd_Rx1 != smpEd_Rx2);
	sample_t *s = getCurSample();
	const bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (sample16Bit)
	{
		if (!sampleDataMarked)
		{
			ptrStart16 = (int16_t *)s->dataPtr;
			ptrEnd16 = (int16_t *)s->dataPtr + (s->length-1);
		}
		else
		{
			ptrStart16 = (int16_t *)s->dataPtr + smpEd_Rx1;
			ptrEnd16 = (int16_t *)s->dataPtr + (smpEd_Rx2-1);
		}

		pauseAudio();
		unfixSample(s);

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
		if (!sampleDataMarked)
		{
			ptrStart = s->dataPtr;
			ptrEnd = &s->dataPtr[s->length-1];
		}
		else
		{
			ptrStart = &s->dataPtr[smpEd_Rx1];
			ptrEnd = &s->dataPtr[smpEd_Rx2-1];
		}

		pauseAudio();
		unfixSample(s);

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

	(void)ptr;
}

void sampleBackwards(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length < 2)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampleBackwardsThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampleChangeSignThread(void *ptr)
{
	sample_t *s = getCurSample();

	pauseAudio();
	unfixSample(s);

	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr;
		for (int32_t i = 0; i < s->length; i++)
			ptr16[i] ^= 0x8000;
	}
	else
	{
		int8_t *ptr8 = s->dataPtr;
		for (int32_t i = 0; i < s->length; i++)
			ptr8[i] ^= 0x80;
	}

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	writeSampleFlag = true;
	return true;

	(void)ptr;
}

void sampleChangeSign(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(sampleChangeSignThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL sampleByteSwapThread(void *ptr)
{
	sample_t *s = getCurSample();

	pauseAudio();
	unfixSample(s);

	int32_t length = s->length;
	if (!(s->flags & SAMPLE_16BIT))
		length >>= 1;

	int8_t *ptr8 = s->dataPtr;
	for (int32_t i = 0; i < length; i++, ptr8 += 2)
	{
		const int8_t tmp = ptr8[0];
		ptr8[0] = ptr8[1];
		ptr8[1] = tmp;
	}

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	writeSampleFlag = true;
	return true;

	(void)ptr;
}

void sampleByteSwap(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	if (!(s->flags & SAMPLE_16BIT))
	{
		if (okBox(2, "System request", "Byte swapping rarely makes sense on an 8-bit sample. Continue?", NULL) != 1)
			return;
	}

	mouseAnimOn();
	thread = SDL_CreateThread(sampleByteSwapThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL fixDCThread(void *ptr)
{
	int8_t *ptr8;
	int16_t *ptr16;
	int32_t length;

	const bool sampleDataMarked = (smpEd_Rx1 != smpEd_Rx2);
	sample_t *s = getCurSample();

	if (s->flags & SAMPLE_16BIT)
	{
		if (!sampleDataMarked)
		{
			ptr16 = (int16_t *)s->dataPtr;
			length = s->length;
		}
		else
		{
			ptr16 = (int16_t *)&s->dataPtr + smpEd_Rx1;
			length = smpEd_Rx2 - smpEd_Rx1;
		}

		if (length < 0 || length > s->length)
		{
			setMouseBusy(false);
			return true;
		}

		pauseAudio();
		unfixSample(s);

		int64_t	averageDC = 0;
		for (int32_t i = 0; i < length; i++)
			averageDC += ptr16[i];
		averageDC = (averageDC + (length>>1)) / length; // rounded

		const int32_t smpSub = (int32_t)averageDC;
		for (int32_t i = 0; i < length; i++)
		{
			int32_t smp32 = ptr16[i] - smpSub;
			CLAMP16(smp32);
			ptr16[i] = (int16_t)smp32;
		}

		fixSample(s);
		resumeAudio();
	}
	else // 8-bit
	{
		if (!sampleDataMarked)
		{
			ptr8 = s->dataPtr;
			length = s->length;
		}
		else
		{
			ptr8 = &s->dataPtr[smpEd_Rx1];
			length = smpEd_Rx2 - smpEd_Rx1;
		}

		if (length < 0 || length > s->length)
		{
			setMouseBusy(false);
			return true;
		}

		pauseAudio();
		unfixSample(s);

		int64_t	averageDC = 0;
		for (int32_t i = 0; i < length; i++)
			averageDC += ptr8[i];
		averageDC = (averageDC + (length>>1)) / length; // rounded

		const int32_t smpSub = (int32_t)averageDC;
		for (int32_t i = 0; i < length; i++)
		{
			int32_t smp32 = ptr8[i] - smpSub;
			CLAMP8(smp32);
			ptr8[i] = (int8_t)smp32;
		}

		fixSample(s);
		resumeAudio();
	}

	setSongModifiedFlag();
	setMouseBusy(false);

	writeSampleFlag = true;
	return true;

	(void)ptr;
}

void fixDC(void)
{
	sample_t *s = getCurSample();
	if (s == NULL || s->dataPtr == NULL || s->length <= 0)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(fixDCThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!", NULL);
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
	if (updateLoopsOnMouseUp)
	{
		updateLoopsOnMouseUp = false;

		sample_t *s = getCurSample();
		if (s == NULL)
			return;

		if (s->loopStart != curSmpLoopStart || s->loopLength != curSmpLoopLength)
		{
			lockMixerCallback();
			unfixSample(s);
			s->loopStart = curSmpLoopStart;
			s->loopLength = curSmpLoopLength;
			fixSample(s);
			unlockMixerCallback();

			setSongModifiedFlag();
			writeSample(true);
		}
	}
}
