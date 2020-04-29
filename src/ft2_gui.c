// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <time.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_about.h"
#include "ft2_mouse.h"
#include "ft2_nibbles.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_scopes.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_wav_renderer.h"
#include "ft2_trim.h"
#include "ft2_video.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

static void releaseMouseStates(void)
{
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.leftButtonPressed = false;
	mouse.leftButtonReleased = false;
	mouse.rightButtonPressed = false;
	mouse.rightButtonReleased = false;
	mouse.firstTimePressingButton = false;
	mouse.buttonCounter = 0;
	mouse.lastX = 0;
	mouse.lastY = 0;
	ui.sampleDataOrLoopDrag = -1;
	ui.leftLoopPinMoving = false;
	ui.rightLoopPinMoving = false;
}

void unstuckLastUsedGUIElement(void)
{
	pushButton_t *p;
	radioButton_t *r;
	checkBox_t *c;
	scrollBar_t *s;

	if (mouse.lastUsedObjectID == OBJECT_ID_NONE)
	{
		/* if last object ID is OBJECT_ID_NONE, check if we moved the
		** sample data loop pins, and unstuck them if so
		*/

		if (ui.leftLoopPinMoving)
		{
			setLeftLoopPinState(false);
			ui.leftLoopPinMoving = false;
		}

		if (ui.rightLoopPinMoving)
		{
			setRightLoopPinState(false);
			ui.rightLoopPinMoving = false;
		}

		releaseMouseStates();
		return;
	}

	switch (mouse.lastUsedObjectType)
	{
		default: break;

		case OBJECT_PUSHBUTTON:
		{
			assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_PUSHBUTTONS);
			p = &pushButtons[mouse.lastUsedObjectID];
			if (p->state == PUSHBUTTON_PRESSED)
			{
				p->state = PUSHBUTTON_UNPRESSED;
				if (p->visible)
					drawPushButton(mouse.lastUsedObjectID);
			}
		}
		break;

		case OBJECT_RADIOBUTTON:
		{
			assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_RADIOBUTTONS);
			r = &radioButtons[mouse.lastUsedObjectID];
			if (r->state == RADIOBUTTON_PRESSED)
			{
				r->state = RADIOBUTTON_UNCHECKED;
				if (r->visible)
					drawRadioButton(mouse.lastUsedObjectID);
			}
		}
		break;

		case OBJECT_CHECKBOX:
		{
			assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_CHECKBOXES);
			c = &checkBoxes[mouse.lastUsedObjectID];
			if (c->state == CHECKBOX_PRESSED)
			{
				c->state = CHECKBOX_UNPRESSED;
				if (c->visible)
					drawCheckBox(mouse.lastUsedObjectID);
			}
		}
		break;

		case OBJECT_SCROLLBAR:
		{
			assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_SCROLLBARS);
			s = &scrollBars[mouse.lastUsedObjectID];
			if (s->state == SCROLLBAR_PRESSED)
			{
				s->state = SCROLLBAR_UNPRESSED;
				if (s->visible)
					drawScrollBar(mouse.lastUsedObjectID);
			}
		}
		break;
	}

	releaseMouseStates();
}

bool setupGUI(void)
{
	int32_t i;
	textBox_t *t;
	pushButton_t *p;
	checkBox_t *c;
	radioButton_t *r;
	scrollBar_t *s;

	// all memory will be NULL-tested and free'd if we return false somewhere in this function

	editor.tmpFilenameU = (UNICHAR *)calloc(PATH_MAX + 1, sizeof (UNICHAR));
	editor.tmpInstrFilenameU = (UNICHAR *)calloc(PATH_MAX + 1, sizeof (UNICHAR));

	if (editor.tmpFilenameU == NULL || editor.tmpInstrFilenameU == NULL)
		goto setupGUI_OOM;

	// set uninitialized GUI struct entries

	for (i = 1; i < NUM_TEXTBOXES; i++) // skip first entry, it's reserved for inputBox())
	{
		t = &textBoxes[i];

		t->visible = false;
		t->bufOffset = 0;
		t->cursorPos = 0;
		t->textPtr = NULL;
		t->renderBufW = (9 + 1) * t->maxChars; // 9 = max character/glyph width possible
		t->renderBufH = 10; // 10 = max character height possible
		t->renderW = t->w - (t->tx * 2);

		t->renderBuf = (uint8_t *)malloc(t->renderBufW * t->renderBufH * sizeof (int8_t));
		if (t->renderBuf == NULL)
			goto setupGUI_OOM;
	}

	for (i = 0; i < NUM_PUSHBUTTONS; i++)
	{
		p = &pushButtons[i];

		p->state = 0;
		p->visible = false;

		if (i == PB_LOGO || i == PB_BADGE)
		{
			p->bitmapFlag = true;
		}
		else
		{
			p->bitmapFlag = false;
			p->bitmapUnpressed = NULL;
			p->bitmapPressed = NULL;
		}
	}

	for (i = 0; i < NUM_CHECKBOXES; i++)
	{
		c = &checkBoxes[i];

		c->state = 0;
		c->checked = false;
		c->visible = false;
	}

	for (i = 0; i < NUM_RADIOBUTTONS; i++)
	{
		r = &radioButtons[i];

		r->state = 0;
		r->visible = false;
	}

	for (i = 0; i < NUM_SCROLLBARS; i++)
	{
		s = &scrollBars[i];

		s->visible = false;
		s->state = 0;
		s->pos = 0;
		s->page = 0;
		s->end  = 0;
		s->thumbX = 0;
		s->thumbY = 0;
		s->thumbW = 0;
		s->thumbH = 0;
	}

	setPal16(palTable[config.cfg_StdPalNr], false);

	seedAboutScreenRandom((uint32_t)time(NULL));
	setupInitialTextBoxPointers();
	setInitialTrimFlags();
	initializeScrollBars();
	setMouseMode(MOUSE_MODE_NORMAL);
	updateTextBoxPointers();
	drawGUIOnRunTime();
	updateSampleEditorSample();
	updatePatternWidth();
	initFTHelp();

	return true;

setupGUI_OOM:
	showErrorMsgBox("Not enough memory!");
	return false;
}

// TEXT ROUTINES

// returns full pixel width of a char/glyph
uint8_t charWidth(char ch)
{
	return font1Widths[ch & 0x7F];
}

// returns full pixel width of a char/glyph (big font)
uint8_t charWidth16(char ch)
{
	return font2Widths[ch & 0x7F];
}

// return full pixel width of a text string
uint16_t textWidth(const char *textPtr)
{
	uint16_t textWidth;

	assert(textPtr != NULL);

	textWidth = 0;
	while (*textPtr != '\0')
		textWidth += charWidth(*textPtr++);

	// there will be a pixel spacer at the end of the last char/glyph, remove it
	if (textWidth > 0)
		textWidth--;

	return textWidth;
}

uint16_t textNWidth(const char *textPtr, int32_t length)
{
	char ch;
	uint16_t textWidth;

	assert(textPtr != NULL);

	textWidth = 0;
	for (int32_t i = 0; i < length; i++)
	{
		ch = textPtr[i];
		if (ch == '\0')
			break;

		textWidth += charWidth(ch);
	}

	// there will be a pixel spacer at the end of the last char/glyph, remove it
	if (textWidth > 0)
		textWidth--;

	return textWidth;
}

// return full pixel width of a text string (big font)
uint16_t textWidth16(const char *textPtr)
{
	uint16_t textWidth;

	assert(textPtr != NULL);

	textWidth = 0;
	while (*textPtr != '\0')
		textWidth += charWidth(*textPtr++);

	// there will be a pixel spacer at the end of the last char/glyph, remove it
	if (textWidth > 0)
		textWidth--;

	return textWidth;
}

void charOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	pixVal = video.palette[paletteIndex];
	srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = pixVal;
			dstPtr[x] = tmp;
#endif
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void charOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, fg, bg;

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	fg = video.palette[fgPalette];
	bg = video.palette[bgPalette];

	srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT1_CHAR_W-1; x++)
			dstPtr[x] = srcPtr[x] ? fg : bg; // compiles nicely into conditional move instructions

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void charOutOutlined(uint16_t x, uint16_t y, uint8_t paletteIndex, char chr)
{
	charOut(x - 1, y,     PAL_BCKGRND, chr);
	charOut(x + 1, y,     PAL_BCKGRND, chr);
	charOut(x,     y - 1, PAL_BCKGRND, chr);
	charOut(x,     y + 1, PAL_BCKGRND, chr);

	charOut(x, y, paletteIndex, chr);
}

void charOutShadow(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr1, *dstPtr2, pixVal1, pixVal2;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	pixVal1 = video.palette[paletteIndex];
	pixVal2 = video.palette[shadowPaletteIndex];
	srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	dstPtr1 = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr2[x];
			if (srcPtr[x] != 0) tmp = pixVal2;
			dstPtr2[x] = tmp;

			tmp = dstPtr1[x];
			if (srcPtr[x] != 0) tmp = pixVal1;
			dstPtr1[x] = tmp;
#endif
		}

		srcPtr += FONT1_WIDTH;
		dstPtr1 += SCREEN_W;
		dstPtr2 += SCREEN_W;
	}
}

void charOutClipX(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, uint16_t clipX)
{
	const uint8_t *srcPtr;
	uint16_t width;
	uint32_t *dstPtr, pixVal;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (xPos > clipX)
		return;

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	pixVal = video.palette[paletteIndex];
	srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	width = FONT1_CHAR_W;
	if (xPos+width > clipX)
		width = FONT1_CHAR_W - ((xPos + width) - clipX);

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = pixVal;
			dstPtr[x] = tmp;
#endif
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void bigCharOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	srcPtr = &bmp.font2[chr * FONT2_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	pixVal = video.palette[paletteIndex];

	for (uint32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT2_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = pixVal;
			dstPtr[x] = tmp;
#endif
		}

		srcPtr += FONT2_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void bigCharOutShadow(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr1, *dstPtr2, pixVal1, pixVal2;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	pixVal1 = video.palette[paletteIndex];
	pixVal2 = video.palette[shadowPaletteIndex];
	srcPtr = &bmp.font2[chr * FONT2_CHAR_W];
	dstPtr1 = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (uint32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT2_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr2[x];
			if (srcPtr[x] != 0) tmp = pixVal2;
			dstPtr2[x] = tmp;

			tmp = dstPtr1[x];
			if (srcPtr[x] != 0) tmp = pixVal1;
			dstPtr1[x] = tmp;
#endif
		}

		srcPtr += FONT2_WIDTH;
		dstPtr1 += SCREEN_W;
		dstPtr2 += SCREEN_W;
	}
}

void textOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		charOut(currX, y, paletteIndex, chr);
		currX += charWidth(chr);
	}
}

void textOutBorder(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t borderPaletteIndex, const char *textPtr)
{
	textOut(x,   y-1, borderPaletteIndex, textPtr); // top
	textOut(x+1, y,   borderPaletteIndex, textPtr); // right
	textOut(x,   y+1, borderPaletteIndex, textPtr); // bottom
	textOut(x-1, y,   borderPaletteIndex, textPtr); // left

	textOut(x, y, paletteIndex, textPtr);
}

// fixed width
void textOutFixed(uint16_t x, uint16_t y, uint8_t fgPaltete, uint8_t bgPalette, const char *textPtr)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutBg(currX, y, fgPaltete, bgPalette, chr);
		currX += FONT1_CHAR_W-1;
	}
}

void textOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutShadow(currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth(chr);
	}
}

void bigTextOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOut(currX, y, paletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void bigTextOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOutShadow(currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void textOutClipX(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX)
{
	char chr;
	uint16_t currX;

	assert(textPtr != NULL);

	currX = x;
	while (true)
	{
		chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutClipX(currX, y, paletteIndex, chr, clipX);

		currX += charWidth(chr);
		if (currX >= clipX)
			break;
	}
}

void hexOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint32_t val, uint8_t numDigits)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;
#ifndef __arm__
	uint32_t tmp;
#endif

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	pixVal = video.palette[paletteIndex];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		srcPtr = &bmp.font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		// render glyph
		for (uint32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (uint32_t x = 0; x < FONT6_CHAR_W; x++)
			{
#ifdef __arm__
				if (srcPtr[x] != 0)
					dstPtr[x] = pixVal;
#else
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = pixVal;
				dstPtr[x] = tmp;
#endif
			}

			srcPtr += FONT6_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W; // xpos += FONT6_CHAR_W
	}
}

void hexOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint32_t val, uint8_t numDigits)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, fg, bg;

	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	fg = video.palette[fgPalette];
	bg = video.palette[bgPalette];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		// extract current nybble and set pointer to glyph
		srcPtr = &bmp.font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		// render glyph
		for (uint32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (uint32_t x = 0; x < FONT6_CHAR_W; x++)
				dstPtr[x] = srcPtr[x] ? fg : bg; // compiles nicely into conditional move instructions

			srcPtr += FONT6_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W; // xpos += FONT6_CHAR_W 
	}
}

void hexOutShadow(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, uint32_t val, uint8_t numDigits)
{
	hexOut(xPos + 1, yPos + 1, shadowPaletteIndex, val, numDigits);
	hexOut(xPos + 0, yPos + 0,       paletteIndex, val, numDigits);
}

// FILL ROUTINES

void clearRect(uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h)
{
	uint32_t *dstPtr, fillNumDwords;

	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	fillNumDwords = w * sizeof (int32_t);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		memset(dstPtr, 0, fillNumDwords);
		dstPtr += SCREEN_W;
	}
}

void fillRect(uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex)
{
	uint32_t *dstPtr, pixVal;

	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	pixVal = video.palette[paletteIndex];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
			dstPtr[x] = pixVal;

		dstPtr += SCREEN_W;
	}
}

void blit32(uint16_t xPos, uint16_t yPos, const uint32_t* srcPtr, uint16_t w, uint16_t h)
{
	uint32_t* dstPtr;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			if (srcPtr[x] != 0x00FF00)
				dstPtr[x] = srcPtr[x] | 0xFF000000; // most significant 8 bits = palette number. 0xFF because no true palette
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blit(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	uint32_t *dstPtr;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			if (srcPtr[x] != PAL_TRANSPR)
				dstPtr[x] = video.palette[srcPtr[x]];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	uint32_t *dstPtr;

	if (clipX > w)
		clipX = w;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < clipX; x++)
		{
			if (srcPtr[x] != PAL_TRANSPR)
				dstPtr[x] = video.palette[srcPtr[x]];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFast(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h) // no transparency/colorkey
{
	uint32_t *dstPtr;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
			dstPtr[x] = video.palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFastClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX) // no transparency/colorkey
{
	uint32_t *dstPtr;

	if (clipX > w)
		clipX = w;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < clipX; x++)
			dstPtr[x] = video.palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

// LINE ROUTINES

void hLine(uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	uint32_t *dstPtr, pixVal;

	assert(x < SCREEN_W && y < SCREEN_H && (x + w) <= SCREEN_W);

	pixVal = video.palette[paletteIndex];

	dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (uint32_t i = 0; i < w; i++)
		dstPtr[i] = pixVal;
}

void vLine(uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	uint32_t *dstPtr, pixVal;

	assert(x < SCREEN_W && y < SCREEN_H && (y + h) <= SCREEN_W);

	pixVal = video.palette[paletteIndex];

	dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (uint32_t i = 0; i < h; i++)
	{
		*dstPtr = pixVal;
		 dstPtr += SCREEN_W;
	}
}

void hLineDouble(uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	hLine(x, y, w, paletteIndex);
	hLine(x, y+1, w, paletteIndex);
}

void vLineDouble(uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	vLine(x, y, h, paletteIndex);
	vLine(x+1, y, h, paletteIndex);
}

void line(int16_t x1, int16_t x2, int16_t y1, int16_t y2, uint8_t paletteIndex)
{
	int16_t d, x, y, sx, sy, dx, dy;
	uint16_t ax, ay;
	int32_t pitch;
	uint32_t pixVal, *dst32;

	// get coefficients
	dx = x2 - x1;
	ax = ABS(dx) * 2;
	sx = SGN(dx);
	dy = y2 - y1;
	ay = ABS(dy) * 2;
	sy = SGN(dy);
	x  = x1;
	y  = y1;

	pixVal = video.palette[paletteIndex];
	pitch  = sy * SCREEN_W;
	dst32  = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		d = ay - (ax / 2);
		while (true)
		{
			*dst32 = pixVal;
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
			*dst32 = pixVal;
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

void drawFramework(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t type)
{
	assert(x < SCREEN_W && y < SCREEN_H && w >= 2 && h >= h);

	h--;
	w--;

	if (type == FRAMEWORK_TYPE1)
	{
		// top left corner
		hLine(x, y,     w,     PAL_DSKTOP1);
		vLine(x, y + 1, h - 1, PAL_DSKTOP1);

		// bottom right corner
		hLine(x,     y + h, w,     PAL_DSKTOP2);
		vLine(x + w, y,     h + 1, PAL_DSKTOP2);

		// fill background
		fillRect(x + 1, y + 1, w - 1, h - 1, PAL_DESKTOP);
	}
	else
	{
		// top left corner
		hLine(x, y,     w + 1, PAL_DSKTOP2);
		vLine(x, y + 1, h,     PAL_DSKTOP2);

		// bottom right corner
		hLine(x + 1, y + h, w,     PAL_DSKTOP1);
		vLine(x + w, y + 1, h - 1, PAL_DSKTOP1);

		// clear background
		clearRect(x + 1, y + 1, w - 1, h - 1);
	}
}

// GUI FUNCTIONS

void showTopLeftMainScreen(bool restoreScreens)
{
	ui.diskOpShown = false;
	ui.sampleEditorExtShown = false;
	ui.instEditorExtShown = false;
	ui.transposeShown = false;
	ui.advEditShown = false;
	ui.wavRendererShown = false;
	ui.trimScreenShown = false;

	ui.scopesShown = true;
	if (restoreScreens)
	{
		switch (ui.oldTopLeftScreen)
		{
			default: break;
			case 1: ui.diskOpShown = true; break;
			case 2: ui.sampleEditorExtShown = true; break;
			case 3: ui.instEditorExtShown = true; break;
			case 4: ui.transposeShown = true; break;
			case 5: ui.advEditShown = true; break;
			case 6: ui.wavRendererShown = true; break;
			case 7: ui.trimScreenShown = true; break;
		}

		if (ui.oldTopLeftScreen > 0)
			ui.scopesShown = false;
	}

	ui.oldTopLeftScreen = 0;

	if (ui.diskOpShown)
	{
		showDiskOpScreen();
	}
	else
	{
		// pos ed.
		drawFramework(0, 0, 112, 77, FRAMEWORK_TYPE1);
		drawFramework(2, 2,  51, 19, FRAMEWORK_TYPE2);
		drawFramework(2,30,  51, 19, FRAMEWORK_TYPE2);
		showScrollBar(SB_POS_ED);
		showPushButton(PB_POSED_POS_UP);
		showPushButton(PB_POSED_POS_DOWN);
		showPushButton(PB_POSED_INS);
		showPushButton(PB_POSED_PATT_UP);
		showPushButton(PB_POSED_PATT_DOWN);
		showPushButton(PB_POSED_DEL);
		showPushButton(PB_POSED_LEN_UP);
		showPushButton(PB_POSED_LEN_DOWN);
		showPushButton(PB_POSED_REP_UP);
		showPushButton(PB_POSED_REP_DOWN);
		textOutShadow(4, 52, PAL_FORGRND, PAL_DSKTOP2, "Songlen.");
		textOutShadow(4, 64, PAL_FORGRND, PAL_DSKTOP2, "Repstart");
		drawPosEdNums(song.songPos);
		drawSongLength();
		drawSongRepS();

		// logo button
		showPushButton(PB_LOGO);
		showPushButton(PB_BADGE);

		// left menu
		drawFramework(291, 0, 65, 173, FRAMEWORK_TYPE1);
		showPushButton(PB_ABOUT);
		showPushButton(PB_NIBBLES);
		showPushButton(PB_KILL);
		showPushButton(PB_TRIM);
		showPushButton(PB_EXTEND_VIEW);
		showPushButton(PB_TRANSPOSE);
		showPushButton(PB_INST_ED_EXT);
		showPushButton(PB_SMP_ED_EXT);
		showPushButton(PB_ADV_EDIT);
		showPushButton(PB_ADD_CHANNELS);
		showPushButton(PB_SUB_CHANNELS);

		// song/pattern
		drawFramework(112, 32, 94, 45, FRAMEWORK_TYPE1);
		drawFramework(206, 32, 85, 45, FRAMEWORK_TYPE1);
		showPushButton(PB_BPM_UP);
		showPushButton(PB_BPM_DOWN);
		showPushButton(PB_SPEED_UP);
		showPushButton(PB_SPEED_DOWN);
		showPushButton(PB_EDITADD_UP);
		showPushButton(PB_EDITADD_DOWN);
		showPushButton(PB_PATT_UP);
		showPushButton(PB_PATT_DOWN);
		showPushButton(PB_PATTLEN_UP);
		showPushButton(PB_PATTLEN_DOWN);
		showPushButton(PB_PATT_EXPAND);
		showPushButton(PB_PATT_SHRINK);
		textOutShadow(116, 36, PAL_FORGRND, PAL_DSKTOP2, "BPM");
		textOutShadow(116, 50, PAL_FORGRND, PAL_DSKTOP2, "Spd.");
		textOutShadow(116, 64, PAL_FORGRND, PAL_DSKTOP2, "Add.");
		textOutShadow(210, 36, PAL_FORGRND, PAL_DSKTOP2, "Ptn.");
		textOutShadow(210, 50, PAL_FORGRND, PAL_DSKTOP2, "Ln.");
		drawSongBPM(song.speed);
		drawSongSpeed(song.tempo);
		drawEditPattern(editor.editPattern);
		drawPatternLength(editor.editPattern);
		drawIDAdd();

		// status bar
		drawFramework(0, 77, 291, 15, FRAMEWORK_TYPE1);
		textOutShadow(4, 80, PAL_FORGRND, PAL_DSKTOP2, "Global volume");
		drawGlobalVol(song.globVol);

		ui.updatePosSections = true;

		textOutShadow(204, 80, PAL_FORGRND, PAL_DSKTOP2, "Time");
		charOutShadow(250, 80, PAL_FORGRND, PAL_DSKTOP2, ':');
		charOutShadow(270, 80, PAL_FORGRND, PAL_DSKTOP2, ':');

		drawPlaybackTime();

		     if (ui.sampleEditorExtShown) drawSampleEditorExt();
		else if (ui.instEditorExtShown)   drawInstEditorExt();
		else if (ui.transposeShown)       drawTranspose();
		else if (ui.advEditShown)         drawAdvEdit();
		else if (ui.wavRendererShown)     drawWavRenderer();
		else if (ui.trimScreenShown)      drawTrimScreen();

		if (ui.scopesShown)
			drawScopeFramework();
	}
}

void hideTopLeftMainScreen(void)
{
	hideDiskOpScreen();
	hideInstEditorExt();
	hideSampleEditorExt();
	hideTranspose();
	hideAdvEdit();
	hideWavRenderer();
	hideTrimScreen();

	ui.scopesShown = false;

	// position editor
	hideScrollBar(SB_POS_ED);

	hidePushButton(PB_POSED_POS_UP);
	hidePushButton(PB_POSED_POS_DOWN);
	hidePushButton(PB_POSED_INS);
	hidePushButton(PB_POSED_PATT_UP);
	hidePushButton(PB_POSED_PATT_DOWN);
	hidePushButton(PB_POSED_DEL);
	hidePushButton(PB_POSED_LEN_UP);
	hidePushButton(PB_POSED_LEN_DOWN);
	hidePushButton(PB_POSED_REP_UP);
	hidePushButton(PB_POSED_REP_DOWN);

	// logo button
	hidePushButton(PB_LOGO);
	hidePushButton(PB_BADGE);

	// left menu
	hidePushButton(PB_ABOUT);
	hidePushButton(PB_NIBBLES);
	hidePushButton(PB_KILL);
	hidePushButton(PB_TRIM);
	hidePushButton(PB_EXTEND_VIEW);
	hidePushButton(PB_TRANSPOSE);
	hidePushButton(PB_INST_ED_EXT);
	hidePushButton(PB_SMP_ED_EXT);
	hidePushButton(PB_ADV_EDIT);
	hidePushButton(PB_ADD_CHANNELS);
	hidePushButton(PB_SUB_CHANNELS);

	// song/pattern
	hidePushButton(PB_BPM_UP);
	hidePushButton(PB_BPM_DOWN);
	hidePushButton(PB_SPEED_UP);
	hidePushButton(PB_SPEED_DOWN);
	hidePushButton(PB_EDITADD_UP);
	hidePushButton(PB_EDITADD_DOWN);
	hidePushButton(PB_PATT_UP);
	hidePushButton(PB_PATT_DOWN);
	hidePushButton(PB_PATTLEN_UP);
	hidePushButton(PB_PATTLEN_DOWN);
	hidePushButton(PB_PATT_EXPAND);
	hidePushButton(PB_PATT_SHRINK);
}

void showTopRightMainScreen(void)
{
	// right menu
	drawFramework(356, 0, 65, 173, FRAMEWORK_TYPE1);
	showPushButton(PB_PLAY_SONG);
	showPushButton(PB_PLAY_PATT);
	showPushButton(PB_STOP);
	showPushButton(PB_RECORD_SONG);
	showPushButton(PB_RECORD_PATT);
	showPushButton(PB_DISK_OP);
	showPushButton(PB_INST_ED);
	showPushButton(PB_SMP_ED);
	showPushButton(PB_CONFIG);
	showPushButton(PB_HELP);

	// instrument switcher
	ui.instrSwitcherShown = true;
	showInstrumentSwitcher();

	// song name
	showTextBox(TB_SONG_NAME);
	drawSongName();
}

void hideTopRightMainScreen(void)
{
	// right menu
	hidePushButton(PB_PLAY_SONG);
	hidePushButton(PB_PLAY_PATT);
	hidePushButton(PB_STOP);
	hidePushButton(PB_RECORD_SONG);
	hidePushButton(PB_RECORD_PATT);
	hidePushButton(PB_DISK_OP);
	hidePushButton(PB_INST_ED);
	hidePushButton(PB_SMP_ED);
	hidePushButton(PB_CONFIG);
	hidePushButton(PB_HELP);

	// instrument switcher
	hideInstrumentSwitcher();
	ui.instrSwitcherShown = false;

	hideTextBox(TB_SONG_NAME);
}

// BOTTOM STUFF

void setOldTopLeftScreenFlag(void)
{
	     if (ui.diskOpShown)          ui.oldTopLeftScreen = 1;
	else if (ui.sampleEditorExtShown) ui.oldTopLeftScreen = 2;
	else if (ui.instEditorExtShown)   ui.oldTopLeftScreen = 3;
	else if (ui.transposeShown)       ui.oldTopLeftScreen = 4;
	else if (ui.advEditShown)         ui.oldTopLeftScreen = 5;
	else if (ui.wavRendererShown)     ui.oldTopLeftScreen = 6;
	else if (ui.trimScreenShown)      ui.oldTopLeftScreen = 7;
}

void hideTopLeftScreen(void)
{
	setOldTopLeftScreenFlag();

	hideTopLeftMainScreen();
	hideNibblesScreen();
	hideConfigScreen();
	hideAboutScreen();
	hideHelpScreen();
}

void hideTopScreen(void)
{
	setOldTopLeftScreenFlag();

	hideTopLeftMainScreen();
	hideTopRightMainScreen();
	hideNibblesScreen();
	hideConfigScreen();
	hideAboutScreen();
	hideHelpScreen();

	ui.instrSwitcherShown = false;
	ui.scopesShown = false;
}

void showTopScreen(bool restoreScreens)
{
	ui.scopesShown = false;

	if (ui.aboutScreenShown)
	{
		showAboutScreen();
	}
	else if (ui.configScreenShown)
	{
		showConfigScreen();
	}
	else if (ui.helpScreenShown)
	{
		showHelpScreen();
	}
	else if (ui.nibblesShown)
	{
		showNibblesScreen();
	}
	else
	{
		showTopLeftMainScreen(restoreScreens); // updates ui.scopesShown
		showTopRightMainScreen();
	}
}

void showBottomScreen(void)
{
	if (ui.extended || ui.patternEditorShown)
		showPatternEditor();
	else if (ui.instEditorShown)
		showInstEditor();
	else if (ui.sampleEditorShown)
		showSampleEditor();
}

void drawGUIOnRunTime(void)
{
	setScrollBarPos(SB_POS_ED, 0, false);

	showTopScreen(false); // false = don't restore screens
	showPatternEditor();

	ui.updatePosSections = true;
}
