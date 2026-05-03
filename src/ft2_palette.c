#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_palette.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_video.h"
#include "ft2_palette.h"
#include "ft2_tables.h"

uint8_t cfg_ColorNum = 0; // globalized

static uint8_t cfg_Red, cfg_Green, cfg_Blue, cfg_Contrast;

static const uint8_t FTC_EditOrder[6] = { PAL_PATTEXT, PAL_BLCKMRK, PAL_BLCKTXT, PAL_MOUSEPT, PAL_DESKTOP, PAL_BUTTONS };
static const uint8_t scaleOrder[3] = { 8, 4, 9 };

static uint8_t palContrast[12][2] = // palette desktop/button contrasts
{
	{59, 55}, {59, 53}, {56, 59}, {68, 55}, {57, 59}, {48, 55},
	{66, 62}, {68, 57}, {58, 42}, {57, 55}, {62, 57}, {52, 57}
};

void setPalette(pal16 *p, bool redrawScreen)
{
#define LOOP_PIN_COL_SUB 96
#define TEXT_MARK_COLOR 0x0078D7
#define BOX_SELECT_COLOR 0x7F7F7F

	int16_t r8, g8, b8;

	// set main palette (w/ 6-bit -> 8-bit conversion)
	for (int32_t i = 0; i < 16; i++)
	{
		r8 = COLOR_6BIT_TO_8BIT(p[i].r);
		g8 = COLOR_6BIT_TO_8BIT(p[i].g);
		b8 = COLOR_6BIT_TO_8BIT(p[i].b);

		// MSB (0xXX000000) is used for palette number
		video.palette[i] = (i << 24) | RGB32(r8, g8, b8);
	}

	// set custom FT2 clone palette entries

	video.palette[PAL_TEXTMRK] = (PAL_TEXTMRK << 24) | TEXT_MARK_COLOR;
	video.palette[PAL_BOXSLCT] = (PAL_BOXSLCT << 24) | BOX_SELECT_COLOR;

	r8 = RGB32_R(video.palette[PAL_PATTEXT]);
	g8 = RGB32_G(video.palette[PAL_PATTEXT]);
	b8 = RGB32_B(video.palette[PAL_PATTEXT]);

	r8 = MAX(r8 - LOOP_PIN_COL_SUB, 0);
	g8 = MAX(g8 - LOOP_PIN_COL_SUB, 0);
	b8 = MAX(b8 - LOOP_PIN_COL_SUB, 0);

	video.palette[PAL_LOOPPIN] = (PAL_LOOPPIN << 24) | RGB32(r8, g8, b8);

	// update framebuffer pixels with new palette
	if (redrawScreen && video.frameBuffer != NULL)
	{
		for (int32_t i = 0; i < SCREEN_W*SCREEN_H; i++)
			video.frameBuffer[i] = video.palette[(video.frameBuffer[i] >> 24) & 15]; // ARGB alpha channel = palette index
	}
}

static void showColorErrorMsg(void)
{
	okBox(0, "System message", "Default colors cannot be modified.", NULL);
}

static void showMouseColorErrorMsg(void)
{
	okBox(0, "System message", "Mouse color can only be changed when \"Software mouse\" is enabled.", NULL);
}

static void drawCurrentPaletteColor(void)
{
	const uint8_t palIndex = FTC_EditOrder[cfg_ColorNum];

	const uint8_t r8 = COLOR_6BIT_TO_8BIT(cfg_Red);
	const uint8_t g8 = COLOR_6BIT_TO_8BIT(cfg_Green);
	const uint8_t b8 = COLOR_6BIT_TO_8BIT(cfg_Blue);

	textOutShadow(516, 3, PAL_FORGRND, PAL_DSKTOP2, "Palette:");
	hexOutBg(573, 3, PAL_FORGRND, PAL_DESKTOP, RGB32(r8, g8, b8) & 0xFFFFFF, 6);
	clearRect(616, 2, 12, 10);
	fillRect(617, 3, 10, 8, palIndex);
}

static void updatePaletteEditor(void)
{
	const uint8_t colorNum = FTC_EditOrder[cfg_ColorNum];

	cfg_Red = palTable[config.cfg_StdPalNum][colorNum].r;
	cfg_Green = palTable[config.cfg_StdPalNum][colorNum].g;
	cfg_Blue = palTable[config.cfg_StdPalNum][colorNum].b;

	if (cfg_ColorNum == 4 || cfg_ColorNum == 5)
		cfg_Contrast = palContrast[config.cfg_StdPalNum][cfg_ColorNum-4];
	else
		cfg_Contrast = 0;

	setScrollBarPos(SB_PAL_R, cfg_Red, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_G, cfg_Green, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_B, cfg_Blue, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, DONT_TRIGGER_CALLBACK);

	drawCurrentPaletteColor();
}

static float fPalPow(float x, float y)
{
	if (y == 1.0f)
		return x;

	y *= logf(fabsf(x));
	y = CLAMP(y, -86.0f, 86.0f);

	return expf(y);
}

static void paletteDragMoved(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showColorErrorMsg();
		return;
	}

	if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showMouseColorErrorMsg();
		return;
	}

	const uint8_t colorNum = FTC_EditOrder[cfg_ColorNum];
	const uint8_t layout = (uint8_t)config.cfg_StdPalNum;

	palTable[layout][colorNum].r = cfg_Red;
	palTable[layout][colorNum].g = cfg_Green;
	palTable[layout][colorNum].b = cfg_Blue;

	if (cfg_ColorNum == 4 || cfg_ColorNum == 5)
	{
		int32_t contrast = cfg_Contrast;
		if (contrast < 1)
			contrast = 1;

		const float fR = (float)cfg_Red;
		const float fG = (float)cfg_Green;
		const float fB = (float)cfg_Blue;
		const float fContrast = (float)contrast * (1.0f / 40.0f);

		for (int32_t i = 0; i < 3; i++)
		{
			const int32_t pal = scaleOrder[i] + (cfg_ColorNum - 4) * 2;
			const float fMul = fPalPow((float)(i + 1) * 0.5f, fContrast);

			int32_t r6 = (int32_t)((fR * fMul) + 0.5f);
			int32_t g6 = (int32_t)((fG * fMul) + 0.5f);
			int32_t b6 = (int32_t)((fB * fMul) + 0.5f);

			palTable[layout][pal].r = (uint8_t)(CLAMP(r6, 0, 63));
			palTable[layout][pal].g = (uint8_t)(CLAMP(g6, 0, 63));
			palTable[layout][pal].b = (uint8_t)(CLAMP(b6, 0, 63));
		}

		palContrast[layout][cfg_ColorNum-4] = cfg_Contrast;
	}
	else
	{
		cfg_Contrast = 0;

		setScrollBarPos(SB_PAL_R, cfg_Red, DONT_TRIGGER_CALLBACK);
		setScrollBarPos(SB_PAL_G, cfg_Green, DONT_TRIGGER_CALLBACK);
		setScrollBarPos(SB_PAL_B, cfg_Blue, DONT_TRIGGER_CALLBACK);
	}

	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, DONT_TRIGGER_CALLBACK);
	drawCurrentPaletteColor();

	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
}

void sbPalRPos(uint32_t pos)
{
	if (cfg_Red != (uint8_t)pos)
	{
		cfg_Red = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalGPos(uint32_t pos)
{
	if (cfg_Green != (uint8_t)pos)
	{
		cfg_Green = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalBPos(uint32_t pos)
{
	if (cfg_Blue != (uint8_t)pos)
	{
		cfg_Blue = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalContrastPos(uint32_t pos)
{
	if (cfg_Contrast != (uint8_t)pos)
	{
		cfg_Contrast = (uint8_t)pos;
		paletteDragMoved();
	}
}

void configPalRDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_R, 1);
}

void configPalRUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_R, 1);
}

void configPalGDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_G, 1);
}

void configPalGUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_G, 1);
}

void configPalBDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_B, 1);
}

void configPalBUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_B, 1);
}

void configPalContDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_CONTRAST, 1);
}

void configPalContUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_CONTRAST, 1);
}

void showPaletteEditor(void)
{
	charOutShadow(503, 17, PAL_FORGRND, PAL_DSKTOP2, 'R');
	charOutShadow(503, 31, PAL_FORGRND, PAL_DSKTOP2, 'G');
	charOutShadow(503, 45, PAL_FORGRND, PAL_DSKTOP2, 'B');

	showScrollBar(SB_PAL_R);
	showScrollBar(SB_PAL_G);
	showScrollBar(SB_PAL_B);
	showPushButton(PB_CONFIG_PAL_R_DOWN);
	showPushButton(PB_CONFIG_PAL_R_UP);
	showPushButton(PB_CONFIG_PAL_G_DOWN);
	showPushButton(PB_CONFIG_PAL_G_UP);
	showPushButton(PB_CONFIG_PAL_B_DOWN);
	showPushButton(PB_CONFIG_PAL_B_UP);

	showRadioButtonGroup(RB_GROUP_CONFIG_PAL_ENTRIES);

	textOutShadow(516, 59, PAL_FORGRND, PAL_DSKTOP2, "Contrast:");
	showScrollBar(SB_PAL_CONTRAST);
	showPushButton(PB_CONFIG_PAL_CONT_DOWN);
	showPushButton(PB_CONFIG_PAL_CONT_UP);

	updatePaletteEditor();
}

void rbConfigPalPatternText(void)
{
	cfg_ColorNum = 0;
	checkRadioButton(RB_CONFIG_PAL_PATTERNTEXT);
	updatePaletteEditor();
}

void rbConfigPalBlockMark(void)
{
	cfg_ColorNum = 1;
	checkRadioButton(RB_CONFIG_PAL_BLOCKMARK);
	updatePaletteEditor();
}

void rbConfigPalTextOnBlock(void)
{
	cfg_ColorNum = 2;
	checkRadioButton(RB_CONFIG_PAL_TEXTONBLOCK);
	updatePaletteEditor();
}

void rbConfigPalMouse(void)
{
	cfg_ColorNum = 3;
	checkRadioButton(RB_CONFIG_PAL_MOUSE);
	updatePaletteEditor();
}

void rbConfigPalDesktop(void)
{
	cfg_ColorNum = 4;
	checkRadioButton(RB_CONFIG_PAL_DESKTOP);
	updatePaletteEditor();
}

void rbConfigPalButttons(void)
{
	cfg_ColorNum = 5;
	checkRadioButton(RB_CONFIG_PAL_BUTTONS);
	updatePaletteEditor();
}

void rbConfigPalArctic(void)
{
	config.cfg_StdPalNum = PAL_ARCTIC;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_ARCTIC);
}

void rbConfigPalLitheDark(void)
{
	config.cfg_StdPalNum = PAL_LITHE_DARK;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_LITHE_DARK);
}

void rbConfigPalAuroraBorealis(void)
{
	config.cfg_StdPalNum = PAL_AURORA_BOREALIS;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_AURORA_BOREALIS);
}

void rbConfigPalRose(void)
{
	config.cfg_StdPalNum = PAL_ROSE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_ROSE);
}

void rbConfigPalBlues(void)
{
	config.cfg_StdPalNum = PAL_BLUES;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_BLUES);
}

void rbConfigPalDarkMode(void)
{
	config.cfg_StdPalNum = PAL_DARK_MODE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_DARK_MODE);
}

void rbConfigPalGold(void)
{
	config.cfg_StdPalNum = PAL_GOLD;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_GOLD);
}

void rbConfigPalViolent(void)
{
	config.cfg_StdPalNum = PAL_VIOLENT;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_VIOLENT);
}

void rbConfigPalHeavyMetal(void)
{
	config.cfg_StdPalNum = PAL_HEAVY_METAL;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_HEAVY_METAL);
}

void rbConfigPalWhyColors(void)
{
	config.cfg_StdPalNum = PAL_WHY_COLORS;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_WHY_COLORS);
}

void rbConfigPalJungle(void)
{
	config.cfg_StdPalNum = PAL_JUNGLE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_JUNGLE);
}

void rbConfigPalUserDefined(void)
{
	config.cfg_StdPalNum = PAL_USER_DEFINED;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_USER_DEFINED);
}
