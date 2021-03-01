#pragma once

#include <stdint.h>
#include <stdbool.h>

enum
{
	ASK_TYPE_QUIT = 0,
	ASK_TYPE_LOAD_SONG = 1,
};

// for thread-safe version of okBox()
typedef struct okBoxData_t
{
	volatile bool active;
	int16_t typ, returnData;
	const char *headline, *text;
} okBoxData_t;

int16_t okBoxThreadSafe(int16_t typ, const char *headline, const char *text);
int16_t okBox(int16_t typ, const char *headline, const char *text);
int16_t quitBox(bool skipQuitMsg);
int16_t inputBox(int16_t typ, const char *headline, char *edText, uint16_t maxStrLen);
bool askUnsavedChanges(uint8_t type);

void myLoaderMsgBoxThreadSafe(const char *fmt, ...);
void myLoaderMsgBox(const char *fmt, ...);

 // ft2_sysreqs.c
extern okBoxData_t okBoxData;
extern void (*loaderMsgBox)(const char *, ...);
extern int16_t (*loaderSysReq)(int16_t, const char *, const char *);
// ---------------
