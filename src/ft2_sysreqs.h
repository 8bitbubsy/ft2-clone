#pragma once

#include <stdint.h>
#include <stdbool.h>

int16_t okBoxThreadSafe(int16_t typ, char *headline, char *text);
int16_t okBox(int16_t typ, char *headline, char *text);
int16_t quitBox(bool skipQuitMsg);
int16_t inputBox(int16_t typ, char *headline, char *edText, uint16_t maxStrLen);

// for thread-safe version of okBox()
struct
{
	volatile bool active;
	int16_t typ, returnData;
	char *headline, *text;
} okBoxData;
