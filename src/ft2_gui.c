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
	if (mouse.lastUsedObjectID == OBJECT_ID_NONE)
	{
		/* If last object ID is OBJECT_ID_NONE, check if we moved the
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
			pushButton_t *p = &pushButtons[mouse.lastUsedObjectID];
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
			radioButton_t *r = &radioButtons[mouse.lastUsedObjectID];
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
			checkBox_t *c = &checkBoxes[mouse.lastUsedObjectID];
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
			scrollBar_t *s = &scrollBars[mouse.lastUsedObjectID];
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
	// all memory will be NULL-tested and free'd if we return false somewhere in this function

	editor.tmpFilenameU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	editor.tmpInstrFilenameU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));

	if (editor.tmpFilenameU == NULL || editor.tmpInstrFilenameU == NULL)
		goto setupGUI_OOM;

	editor.tmpFilenameU[0] = 0;
	editor.tmpInstrFilenameU[0] = 0;

	// set uninitialized GUI struct entries

	textBox_t *t = &textBoxes[1]; // skip first entry, it's reserved for inputBox())
	for (int32_t i = 1; i < NUM_TEXTBOXES; i++, t++)
	{
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

	pushButton_t *p = pushButtons;
	for (int32_t i = 0; i < NUM_PUSHBUTTONS; i++, p++)
	{
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

	checkBox_t *c = checkBoxes;
	for (int32_t i = 0; i < NUM_CHECKBOXES; i++, c++)
	{
		c->state = 0;
		c->checked = false;
		c->visible = false;
	}

	radioButton_t *r = radioButtons;
	for (int32_t i = 0; i < NUM_RADIOBUTTONS; i++, r++)
	{
		r->state = 0;
		r->visible = false;
	}

	scrollBar_t *s = scrollBars;
	for (int32_t i = 0; i < NUM_SCROLLBARS; i++, s++)
	{
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
	assert(textPtr != NULL);

	uint16_t textWidth = 0;
	while (*textPtr != '\0')
		textWidth += charWidth(*textPtr++);

	// there will be a pixel spacer at the end of the last char/glyph, remove it
	if (textWidth > 0)
		textWidth--;

	return textWidth;
}

uint16_t textNWidth(const char *textPtr, int32_t length)
{
	assert(textPtr != NULL);

	uint16_t textWidth = 0;
	for (int32_t i = 0; i < length; i++)
	{
		const char ch = textPtr[i];
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
	assert(textPtr != NULL);

	uint16_t textWidth = 0;
	while (*textPtr != '\0')
		textWidth += charWidth(*textPtr++);

	// there will be a pixel spacer at the end of the last char/glyph, remove it
	if (textWidth > 0)
		textWidth--;

	return textWidth;
}

void textOutTiny(int32_t xPos, int32_t yPos, char *str, uint32_t color) // A..Z/a..z and 0..9
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	while (*str != '\0')
	{
		char chr = *str++;

		if (chr >= '0' && chr <= '9')
		{
			chr -= '0';
		}
		else if (chr >= 'a' && chr <= 'z')
		{
			chr -= 'a';
			chr += 10;
		}
		else if (chr >= 'A' && chr <= 'Z')
		{
			chr -= 'A';
			chr += 10;
		}
		else
		{
			dstPtr += FONT3_CHAR_W;
			continue;
		}

		const uint8_t *srcPtr = &bmp.font3[chr * FONT3_CHAR_W];
		for (int32_t y = 0; y < FONT3_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT3_CHAR_W; x++)
			{
#ifdef __arm__
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
#else
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				uint32_t tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = color;
				dstPtr[x] = tmp;
#endif
			}

			srcPtr += FONT3_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT3_CHAR_H) - FONT3_CHAR_W;
	}
}

void textOutTinyOutline(int32_t xPos, int32_t yPos, char *str) // A..Z/a..z and 0..9
{
	const uint32_t bgColor = video.palette[PAL_BCKGRND];
	const uint32_t fgColor = video.palette[PAL_FORGRND];

	textOutTiny(xPos-1, yPos,   str, bgColor);
	textOutTiny(xPos,   yPos-1, str, bgColor);
	textOutTiny(xPos+1, yPos,   str, bgColor);
	textOutTiny(xPos,   yPos+1, str, bgColor);

	textOutTiny(xPos, yPos, str, fgColor);
}

void charOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr)
{
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t pixVal = video.palette[paletteIndex];
	const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			uint32_t tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = pixVal;
			dstPtr[x] = tmp;
#endif
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void charOutFade(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, int32_t fade) // for about screen
{
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t pixVal = video.palette[paletteIndex];
	const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT1_CHAR_W; x++)
		{
			if (srcPtr[x] != 0)
			{
				const int32_t r = (RGB32_R(pixVal) * fade) >> 8;
				const int32_t g = (RGB32_G(pixVal) * fade) >> 8;
				const int32_t b = (RGB32_B(pixVal) * fade) >> 8;

				dstPtr[x] = RGB32(r, g, b) | 0xFF000000;
			}
		}

		srcPtr += FONT1_WIDTH;
		dstPtr += SCREEN_W;
	}
}

void charOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr)
{
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t fg = video.palette[fgPalette];
	const uint32_t bg = video.palette[bgPalette];

	const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT1_CHAR_W-1; x++)
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t pixVal1 = video.palette[paletteIndex];
	const uint32_t pixVal2 = video.palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr1 = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT1_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			uint32_t tmp = dstPtr2[x];
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	if (xPos > clipX)
		return;

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t pixVal = video.palette[paletteIndex];
	const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	int32_t width = FONT1_CHAR_W;
	if (xPos+width > clipX)
		width = FONT1_CHAR_W - ((xPos + width) - clipX);

	for (int32_t y = 0; y < FONT1_CHAR_H; y++)
	{
		for (int32_t x = 0; x < width; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			uint32_t tmp = dstPtr[x];
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint8_t *srcPtr = &bmp.font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	const uint32_t pixVal = video.palette[paletteIndex];

	for (int32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
				dstPtr[x] = pixVal;
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			uint32_t tmp = dstPtr[x];
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	chr &= 0x7F; // this is important to get the nordic glyphs in the font
	if (chr == ' ')
		return;

	const uint32_t pixVal1 = video.palette[paletteIndex];
	const uint32_t pixVal2 = video.palette[shadowPaletteIndex];
	const uint8_t *srcPtr = &bmp.font2[chr * FONT2_CHAR_W];
	uint32_t *dstPtr1 = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	uint32_t *dstPtr2 = dstPtr1 + (SCREEN_W+1);

	for (int32_t y = 0; y < FONT2_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT2_CHAR_W; x++)
		{
#ifdef __arm__
			if (srcPtr[x] != 0)
			{
				dstPtr2[x] = pixVal2;
				dstPtr1[x] = pixVal1;
			}
#else
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			uint32_t tmp = dstPtr2[x];
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
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOut(currX, y, paletteIndex, chr);
		currX += charWidth(chr);
	}
}

void textOutFade(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, int32_t fade) // for about screen
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

		charOutFade(currX, y, paletteIndex, chr, fade);
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
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutBg(currX, y, fgPaltete, bgPalette, chr);
		currX += FONT1_CHAR_W-1;
	}
}

void textOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		charOutShadow(currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth(chr);
	}
}

void bigTextOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOut(currX, y, paletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void bigTextOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
		if (chr == '\0')
			break;

		bigCharOutShadow(currX, y, paletteIndex, shadowPaletteIndex, chr);
		currX += charWidth16(chr);
	}
}

void textOutClipX(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	while (true)
	{
		const char chr = *textPtr++;
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	const uint32_t pixVal = video.palette[paletteIndex];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		const uint8_t *srcPtr = &bmp.font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		// render glyph
		for (int32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
			{
#ifdef __arm__
				if (srcPtr[x] != 0)
					dstPtr[x] = pixVal;
#else
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				uint32_t tmp = dstPtr[x];
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H);

	const uint32_t fg = video.palette[fgPalette];
	const uint32_t bg = video.palette[bgPalette];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits-1; i >= 0; i--)
	{
		// extract current nybble and set pointer to glyph
		const uint8_t *srcPtr = &bmp.font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		// render glyph
		for (int32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
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
	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	const uint32_t width = w * sizeof (int32_t);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		memset(dstPtr, 0, width);
		dstPtr += SCREEN_W;
	}
}

void fillRect(uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex)
{
	assert(xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	const uint32_t pixVal = video.palette[paletteIndex];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			dstPtr[x] = pixVal;

		dstPtr += SCREEN_W;
	}
}

void blit32(uint16_t xPos, uint16_t yPos, const uint32_t* srcPtr, uint16_t w, uint16_t h)
{
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
		{
			if (srcPtr[x] != 0x00FF00)
				dstPtr[x] = srcPtr[x] | 0xFF000000; // most significant 8 bits = palette number. 0xFF because no true palette
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blit32Fade(uint16_t xPos, uint16_t yPos, const uint32_t* srcPtr, uint16_t w, uint16_t h, int32_t fade) // for about screen
{
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
		{
			const uint32_t pixel = srcPtr[x];
			if (pixel != 0x00FF00)
			{
				const int32_t r = (RGB32_R(pixel) * fade) >> 8;
				const int32_t g = (RGB32_G(pixel) * fade) >> 8;
				const int32_t b = (RGB32_B(pixel) * fade) >> 8;

				dstPtr[x] = RGB32(r, g, b) | 0xFF000000; // most significant 8 bits = palette number. 0xFF because no true palette
			}
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blit(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h)
{
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
		{
			const uint32_t pixel = srcPtr[x];
			if (pixel != PAL_TRANSPR)
				dstPtr[x] = video.palette[pixel];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX)
{
	if (clipX > w)
		clipX = w;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < clipX; x++)
		{
			const uint32_t pixel = srcPtr[x];
			if (pixel != PAL_TRANSPR)
				dstPtr[x] = video.palette[pixel];
		}

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFast(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h) // no transparency/colorkey
{
	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + w) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			dstPtr[x] = video.palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

void blitFastClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX) // no transparency/colorkey
{
	if (clipX > w)
		clipX = w;

	assert(srcPtr != NULL && xPos < SCREEN_W && yPos < SCREEN_H && (xPos + clipX) <= SCREEN_W && (yPos + h) <= SCREEN_H);

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < clipX; x++)
			dstPtr[x] = video.palette[srcPtr[x]];

		srcPtr += w;
		dstPtr += SCREEN_W;
	}
}

// LINE ROUTINES

void hLine(uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex)
{
	assert(x < SCREEN_W && y < SCREEN_H && (x + w) <= SCREEN_W);

	const uint32_t pixVal = video.palette[paletteIndex];

	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < w; i++)
		dstPtr[i] = pixVal;
}

void vLine(uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex)
{
	assert(x < SCREEN_W && y < SCREEN_H && (y + h) <= SCREEN_W);

	const uint32_t pixVal = video.palette[paletteIndex];

	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (int32_t i = 0; i < h; i++)
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
	const int16_t dx = x2 - x1;
	const uint16_t ax = ABS(dx) * 2;
	const int16_t sx = SGN(dx);
	const int16_t dy = y2 - y1;
	const uint16_t ay = ABS(dy) * 2;
	const int16_t sy = SGN(dy);
	int16_t x = x1;
	int16_t y  = y1;

	uint32_t pixVal = video.palette[paletteIndex];
	const int32_t pitch  = sy * SCREEN_W;
	uint32_t *dst32  = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		int16_t d = ay - (ax >> 1);
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
		int16_t d = ax - (ay >> 1);
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
