/* This file contains the routines for the following sample editor functions:
** - Resampler
** - Echo
** - Mix
** - Volume
**/

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_mouse.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_keyboard.h"
#include "ft2_tables.h"
#include "ft2_structs.h"

static volatile bool stopThread;

static int8_t smpEd_RelReSmp, mix_Balance = 50;
static bool echo_AddMemory, exitFlag, outOfMemory;
static int16_t echo_nEcho = 1, echo_VolChange = 30;
static int32_t echo_Distance = 0x100;
static double dVol_StartVol = 100.0, dVol_EndVol = 100.0;
static SDL_Thread *thread;

static void pbExit(void)
{
	ui.sysReqShown = false;
	exitFlag = true;
}

static void windowOpen(void)
{
	ui.sysReqShown = true;
	ui.sysReqEnterPressed = false;

	unstuckLastUsedGUIElement();
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
}

static void windowClose(bool rewriteSample)
{
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	if (exitFlag || rewriteSample)
		writeSample(true);
	else
		updateNewSample();

	mouseAnimOff();
}

static void sbSetResampleTones(uint32_t pos)
{
	if (smpEd_RelReSmp != (int8_t)(pos - 36))
		smpEd_RelReSmp = (int8_t)(pos - 36);
}

static void pbResampleTonesDown(void)
{
	if (smpEd_RelReSmp > -36)
		smpEd_RelReSmp--;
}

static void pbResampleTonesUp(void)
{
	if (smpEd_RelReSmp < 36)
		smpEd_RelReSmp++;
}

static int32_t SDLCALL resampleThread(void *ptr)
{
	smpPtr_t sp;

	if (instr[editor.curInstr] == NULL)
		return true;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	const double dRatio = exp2((int32_t)smpEd_RelReSmp / 12.0);

	double dNewLen = s->length * dRatio;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	const uint32_t newLen = (int32_t)floor(dNewLen);
	if (!allocateSmpDataPtr(&sp, newLen, sample16Bit))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	int8_t *dst = sp.ptr;
	int8_t *src = s->dataPtr;

	// 32.32 fixed-point logic
	const uint64_t delta64 = (const uint64_t)round((UINT32_MAX+1.0) / dRatio);
	uint64_t posFrac64 = 0;

	pauseAudio();
	unfixSample(s);

	/* Nearest-neighbor resampling (no interpolation).
	**
	** Could benefit from windowed-sinc interpolation,
	** but it seems like some people prefer no resampling
	** interpolation (like FT2).
	*/

	if (newLen > 0)
	{
		if (sample16Bit)
		{
			const int16_t *src16 = (const int16_t *)src;
			int16_t *dst16 = (int16_t *)dst;

			for (uint32_t i = 0; i < newLen; i++)
			{
				const uint32_t position = posFrac64 >> 32;
				dst16[i] = src16[position];
				posFrac64 += delta64;
			}
		}
		else // 8-bit
		{
			const int8_t *src8 = src;
			int8_t *dst8 = dst;

			for (uint32_t i = 0; i < newLen; i++)
			{
				const uint32_t position = posFrac64 >> 32;
				dst8[i] = src8[position];
				posFrac64 += delta64;
			}
		}
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	s->relativeNote += smpEd_RelReSmp;
	s->length = newLen;
	s->loopStart = (int32_t)(s->loopStart * dRatio);
	s->loopLength = (int32_t)(s->loopLength * dRatio);

	sanitizeSample(s);

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbDoResampling(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(resampleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static void drawResampleBox(void)
{
	char sign;
	const int16_t x = 209;
	const int16_t y = 230;
	const int16_t w = 214;
	const int16_t h = 54;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	double dLenMul = exp2(smpEd_RelReSmp * (1.0 / 12.0));

	double dNewLen = s->length * dLenMul;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	textOutShadow(215, 236, PAL_FORGRND, PAL_BUTTON2, "Rel. h.tones");
	textOutShadow(215, 250, PAL_FORGRND, PAL_BUTTON2, "New sample size");
	hexOut(361, 250, PAL_FORGRND, (int32_t)dNewLen, 8);

	     if (smpEd_RelReSmp == 0) sign = ' ';
	else if (smpEd_RelReSmp  < 0) sign = '-';
	else sign = '+';

	uint16_t val = ABS(smpEd_RelReSmp);
	if (val > 9)
	{
		charOut(291, 236, PAL_FORGRND, sign);
		charOut(298, 236, PAL_FORGRND, '0' + ((val / 10) % 10));
		charOut(305, 236, PAL_FORGRND, '0' + (val % 10));
	}
	else
	{
		charOut(298, 236, PAL_FORGRND, sign);
		charOut(305, 236, PAL_FORGRND, '0' + (val % 10));
	}
}

static void setupResampleBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 214;
	p->y = 264;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbDoResampling;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 345;
	p->y = 264;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 314;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbResampleTonesDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 395;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbResampleTonesUp;
	p->visible = true;

	// echo num scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 337;
	s->y = 234;
	s->w = 58;
	s->h = 13;
	s->callbackFunc = sbSetResampleTones;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 36 * 2);
}

void pbSampleResample(void)
{
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupResampleBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbDoResampling();

		setSyncedReplayerVars();
		handleRedrawing();

		drawResampleBox();
		setScrollBarPos(0, smpEd_RelReSmp + 36, false);
		drawCheckBox(0);
		for (i = 0; i < 4; i++) drawPushButton(i);
		drawScrollBar(0);

		flipFrame();
	}

	for (i = 0; i < 4; i++) hidePushButton(i);
	hideScrollBar(0);

	windowClose(false);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!");
}

static void cbEchoAddMemory(void)
{
	echo_AddMemory ^= 1;
}

static void sbSetEchoNumPos(uint32_t pos)
{
	if (echo_nEcho != (int32_t)pos)
		echo_nEcho = (int16_t)pos;
}

static void sbSetEchoDistPos(uint32_t pos)
{
	if (echo_Distance != (int32_t)pos)
		echo_Distance = (int32_t)pos;
}

static void sbSetEchoFadeoutPos(uint32_t pos)
{
	if (echo_VolChange != (int32_t)pos)
		echo_VolChange = (int16_t)pos;
}

static void pbEchoNumDown(void)
{
	if (echo_nEcho > 0)
		echo_nEcho--;
}

static void pbEchoNumUp(void)
{
	if (echo_nEcho < 64)
		echo_nEcho++;
}

static void pbEchoDistDown(void)
{
	if (echo_Distance > 0)
		echo_Distance--;
}

static void pbEchoDistUp(void)
{
	if (echo_Distance < 16384)
		echo_Distance++;
}

static void pbEchoFadeoutDown(void)
{
	if (echo_VolChange > 0)
		echo_VolChange--;
}

static void pbEchoFadeoutUp(void)
{
	if (echo_VolChange < 100)
		echo_VolChange++;
}

static int32_t SDLCALL createEchoThread(void *ptr)
{
	smpPtr_t sp;

	if (echo_nEcho < 1)
	{
		ui.sysReqShown = false;
		return true;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	int32_t readLen = s->length;
	int8_t *readPtr = s->dataPtr;
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	int32_t distance = echo_Distance * 16;
	double dVolChange = echo_VolChange / 100.0;

	// calculate real number of echoes
	double dSmp = sample16Bit ? 32768.0 : 128.0;
	int32_t k = 0;
	while (k++ < echo_nEcho && dSmp >= 1.0)
		dSmp *= dVolChange;
	int32_t nEchoes = k + 1;

	if (nEchoes < 1)
	{
		ui.sysReqShown = false;
		return true;
	}

	// set write length (either original length or full echo length)
	int32_t writeLen = readLen;
	if (echo_AddMemory)
	{
		int64_t tmp64 = (int64_t)distance * (nEchoes - 1);

		tmp64 += writeLen;
		if (tmp64 > MAX_SAMPLE_LEN)
			tmp64 = MAX_SAMPLE_LEN;

		writeLen = (uint32_t)tmp64;
	}

	if (!allocateSmpDataPtr(&sp, writeLen, sample16Bit))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return false;
	}

	pauseAudio();
	unfixSample(s);

	int32_t writeIdx = 0;

	if (sample16Bit)
	{
		const int16_t *readPtr16 = (const int16_t *)readPtr;
		int16_t *writePtr16 = (int16_t *)sp.ptr;

		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;

			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;

			while (!stopThread)
			{
				if (echoRead < readLen)
					dSmpOut += (int32_t)readPtr16[echoRead] * dSmpMul;

				dSmpMul *= dVolChange;

				echoRead -= distance;
				if (echoRead <= 0 || --echoCycle <= 0)
					break;
			}

			DROUND(dSmpOut);

			int32_t smp32 = (int32_t)dSmpOut;
			CLAMP16(smp32);
			writePtr16[writeIdx++] = (int16_t)smp32;
		}
	}
	else // 8-bit
	{
		int8_t *writePtr8 = sp.ptr;
		while (writeIdx < writeLen)
		{
			double dSmpOut = 0.0;
			double dSmpMul = 1.0;

			int32_t echoRead = writeIdx;
			int32_t echoCycle = nEchoes;

			while (!stopThread)
			{
				if (echoRead < readLen)
					dSmpOut += (int32_t)readPtr[echoRead] * dSmpMul;

				dSmpMul *= dVolChange;

				echoRead -= distance;
				if (echoRead <= 0 || --echoCycle <= 0)
					break;
			}

			DROUND(dSmpOut);

			int32_t smp32 = (int32_t)dSmpOut;
			CLAMP8(smp32);
			writePtr8[writeIdx++] = (int8_t)smp32;
		}
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	if (stopThread) // we stopped before echo was done, realloc length
	{
		writeLen = writeIdx;
		reallocateSmpData(s, writeLen, sample16Bit);
		editor.updateCurSmp = true;
	}

	s->length = writeLen;

	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbCreateEcho(void)
{
	stopThread = false;

	mouseAnimOn();
	thread = SDL_CreateThread(createEchoThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static void drawEchoBox(void)
{
	const int16_t x = 171;
	const int16_t y = 220;
	const int16_t w = 291;
	const int16_t h = 66;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(177, 226, PAL_FORGRND, PAL_BUTTON2, "Number of echoes");
	textOutShadow(177, 240, PAL_FORGRND, PAL_BUTTON2, "Echo distance");
	textOutShadow(177, 254, PAL_FORGRND, PAL_BUTTON2, "Fade out");
	textOutShadow(192, 270, PAL_FORGRND, PAL_BUTTON2, "Add memory to sample");

	assert(echo_nEcho <= 64);
	charOut(315 + (2 * 7), 226, PAL_FORGRND, '0' + (char)(echo_nEcho / 10));
	charOut(315 + (3 * 7), 226, PAL_FORGRND, '0' + (echo_nEcho % 10));

	assert(echo_Distance <= 0x4000);
	hexOut(308, 240, PAL_FORGRND, echo_Distance << 4, 5);

	assert(echo_VolChange <= 100);
	textOutFixed(312, 254, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[echo_VolChange]);

	charOutShadow(313 + (3 * 7), 254, PAL_FORGRND, PAL_BUTTON2, '%');
}

static void setupEchoBoxWidgets(void)
{
	checkBox_t *c;
	pushButton_t *p;
	scrollBar_t *s;

	// "Add memory to sample" checkbox
	c = &checkBoxes[0];
	memset(c, 0, sizeof (checkBox_t));
	c->x = 176;
	c->y = 268;
	c->clickAreaWidth = 146;
	c->clickAreaHeight = 12;
	c->callbackFunc = cbEchoAddMemory;
	c->checked = echo_AddMemory ? CHECKBOX_CHECKED : CHECKBOX_UNCHECKED;
	c->visible = true;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 345;
	p->y = 266;
	p->w = 56;
	p->h = 16;
	p->callbackFuncOnUp = pbCreateEcho;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 402;
	p->y = 266;
	p->w = 55;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 224;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoNumDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 224;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoNumUp;
	p->visible = true;

	p = &pushButtons[4];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 238;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoDistDown;
	p->visible = true;

	p = &pushButtons[5];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 238;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoDistUp;
	p->visible = true;

	p = &pushButtons[6];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 345;
	p->y = 252;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoFadeoutDown;
	p->visible = true;

	p = &pushButtons[7];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 434;
	p->y = 252;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbEchoFadeoutUp;
	p->visible = true;

	// echo num scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 224;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoNumPos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 64);

	// echo distance scrollbar
	s = &scrollBars[1];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 238;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoDistPos;
	s->visible = true;
	setScrollBarPageLength(1, 1);
	setScrollBarEnd(1, 16384);

	// echo fadeout scrollbar
	s = &scrollBars[2];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 368;
	s->y = 252;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetEchoFadeoutPos;
	s->visible = true;
	setScrollBarPageLength(2, 1);
	setScrollBarEnd(2, 100);
}

void handleEchoToolPanic(void)
{
	stopThread = true;
}

void pbSampleEcho(void)
{
	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupEchoBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbCreateEcho();

		setSyncedReplayerVars();
		handleRedrawing();

		drawEchoBox();
		setScrollBarPos(0, echo_nEcho, false);
		setScrollBarPos(1, echo_Distance, false);
		setScrollBarPos(2, echo_VolChange, false);
		drawCheckBox(0);
		for (uint16_t i = 0; i < 8; i++) drawPushButton(i);
		for (uint16_t i = 0; i < 3; i++) drawScrollBar(i);

		flipFrame();
	}

	hideCheckBox(0);
	for (uint16_t i = 0; i < 8; i++) hidePushButton(i);
	for (uint16_t i = 0; i < 3; i++) hideScrollBar(i);

	windowClose(echo_AddMemory ? false : true);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!");
}

static int32_t SDLCALL mixThread(void *ptr)
{
	smpPtr_t sp;

	int8_t *dstPtr, *mixPtr;
	uint8_t mixFlags, dstFlags;
	int32_t dstLen, mixLen;

	int16_t dstIns = editor.curInstr;
	int16_t dstSmp = editor.curSmp;
	int16_t mixIns = editor.srcInstr;
	int16_t mixSmp = editor.srcSmp;

	sample_t *s = &instr[dstIns]->smp[dstSmp];
	sample_t *sSrc = &instr[mixIns]->smp[mixSmp];

	if (dstIns == mixIns && dstSmp == mixSmp)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (instr[mixIns] == NULL)
	{
		mixLen = 0;
		mixPtr = NULL;
		mixFlags = 0;
	}
	else
	{
		mixLen = sSrc->length;
		mixPtr = sSrc->dataPtr;
		mixFlags = sSrc->flags;

		if (mixPtr == NULL)
		{
			mixLen = 0;
			mixFlags = 0;
		}
	}

	if (instr[dstIns] == NULL)
	{
		dstLen = 0;
		dstPtr = NULL;
		dstFlags = 0;
	}
	else
	{
		dstLen = s->length;
		dstPtr = s->dataPtr;
		dstFlags = s->flags;

		if (dstPtr == NULL)
		{
			dstLen = 0;
			dstFlags = 0;
		}
	}

	bool src16Bits = !!(mixFlags & SAMPLE_16BIT);
	bool dst16Bits = !!(dstFlags & SAMPLE_16BIT);

	int32_t maxLen = (dstLen > mixLen) ? dstLen : mixLen;
	if (maxLen == 0)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (!allocateSmpDataPtr(&sp, maxLen, dst16Bits))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}
	memset(sp.ptr, 0, maxLen);

	if (instr[dstIns] == NULL && !allocateInstr(dstIns))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	pauseAudio();
	unfixSample(s);

	// unfix source sample
	if (instr[mixIns] != NULL)
		unfixSample(sSrc);

	const double dAmp1 = mix_Balance / 100.0;
	const double dAmp2 = 1.0 - dAmp1;
	const double dSmp1ScaleMul = src16Bits ? (1.0 / 32768.0) : (1.0 / 128.0);
	const double dSmp2ScaleMul = dst16Bits ? (1.0 / 32768.0) : (1.0 / 128.0);
	const double dNormalizeMul = dst16Bits ? 32768.0 : 128.0;

	for (int32_t i = 0; i < maxLen; i++)
	{
		double dSmp1 = (i >= mixLen) ? 0.0 : (getSampleValue(mixPtr, i, src16Bits) * dSmp1ScaleMul); // -1.0 .. 0.999inf
		double dSmp2 = (i >= dstLen) ? 0.0 : (getSampleValue(dstPtr, i, dst16Bits) * dSmp2ScaleMul); // -1.0 .. 0.999inf

		const double dSmp = ((dSmp1 * dAmp1) + (dSmp2 * dAmp2)) * dNormalizeMul;
		putSampleValue(sp.ptr, i, dSmp, dst16Bits);
	}

	freeSmpData(s);
	setSmpDataPtr(s, &sp);

	s->length = maxLen;
	s->flags = dstFlags;

	fixSample(s);

	// re-fix source sample again
	if (instr[mixIns] != NULL)
		fixSample(sSrc);

	resumeAudio();

	setSongModifiedFlag();
	setMouseBusy(false);

	ui.sysReqShown = false;
	return true;

	(void)ptr;
}

static void pbMix(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(mixThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static void sbSetMixBalancePos(uint32_t pos)
{
	if (mix_Balance != (int8_t)pos)
		mix_Balance = (int8_t)pos;
}

static void pbMixBalanceDown(void)
{
	if (mix_Balance > 0)
		mix_Balance--;
}

static void pbMixBalanceUp(void)
{
	if (mix_Balance < 100)
		mix_Balance++;
}

static void drawMixSampleBox(void)
{
	const int16_t x = 192;
	const int16_t y = 240;
	const int16_t w = 248;
	const int16_t h = 38;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(198, 246, PAL_FORGRND, PAL_BUTTON2, "Mixing balance");

	assert((mix_Balance >= 0) && (mix_Balance <= 100));
	textOutFixed(299, 246, PAL_FORGRND, PAL_BUTTONS, dec3StrTab[mix_Balance]);
}

static void setupMixBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 197;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbMix;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 361;
	p->y = 258;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 322;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbMixBalanceDown;
	p->visible = true;

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 411;
	p->y = 244;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbMixBalanceUp;
	p->visible = true;

	// mixing balance scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 345;
	s->y = 244;
	s->w = 66;
	s->h = 13;
	s->callbackFunc = sbSetMixBalancePos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 100);
}

void pbSampleMix(void)
{
	uint16_t i;

	if (editor.curInstr == 0)
		return;

	setupMixBoxWidgets();
	windowOpen();

	outOfMemory = false;

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
			pbMix();

		setSyncedReplayerVars();
		handleRedrawing();

		drawMixSampleBox();
		setScrollBarPos(0, mix_Balance, false);
		for (i = 0; i < 4; i++) drawPushButton(i);
		drawScrollBar(0);

		flipFrame();
	}

	for (i = 0; i < 4; i++) hidePushButton(i);
	hideScrollBar(0);

	windowClose(false);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!");
}

static void sbSetStartVolPos(uint32_t pos)
{
	int32_t val = (int32_t)(pos - 200);
	if (val != (int32_t)dVol_StartVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val + 100) < 10) val = -100;

		dVol_StartVol = (double)val;
	}
}

static void sbSetEndVolPos(uint32_t pos)
{
	int32_t val = (int32_t)(pos - 200);
	if (val != (int32_t)dVol_EndVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val + 100) < 10) val = -100;

		dVol_EndVol = val;
	}
}

static void pbSampStartVolDown(void)
{
	if (dVol_StartVol > -200.0)
		dVol_StartVol -= 1.0;

	dVol_StartVol = floor(dVol_StartVol);
}

static void pbSampStartVolUp(void)
{
	if (dVol_StartVol < 200.0)
		dVol_StartVol += 1.0;

	dVol_StartVol = floor(dVol_StartVol);
}

static void pbSampEndVolDown(void)
{
	if (dVol_EndVol > -200.0)
		dVol_EndVol -= 1.0;

	dVol_EndVol = floor(dVol_EndVol);
}

static void pbSampEndVolUp(void)
{
	if (dVol_EndVol < 200.0)
		dVol_EndVol += 1.0;

	dVol_EndVol = floor(dVol_EndVol);
}

static int32_t SDLCALL applyVolumeThread(void *ptr)
{
	int32_t x1, x2;

	if (instr[editor.curInstr] == NULL)
		goto applyVolumeExit;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto applyVolumeExit;
	}
	else
	{
		// no mark, operate on whole sample
		x1 = 0;
		x2 = s->length;
	}

	const int32_t len = x2 - x1;
	if (len <= 0)
		goto applyVolumeExit;

	bool mustInterpolate = (dVol_StartVol != dVol_EndVol);
	const double dVol = dVol_StartVol / 100.0;
	const double dPosMul = ((dVol_EndVol / 100.0) - dVol) / len;

	pauseAudio();
	unfixSample(s);
	if (s->flags & SAMPLE_16BIT)
	{
		int16_t *ptr16 = (int16_t *)s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				double dSmp = (int32_t)ptr16[i] * (dVol + (i * dPosMul)); // linear interpolation
				DROUND(dSmp);

				int32_t smp32 = (int32_t)dSmp;
				CLAMP16(smp32);
				ptr16[i] = (int16_t)smp32;
			}

		}
		else // no interpolation needed
		{
			for (int32_t i = 0; i < len; i++)
			{
				double dSmp = (int32_t)ptr16[i] * dVol;
				DROUND(dSmp);

				int32_t smp32 = (int32_t)dSmp;
				CLAMP16(smp32);
				ptr16[i] = (int16_t)smp32;
			}
		}
	}
	else // 8-bit sample
	{
		int8_t *ptr8 = s->dataPtr + x1;
		if (mustInterpolate)
		{
			for (int32_t i = 0; i < len; i++)
			{
				double dSmp = (int32_t)ptr8[i] * (dVol + (i * dPosMul)); // linear interpolation
				DROUND(dSmp);

				int32_t smp32 = (int32_t)dSmp;
				CLAMP8(smp32);
				ptr8[i] = (int8_t)smp32;
			}
		}
		else // no interpolation needed
		{
			for (int32_t i = 0; i < len; i++)
			{
				double dSmp = (int32_t)ptr8[i] * dVol;
				DROUND(dSmp);

				int32_t smp32 = (int32_t)dSmp;
				CLAMP8(smp32);
				ptr8[i] = (int8_t)smp32;
			}
		}
	}
	fixSample(s);
	resumeAudio();

	setSongModifiedFlag();

applyVolumeExit:
	setMouseBusy(false);
	ui.sysReqShown = false;

	return true;

	(void)ptr;
}

static void pbApplyVolume(void)
{
	if (dVol_StartVol == 100.0 && dVol_EndVol == 100.0)
	{
		ui.sysReqShown = false;
		return; // no volume change to be done
	}

	mouseAnimOn();
	thread = SDL_CreateThread(applyVolumeThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int32_t SDLCALL getMaxScaleThread(void *ptr)
{
	int32_t x1, x2;

	if (instr[editor.curInstr] == NULL)
		goto getScaleExit;

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->length)
			x2 = s->length;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto getScaleExit;
	}
	else
	{
		// no sample marking, operate on the whole sample
		x1 = 0;
		x2 = s->length;
	}

	uint32_t len = x2 - x1;
	if (len <= 0)
	{
		dVol_StartVol = dVol_EndVol = 100.0;
		goto getScaleExit;
	}

	double dVolChange = 100.0;

	/* If sample is looped and the loopEnd point is inside the marked range,
	** we need to unfix the fixed interpolation sample before scanning,
	** and fix it again after we're done.
	*/
	bool hasLoop = GET_LOOPTYPE(s->flags) != LOOP_OFF;
	const int32_t loopEnd = s->loopStart + s->loopLength;
	bool fixedSampleInRange = hasLoop && (x1 <= loopEnd) && (x2 >= loopEnd);

	if (fixedSampleInRange)
		unfixSample(s);

	int32_t maxAmp = 0;
	if (s->flags & SAMPLE_16BIT)
	{
		const int16_t *ptr16 = (const int16_t *)s->dataPtr + x1;
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr16[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}

		if (maxAmp > 0)
			dVolChange = (32767.0 / maxAmp) * 100.0;
	}
	else // 8-bit
	{
		const int8_t *ptr8 = (const int8_t *)&s->dataPtr[x1];
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr8[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}

		if (maxAmp > 0)
			dVolChange = (127.0 / maxAmp) * 100.0;
	}

	if (fixedSampleInRange)
		fixSample(s);

	if (dVolChange < 100.0) // yes, this can happen...
		dVolChange = 100.0;

	dVol_StartVol = dVol_EndVol = dVolChange;

getScaleExit:
	setMouseBusy(false);
	return true;

	(void)ptr;
}

static void pbGetMaxScale(void)
{
	mouseAnimOn();
	thread = SDL_CreateThread(getMaxScaleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static void drawSampleVolumeBox(void)
{
	char sign;
	const int16_t x = 166;
	const int16_t y = 230;
	const int16_t w = 301;
	const int16_t h = 52;
	uint32_t val;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	textOutShadow(172, 236, PAL_FORGRND, PAL_BUTTON2, "Start volume");
	textOutShadow(172, 250, PAL_FORGRND, PAL_BUTTON2, "End volume");
	charOutShadow(282, 236, PAL_FORGRND, PAL_BUTTON2, '%');
	charOutShadow(282, 250, PAL_FORGRND, PAL_BUTTON2, '%');

	const int32_t startVol = (int32_t)dVol_StartVol;
	const int32_t endVol = (int32_t)dVol_EndVol;

	if (startVol > 200)
	{
		charOut(253, 236, PAL_FORGRND, '>');
		charOut(260, 236, PAL_FORGRND, '2');
		charOut(267, 236, PAL_FORGRND, '0');
		charOut(274, 236, PAL_FORGRND, '0');
	}
	else
	{
		     if (startVol == 0) sign = ' ';
		else if (startVol  < 0) sign = '-';
		else sign = '+';

		val = ABS(startVol);
		if (val > 99)
		{
			charOut(253, 236, PAL_FORGRND, sign);
			charOut(260, 236, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(267, 236, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(260, 236, PAL_FORGRND, sign);
			charOut(267, 236, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(274, 236, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(267, 236, PAL_FORGRND, sign);
			charOut(274, 236, PAL_FORGRND, '0' + (char)val);
		}
	}

	if (endVol > 200)
	{
		charOut(253, 250, PAL_FORGRND, '>');
		charOut(260, 250, PAL_FORGRND, '2');
		charOut(267, 250, PAL_FORGRND, '0');
		charOut(274, 250, PAL_FORGRND, '0');
	}
	else
	{
		     if (endVol == 0) sign = ' ';
		else if (endVol  < 0) sign = '-';
		else sign = '+';

		val = ABS(endVol);
		if (val > 99)
		{
			charOut(253, 250, PAL_FORGRND, sign);
			charOut(260, 250, PAL_FORGRND, '0' + (char)(val / 100));
			charOut(267, 250, PAL_FORGRND, '0' + ((val / 10) % 10));
			charOut(274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else if (val > 9)
		{
			charOut(260, 250, PAL_FORGRND, sign);
			charOut(267, 250, PAL_FORGRND, '0' + (char)(val / 10));
			charOut(274, 250, PAL_FORGRND, '0' + (val % 10));
		}
		else
		{
			charOut(267, 250, PAL_FORGRND, sign);
			charOut(274, 250, PAL_FORGRND, '0' + (char)val);
		}
	}
}

static void setupVolumeBoxWidgets(void)
{
	pushButton_t *p;
	scrollBar_t *s;

	// "Apply" pushbutton
	p = &pushButtons[0];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Apply";
	p->x = 171;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbApplyVolume;
	p->visible = true;

	// "Get maximum scale" pushbutton
	p = &pushButtons[1];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Get maximum scale";
	p->x = 245;
	p->y = 262;
	p->w = 143;
	p->h = 16;
	p->callbackFuncOnUp = pbGetMaxScale;
	p->visible = true;

	// "Exit" pushbutton
	p = &pushButtons[2];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = "Exit";
	p->x = 389;
	p->y = 262;
	p->w = 73;
	p->h = 16;
	p->callbackFuncOnUp = pbExit;
	p->visible = true;

	// scrollbar buttons

	p = &pushButtons[3];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampStartVolDown;
	p->visible = true;

	p = &pushButtons[4];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 234;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampStartVolUp;
	p->visible = true;

	p = &pushButtons[5];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_LEFT_STRING;
	p->x = 292;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampEndVolDown;
	p->visible = true;

	p = &pushButtons[6];
	memset(p, 0, sizeof (pushButton_t));
	p->caption = ARROW_RIGHT_STRING;
	p->x = 439;
	p->y = 248;
	p->w = 23;
	p->h = 13;
	p->preDelay = 1;
	p->delayFrames = 3;
	p->callbackFuncOnDown = pbSampEndVolUp;
	p->visible = true;

	// volume start scrollbar
	s = &scrollBars[0];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 315;
	s->y = 234;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = sbSetStartVolPos;
	s->visible = true;
	setScrollBarPageLength(0, 1);
	setScrollBarEnd(0, 200 * 2);
	setScrollBarPos(0, 200, false);

	// volume end scrollbar
	s = &scrollBars[1];
	memset(s, 0, sizeof (scrollBar_t));
	s->x = 315;
	s->y = 248;
	s->w = 124;
	s->h = 13;
	s->callbackFunc = sbSetEndVolPos;
	s->visible = true;
	setScrollBarPageLength(1, 1);
	setScrollBarEnd(1, 200 * 2);
	setScrollBarPos(1, 200, false);
}

void pbSampleVolume(void)
{
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->smp[editor.curSmp].dataPtr == NULL)
	{
		return;
	}

	setupVolumeBoxWidgets();
	windowOpen();

	exitFlag = false;
	while (ui.sysReqShown)
	{
		readInput();
		if (ui.sysReqEnterPressed)
		{
			pbApplyVolume();
			keyb.ignoreCurrKeyUp = true; // don't handle key up event for this key release
		}

		setSyncedReplayerVars();
		handleRedrawing();

		// this is needed for the "Get maximum scale" button
		if (ui.setMouseIdle) mouseAnimOff();

		drawSampleVolumeBox();

		const int32_t startVol = (int32_t)dVol_StartVol;
		const int32_t endVol = (int32_t)dVol_EndVol;

		setScrollBarPos(0, 200 + startVol, false);
		setScrollBarPos(1, 200 + endVol, false);
		for (i = 0; i < 7; i++) drawPushButton(i);
		for (i = 0; i < 2; i++) drawScrollBar(i);

		flipFrame();
	}

	for (i = 0; i < 7; i++) hidePushButton(i);
	for (i = 0; i < 2; i++) hideScrollBar(i);

	windowClose(true);
}
