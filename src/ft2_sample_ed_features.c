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
static int16_t vol_StartVol = 100, vol_EndVol = 100, echo_nEcho = 1, echo_VolChange = 30;
static int32_t echo_Distance = 0x100;
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
	if (instr[editor.curInstr] == NULL)
		return true;

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	const uint32_t mask = (s->typ & 16) ? 0xFFFFFFFE : 0xFFFFFFFF;
	const double dRatio = exp2(smpEd_RelReSmp / 12.0);

	double dNewLen = s->len * dRatio;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	const uint32_t newLen = (int32_t)dNewLen & mask;

	int8_t *p2 = (int8_t *)malloc(newLen + LOOP_FIX_LEN);
	if (p2 == NULL)
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	int8_t *newPtr = p2 + SMP_DAT_OFFSET;
	int8_t *p1 = s->pek;

	// 32.32 fixed-point logic
	const uint64_t delta64 = (const uint64_t)round((UINT32_MAX+1.0) / dRatio);
	uint64_t posFrac64 = 0;

	pauseAudio();
	restoreSample(s);

	/* Nearest-neighbor resampling (no interpolation).
	**
	** Could benefit from windowed-sinc interpolation,
	** but it seems like some people prefer no resampling
	** interpolation (like FT2).
	*/

	if (newLen > 0)
	{
		if (s->typ & 16)
		{
			const int16_t *src16 = (const int16_t *)p1;
			int16_t *dst16 = (int16_t *)newPtr;

			const uint32_t resampleLen = newLen >> 1;
			for (uint32_t i = 0; i < resampleLen; i++)
			{
				dst16[i] = src16[posFrac64 >> 32];
				posFrac64 += delta64;
			}
		}
		else
		{
			const int8_t *src8 = p1;
			int8_t *dst8 = newPtr;

			for (uint32_t i = 0; i < newLen; i++)
			{
				dst8[i] = src8[posFrac64 >> 32];
				posFrac64 += delta64;
			}
		}
	}

	free(s->origPek);

	s->relTon = CLAMP(s->relTon + smpEd_RelReSmp, -48, 71);
	s->len = newLen & mask;
	s->origPek = p2;
	s->pek = s->origPek + SMP_DAT_OFFSET;
	s->repS = (int32_t)(s->repS * dRatio);
	s->repL = (int32_t)(s->repL * dRatio);
	s->repS &= mask;
	s->repL &= mask;

	if (s->repS >= s->len)
		s->repS = s->len-1;

	if (s->repS+s->repL > s->len)
		s->repL = s->len - s->repS;

	if (s->typ & 16)
	{
		s->len &= 0xFFFFFFFE;
		s->repS &= 0xFFFFFFFE;
		s->repL &= 0xFFFFFFFE;
	}

	if (s->repL <= 0)
		s->typ &= ~3; // disable loop

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

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	uint32_t mask = (s->typ & 16) ? 0xFFFFFFFE : 0xFFFFFFFF;
	double dLenMul = exp2(smpEd_RelReSmp * (1.0 / 12.0));

	double dNewLen = s->len * dLenMul;
	if (dNewLen > (double)MAX_SAMPLE_LEN)
		dNewLen = (double)MAX_SAMPLE_LEN;

	textOutShadow(215, 236, PAL_FORGRND, PAL_BUTTON2, "Rel. h.tones");
	textOutShadow(215, 250, PAL_FORGRND, PAL_BUTTON2, "New sample size");
	hexOut(361, 250, PAL_FORGRND, (uint32_t)dNewLen & mask, 8);

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
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
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
	if (echo_nEcho < 1)
	{
		ui.sysReqShown = false;
		return true;
	}

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	int32_t readLen = s->len;
	int8_t *readPtr = s->pek;
	bool is16Bit = (s->typ & 16) ? true : false;
	int32_t distance = echo_Distance * 16;

	double dVolChange = echo_VolChange / 100.0;

	// calculate real number of echoes
	double dSmp = is16Bit ? 32768.0 : 128.0;
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
		if (is16Bit)
			tmp64 <<= 1;

		tmp64 += writeLen;
		if (tmp64 > MAX_SAMPLE_LEN)
			tmp64 = MAX_SAMPLE_LEN;

		writeLen = (int32_t)tmp64;

		if (is16Bit)
			writeLen &= 0xFFFFFFFE;
	}

	int8_t *writePtr = (int8_t *)malloc(writeLen + LOOP_FIX_LEN);
	if (writePtr == NULL)
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return false;
	}

	pauseAudio();
	restoreSample(s);

	int32_t writeIdx = 0;

	if (is16Bit)
	{
		const int16_t *readPtr16 = (const int16_t *)readPtr;
		int16_t *writePtr16 = (int16_t *)&writePtr[SMP_DAT_OFFSET];

		writeLen >>= 1;
		readLen >>= 1;

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

			// rounding (faster than calling round())
			     if (dSmpOut < 0.0) dSmpOut -= 0.5;
			else if (dSmpOut > 0.0) dSmpOut += 0.5;

			int32_t smpOut = (int32_t)dSmpOut;
			CLAMP16(smpOut);
			writePtr16[writeIdx++] = (int16_t)smpOut;
		}

		writeLen <<= 1;
	}
	else
	{
		int8_t *writePtr8 = writePtr + SMP_DAT_OFFSET;
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

			// rounding (faster than calling round())
			     if (dSmpOut < 0.0) dSmpOut -= 0.5;
			else if (dSmpOut > 0.0) dSmpOut += 0.5;

			int32_t smpOut = (int32_t)dSmpOut;
			     if (smpOut < -128) smpOut = -128;
			else if (smpOut >  127) smpOut =  127;
			writePtr8[writeIdx++] = (int8_t)smpOut;
		}
	}

	free(s->origPek);

	if (stopThread)
	{
		writeLen = writeIdx;

		int8_t *newPtr = (int8_t *)realloc(writePtr, writeIdx + LOOP_FIX_LEN);
		if (newPtr != NULL)
		{
			s->origPek = newPtr;
			s->pek = s->origPek + SMP_DAT_OFFSET;
		}
		else
		{
			if (writePtr != NULL)
				free(writePtr);

			s->origPek = s->pek = NULL;
			writeLen = 0;
		}

		editor.updateCurSmp = true;
	}
	else
	{
		s->origPek = writePtr;
		s->pek = s->origPek + SMP_DAT_OFFSET;
	}

	if (is16Bit)
		writeLen &= 0xFFFFFFFE;

	s->len = writeLen;

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
	hexOut(308, 240, PAL_FORGRND, (uint32_t)echo_Distance << 4, 5);

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
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
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
		for (i = 0; i < 8; i++) drawPushButton(i);
		for (i = 0; i < 3; i++) drawScrollBar(i);

		flipFrame();
	}

	hideCheckBox(0);
	for (i = 0; i < 8; i++) hidePushButton(i);
	for (i = 0; i < 3; i++) hideScrollBar(i);

	windowClose(echo_AddMemory ? false : true);

	if (outOfMemory)
		okBox(0, "System message", "Not enough memory!");
}

static int32_t SDLCALL mixThread(void *ptr)
{
	int8_t *destPtr, *mixPtr;
	uint8_t mixTyp, destTyp;
	int32_t destLen, mixLen;

	int16_t destIns = editor.curInstr;
	int16_t destSmp = editor.curSmp;
	int16_t mixIns = editor.srcInstr;
	int16_t mixSmp = editor.srcSmp;

	if (destIns == mixIns && destSmp == mixSmp)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (instr[mixIns] == NULL)
	{
		mixLen = 0;
		mixPtr = NULL;
		mixTyp = 0;
	}
	else
	{
		mixLen = instr[mixIns]->samp[mixSmp].len;
		mixPtr = instr[mixIns]->samp[mixSmp].pek;
		mixTyp = instr[mixIns]->samp[mixSmp].typ;

		if (mixPtr == NULL)
		{
			mixLen = 0;
			mixTyp = 0;
		}
	}

	if (instr[destIns] == NULL)
	{
		destLen = 0;
		destPtr = NULL;
		destTyp = 0;
	}
	else
	{
		destLen = instr[destIns]->samp[destSmp].len;
		destPtr = instr[destIns]->samp[destSmp].pek;
		destTyp = instr[destIns]->samp[destSmp].typ;

		if (destPtr == NULL)
		{
			destLen = 0;
			destTyp = 0;
		}
	}

	bool src16Bits = (mixTyp >> 4) & 1;
	bool dst16Bits = (destTyp >> 4) & 1;

	int32_t mix8Size = src16Bits ? (mixLen >> 1) : mixLen;
	int32_t dest8Size = dst16Bits ? (destLen >> 1) : destLen;
	int32_t max8Size = (dest8Size > mix8Size) ? dest8Size : mix8Size;
	int32_t maxLen = dst16Bits ? (max8Size << 1) : max8Size;

	if (maxLen <= 0)
	{
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	int8_t *p = (int8_t *)calloc(maxLen + LOOP_FIX_LEN, sizeof (int8_t));
	if (p == NULL)
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	if (instr[destIns] == NULL && !allocateInstr(destIns))
	{
		outOfMemory = true;
		setMouseBusy(false);
		ui.sysReqShown = false;
		return true;
	}

	pauseAudio();

	restoreSample(&instr[destIns]->samp[destSmp]);

	// restore source sample
	if (instr[mixIns] != NULL)
		restoreSample(&instr[mixIns]->samp[mixSmp]);

	const double dAmp1 = mix_Balance / 100.0;
	const double dAmp2 = 1.0 - dAmp1;

	int8_t *destPek = p + SMP_DAT_OFFSET;
	for (int32_t i = 0; i < max8Size; i++)
	{
		int32_t index16 = i << 1;

		int32_t smp1 = (i >= mix8Size) ? 0 : getSampleValue(mixPtr, mixTyp, src16Bits ? index16 : i);
		int32_t smp2 = (i >= dest8Size) ? 0 : getSampleValue(destPtr, destTyp, dst16Bits ? index16 : i);

		if (!src16Bits) smp1 <<= 8;
		if (!dst16Bits) smp2 <<= 8;

		double dSmp = (smp1 * dAmp1) + (smp2 * dAmp2);
		if (!dst16Bits)
			dSmp *= 1.0 / 256.0;

		// rounding (faster than calling round())
		     if (dSmp < 0.0) dSmp -= 0.5;
		else if (dSmp > 0.0) dSmp += 0.5;

		int32_t smp32 = (int32_t)dSmp;
		if (dst16Bits)
		{
			CLAMP16(smp32);
		}
		else
		{
			     if (smp32 < -128) smp32 = -128;
			else if (smp32 >  127) smp32 =  127;
		}

		putSampleValue(destPek, destTyp, dst16Bits ? index16 : i, (int16_t)smp32);
	}

	if (instr[destIns]->samp[destSmp].origPek != NULL)
		free(instr[destIns]->samp[destSmp].origPek);

	instr[destIns]->samp[destSmp].origPek = p;
	instr[destIns]->samp[destSmp].pek = instr[destIns]->samp[destSmp].origPek + SMP_DAT_OFFSET;
	instr[destIns]->samp[destSmp].len = maxLen;
	instr[destIns]->samp[destSmp].typ = destTyp;

	if (dst16Bits)
		instr[destIns]->samp[destSmp].len &= 0xFFFFFFFE;

	fixSample(&instr[destIns]->samp[destSmp]);

	// fix source sample
	if (instr[mixIns] != NULL)
		fixSample(&instr[mixIns]->samp[mixSmp]);

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
	int16_t val = (int16_t)(pos - 500);
	if (val != vol_StartVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val - 200) < 10) val =  200;
		else if (ABS(val - 300) < 10) val =  300;
		else if (ABS(val - 400) < 10) val =  400;
		else if (ABS(val + 100) < 10) val = -100;
		else if (ABS(val + 200) < 10) val = -200;
		else if (ABS(val + 300) < 10) val = -300;
		else if (ABS(val + 400) < 10) val = -400;

		vol_StartVol = val;
	}
}

static void sbSetEndVolPos(uint32_t pos)
{
	int16_t val = (int16_t)(pos - 500);
	if (val != vol_EndVol)
	{
		     if (ABS(val)       < 10) val =    0;
		else if (ABS(val - 100) < 10) val =  100;
		else if (ABS(val - 200) < 10) val =  200;
		else if (ABS(val - 300) < 10) val =  300;
		else if (ABS(val - 400) < 10) val =  400;
		else if (ABS(val + 100) < 10) val = -100;
		else if (ABS(val + 200) < 10) val = -200;
		else if (ABS(val + 300) < 10) val = -300;
		else if (ABS(val + 400) < 10) val = -400;

		vol_EndVol = val;
	}
}

static void pbSampStartVolDown(void)
{
	if (vol_StartVol > -500)
		vol_StartVol--;
}

static void pbSampStartVolUp(void)
{
	if (vol_StartVol < 500)
		vol_StartVol++;
}

static void pbSampEndVolDown(void)
{
	if (vol_EndVol > -500)
		vol_EndVol--;
}

static void pbSampEndVolUp(void)
{
	if (vol_EndVol < 500)
		vol_EndVol++;
}

static int32_t SDLCALL applyVolumeThread(void *ptr)
{
	int32_t x1, x2;

	if (instr[editor.curInstr] == NULL)
		goto applyVolumeExit;

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->len)
			x2 = s->len;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto applyVolumeExit;

		if (s->typ & 16)
		{
			x1 &= 0xFFFFFFFE;
			x2 &= 0xFFFFFFFE;
		}
	}
	else
	{
		x1 = 0;
		x2 = s->len;
	}

	if (s->typ & 16)
	{
		x1 >>= 1;
		x2 >>= 1;
	}

	const int32_t len = x2 - x1;
	if (len <= 0)
		goto applyVolumeExit;

	const double dVol1 = vol_StartVol / 100.0;
	const double dVol2 = vol_EndVol / 100.0;
	const double dPosMul = (dVol2 - dVol1) / len;

	pauseAudio();
	restoreSample(s);
	if (s->typ & 16)
	{
		int16_t *ptr16 = (int16_t *)s->pek + x1;
		for (int32_t i = 0; i < len; i++)
		{
			const double dAmp = dVol1 + (i * dPosMul); // linear interpolation

			double dSmp = ptr16[i] * dAmp;

			// rounding (faster than calling round())
			     if (dSmp < 0.0) dSmp -= 0.5;
			else if (dSmp > 0.0) dSmp += 0.5;

			int32_t amp32 = (int32_t)dSmp;
			CLAMP16(amp32);
			ptr16[i] = (int16_t)amp32;
		}
	}
	else
	{
		int8_t *ptr8 = s->pek + x1;
		for (int32_t i = 0; i < len; i++)
		{
			const double dAmp = dVol1 + (i * dPosMul); // linear interpolation

			double dSmp = ptr8[i] * dAmp;

			// rounding (faster than calling round())
			     if (dSmp < 0.0) dSmp -= 0.5;
			else if (dSmp > 0.0) dSmp += 0.5;

			int32_t amp32 = (int32_t)dSmp;
			CLAMP8(amp32);
			ptr8[i] = (int8_t)amp32;
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
	if (vol_StartVol == 100 && vol_EndVol == 100)
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

	sampleTyp *s = &instr[editor.curInstr]->samp[editor.curSmp];

	if (smpEd_Rx1 < smpEd_Rx2)
	{
		x1 = smpEd_Rx1;
		x2 = smpEd_Rx2;

		if (x2 > s->len)
			x2 = s->len;

		if (x1 < 0)
			x1 = 0;

		if (x2 <= x1)
			goto getScaleExit;

		if (s->typ & 16)
		{
			x1 &= 0xFFFFFFFE;
			x2 &= 0xFFFFFFFE;
		}
	}
	else
	{
		// no sample marking, operate on the whole sample
		x1 = 0;
		x2 = s->len;
	}

	uint32_t len = x2 - x1;
	if (s->typ & 16)
		len >>= 1;

	if (len <= 0)
	{
		vol_StartVol = 0;
		vol_EndVol = 0;
		goto getScaleExit;
	}

	restoreSample(s);

	int32_t maxAmp = 0;
	if (s->typ & 16)
	{
		const int16_t *ptr16 = (const int16_t *)&s->pek[x1];
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr16[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}
	}
	else
	{
		const int8_t *ptr8 = (const int8_t *)&s->pek[x1];
		for (uint32_t i = 0; i < len; i++)
		{
			const int32_t absSmp = ABS(ptr8[i]);
			if (absSmp > maxAmp)
				maxAmp = absSmp;
		}

		maxAmp <<= 8;
	}

	fixSample(s);

	if (maxAmp <= 0)
	{
		vol_StartVol = 0;
		vol_EndVol = 0;
	}
	else
	{
		int32_t vol = (100 * 32768) / maxAmp;
		if (vol > 500)
			vol = 500;

		vol_StartVol = (int16_t)vol;
		vol_EndVol = (int16_t)vol;
	}

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
	uint16_t val;

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

	     if (vol_StartVol == 0) sign = ' ';
	else if (vol_StartVol  < 0) sign = '-';
	else sign = '+';

	val = ABS(vol_StartVol);
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

	     if (vol_EndVol == 0) sign = ' ';
	else if (vol_EndVol  < 0) sign = '-';
	else sign = '+';

	val = ABS(vol_EndVol);
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
	setScrollBarEnd(0, 500 * 2);
	setScrollBarPos(0, 500, false);

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
	setScrollBarEnd(1, 500 * 2);
	setScrollBarPos(1, 500, false);
}

void pbSampleVolume(void)
{
	uint16_t i;

	if (editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL)
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
		setScrollBarPos(0, 500 + vol_StartVol, false);
		setScrollBarPos(1, 500 + vol_EndVol, false);
		for (i = 0; i < 7; i++) drawPushButton(i);
		for (i = 0; i < 2; i++) drawScrollBar(i);

		flipFrame();
	}

	for (i = 0; i < 7; i++) hidePushButton(i);
	for (i = 0; i < 2; i++) hideScrollBar(i);

	windowClose(true);
}
