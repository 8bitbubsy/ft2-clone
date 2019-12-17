// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_pattern_ed.h"
#include "ft2_gfxdata.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"

static const uint8_t vol2charTab1[16] = { 39, 0, 1, 2, 3, 4, 36, 52, 53, 54, 28, 31, 25, 58, 59, 22 };
static const uint8_t vol2charTab2[16] = { 42, 0, 1, 2, 3, 4, 36, 37, 38, 39, 28, 31, 25, 40, 41, 22 };
static const uint8_t columnModeTab[12] = { 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3 };
static const uint8_t sharpNote1Char_small[12] = {  8*6,  8*6,  9*6,  9*6, 10*6, 11*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6 };
static const uint8_t sharpNote2Char_small[12] = { 16*6, 15*6, 16*6, 15*6, 16*6, 16*6, 15*6, 16*6, 15*6, 16*6, 15*6, 16*6 };
static const uint8_t flatNote1Char_small[12] = {  8*6,  9*6,  9*6, 10*6, 10*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6, 14*6 };
static const uint8_t flatNote2Char_small[12] = { 16*6, 17*6, 16*6, 17*6, 16*6, 16*6, 17*6, 16*6, 17*6, 16*6, 17*6, 16*6 };
static const uint8_t sharpNote1Char_med[12] = { 12*8, 12*8, 13*8, 13*8, 14*8, 15*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8 };
static const uint16_t sharpNote2Char_med[12] = { 36*8, 37*8, 36*8, 37*8, 36*8, 36*8, 37*8, 36*8, 37*8, 36*8, 37*8, 36*8 };
static const uint8_t flatNote1Char_med[12] = { 12*8, 13*8, 13*8, 14*8, 14*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8, 11*8 };
static const uint16_t flatNote2Char_med[12] = { 36*8, 38*8, 36*8, 38*8, 36*8, 36*8, 38*8, 36*8, 38*8, 36*8, 38*8, 36*8 };
static const uint16_t sharpNote1Char_big[12] = { 12*16, 12*16, 13*16, 13*16, 14*16, 15*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16 };
static const uint16_t sharpNote2Char_big[12] = { 36*16, 37*16, 36*16, 37*16, 36*16, 36*16, 37*16, 36*16, 37*16, 36*16, 37*16, 36*16 };
static const uint16_t flatNote1Char_big[12] = { 12*16, 13*16, 13*16, 14*16, 14*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16, 11*16 };
static const uint16_t flatNote2Char_big[12] = { 36*16, 38*16, 36*16, 38*16, 36*16, 36*16, 38*16, 36*16, 38*16, 36*16, 38*16, 36*16 };

static tonTyp emptyNote;

// ft2_pattern_ed.c
extern const uint16_t chanWidths[6];

// defined at the bottom of this file
extern const pattCoord_t pattCoordTable[2][2][2];
extern const pattCoord2_t pattCoord2Table[2][2][2];
extern const markCoord_t markCoordTable[2][2][2];
extern const uint8_t pattCursorXTab[2 * 4 * 8];
extern const uint8_t pattCursorWTab[2 * 4 * 8];

static void rowNumOut(uint32_t yPos, uint8_t paletteIndex, uint8_t rowChar1, uint8_t rowChar2);
static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t paletteIndex, uint8_t chr, uint8_t fontType);
static void drawEmptyNoteSmall(uint16_t x, uint16_t y, uint8_t paletteIndex);
static void drawKeyOffSmall(uint16_t x, uint16_t y, uint8_t paletteIndex);
static void drawNoteSmall(uint16_t x, uint16_t y, uint8_t paletteIndex, int16_t ton);
static void drawEmptyNoteMedium(uint16_t x, uint16_t y, uint8_t paletteIndex);
static void drawKeyOffMedium(uint16_t x, uint16_t y, uint8_t paletteIndex);
static void drawNoteMedium(uint16_t x, uint16_t y, uint8_t paletteIndex, int16_t ton);
static void drawEmptyNoteBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex);
static void drawKeyOffBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex);
static void drawNoteBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, int16_t ton);

void drawPatternBorders(void)
{
	uint8_t chans;
	uint16_t xOffs, chanWidth;
	int32_t clearSize;
	const pattCoord2_t *pattCoord;

	// get heights/pos/rows depending on configuration
	pattCoord = &pattCoord2Table[config.ptnUnpressed][editor.ui.pattChanScrollShown][editor.ui.extended];

	// set pattern cursor Y position
	editor.ptnCursorY = pattCoord->lowerRowsY - 9;

	chans = editor.ui.numChannelsShown;
	if (chans > editor.ui.maxVisibleChannels)
		chans = editor.ui.maxVisibleChannels;

	// in some configurations, there will be two empty channels to the right, fix that
	if (chans == 2)
		chans = 4;
	else if (chans == 10 && !config.ptnS3M)
		chans = 12;

	assert(chans >= 2 && chans <= 12);

	chanWidth = chanWidths[(chans / 2) - 1] + 2;

	// fill scrollbar framework (if needed)
	if (editor.ui.pattChanScrollShown)
		drawFramework(0, 383, 632, 17, FRAMEWORK_TYPE1);

	if (config.ptnFrmWrk)
	{
		// pattern editor w/ framework

		if (editor.ui.extended)
		{
			vLine(0,   54, 345, PAL_DSKTOP1);
			vLine(631, 53, 346, PAL_DSKTOP2);

			vLine(1,   54, 345, PAL_DESKTOP);
			vLine(630, 54, 345, PAL_DESKTOP);

			hLine(0, 53, 631, PAL_DSKTOP1);
			hLine(1, 54, 630, PAL_DESKTOP);

			if (!editor.ui.pattChanScrollShown)
			{
				hLine(1, 398, 630, PAL_DESKTOP);
				hLine(0, 399, 632, PAL_DSKTOP2);
			}
		}
		else
		{
			vLine(0,   174, 225, PAL_DSKTOP1);
			vLine(631, 173, 226, PAL_DSKTOP2);

			vLine(1,   174, 225, PAL_DESKTOP);
			vLine(630, 174, 225, PAL_DESKTOP);

			hLine(0, 173, 631, PAL_DSKTOP1);
			hLine(1, 174, 630, PAL_DESKTOP);

			if (!editor.ui.pattChanScrollShown)
			{
				hLine(1, 398, 630, PAL_DESKTOP);
				hLine(0, 399, 632, PAL_DSKTOP2);
			}
		}

		// fill middle (current row)
		fillRect(2, pattCoord->lowerRowsY - 9, 628, 9, PAL_DESKTOP);

		// fill row number boxes
		drawFramework(2, pattCoord->upperRowsY, 25, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top left
		drawFramework(604, pattCoord->upperRowsY, 26, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top right
		drawFramework(2, pattCoord->lowerRowsY, 25, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom left
		drawFramework(604, pattCoord->lowerRowsY, 26, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom right

		// draw channels
		xOffs = 28;
		for (uint8_t i = 0; i < chans; i++)
		{
			vLine(xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
			vLine(xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);

			drawFramework(xOffs, pattCoord->upperRowsY, chanWidth, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top part
			drawFramework(xOffs, pattCoord->lowerRowsY, chanWidth, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom part

			xOffs += (chanWidth + 1);
		}

		vLine(xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
		vLine(xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);
	}
	else
	{
		// pattern editor without framework

		if (editor.ui.extended)
		{
			clearSize = editor.ui.pattChanScrollShown ? (SCREEN_W * sizeof (int32_t) * 330) : (SCREEN_W * sizeof (int32_t) * 347);
			memset(&video.frameBuffer[53 * SCREEN_W], 0, clearSize);
		}
		else
		{
			clearSize = editor.ui.pattChanScrollShown ? (SCREEN_W * sizeof(int32_t) * 210) : (SCREEN_W * sizeof(int32_t) * 227);
			memset(&video.frameBuffer[173 * SCREEN_W], 0, clearSize);
		}

		drawFramework(0, pattCoord->lowerRowsY - 10, SCREEN_W, 11, FRAMEWORK_TYPE1);
	}

	if (editor.ui.pattChanScrollShown)
	{
		showScrollBar(SB_CHAN_SCROLL);
		showPushButton(PB_CHAN_SCROLL_LEFT);
		showPushButton(PB_CHAN_SCROLL_RIGHT);
	}
	else
	{
		hideScrollBar(SB_CHAN_SCROLL);
		hidePushButton(PB_CHAN_SCROLL_LEFT);
		hidePushButton(PB_CHAN_SCROLL_RIGHT);

		setScrollBarPos(SB_CHAN_SCROLL, 0, false);
	}
}

static void writeCursor(void)
{
	uint32_t *dstPtr, xPos, width, tabOffset;

	tabOffset = (config.ptnS3M * 32) + (columnModeTab[editor.ui.numChannelsShown - 1] * 8) + editor.cursor.object;

	xPos = pattCursorXTab[tabOffset];
	width = pattCursorWTab[tabOffset];

	assert(editor.ptnCursorY > 0 && xPos > 0 && width > 0);
	xPos += ((editor.cursor.ch - editor.ui.channelOffset) * editor.ui.patternChannelWidth);

	dstPtr = &video.frameBuffer[(editor.ptnCursorY * SCREEN_W) + xPos];
	for (uint32_t y = 0; y < 9; y++)
	{
		for (uint32_t x = 0; x < width; x++)
			dstPtr[x] = video.palette[(dstPtr[x] >> 24) ^ 4]; // ">> 24" to get palette, XOR 4 to change to cursor palette

		dstPtr += SCREEN_W;
	}
}

static void writePatternBlockMark(int16_t currRow, uint16_t rowHeight, const pattCoord_t *pattCoord)
{
	uint8_t startCh, endCh;
	int16_t startRow, endRow, x1, x2, y1, y2;
	uint16_t pattYStart, pattYEnd;
	uint32_t w, h, *ptr32;
	const markCoord_t *markCoord;

	// this can happen (buggy FT2 code), treat like no mark
	if (pattMark.markY1 > pattMark.markY2)
		return;

	startCh = editor.ui.channelOffset;
	endCh = editor.ui.channelOffset + (editor.ui.numChannelsShown - 1);
	startRow = currRow - pattCoord->numUpperRows;
	endRow = currRow + pattCoord->numLowerRows;

	// test if pattern marking is outside of visible area (don't draw)
	if (pattMark.markX1 > endCh || pattMark.markX2 < startCh || pattMark.markY1 > endRow || pattMark.markY2 < startRow)
		return;

	markCoord = &markCoordTable[config.ptnUnpressed][editor.ui.pattChanScrollShown][editor.ui.extended];
	pattYStart = markCoord->upperRowsY;

	// X1

	x1 = 32 + ((pattMark.markX1 - editor.ui.channelOffset) * editor.ui.patternChannelWidth);
	if (x1 < 32)
		x1 = 32;

	// X2

	x2 = (32 - 8) + (((pattMark.markX2 + 1) - editor.ui.channelOffset) * editor.ui.patternChannelWidth);
	if (x2 > 608)
		x2 = 608;

	// Y1

	if (pattMark.markY1 < currRow)
	{
		y1 = pattYStart + ((pattMark.markY1 - startRow) * rowHeight);
		if (y1 < pattYStart)
			y1 = pattYStart;
	}
	else if (pattMark.markY1 == currRow)
	{
		y1 = markCoord->midRowY;
	}
	else
	{
		y1 = markCoord->lowerRowsY + ((pattMark.markY1 - (currRow + 1)) * rowHeight);
	}

	// Y2

	if (pattMark.markY2-1 < currRow)
	{
		y2 = pattYStart + ((pattMark.markY2 - startRow) * rowHeight);
	}
	else if (pattMark.markY2-1 == currRow)
	{
		y2 = markCoord->midRowY + 11;
	}
	else
	{
		pattYEnd = markCoord->lowerRowsY + (pattCoord->numLowerRows * rowHeight);

		y2 = markCoord->lowerRowsY + ((pattMark.markY2 - (currRow + 1)) * rowHeight);
		if (y2 > pattYEnd)
			y2 = pattYEnd;
	}

	// kludge! (some mark situations could overwrite illegal areas)
	if (config.ptnUnpressed && editor.ui.pattChanScrollShown)
	{
		if (y1 == pattCoord->upperRowsY-1 || y1 == pattCoord->lowerRowsY-1)
			y1++;

		if (y2 == 384)
			y2--;

		// this can actually happen here, don't render in that case
		if (y1 >= y2)
			return;
	}

	assert(x1 > 0 && x1 < SCREEN_W && x2 > 0 && x2 < SCREEN_W &&
	       y1 > 0 && y1 < SCREEN_H && y2 > 0 && y2 < SCREEN_H);

	// pattern mark drawing

	w = x2 - x1;
	h = y2 - y1;

	assert(x1+w <= SCREEN_W && y1+h <= SCREEN_H);

	ptr32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
			ptr32[x] = video.palette[(ptr32[x] >> 24) ^ 2]; // ">> 24" to get palette of pixel, XOR 2 to change to mark palette

		ptr32 += SCREEN_W;
	}
}

static void drawChannelNumbering(uint16_t yPos)
{
	uint8_t chNum;
	uint16_t xPos;

	xPos = 29;
	for (uint8_t i = 0; i < editor.ui.numChannelsShown; i++)
	{
		chNum = editor.ui.channelOffset + i + 1;
		if (chNum < 10)
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, '0' + chNum);
		}
		else
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, '0' + (chNum / 10));
			charOutOutlined(xPos + 9, yPos, PAL_MOUSEPT, '0' + (chNum % 10));
		}

		xPos += editor.ui.patternChannelWidth;
	}
}

static void drawRowNum(uint16_t yPos, uint16_t row, bool middleRowFlag)
{
	uint8_t pal;

	// set color based on some conditions
	if (middleRowFlag)
		pal = PAL_FORGRND;
	else if ((row & 3) == 0 && config.ptnLineLight)
		pal = PAL_BLCKTXT;
	else
		pal = PAL_PATTEXT;

	if (config.ptnHex)
	{
		rowNumOut(yPos, pal, (uint8_t)(row >> 4), row & 0x0F);
	}
	else
	{
		row %= 100;
		rowNumOut(yPos, pal, (uint8_t)(row / 10), row % 10);
	}
}

// DRAWING ROUTINES (WITH VOLUME COLUMN)

static void showNoteNum(uint8_t pal, uint16_t xPos, uint16_t yPos, int16_t ton)
{
	xPos += 3;

	assert(ton >= 0 && ton <= 97);

	if (editor.ui.numChannelsShown <= 4)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteBig(xPos, yPos, pal);
		else if (ton == 97)
			drawKeyOffBig(xPos, yPos, pal);
		else
			drawNoteBig(xPos, yPos, pal, ton);
	}
	else
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteMedium(xPos, yPos, pal);
		else if (ton == 97)
			drawKeyOffMedium(xPos, yPos, pal);
		else
			drawNoteMedium(xPos, yPos, pal, ton);
	}
}

static void showInstrNum(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t ins)
{
	uint8_t chr1, chr2, charW, fontType;

	if (editor.ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 67;
	}
	else if (editor.ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 27;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xPos += 31;
	}

	if (config.ptnInstrZero)
	{
		pattCharOut(xPos,         yPos, pal, ins >> 4,   fontType);
		pattCharOut(xPos + charW, yPos, pal, ins & 0x0F, fontType);
	}
	else
	{
		chr1 = ins >> 4;
		chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, pal, chr1, fontType);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, pal, chr2, fontType);
	}
}

static void showVolEfx(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t vol)
{
	uint8_t char1, char2, fontType, charW;

	if (editor.ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 91;

		char1 = vol2charTab1[vol >> 4];
		if (vol < 0x10)
			char2 = 39;
		else
			char2 = vol & 0x0F;
	}
	else if (editor.ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 51;

		char1 = vol2charTab1[vol >> 4];
		if (vol < 0x10)
			char2 = 39;
		else
			char2 = vol & 0x0F;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xPos += 43;

		char1 = vol2charTab2[vol >> 4];
		if (vol < 0x10)
			char2 = 42;
		else
			char2 = vol & 0x0F;
	}

	pattCharOut(xPos,         yPos, pal, char1, fontType);
	pattCharOut(xPos + charW, yPos, pal, char2, fontType);
}

static void showEfx(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t effTyp, uint8_t eff)
{
	uint8_t fontType, charW;

	if (editor.ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 115;
	}
	else if (editor.ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 67;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xPos += 55;
	}

	pattCharOut(xPos,               yPos, pal, effTyp,     fontType);
	pattCharOut(xPos +  charW,      yPos, pal, eff >> 4,   fontType);
	pattCharOut(xPos + (charW * 2), yPos, pal, eff & 0x0F, fontType);
}

// DRAWING ROUTINES (WITHOUT VOLUME COLUMN)

static void showNoteNumNoVolColumn(uint8_t pal, uint16_t xPos, uint16_t yPos, int16_t ton)
{
	xPos += 3;

	assert(ton >= 0 && ton <= 97);

	if (editor.ui.numChannelsShown <= 6)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteBig(xPos, yPos, pal);
		else if (ton == 97)
			drawKeyOffBig(xPos, yPos, pal);
		else
			drawNoteBig(xPos, yPos, pal, ton);
	}
	else if (editor.ui.numChannelsShown <= 8)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteMedium(xPos, yPos, pal);
		else if (ton == 97)
			drawKeyOffMedium(xPos, yPos, pal);
		else
			drawNoteMedium(xPos, yPos, pal, ton);
	}
	else
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteSmall(xPos, yPos, pal);
		else if (ton == 97)
			drawKeyOffSmall(xPos, yPos, pal);
		else
			drawNoteSmall(xPos, yPos, pal, ton);
	}
}

static void showInstrNumNoVolColumn(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t ins)
{
	uint8_t chr1, chr2, charW, fontType;

	if (editor.ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xPos += 59;
	}
	else if (editor.ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 51;
	}
	else if (editor.ui.numChannelsShown <= 8)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 27;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xPos += 23;
	}

	if (config.ptnInstrZero)
	{
		pattCharOut(xPos,         yPos, pal, ins >> 4,   fontType);
		pattCharOut(xPos + charW, yPos, pal, ins & 0x0F, fontType);
	}
	else
	{
		chr1 = ins >> 4;
		chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, pal, chr1, fontType);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, pal, chr2, fontType);
	}
}

static void showNoVolEfx(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t vol)
{
	// make compiler happy
	(void)pal;
	(void)xPos;
	(void)yPos;
	(void)vol;
}

static void showEfxNoVolColumn(uint8_t pal, uint16_t xPos, uint16_t yPos, uint8_t effTyp, uint8_t eff)
{
	uint8_t charW, fontType;

	if (editor.ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xPos += 91;
	}
	else if (editor.ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 67;
	}
	else if (editor.ui.numChannelsShown <= 8)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 43;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xPos += 31;
	}

	pattCharOut(xPos,               yPos, pal, effTyp,     fontType);
	pattCharOut(xPos +  charW,      yPos, pal, eff >> 4,   fontType);
	pattCharOut(xPos + (charW * 2), yPos, pal, eff & 0x0F, fontType);
}

static void drawRowNumbers(const pattCoord_t *pattCoord, int16_t currRow, uint16_t rowHeight, uint16_t upperRowsYEnd, uint16_t pattLen)
{
	int16_t j, rowYPos, numRows;

	// upper rows
	numRows = currRow;
	if (numRows > 0)
	{
		if (numRows > pattCoord->numUpperRows)
			numRows = pattCoord->numUpperRows;

		rowYPos = upperRowsYEnd;
		for (j = 0; j < numRows; j++)
		{
			drawRowNum(rowYPos, currRow - j - 1, false);
			rowYPos -= rowHeight;
		}
	}

	// current row
	drawRowNum(pattCoord->midRowTextY, currRow, true);

	// lower rows
	numRows = (pattLen - 1) - currRow;
	if (numRows > 0)
	{
		if (numRows > pattCoord->numLowerRows)
			numRows = pattCoord->numLowerRows;

		rowYPos = pattCoord->lowerRowsTextY;
		for (j = 0; j < numRows; j++)
		{
			drawRowNum(rowYPos, currRow + j + 1, false);
			rowYPos += rowHeight;
		}
	}
}

void writePattern(int16_t currRow, int16_t pattern)
{
	uint8_t chans, chNum;
	int16_t numRows, j;
	uint16_t pattLen, xPos, rowYPos, rowHeight, chanWidth, upperRowsYEnd;
	tonTyp *note, *pattPtr;
	const pattCoord_t *pattCoord;
	void (*drawNote)(uint8_t, uint16_t, uint16_t, int16_t);
	void (*drawInst)(uint8_t, uint16_t, uint16_t, uint8_t);
	void (*drawVolEfx)(uint8_t, uint16_t, uint16_t, uint8_t);
	void (*drawEfx)(uint8_t, uint16_t, uint16_t, uint8_t, uint8_t);

	/* We're too lazy to carefully erase things as needed, just render
	** the whole pattern framework first (fast enough on modern PCs) */
	drawPatternBorders();

	chans = editor.ui.numChannelsShown;
	if (chans > editor.ui.maxVisibleChannels)
		chans = editor.ui.maxVisibleChannels;

	assert(chans >= 2 && chans <= 12);

	// get channel width
	chanWidth = chanWidths[(chans / 2) - 1];
	editor.ui.patternChannelWidth = chanWidth + 3;

	// get heights/pos/rows depending on configuration
	pattCoord = &pattCoordTable[config.ptnUnpressed][editor.ui.pattChanScrollShown][editor.ui.extended];
	rowHeight = config.ptnUnpressed ? 11 : 8;
	upperRowsYEnd = pattCoord->upperRowsTextY + ((pattCoord->numUpperRows - 1) * rowHeight);

	pattPtr = patt[pattern];
	pattLen = pattLens[pattern];

	// draw row numbers
	drawRowNumbers(pattCoord, currRow, rowHeight, upperRowsYEnd, pattLen);

	// set up function pointers for drawing
	if (config.ptnS3M)
	{
		drawNote = showNoteNum;
		drawInst = showInstrNum;
		drawVolEfx = showVolEfx;
		drawEfx = showEfx;
	}
	else
	{
		drawNote = showNoteNumNoVolColumn;
		drawInst = showInstrNumNoVolColumn;
		drawVolEfx = showNoVolEfx;
		drawEfx = showEfxNoVolColumn;
	}

	// draw pattern data

	xPos = 29;
	for (uint8_t i = 0; i < editor.ui.numChannelsShown; i++)
	{
		chNum = editor.ui.channelOffset + i;

		// upper rows
		numRows = currRow;
		if (numRows > 0)
		{
			if (numRows > pattCoord->numUpperRows)
				numRows = pattCoord->numUpperRows;

			rowYPos = upperRowsYEnd;

			if (pattPtr == NULL)
				note = &emptyNote;
			else
				note = &pattPtr[((currRow - 1) * MAX_VOICES) + chNum];

			for (j = 0; j < numRows; j++)
			{
				drawNote(PAL_PATTEXT, xPos, rowYPos, note->ton);
				drawInst(PAL_PATTEXT, xPos, rowYPos, note->instr);
				drawVolEfx(PAL_PATTEXT, xPos, rowYPos, note->vol);
				drawEfx(PAL_PATTEXT, xPos, rowYPos, note->effTyp, note->eff);

				if (pattPtr != NULL)
					note -= MAX_VOICES;

				rowYPos -= rowHeight;
			}
		}

		// current row

		if (pattPtr == NULL)
			note = &emptyNote;
		else
			note = &pattPtr[(currRow * MAX_VOICES) + chNum];

		rowYPos = pattCoord->midRowTextY;

		drawNote(PAL_FORGRND, xPos, rowYPos, note->ton);
		drawInst(PAL_FORGRND, xPos, rowYPos, note->instr);
		drawVolEfx(PAL_FORGRND, xPos, rowYPos, note->vol);
		drawEfx(PAL_FORGRND, xPos, rowYPos, note->effTyp, note->eff);

		// lower rows
		numRows = (pattLen - 1) - currRow;
		if (numRows > 0)
		{
			if (numRows > pattCoord->numLowerRows)
				numRows = pattCoord->numLowerRows;

			rowYPos = pattCoord->lowerRowsTextY;

			if (pattPtr == NULL)
				note = &emptyNote;
			else
				note = &pattPtr[((currRow + 1) * MAX_VOICES) + chNum];

			for (j = 0; j < numRows; j++)
			{
				drawNote(PAL_PATTEXT, xPos, rowYPos, note->ton);
				drawInst(PAL_PATTEXT, xPos, rowYPos, note->instr);
				drawVolEfx(PAL_PATTEXT, xPos, rowYPos, note->vol);
				drawEfx(PAL_PATTEXT, xPos, rowYPos, note->effTyp, note->eff);

				if (pattPtr != NULL)
					note += MAX_VOICES;

				rowYPos += rowHeight;
			}
		}

		xPos += editor.ui.patternChannelWidth;
	}

	writeCursor();

	if (pattMark.markY1 != pattMark.markY2) // do we have a pattern mark?
		writePatternBlockMark(currRow, rowHeight, pattCoord);

	// channel numbers must be drawn in the very end
	if (config.ptnChnNumbers)
		drawChannelNumbering(pattCoord->upperRowsTextY);
}

// ========== OPTIMIZED CHARACTER DRAWING ROUTINES FOR PATTERN EDITOR ==========

void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t paletteIndex, uint8_t val)
{
	const uint8_t *ch1Ptr, *ch2Ptr;
	uint32_t *dstPtr, pixVal, offset;

	pixVal = video.palette[paletteIndex];
	offset = config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H);
	ch1Ptr = &font4Data[((val   >> 4) * FONT4_CHAR_W) + offset];
	ch2Ptr = &font4Data[((val & 0x0F) * FONT4_CHAR_W) + offset];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x]) dstPtr[x] = pixVal;
			if (ch2Ptr[x]) dstPtr[FONT4_CHAR_W + x] = pixVal;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void rowNumOut(uint32_t yPos, uint8_t paletteIndex, uint8_t rowChar1, uint8_t rowChar2)
{
	const uint8_t *ch1Ptr, *ch2Ptr;
	uint32_t *dstPtr, pixVal, offset;

	pixVal = video.palette[paletteIndex];
	offset = config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H);
	ch1Ptr = &font4Data[(rowChar1 * FONT4_CHAR_W) + offset];
	ch2Ptr = &font4Data[(rowChar2 * FONT4_CHAR_W) + offset];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + 8];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x])
			{
				dstPtr[x] = pixVal; // left side
				dstPtr[600 + x] = pixVal; // right side
			}

			if (ch2Ptr[x])
			{
				dstPtr[ FONT4_CHAR_W + x] = pixVal; // left side
				dstPtr[(600 + FONT4_CHAR_W) + x] = pixVal; // right side
			}
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t paletteIndex, uint8_t chr, uint8_t fontType)
{
	const uint8_t *srcPtr;
	uint32_t x, y, *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (fontType == FONT_TYPE3)
	{
		srcPtr = &font3Data[chr * FONT3_CHAR_W];
		for (y = 0; y < FONT3_CHAR_H; y++)
		{
			for (x = 0; x < FONT3_CHAR_W; x++)
			{
				if (srcPtr[x])
					dstPtr[x] = pixVal;
			}

			srcPtr += FONT3_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE4)
	{
		srcPtr = &font4Data[(chr * FONT4_CHAR_W) + (config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H))];
		for (y = 0; y < FONT4_CHAR_H; y++)
		{
			for (x = 0; x < FONT4_CHAR_W; x++)
			{
				if (srcPtr[x])
					dstPtr[x] = pixVal;
			}

			srcPtr += FONT4_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE5)
	{
		srcPtr = &font5Data[(chr * FONT5_CHAR_W) + (config.ptnFont * (FONT5_WIDTH * FONT5_CHAR_H))];
		for (y = 0; y < FONT5_CHAR_H; y++)
		{
			for (x = 0; x < FONT5_CHAR_W; x++)
			{
				if (srcPtr[x])
					dstPtr[x] = pixVal;
			}

			srcPtr += FONT5_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else
	{
		srcPtr = &font7Data[chr * FONT7_CHAR_W];
		for (y = 0; y < FONT7_CHAR_H; y++)
		{
			for (x = 0; x < FONT7_CHAR_W; x++)
			{
				if (srcPtr[x])
					dstPtr[x] = pixVal;
			}

			srcPtr += FONT7_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawEmptyNoteSmall(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	srcPtr = &font7Data[18 * FONT7_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W*3; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffSmall(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	srcPtr = &font7Data[21 * FONT7_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + (xPos + 2)];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W*2; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteSmall(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, int16_t ton)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint8_t note;
	uint32_t *dstPtr, pixVal, char1, char2, char3;

	assert(ton >= 1 && ton <= 97);

	ton--;

	note  =  ton % 12;
	char3 = (ton / 12) * FONT7_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_small[note];
		char2 = sharpNote2Char_small[note];
	}
	else
	{
		char1 = flatNote1Char_small[note];
		char2 = flatNote2Char_small[note];
	}

	pixVal = video.palette[paletteIndex];
	ch1Ptr = &font7Data[char1];
	ch2Ptr = &font7Data[char2];
	ch3Ptr = &font7Data[char3];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W; x++)
		{
			if (ch1Ptr[x]) dstPtr[ (FONT7_CHAR_W * 0)      + x] = pixVal;
			if (ch2Ptr[x]) dstPtr[ (FONT7_CHAR_W * 1)      + x] = pixVal;
			if (ch3Ptr[x]) dstPtr[((FONT7_CHAR_W * 2) - 2) + x] = pixVal;
		}

		ch1Ptr += FONT7_WIDTH;
		ch2Ptr += FONT7_WIDTH;
		ch3Ptr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteMedium(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	srcPtr = &font4Data[(43 * FONT4_CHAR_W) + (config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H))];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffMedium(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	srcPtr = &font4Data[(40 * FONT4_CHAR_W) + (config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H))];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteMedium(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, int16_t ton)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint8_t note;
	uint32_t *dstPtr, pixVal, fontOffset, char1, char2, char3;

	ton--;

	note  =  ton % 12;
	char3 = (ton / 12) * FONT4_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_med[note];
		char2 = sharpNote2Char_med[note];
	}
	else
	{
		char1 = flatNote1Char_med[note];
		char2 = flatNote2Char_med[note];
	}

	pixVal = video.palette[paletteIndex];
	fontOffset = config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H);
	ch1Ptr = &font4Data[char1 + fontOffset];
	ch2Ptr = &font4Data[char2 + fontOffset];
	ch3Ptr = &font4Data[char3 + fontOffset];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x]) dstPtr[(FONT4_CHAR_W * 0) + x] = pixVal;
			if (ch2Ptr[x]) dstPtr[(FONT4_CHAR_W * 1) + x] = pixVal;
			if (ch3Ptr[x]) dstPtr[(FONT4_CHAR_W * 2) + x] = pixVal;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		ch3Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	srcPtr = &font4Data[(67 * FONT4_CHAR_W) + (config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H))];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, pixVal;

	pixVal = video.palette[paletteIndex];
	srcPtr = &font4Data[(61 * FONT4_CHAR_W) + (config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H))];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			if (srcPtr[x])
				dstPtr[x] = pixVal;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteBig(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, int16_t ton)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint8_t note;
	uint32_t *dstPtr, pixVal, fontOffset, char1, char2, char3;

	ton--;

	note  =  ton % 12;
	char3 = (ton / 12) * FONT5_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_big[note];
		char2 = sharpNote2Char_big[note];
	}
	else
	{
		char1 = flatNote1Char_big[note];
		char2 = flatNote2Char_big[note];
	}

	pixVal = video.palette[paletteIndex];
	fontOffset = config.ptnFont * (FONT5_WIDTH * FONT5_CHAR_H);
	ch1Ptr = &font5Data[char1 + fontOffset];
	ch2Ptr = &font5Data[char2 + fontOffset];
	ch3Ptr = &font5Data[char3 + fontOffset];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT5_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT5_CHAR_W; x++)
		{
			if (ch1Ptr[x]) dstPtr[(FONT5_CHAR_W * 0) + x] = pixVal;
			if (ch2Ptr[x]) dstPtr[(FONT5_CHAR_W * 1) + x] = pixVal;
			if (ch3Ptr[x]) dstPtr[(FONT5_CHAR_W * 2) + x] = pixVal;
		}

		ch1Ptr += FONT5_WIDTH;
		ch2Ptr += FONT5_WIDTH;
		ch3Ptr += FONT5_WIDTH;
		dstPtr += SCREEN_W;
	}
}

const pattCoord_t pattCoordTable[2][2][2] =
{
	/*
		uint16_t upperRowsY, lowerRowsY;
		uint16_t upperRowsTextY, midRowTextY, lowerRowsTextY;
		uint16_t numUpperRows, numLowerRows;
	*/

	// no pattern stretch
	{
		// no pattern channel scroll
		{ 
			{ 176, 292, 177, 283, 293, 13, 13 }, // normal pattern editor
			{  56, 228,  57, 219, 229, 20, 21 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 176, 285, 177, 276, 286, 12, 12 }, // normal pattern editor
			{  56, 221,  57, 212, 222, 19, 20 }, // extended pattern editor
		}
	},

	// pattern stretch
	{
		// no pattern channel scroll
		{
			{ 177, 286, 178, 277, 288,  9, 10 }, // normal pattern editor
			{  56, 232,  58, 223, 234, 15, 15 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{  176, 285, 177, 276, 286,  9,  9 }, // normal pattern editor
			{   56, 220,  57, 211, 221, 14, 15 }, // extended pattern editor
		},
	}
};

const pattCoord2_t pattCoord2Table[2][2][2] =
{
	/*
		uint16_t upperRowsY, lowerRowsY;
		uint16_t upperRowsH, lowerRowsH;
	*/

	// no pattern stretch
	{
		// no pattern channel scroll
		{
			{ 175, 291, 107, 107 }, //   normal pattern editor
			{  55, 227, 163, 171 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 175, 284, 100, 100 }, //   normal pattern editor
			{  55, 220, 156, 164 }, // extended pattern editor
		}
	},

	// pattern stretch
	{
		// no pattern channel scroll
		{
			{ 175, 285, 101, 113 }, //   normal pattern editor
			{  55, 231, 167, 167 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 175, 284, 100, 100 }, //   normal pattern editor
			{  55, 219, 155, 165 }, // extended pattern editor
		},
	}
};

const markCoord_t markCoordTable[2][2][2] =
{
	// uint16_t upperRowsY, midRowY, lowerRowsY;

	// no pattern stretch
	{
		// no pattern channel scroll
		{
			{ 177, 281, 293 }, //   normal pattern editor
			{  57, 217, 229 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 177, 274, 286 }, //   normal pattern editor
			{  57, 210, 222 }, // extended pattern editor
		}
	},

	// pattern stretch
	{
		// no pattern channel scroll
		{
			{ 176, 275, 286 }, //   normal pattern editor
			{  56, 221, 232 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 175, 274, 284 }, //   normal pattern editor
			{  55, 209, 219 }, // extended pattern editor
		},
	}
};

const uint8_t pattCursorXTab[2 * 4 * 8] =
{
	// no volume column shown
	32, 88, 104, 0, 0, 120, 136, 152, //  4 columns visible
	32, 80,  88, 0, 0,  96, 104, 112, //  6 columns visible
	32, 56,  64, 0, 0,  72,  80,  88, //  8 columns visible
	32, 52,  56, 0, 0,  60,  64,  68, // 12 columns visible

	// volume column shown
	32, 96, 104, 120, 128, 144, 152, 160, //  4 columns visible
	32, 56,  64,  80,  88,  96, 104, 112, //  6 columns visible
	32, 60,  64,  72,  76,  84,  88,  92, //  8 columns visible
	32, 60,  64,  72,  76,  84,  88,  92, // 12 columns visible
};

const uint8_t pattCursorWTab[2 * 4 * 8] =
{
	// no volume column shown
	48, 16, 16, 0, 0, 16, 16, 16, //  4 columns visible
	48,  8,  8, 0, 0,  8,  8,  8, //  6 columns visible
	24,  8,  8, 0, 0,  8,  8,  8, //  8 columns visible
	16,  4,  4, 0, 0,  4,  4,  4, // 12 columns visible

	// volume column shown
	48,  8,  8,  8,  8,  8,  8,  8, //  4 columns visible
	24,  8,  8,  8,  8,  8,  8,  8, //  6 columns visible
	24,  4,  4,  4,  4,  4,  4,  4, //  8 columns visible
	24,  4,  4,  4,  4,  4,  4,  4  // 12 columns visible
};
