#include <stdint.h>
#include <stdbool.h>
#include "ft2_palette.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_video.h"
#include "ft2_palette.h"
#include "ft2_tables.h"

uint8_t cfg_ColorNr = 0; // globalized

static uint8_t cfg_Red, cfg_Green, cfg_Blue, cfg_Contrast;

static const uint8_t FTC_EditOrder[6] = { PAL_PATTEXT, PAL_BLCKMRK, PAL_BLCKTXT, PAL_MOUSEPT, PAL_DESKTOP, PAL_BUTTONS };
static const uint8_t scaleOrder[3] = { 8, 4, 9 };

static uint8_t palContrast[12][2] = // palette desktop/button contrasts
{
	{59, 55}, {59, 53}, {56, 59}, {68, 55}, {57, 59}, {48, 55},
	{66, 62}, {68, 57}, {58, 42}, {57, 55}, {62, 57}, {52, 57}
};

void setCustomPalColor(uint32_t color)
{
	video.palette[PAL_CUSTOM] = (PAL_CUSTOM << 24) | color;
}

void setPal16(pal16 *p, bool redrawScreen)
{
#define LOOP_PIN_COL_SUB 96
#define TEXT_MARK_COLOR 0x0078D7
#define BOX_SELECT_COLOR 0x7F7F7F

	int16_t r, g, b;

	// set main palette w/ 18-bit -> 24-bit conversion
	for (int32_t i = 0; i < 16; i++)
	{
		r = P6_TO_P8(p[i].r); // 0..63 -> 0..255
		g = P6_TO_P8(p[i].g);
		b = P6_TO_P8(p[i].b);

		video.palette[i] = (i << 24) | RGB32(r, g, b);
	}

	// set custom FT2 clone palette entries

	video.palette[PAL_TEXTMRK] = (PAL_TEXTMRK << 24) | TEXT_MARK_COLOR;
	video.palette[PAL_BOXSLCT] = (PAL_BOXSLCT << 24) | BOX_SELECT_COLOR;

	r = RGB32_R(video.palette[PAL_PATTEXT]);
	g = RGB32_G(video.palette[PAL_PATTEXT]);
	b = RGB32_B(video.palette[PAL_PATTEXT]);

	r = MAX(r - LOOP_PIN_COL_SUB, 0);
	g = MAX(g - LOOP_PIN_COL_SUB, 0);
	b = MAX(b - LOOP_PIN_COL_SUB, 0);

	video.palette[PAL_LOOPPIN] = (PAL_LOOPPIN << 24) | RGB32(r, g, b);

	// update framebuffer pixels with new palette
	if (redrawScreen && video.frameBuffer != NULL)
	{
		for (int32_t i = 0; i < SCREEN_W*SCREEN_H; i++)
			video.frameBuffer[i] = video.palette[(video.frameBuffer[i] >> 24) & 15]; // ARGB alpha channel = palette index
	}
}

static void showColorErrorMsg(void)
{
	okBox(0, "System message", "Default colors cannot be modified.");
}

static void showMouseColorErrorMsg(void)
{
	okBox(0, "System message", "Mouse color can only be changed when \"Software mouse\" is enabled.");
}

static double palPow(double dX, double dY)
{
	if (dY == 1.0)
		return dX;

	dY *= log(fabs(dX));
	dY = CLAMP(dY, -86.0, 86.0);

	return exp(dY);
}

uint8_t palMax(int32_t c)
{
	return (uint8_t)(CLAMP(c, 0, 63));
}

static void drawCurrentPaletteColor(void)
{
	const uint8_t palIndex = FTC_EditOrder[cfg_ColorNr];

	const uint8_t r = P6_TO_P8(cfg_Red);
	const uint8_t g = P6_TO_P8(cfg_Green);
	const uint8_t b = P6_TO_P8(cfg_Blue);

	textOutShadow(516, 3, PAL_FORGRND, PAL_DSKTOP2, "Palette:");
	hexOutBg(573, 3, PAL_FORGRND, PAL_DESKTOP, RGB32(r, g, b) & 0xFFFFFF, 6);
	clearRect(616, 2, 12, 10);
	fillRect(617, 3, 10, 8, palIndex);
}

static void updatePaletteEditor(void)
{
	const uint8_t nr = FTC_EditOrder[cfg_ColorNr];

	cfg_Red = palTable[config.cfg_StdPalNr][nr].r;
	cfg_Green = palTable[config.cfg_StdPalNr][nr].g;
	cfg_Blue = palTable[config.cfg_StdPalNr][nr].b;

	if (cfg_ColorNr == 4 || cfg_ColorNr == 5)
		cfg_Contrast = palContrast[config.cfg_StdPalNr][cfg_ColorNr-4];
	else
		cfg_Contrast = 0;

	setScrollBarPos(SB_PAL_R, cfg_Red, false);
	setScrollBarPos(SB_PAL_G, cfg_Green, false);
	setScrollBarPos(SB_PAL_B, cfg_Blue, false);
	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, false);

	drawCurrentPaletteColor();
}

static void paletteDragMoved(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showColorErrorMsg();
		return;
	}

	if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showMouseColorErrorMsg();
		return;
	}

	const uint8_t nr = FTC_EditOrder[cfg_ColorNr];
	const uint8_t p = (uint8_t)config.cfg_StdPalNr;

	palTable[p][nr].r = cfg_Red;
	palTable[p][nr].g = cfg_Green;
	palTable[p][nr].b = cfg_Blue;

	if (cfg_ColorNr == 4 || cfg_ColorNr == 5)
	{
		double dRed = cfg_Red;
		double dGreen = cfg_Green;
		double dBlue = cfg_Blue;

		int32_t contrast = cfg_Contrast;
		if (contrast < 1)
			contrast = 1;

		const double dContrast = contrast * (1.0 / 40.0);

		for (int32_t i = 0; i < 3; i++)
		{
			const int32_t k = scaleOrder[i] + (cfg_ColorNr - 4) * 2;

			double dMul = palPow((i + 1) * (1.0 / 2.0), dContrast);

			palTable[p][k].r = palMax((int32_t)((dRed * dMul) + 0.5));
			palTable[p][k].g = palMax((int32_t)((dGreen * dMul) + 0.5));
			palTable[p][k].b = palMax((int32_t)((dBlue * dMul) + 0.5));
		}

		palContrast[p][cfg_ColorNr-4] = cfg_Contrast;
	}
	else
	{
		cfg_Contrast = 0;

		setScrollBarPos(SB_PAL_R, cfg_Red, false);
		setScrollBarPos(SB_PAL_G, cfg_Green, false);
		setScrollBarPos(SB_PAL_B, cfg_Blue, false);
	}

	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, false);

	setPal16(palTable[config.cfg_StdPalNr], true);
	drawCurrentPaletteColor();
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
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_R, 1);
}

void configPalRUp(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_R, 1);
}

void configPalGDown(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_G, 1);
}

void configPalGUp(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_G, 1);
}

void configPalBDown(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_B, 1);
}

void configPalBUp(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_B, 1);
}

void configPalContDown(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_CONTRAST, 1);
}

void configPalContUp(void)
{
	if (config.cfg_StdPalNr != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNr == 3)
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
	cfg_ColorNr = 0;
	checkRadioButton(RB_CONFIG_PAL_PATTERNTEXT);
	updatePaletteEditor();
}

void rbConfigPalBlockMark(void)
{
	cfg_ColorNr = 1;
	checkRadioButton(RB_CONFIG_PAL_BLOCKMARK);
	updatePaletteEditor();
}

void rbConfigPalTextOnBlock(void)
{
	cfg_ColorNr = 2;
	checkRadioButton(RB_CONFIG_PAL_TEXTONBLOCK);
	updatePaletteEditor();
}

void rbConfigPalMouse(void)
{
	cfg_ColorNr = 3;
	checkRadioButton(RB_CONFIG_PAL_MOUSE);
	updatePaletteEditor();
}

void rbConfigPalDesktop(void)
{
	cfg_ColorNr = 4;
	checkRadioButton(RB_CONFIG_PAL_DESKTOP);
	updatePaletteEditor();
}

void rbConfigPalButttons(void)
{
	cfg_ColorNr = 5;
	checkRadioButton(RB_CONFIG_PAL_BUTTONS);
	updatePaletteEditor();
}

void rbConfigPalArctic(void)
{
	config.cfg_StdPalNr = PAL_ARCTIC;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_ARCTIC);
}

void rbConfigPalLitheDark(void)
{
	config.cfg_StdPalNr = PAL_LITHE_DARK;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_LITHE_DARK);
}

void rbConfigPalAuroraBorealis(void)
{
	config.cfg_StdPalNr = PAL_AURORA_BOREALIS;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_AURORA_BOREALIS);
}

void rbConfigPalRose(void)
{
	config.cfg_StdPalNr = PAL_ROSE;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_ROSE);
}

void rbConfigPalBlues(void)
{
	config.cfg_StdPalNr = PAL_BLUES;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_BLUES);
}

void rbConfigPalDarkMode(void)
{
	config.cfg_StdPalNr = PAL_DARK_MODE;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_DARK_MODE);
}

void rbConfigPalGold(void)
{
	config.cfg_StdPalNr = PAL_GOLD;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_GOLD);
}

void rbConfigPalViolent(void)
{
	config.cfg_StdPalNr = PAL_VIOLENT;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_VIOLENT);
}

void rbConfigPalHeavyMetal(void)
{
	config.cfg_StdPalNr = PAL_HEAVY_METAL;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_HEAVY_METAL);
}

void rbConfigPalWhyColors(void)
{
	config.cfg_StdPalNr = PAL_WHY_COLORS;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_WHY_COLORS);
}

void rbConfigPalJungle(void)
{
	config.cfg_StdPalNr = PAL_JUNGLE;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_JUNGLE);
}

void rbConfigPalUserDefined(void)
{
	config.cfg_StdPalNr = PAL_USER_DEFINED;
	updatePaletteEditor();
	setPal16(palTable[config.cfg_StdPalNr], true);
	checkRadioButton(RB_CONFIG_PAL_USER_DEFINED);
}
