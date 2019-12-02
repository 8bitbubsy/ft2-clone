#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

enum // TEXTBOXES
{
	TB_RES_1, // reserved

	TB_INST1,
	TB_INST2,
	TB_INST3,
	TB_INST4,
	TB_INST5,
	TB_INST6,
	TB_INST7,
	TB_INST8,

	TB_SAMP1,
	TB_SAMP2,
	TB_SAMP3,
	TB_SAMP4,
	TB_SAMP5,

	TB_SONG_NAME,

	TB_DISKOP_FILENAME,

	TB_CONF_DEF_MODS_DIR,
	TB_CONF_DEF_INSTRS_DIR,
	TB_CONF_DEF_SAMPS_DIR,
	TB_CONF_DEF_PATTS_DIR,
	TB_CONF_DEF_TRACKS_DIR,

	NUM_TEXTBOXES
};

#define TEXT_CURSOR_BLINK_RATE 6
#define TEXT_SCROLL_VALUE 30

enum
{
	TEXTBOX_NO_UPDATE = 0,
	TEXTBOX_UPDATE = 1
};

typedef struct textBox_t // DO NOT TOUCH!
{
	uint16_t x, y, w;
	uint8_t h, tx, ty;
	uint16_t maxChars;
	bool rightMouseButton, changeMouseCursor;

	// these ones are changed at run time
	char *textPtr;
	bool visible;
	uint8_t *renderBuf;
	int16_t cursorPos;
	uint16_t renderW, renderBufW, renderBufH;
	int32_t bufOffset;
} textBox_t;

bool textIsMarked(void);
void exitTextEditing(void);
int16_t getTextCursorX(textBox_t *t);
int16_t getTextCursorY(textBox_t *t);
void drawTextBox(uint16_t textBoxID);
void showTextBox(uint16_t textBoxID);
void hideTextBox(uint16_t textBoxID);
bool testTextBoxMouseDown(void);
void updateTextBoxPointers(void);
void setupInitialTextBoxPointers(void);
void setTextCursorToEnd(textBox_t *t);
void handleTextEditControl(SDL_Keycode keycode);
void handleTextEditInputChar(char textChar);
void handleTextBoxWhileMouseDown(void);
void freeTextBoxes(void);
