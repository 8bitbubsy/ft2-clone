// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_pattern_ed.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

static note_t emptyPattern[MAX_CHANNELS * MAX_PATT_LEN];

static const uint8_t *font4Ptr, *font5Ptr;
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

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color);
static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);
static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);
static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);

void updatePattFontPtrs(void)
{
	//config.ptnFont is pre-clamped and safe to use
	font4Ptr = &bmp.font4[config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H)];
	font5Ptr = &bmp.font4[(4 + config.ptnFont) * (FONT4_WIDTH * FONT4_CHAR_H)];
}

void drawPatternBorders(void)
{
	// get heights/pos/rows depending on configuration
	const pattCoord2_t *pattCoord = &pattCoord2Table[config.ptnStretch][ui.pattChanScrollShown][ui.extendedPatternEditor];

	// set pattern cursor Y position
	editor.ptnCursorY = pattCoord->lowerRowsY - 9;

	int32_t chans = ui.numChannelsShown;
	if (chans > ui.maxVisibleChannels)
		chans = ui.maxVisibleChannels;

	// in some configurations, there will be two empty channels to the right, fix that
	if (chans == 2)
		chans = 4;
	else if (chans == 10 && !config.ptnShowVolColumn)
		chans = 12;

	assert(chans >= 2 && chans <= 12);

	const uint16_t chanWidth = chanWidths[(chans >> 1) - 1] + 2;

	// fill scrollbar framework (if needed)
	if (ui.pattChanScrollShown)
		drawFramework(0, 383, 632, 17, FRAMEWORK_TYPE1);

	if (config.ptnFrmWrk)
	{
		// pattern editor w/ framework

		if (ui.extendedPatternEditor)
		{
			vLine(0,   69, 330, PAL_DSKTOP1);
			vLine(631, 68, 331, PAL_DSKTOP2);

			vLine(1,   69, 330, PAL_DESKTOP);
			vLine(630, 69, 330, PAL_DESKTOP);

			hLine(0, 68, 631, PAL_DSKTOP1);
			hLine(1, 69, 630, PAL_DESKTOP);

			if (!ui.pattChanScrollShown)
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

			if (!ui.pattChanScrollShown)
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
		uint16_t xOffs = 28;
		for (int32_t i = 0; i < chans; i++)
		{
			vLine(xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
			vLine(xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);

			drawFramework(xOffs, pattCoord->upperRowsY, chanWidth, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top part
			drawFramework(xOffs, pattCoord->lowerRowsY, chanWidth, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom part

			xOffs += chanWidth+1;
		}

		vLine(xOffs-1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
		vLine(xOffs-1, pattCoord->lowerRowsY, pattCoord->lowerRowsH+1, PAL_DESKTOP);
	}
	else
	{
		// pattern editor without framework

		if (ui.extendedPatternEditor)
		{
			const int32_t clearSize = ui.pattChanScrollShown ? (SCREEN_W * sizeof (int32_t) * 315) : (SCREEN_W * sizeof (int32_t) * 332);
			memset(&video.frameBuffer[68 * SCREEN_W], 0, clearSize);
		}
		else
		{
			const int32_t clearSize = ui.pattChanScrollShown ? (SCREEN_W * sizeof(int32_t) * 210) : (SCREEN_W * sizeof(int32_t) * 227);
			memset(&video.frameBuffer[173 * SCREEN_W], 0, clearSize);
		}

		drawFramework(0, pattCoord->lowerRowsY - 10, SCREEN_W, 11, FRAMEWORK_TYPE1);
	}

	if (ui.pattChanScrollShown)
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
	const int32_t tabOffset = (config.ptnShowVolColumn * 32) + (columnModeTab[ui.numChannelsShown-1] * 8) + cursor.object;

	int32_t xPos = pattCursorXTab[tabOffset];
	const int32_t width = pattCursorWTab[tabOffset];

	assert(editor.ptnCursorY > 0 && xPos > 0 && width > 0);
	xPos += ((cursor.ch - ui.channelOffset) * ui.patternChannelWidth);

	uint32_t *dstPtr = &video.frameBuffer[(editor.ptnCursorY * SCREEN_W) + xPos];
	for (int32_t y = 0; y < 9; y++)
	{
		for (int32_t x = 0; x < width; x++)
			dstPtr[x] = video.palette[(dstPtr[x] >> 24) ^ 4]; // ">> 24" to get palette, XOR 4 to change to cursor palette

		dstPtr += SCREEN_W;
	}
}

static void writePatternBlockMark(int32_t currRow, uint32_t rowHeight, const pattCoord_t *pattCoord)
{
	int32_t y1, y2;

	// this can happen (buggy FT2 code), treat like no mark
	if (pattMark.markY1 > pattMark.markY2)
		return;

	const int32_t startCh = ui.channelOffset;
	const int32_t endCh = ui.channelOffset + (ui.numChannelsShown - 1);
	const int32_t startRow = currRow - pattCoord->numUpperRows;
	const int32_t endRow = currRow + pattCoord->numLowerRows;

	// test if pattern marking is outside of visible area (don't draw)
	if (pattMark.markX1 > endCh || pattMark.markX2 < startCh || pattMark.markY1 > endRow || pattMark.markY2 < startRow)
		return;

	const markCoord_t *markCoord = &markCoordTable[config.ptnStretch][ui.pattChanScrollShown][ui.extendedPatternEditor];
	const int32_t pattYStart = markCoord->upperRowsY;

	// X1

	int32_t x1 = 32 + ((pattMark.markX1 - ui.channelOffset) * ui.patternChannelWidth);
	if (x1 < 32)
		x1 = 32;

	// X2

	int32_t x2 = (32 - 8) + (((pattMark.markX2 + 1) - ui.channelOffset) * ui.patternChannelWidth);
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
		const int32_t pattYEnd = markCoord->lowerRowsY + (pattCoord->numLowerRows * rowHeight);

		y2 = markCoord->lowerRowsY + ((pattMark.markY2 - (currRow + 1)) * rowHeight);
		if (y2 > pattYEnd)
			y2 = pattYEnd;
	}

	// kludge! (some mark situations could overwrite illegal areas)
	if (config.ptnStretch && ui.pattChanScrollShown)
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

	const int32_t w = x2 - x1;
	const int32_t h = y2 - y1;

	assert(x1+w <= SCREEN_W && y1+h <= SCREEN_H);

	uint32_t *ptr32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			ptr32[x] = video.palette[(ptr32[x] >> 24) ^ 2]; // ">> 24" to get palette of pixel, XOR 2 to change to mark palette

		ptr32 += SCREEN_W;
	}
}

static void drawChannelNumbering(uint16_t yPos)
{
	uint16_t xPos = 30;
	int32_t ch = ui.channelOffset + 1;

	for (uint8_t i = 0; i < ui.numChannelsShown; i++)
	{
		if (ch < 10)
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, '0' + (char)ch);
		}
		else
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, chDecTab1[ch]);
			charOutOutlined(xPos + (FONT1_CHAR_W + 1), yPos, PAL_MOUSEPT, chDecTab2[ch]);
		}

		ch++;
		xPos += ui.patternChannelWidth;
	}
}

static void drawRowNums(int32_t yPos, uint8_t row, bool selectedRowFlag)
{
#define LEFT_ROW_XPOS 8
#define RIGHT_ROW_XPOS 608

	uint32_t pixVal;

	// set color based on some conditions
	if (selectedRowFlag)
		pixVal = video.palette[PAL_FORGRND];
	else if (config.ptnLineLight && !(row & 3))
		pixVal = video.palette[PAL_BLCKTXT];
	else
		pixVal = video.palette[PAL_PATTEXT];

	if (!config.ptnHex)
		row = hex2Dec[row];

	const uint8_t *src1Ptr = &font4Ptr[(row   >> 4) * FONT4_CHAR_W];
	const uint8_t *src2Ptr = &font4Ptr[(row & 0x0F) * FONT4_CHAR_W];
	uint32_t *dst1Ptr = &video.frameBuffer[(yPos * SCREEN_W) + LEFT_ROW_XPOS];
	uint32_t *dst2Ptr = dst1Ptr + (RIGHT_ROW_XPOS - LEFT_ROW_XPOS);

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (src1Ptr[x])
			{
				dst1Ptr[x] = pixVal; // left side
				dst2Ptr[x] = pixVal; // right side
			}

			if (src2Ptr[x])
			{
				dst1Ptr[FONT4_CHAR_W+x] = pixVal; // left side
				dst2Ptr[FONT4_CHAR_W+x] = pixVal; // right side
			}
		}

		src1Ptr += FONT4_WIDTH;
		src2Ptr += FONT4_WIDTH;
		dst1Ptr += SCREEN_W;
		dst2Ptr += SCREEN_W;
	}
}

// DRAWING ROUTINES (WITH VOLUME COLUMN)

static void showNoteNum(uint32_t xPos, uint32_t yPos, int16_t note, uint32_t color)
{
	xPos += 3;

	assert(note >= 0 && note <= 97);

	if (ui.numChannelsShown <= 4)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, note, color);
	}
	else
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, note, color);
	}
}

static void showInstrNum(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
{
	uint8_t charW, fontType;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 67;
	}
	else if (ui.numChannelsShown <= 6)
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
		pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
		pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
	}
	else
	{
		const uint8_t chr1 = ins >> 4;
		const uint8_t chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, chr1, fontType, color);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, chr2, fontType, color);
	}
}

static void showVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
{
	uint8_t char1, char2, fontType, charW;

	if (ui.numChannelsShown <= 4)
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
	else if (ui.numChannelsShown <= 6)
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

	pattCharOut(xPos,         yPos, char1, fontType, color);
	pattCharOut(xPos + charW, yPos, char2, fontType, color);
}

static void showEfx(uint32_t xPos, uint32_t yPos, uint8_t efx, uint8_t efxData, uint32_t color)
{
	uint8_t fontType, charW;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 115;
	}
	else if (ui.numChannelsShown <= 6)
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

	pattCharOut(xPos,               yPos, efx,            fontType, color);
	pattCharOut(xPos +  charW,      yPos, efxData >> 4,   fontType, color);
	pattCharOut(xPos + (charW * 2), yPos, efxData & 0x0F, fontType, color);
}

// DRAWING ROUTINES (WITHOUT VOLUME COLUMN)

static void showNoteNumNoVolColumn(uint32_t xPos, uint32_t yPos, int16_t note, uint32_t color)
{
	xPos += 3;

	assert(note >= 0 && note <= 97);

	if (ui.numChannelsShown <= 6)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, note, color);
	}
	else if (ui.numChannelsShown <= 8)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, note, color);
	}
	else
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteSmall(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffSmall(xPos, yPos, color);
		else
			drawNoteSmall(xPos, yPos, note, color);
	}
}

static void showInstrNumNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
{
	uint8_t charW, fontType;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xPos += 59;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 51;
	}
	else if (ui.numChannelsShown <= 8)
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
		pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
		pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
	}
	else
	{
		const uint8_t chr1 = ins >> 4;
		const uint8_t chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, chr1, fontType, color);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, chr2, fontType, color);
	}
}

static void showNoVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
{
	(void)xPos;
	(void)yPos;
	(void)vol;
	(void)color;
}

static void showEfxNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t efx, uint8_t efxData, uint32_t color)
{
	uint8_t charW, fontType;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xPos += 91;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xPos += 67;
	}
	else if (ui.numChannelsShown <= 8)
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

	pattCharOut(xPos,               yPos, efx,            fontType, color);
	pattCharOut(xPos +  charW,      yPos, efxData >> 4,   fontType, color);
	pattCharOut(xPos + (charW * 2), yPos, efxData & 0x0F, fontType, color);
}

void writePattern(int32_t currRow, int32_t currPattern)
{
	uint32_t noteTextColors[2];

	void (*drawNote)(uint32_t, uint32_t, int16_t, uint32_t);
	void (*drawInst)(uint32_t, uint32_t, uint8_t, uint32_t);
	void (*drawVolEfx)(uint32_t, uint32_t, uint8_t, uint32_t);
	void (*drawEfx)(uint32_t, uint32_t, uint8_t, uint8_t, uint32_t);

	/* Draw pattern framework every time (erasing existing content).
	** FT2 doesn't do this. This is quite lazy and consumes more CPU
	** time than needed (overlapped drawing), but it makes the pattern
	** mark/cursor drawing MUCH simpler to implement...
	*/
	drawPatternBorders();

	// setup variables

	uint32_t chans = ui.numChannelsShown;
	if (chans > ui.maxVisibleChannels)
		chans = ui.maxVisibleChannels;

	assert(chans >= 2 && chans <= 12);

	// get channel width
	const uint32_t chanWidth = chanWidths[(chans / 2) - 1];
	ui.patternChannelWidth = (uint16_t)(chanWidth + 3);

	// get heights/pos/rows depending on configuration
	uint32_t rowHeight = config.ptnStretch ? 11 : 8;
	const pattCoord_t *pattCoord = &pattCoordTable[config.ptnStretch][ui.pattChanScrollShown][ui.extendedPatternEditor];
	const pattCoord2_t *pattCoord2 = &pattCoord2Table[config.ptnStretch][ui.pattChanScrollShown][ui.extendedPatternEditor];
	const int32_t midRowTextY = pattCoord->midRowTextY;
	const int32_t lowerRowsTextY = pattCoord->lowerRowsTextY;
	int32_t row = currRow - pattCoord->numUpperRows;
	const int32_t rowsOnScreen = pattCoord->numUpperRows + 1 + pattCoord->numLowerRows;
	int32_t textY = pattCoord->upperRowsTextY;
	const int32_t afterCurrRow = currRow + 1;
	const int32_t numChannels = ui.numChannelsShown;
	note_t *pattPtr = pattern[currPattern];
	const int32_t numRows = patternNumRows[currPattern];

	// increment pattern data pointer by horizontal scrollbar offset/channel
	if (pattPtr != NULL)
		pattPtr += ui.channelOffset;

	// set up function pointers for drawing
	if (config.ptnShowVolColumn)
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

	noteTextColors[0] = video.palette[PAL_PATTEXT]; // not selected
	noteTextColors[1] = video.palette[PAL_FORGRND]; // selected

	// draw pattern data
	for (int32_t i = 0; i < rowsOnScreen; i++)
	{
		if (row >= 0)
		{
			const bool selectedRowFlag = (row == currRow);

			drawRowNums(textY, (uint8_t)row, selectedRowFlag);

			const note_t *p = (pattPtr == NULL) ? emptyPattern : &pattPtr[(uint32_t)row * MAX_CHANNELS];
			const int32_t xWidth = ui.patternChannelWidth;
			const uint32_t color = noteTextColors[selectedRowFlag];

			int32_t xPos = 29;
			for (int32_t j = 0; j < numChannels; j++, p++, xPos += xWidth)
			{
				drawNote(xPos, textY, p->note, color);
				drawInst(xPos, textY, p->instr, color);
				drawVolEfx(xPos, textY, p->vol, color);
				drawEfx(xPos, textY, p->efx, p->efxData, color);
			}
		}

		// next row
		if (++row >= numRows)
			break;

		// adjust textY position
		if (row == currRow)
			textY = midRowTextY;
		else if (row == afterCurrRow)
			textY = lowerRowsTextY;
		else
			textY += rowHeight;
	}

	writeCursor();

	// draw pattern marking (if anything is marked)
	if (pattMark.markY1 != pattMark.markY2)
		writePatternBlockMark(currRow, rowHeight, pattCoord);

	// channel numbers must be drawn lastly
	if (config.ptnChnNumbers)
		drawChannelNumbering(pattCoord2->upperRowsY+2);
}

// ========== CHARACTER DRAWING ROUTINES FOR PATTERN EDITOR ==========

void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color)
{
	const uint8_t *ch1Ptr = &font4Ptr[(val   >> 4) * FONT4_CHAR_W];
	const uint8_t *ch2Ptr = &font4Ptr[(val & 0x0F) * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT4_CHAR_W+x] = color;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color)
{
	const uint8_t *srcPtr;
	int32_t x, y;

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (fontType == FONT_TYPE3)
	{
		srcPtr = &bmp.font3[chr * FONT3_CHAR_W];
		for (y = 0; y < FONT3_CHAR_H; y++)
		{
			for (x = 0; x < FONT3_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT3_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE4)
	{
		srcPtr = &font4Ptr[chr * FONT4_CHAR_W];
		for (y = 0; y < FONT4_CHAR_H; y++)
		{
			for (x = 0; x < FONT4_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT4_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE5)
	{
		srcPtr = &font5Ptr[chr * FONT5_CHAR_W];
		for (y = 0; y < FONT5_CHAR_H; y++)
		{
			for (x = 0; x < FONT5_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT5_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else
	{
		srcPtr = &bmp.font7[chr * FONT7_CHAR_W];
		for (y = 0; y < FONT7_CHAR_H; y++)
		{
			for (x = 0; x < FONT7_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT7_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &bmp.font7[18 * FONT7_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT7_CHAR_W*3; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &bmp.font7[21 * FONT7_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + (xPos + 2)];

	for (int32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT7_CHAR_W*2; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT7_CHAR_W;

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

	const uint8_t *ch1Ptr = &bmp.font7[char1];
	const uint8_t *ch2Ptr = &bmp.font7[char2];
	const uint8_t *ch3Ptr = &bmp.font7[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT7_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT7_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[((FONT7_CHAR_W*2)-2)+x] = color;
		}

		ch1Ptr += FONT7_WIDTH;
		ch2Ptr += FONT7_WIDTH;
		ch3Ptr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	const uint8_t *srcPtr = &font4Ptr[43 * FONT4_CHAR_W];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &font4Ptr[40 * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT4_CHAR_W;

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

	const uint8_t *ch1Ptr = &font4Ptr[char1];
	const uint8_t *ch2Ptr = &font4Ptr[char2];
	const uint8_t *ch3Ptr = &font4Ptr[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT4_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[(FONT4_CHAR_W*2)+x] = color;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		ch3Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &font4Ptr[67 * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &bmp.font4[61 * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT5_CHAR_W;

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

	const uint8_t *ch1Ptr = &font5Ptr[char1];
	const uint8_t *ch2Ptr = &font5Ptr[char2];
	const uint8_t *ch3Ptr = &font5Ptr[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT5_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT5_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT5_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[(FONT5_CHAR_W*2)+x] = color;
		}

		ch1Ptr += FONT5_WIDTH;
		ch2Ptr += FONT5_WIDTH;
		ch3Ptr += FONT5_WIDTH;
		dstPtr += SCREEN_W;
	}
}
