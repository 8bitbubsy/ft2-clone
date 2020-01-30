#pragma once

#include <stdint.h>
#include <stdbool.h>

int16_t okBoxThreadSafe(int16_t typ, const char *headline, const char *text);
int16_t okBox(int16_t typ, const char *headline, const char *text);
int16_t quitBox(bool skipQuitMsg);
int16_t inputBox(int16_t typ, const char *headline, char *edText, uint16_t maxStrLen);
bool askUnsavedChanges(uint8_t type);

// for thread-safe version of okBox()
struct
{
	volatile bool active;
	int16_t typ, returnData;
	const char *headline, *text;
} okBoxData;

enum
{
	ASK_TYPE_QUIT = 0,
	ASK_TYPE_LOAD_SONG = 1,
};
