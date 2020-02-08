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

static tonTyp emptyPattern[MAX_VOICES * MAX_PATT_LEN];

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

static const uint8_t noteTab1[96] =
{
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11,
	0,1,2,3,4,5,6,7,8,9,10,11
};

static const uint8_t noteTab2[96] =
{
	0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3,3,3,3,3,
	4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7
};

static const uint8_t hex2Dec[256] =
{
	0,1,2,3,4,5,6,7,8,9,
	16,17,18,19,20,21,22,23,24,25,
	32,33,34,35,36,37,38,39,40,41,
	48,49,50,51,52,53,54,55,56,57,
	64,65,66,67,68,69,70,71,72,73,
	80,81,82,83,84,85,86,87,88,89,
	96,97,98,99,100,101,102,103,104,105,
	112,113,114,115,116,117,118,119,120,121,
	128,129,130,131,132,133,134,135,136,137,
	144,145,146,147,148,149,150,151,152,153,

	0,1,2,3,4,5,6,7,8,9,
	16,17,18,19,20,21,22,23,24,25,
	32,33,34,35,36,37,38,39,40,41,
	48,49,50,51,52,53,54,55,56,57,
	64,65,66,67,68,69,70,71,72,73,
	80,81,82,83,84,85,86,87,88,89,
	96,97,98,99,100,101,102,103,104,105,
	112,113,114,115,116,117,118,119,120,121,
	128,129,130,131,132,133,134,135,136,137,
	144,145,146,147,148,149,150,151,152,153,

	0,1,2,3,4,5,6,7,8,9,
	16,17,18,19,20,21,22,23,24,25,
	32,33,34,35,36,37,38,39,40,41,
	48,49,50,51,52,53,54,55,56,57,
	64,65,66,67,68,69,70,71,72,73,
	80,81,82,83,84,85
};

static const pattCoord_t pattCoordTable[2][2][2] =
{
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

static const pattCoord2_t pattCoord2Table[2][2][2] =
{
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

static const markCoord_t markCoordTable[2][2][2] =
{
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

static const uint8_t pattCursorXTab[2 * 4 * 8] =
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

static const uint8_t pattCursorWTab[2 * 4 * 8] =
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

// global tables

const char chDecTab1[MAX_VOICES+1] = 
{
	'0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
	'1', '1', '1', '1', '1', '1', '1', '1', '1', '1',
	'2', '2', '2', '2', '2', '2', '2', '2', '2', '2',
	'3', '3', '3'
};

const char chDecTab2[MAX_VOICES+1] = 
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'0', '1', '2'
};

// ft2_pattern_ed.c
extern const uint16_t chanWidths[6];

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color);
static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color);
static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color);
static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color);

void updatePattFontPtrs(void)
{
	//config.ptnFont is pre-clamped and safe
	font4Ptr = &font4Data[config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H)];
	font5Ptr = &font5Data[config.ptnFont * (FONT5_WIDTH * FONT5_CHAR_H)];
}

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

			xOffs += chanWidth+1;
		}

		vLine(xOffs-1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
		vLine(xOffs-1, pattCoord->lowerRowsY, pattCoord->lowerRowsH+1, PAL_DESKTOP);
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

static void writePatternBlockMark(int32_t currRow, uint32_t rowHeight, const pattCoord_t *pattCoord)
{
	int32_t startCh, endCh, startRow, endRow, x1, x2, y1, y2, pattYStart, pattYEnd;
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
#define CH_NUM_XPOS 29

	uint16_t xPos = CH_NUM_XPOS;
	int32_t ch = editor.ui.channelOffset + 1;

	for (uint8_t i = 0; i < editor.ui.numChannelsShown; i++)
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
		xPos += editor.ui.patternChannelWidth;
	}
}

static void drawRowNums(int32_t yPos, uint8_t row, bool selectedRowFlag)
{
#define LEFT_ROW_XPOS 8
#define RIGHT_ROW_XPOS 608

	const uint8_t *src1Ptr, *src2Ptr;
	uint32_t *dst1Ptr, *dst2Ptr, pixVal;

	// set color based on some conditions
	if (selectedRowFlag)
		pixVal = video.palette[PAL_FORGRND];
	else if (config.ptnLineLight && !(row & 3))
		pixVal = video.palette[PAL_BLCKTXT];
	else
		pixVal = video.palette[PAL_PATTEXT];

	if (!config.ptnHex)
		row = hex2Dec[row];

	src1Ptr = &font4Ptr[(row   >> 4) * FONT4_CHAR_W];
	src2Ptr = &font4Ptr[(row & 0x0F) * FONT4_CHAR_W];
	dst1Ptr = &video.frameBuffer[(yPos * SCREEN_W) + LEFT_ROW_XPOS];
	dst2Ptr = dst1Ptr + (RIGHT_ROW_XPOS - LEFT_ROW_XPOS);

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
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

static void showNoteNum(uint32_t xPos, uint32_t yPos, int16_t ton, uint32_t color)
{
	xPos += 3;

	assert(ton >= 0 && ton <= 97);

	if (editor.ui.numChannelsShown <= 4)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (ton == 97)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, ton, color);
	}
	else
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (ton == 97)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, ton, color);
	}
}

static void showInstrNum(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
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
		pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
		pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
	}
	else
	{
		chr1 = ins >> 4;
		chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, chr1, fontType, color);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, chr2, fontType, color);
	}
}

static void showVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
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

	pattCharOut(xPos,         yPos, char1, fontType, color);
	pattCharOut(xPos + charW, yPos, char2, fontType, color);
}

static void showEfx(uint32_t xPos, uint32_t yPos, uint8_t effTyp, uint8_t eff, uint32_t color)
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

	pattCharOut(xPos,               yPos, effTyp,     fontType, color);
	pattCharOut(xPos +  charW,      yPos, eff >> 4,   fontType, color);
	pattCharOut(xPos + (charW * 2), yPos, eff & 0x0F, fontType, color);
}

// DRAWING ROUTINES (WITHOUT VOLUME COLUMN)

static void showNoteNumNoVolColumn(uint32_t xPos, uint32_t yPos, int16_t ton, uint32_t color)
{
	xPos += 3;

	assert(ton >= 0 && ton <= 97);

	if (editor.ui.numChannelsShown <= 6)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (ton == 97)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, ton, color);
	}
	else if (editor.ui.numChannelsShown <= 8)
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (ton == 97)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, ton, color);
	}
	else
	{
		if (ton <= 0 || ton > 97)
			drawEmptyNoteSmall(xPos, yPos, color);
		else if (ton == 97)
			drawKeyOffSmall(xPos, yPos, color);
		else
			drawNoteSmall(xPos, yPos, ton, color);
	}
}

static void showInstrNumNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
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
		pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
		pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
	}
	else
	{
		chr1 = ins >> 4;
		chr2 = ins & 0x0F;

		if (chr1 > 0)
			pattCharOut(xPos, yPos, chr1, fontType, color);

		if (chr1 > 0 || chr2 > 0)
			pattCharOut(xPos + charW, yPos, chr2, fontType, color);
	}
}

static void showNoVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
{
	// make compiler happy
	(void)xPos;
	(void)yPos;
	(void)vol;
	(void)color;
}

static void showEfxNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t effTyp, uint8_t eff, uint32_t color)
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

	pattCharOut(xPos,               yPos, effTyp,     fontType, color);
	pattCharOut(xPos +  charW,      yPos, eff >> 4,   fontType, color);
	pattCharOut(xPos + (charW * 2), yPos, eff & 0x0F, fontType, color);
}

void writePattern(int32_t currRow, int32_t pattern)
{
	int32_t row, rowsOnScreen, numRows, afterCurrRow, numChannels;
	int32_t textY, midRowTextY, lowerRowsTextY, xPos, xWidth;
	uint32_t rowHeight, chanWidth, chans, noteTextColors[2], color;
	tonTyp *note, *pattPtr;
	const pattCoord_t *pattCoord;
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

	chans = editor.ui.numChannelsShown;
	if (chans > editor.ui.maxVisibleChannels)
		chans = editor.ui.maxVisibleChannels;

	assert(chans >= 2 && chans <= 12);

	// get channel width
	chanWidth = chanWidths[(chans / 2) - 1];
	editor.ui.patternChannelWidth = (uint16_t)(chanWidth + 3);

	// get heights/pos/rows depending on configuration
	rowHeight = config.ptnUnpressed ? 11 : 8;
	pattCoord = &pattCoordTable[config.ptnUnpressed][editor.ui.pattChanScrollShown][editor.ui.extended];
	midRowTextY = pattCoord->midRowTextY;
	lowerRowsTextY = pattCoord->lowerRowsTextY;
	row = currRow - pattCoord->numUpperRows;
	rowsOnScreen = pattCoord->numUpperRows + 1 + pattCoord->numLowerRows;
	textY = pattCoord->upperRowsTextY;

	afterCurrRow = currRow + 1;
	numChannels = editor.ui.numChannelsShown;
	pattPtr = patt[pattern];
	numRows = pattLens[pattern];
	noteTextColors[0] = video.palette[PAL_PATTEXT]; // not selected
	noteTextColors[1] = video.palette[PAL_FORGRND]; // selected

	// increment pattern data pointer by horizontal scrollbar offset/channel
	if (pattPtr != NULL)
		pattPtr += editor.ui.channelOffset;

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
	for (int32_t i = 0; i < rowsOnScreen; i++)
	{
		if (row >= 0)
		{
			bool selectedRowFlag = row == currRow;

			drawRowNums(textY, (uint8_t)row, selectedRowFlag);
	
			if (pattPtr == NULL)
				note = emptyPattern;
			else
				note = &pattPtr[(uint32_t)row * MAX_VOICES];

			xPos = 29;
			xWidth = editor.ui.patternChannelWidth;

			color = noteTextColors[selectedRowFlag];
			for (int32_t j = 0; j < numChannels; j++)
			{
				drawNote(xPos, textY, note->ton, color);
				drawInst(xPos, textY, note->instr, color);
				drawVolEfx(xPos, textY, note->vol, color);
				drawEfx(xPos, textY, note->effTyp, note->eff, color);

				xPos += xWidth;
				note++;
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
		drawChannelNumbering(pattCoord->upperRowsTextY);
}

// ========== OPTIMIZED CHARACTER DRAWING ROUTINES FOR PATTERN EDITOR ==========

void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color)
{
	const uint8_t *ch1Ptr, *ch2Ptr;
	uint32_t *dstPtr, tmp;

	ch1Ptr = &font4Ptr[(val   >> 4) * FONT4_CHAR_W];
	ch2Ptr = &font4Ptr[(val & 0x0F) * FONT4_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (ch1Ptr[x] != 0) tmp = color;
			dstPtr[x] = tmp;

			tmp = dstPtr[FONT4_CHAR_W+x];
			if (ch2Ptr[x] != 0) tmp = color;
			dstPtr[FONT4_CHAR_W+x] = tmp;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t x, y, *dstPtr, tmp;

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (fontType == FONT_TYPE3)
	{
		srcPtr = &font3Data[chr * FONT3_CHAR_W];
		for (y = 0; y < FONT3_CHAR_H; y++)
		{
			for (x = 0; x < FONT3_CHAR_W; x++)
			{
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = color;
				dstPtr[x] = tmp;
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
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = color;
				dstPtr[x] = tmp;
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
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = color;
				dstPtr[x] = tmp;
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
				// carefully written like this to generate conditional move instructions (font data is hard to predict)
				tmp = dstPtr[x];
				if (srcPtr[x] != 0) tmp = color;
				dstPtr[x] = tmp;
			}

			srcPtr += FONT7_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	srcPtr = &font7Data[18 * FONT7_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W*3; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	srcPtr = &font7Data[21 * FONT7_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + (xPos + 2)];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W*2; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint8_t note;
	uint32_t *dstPtr, char1, char2, char3, tmp;

	assert(ton >= 1 && ton <= 97);

	ton--;

	note = noteTab1[ton];
	char3 = noteTab2[ton] * FONT7_CHAR_W;

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

	ch1Ptr = &font7Data[char1];
	ch2Ptr = &font7Data[char2];
	ch3Ptr = &font7Data[char3];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT7_CHAR_W; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (ch1Ptr[x] != 0) tmp = color;
			dstPtr[x] = tmp;

			tmp = dstPtr[FONT7_CHAR_W+x];
			if (ch2Ptr[x] != 0) tmp = color;
			dstPtr[FONT7_CHAR_W+x] = tmp;

			tmp = dstPtr[((FONT7_CHAR_W*2)-2)+x]; // -2 to get correct alignment for ending glyph
			if (ch3Ptr[x] != 0) tmp = color;
			dstPtr[((FONT7_CHAR_W*2)-2)+x] = tmp;
		}

		ch1Ptr += FONT7_WIDTH;
		ch2Ptr += FONT7_WIDTH;
		ch3Ptr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	srcPtr = &font4Ptr[43 * FONT4_CHAR_W];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	srcPtr = &font4Ptr[40 * FONT4_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint32_t note, *dstPtr, char1, char2, char3, tmp;

	ton--;

	note = noteTab1[ton];
	char3 = noteTab2[ton] * FONT4_CHAR_W;

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

	ch1Ptr = &font4Ptr[char1];
	ch2Ptr = &font4Ptr[char2];
	ch3Ptr = &font4Ptr[char3];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (ch1Ptr[x] != 0) tmp = color;
			dstPtr[x] = tmp;

			tmp = dstPtr[FONT4_CHAR_W+x];
			if (ch2Ptr[x] != 0) tmp = color;
			dstPtr[FONT4_CHAR_W+x] = tmp;

			tmp = dstPtr[(FONT4_CHAR_W*2)+x];
			if (ch3Ptr[x] != 0) tmp = color;
			dstPtr[(FONT4_CHAR_W*2)+x] = tmp;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		ch3Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	srcPtr = &font4Ptr[67 * FONT4_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, tmp;

	srcPtr = &font4Data[61 * FONT4_CHAR_W];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (srcPtr[x] != 0) tmp = color;
			dstPtr[x] = tmp;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t ton, uint32_t color)
{
	const uint8_t *ch1Ptr, *ch2Ptr, *ch3Ptr;
	uint8_t note;
	uint32_t *dstPtr, char1, char2, char3, tmp;

	ton--;

	note = noteTab1[ton];
	char3 = noteTab2[ton] * FONT5_CHAR_W;

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

	ch1Ptr = &font5Ptr[char1];
	ch2Ptr = &font5Ptr[char2];
	ch3Ptr = &font5Ptr[char3];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (uint32_t y = 0; y < FONT5_CHAR_H; y++)
	{
		for (uint32_t x = 0; x < FONT5_CHAR_W; x++)
		{
			// carefully written like this to generate conditional move instructions (font data is hard to predict)
			tmp = dstPtr[x];
			if (ch1Ptr[x] != 0) tmp = color;
			dstPtr[x] = tmp;

			tmp = dstPtr[FONT5_CHAR_W+x];
			if (ch2Ptr[x] != 0) tmp = color;
			dstPtr[FONT5_CHAR_W+x] = tmp;

			tmp = dstPtr[(FONT5_CHAR_W*2)+x];
			if (ch3Ptr[x] != 0) tmp = color;
			dstPtr[(FONT5_CHAR_W*2)+x] = tmp;
		}

		ch1Ptr += FONT5_WIDTH;
		ch2Ptr += FONT5_WIDTH;
		ch3Ptr += FONT5_WIDTH;
		dstPtr += SCREEN_W;
	}
}
