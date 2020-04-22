#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RGB32_R(x) (((x) >> 16) & 0xFF)
#define RGB32_G(x) (((x) >> 8) & 0xFF)
#define RGB32_B(x) ((x) & 0xFF)
#define RGB32(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define P6_TO_P8(x) (((x) << 2) + ((x) >> 4))

#define PAL_TRANSPR 127

// FT2 palette (exact order as real FT2)
enum
{
	PAL_BCKGRND = 0,
	PAL_PATTEXT = 1,
	PAL_BLCKMRK = 2,
	PAL_BLCKTXT = 3,
	PAL_DESKTOP = 4,
	PAL_FORGRND = 5,
	PAL_BUTTONS = 6,
	PAL_BTNTEXT = 7,
	PAL_DSKTOP2 = 8,
	PAL_DSKTOP1 = 9,
	PAL_BUTTON2 = 10,
	PAL_BUTTON1 = 11,
	PAL_MOUSEPT = 12,

	// these are used for mouse XOR when hovering over the piano (?)
	PAL_PIANOXOR1 = 13,
	PAL_PIANOXOR2 = 14,
	PAL_PIANOXOR3 = 15,

	// custom clone palettes
	PAL_LOOPPIN = 16,
	PAL_TEXTMRK = 17,
	PAL_BOXSLCT = 18,

	// modifiable with setCustomPalColor()
	PAL_CUSTOM = 19,

	PAL_NUM
};

typedef struct pal16_t
{
	uint8_t r, g, b;
} pal16;

extern uint8_t cfg_ColorNr;

void setCustomPalColor(uint32_t color);

uint8_t palMax(int32_t c);
void setPal16(pal16 *p, bool redrawScreen);

void sbPalRPos(uint32_t pos);
void sbPalGPos(uint32_t pos);
void sbPalBPos(uint32_t pos);
void sbPalContrastPos(uint32_t pos);
void configPalRDown(void);
void configPalRUp(void);
void configPalGDown(void);
void configPalGUp(void);
void configPalBDown(void);
void configPalBUp(void);
void configPalContDown(void);
void configPalContUp(void);
void showPaletteEditor(void);

void rbConfigPalPatternText(void);
void rbConfigPalBlockMark(void);
void rbConfigPalTextOnBlock(void);
void rbConfigPalMouse(void);
void rbConfigPalDesktop(void);
void rbConfigPalButttons(void);
void rbConfigPalArctic(void);
void rbConfigPalLitheDark(void);
void rbConfigPalAuroraBorealis(void);
void rbConfigPalRose(void);
void rbConfigPalBlues(void);
void rbConfigPalDarkMode(void);
void rbConfigPalGold(void);
void rbConfigPalViolent(void);
void rbConfigPalHeavyMetal(void);
void rbConfigPalWhyColors(void);
void rbConfigPalJungle(void);
void rbConfigPalUserDefined(void);
