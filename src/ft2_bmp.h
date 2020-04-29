#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct bmp_t
{
	uint8_t *font1, *font2, *font3, *font4, *font6, *font7, *font8;
	uint8_t *ft2LogoBadges, *ft2ByBadges, *radiobuttonGfx, *checkboxGfx;
	uint8_t *midiLogo, *nibblesLogo, *nibblesStages, *loopPins;
	uint8_t *mouseCursors, *mouseCursorBusyClock, *mouseCursorBusyGlass;
	uint8_t *whitePianoKeys, *blackPianoKeys, *vibratoWaveforms, *scopeRec, *scopeMute;
	uint32_t *ft2AboutLogo;
} bmp_t;

extern bmp_t bmp; // ft2_bmp.c

bool loadBMPs(void);
void freeBMPs(void);
