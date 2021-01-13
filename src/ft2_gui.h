#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_pushbuttons.h"
#include "ft2_radiobuttons.h"
#include "ft2_checkboxes.h"
#include "ft2_scrollbars.h"
#include "ft2_sysreqs.h"
#include "ft2_textboxes.h"
#include "ft2_palette.h"

#define FONT1_CHAR_W 8
#define FONT1_CHAR_H 10
#define FONT1_WIDTH 1024
#define FONT2_CHAR_W 16
#define FONT2_CHAR_H 20
#define FONT2_WIDTH 2048
#define FONT3_CHAR_W 4
#define FONT3_CHAR_H 7
#define FONT3_WIDTH 172
#define FONT4_CHAR_W 8
#define FONT4_CHAR_H 8
#define FONT4_WIDTH 624
#define FONT5_CHAR_W 16
#define FONT5_CHAR_H 8
#define FONT5_WIDTH 624
#define FONT6_CHAR_W 7
#define FONT6_CHAR_H 8
#define FONT6_WIDTH 112
#define FONT7_CHAR_W 6
#define FONT7_CHAR_H 7
#define FONT7_WIDTH 140

enum
{
	FRAMEWORK_TYPE1 = 0,
	FRAMEWORK_TYPE2 = 1,

	FONT_TYPE1 = 0,
	FONT_TYPE2 = 1,
	FONT_TYPE3 = 2,
	FONT_TYPE4 = 3,
	FONT_TYPE5 = 4,
	FONT_TYPE6 = 5,
	FONT_TYPE7 = 6,

	OBJECT_ID_NONE = -1,

	OBJECT_NONE = 0,
	OBJECT_PUSHBUTTON = 1,
	OBJECT_RADIOBUTTON = 2,
	OBJECT_CHECKBOX = 3,
	OBJECT_SCROLLBAR = 4,
	OBJECT_TEXTBOX = 5,
	OBJECT_INSTRSWITCH = 6,
	OBJECT_PATTERNMARK = 7,
	OBJECT_DISKOPLIST = 8,
	OBJECT_SMPDATA = 9,
	OBJECT_PIANO = 10,
	OBJECT_INSVOLENV = 11,
	OBJECT_INSPANENV = 12
};

extern pushButton_t pushButtons[NUM_PUSHBUTTONS];
extern radioButton_t radioButtons[NUM_RADIOBUTTONS];
extern checkBox_t checkBoxes[NUM_CHECKBOXES];
extern scrollBar_t scrollBars[NUM_SCROLLBARS];
extern textBox_t textBoxes[NUM_TEXTBOXES];

void unstuckLastUsedGUIElement(void);
bool setupGUI(void);

void hLine(uint16_t x, uint16_t y, uint16_t width, uint8_t paletteIndex);
void vLine(uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex);
void hLineDouble(uint16_t x, uint16_t y, uint16_t w, uint8_t paletteIndex);
void vLineDouble(uint16_t x, uint16_t y, uint16_t h, uint8_t paletteIndex);
void line(int16_t x1, int16_t x2, int16_t y1, int16_t y2, uint8_t paletteIndex);
void clearRect(uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h);
void fillRect(uint16_t xPos, uint16_t yPos, uint16_t w, uint16_t h, uint8_t paletteIndex);
void drawFramework(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t type);
void blit32(uint16_t xPos, uint16_t yPos, const uint32_t* srcPtr, uint16_t w, uint16_t h);
void blit(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h);
void blitClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX);
void blitFast(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h); // no transparency/colorkey
void blitFastClipX(uint16_t xPos, uint16_t yPos, const uint8_t *srcPtr, uint16_t w, uint16_t h, uint16_t clipX); // no transparency/colorkey
void hexOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint32_t val, uint8_t numDigits);
void hexOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint32_t val, uint8_t numDigits);
void hexOutShadow(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, uint32_t val, uint8_t numDigits);
void textOutTiny(int32_t xPos, int32_t yPos, char *str, uint32_t color); // A..Z/a..z and 0..9
void textOutTinyOutline(int32_t xPos, int32_t yPos, char *str); // A..Z/a..z and 0..9
void charOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr);
void charOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, char chr);
void charOutShadow(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr);
void charOutClipX(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr, uint16_t clipX);
void bigCharOut(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, char chr);
void charOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, char chr);
void charOutOutlined(uint16_t x, uint16_t y, uint8_t paletteIndex, char chr);
void textOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr);
void textOutBorder(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t borderPaletteIndex, const char *textPtr);
void textOutFixed(uint16_t x, uint16_t y, uint8_t fgPaltete, uint8_t bgPalette, const char *textPtr);
void bigTextOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr);
void bigTextOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr);
void textOutClipX(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, uint16_t clipX);
void textOutShadow(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr);
uint8_t charWidth(char ch);
uint8_t charWidth16(char ch);
uint16_t textWidth(const char *textPtr);
uint16_t textNWidth(const char *textPtr, int32_t length);
uint16_t textWidth16(const char *textPtr);
void drawGUIOnRunTime(void);
void showTopLeftMainScreen(bool restoreScreens);
void hideTopLeftMainScreen(void);
void showTopRightMainScreen(void);
void hideTopRightMainScreen(void);
void hideTopLeftScreen(void);
void hideTopScreen(void);
void showTopScreen(bool restoreScreens);
void showBottomScreen(void);

// for about screen
void textOutFade(uint16_t x, uint16_t y, uint8_t paletteIndex, const char *textPtr, int32_t fade);
void blit32Fade(uint16_t xPos, uint16_t yPos, const uint32_t* srcPtr, uint16_t w, uint16_t h, int32_t fade);
