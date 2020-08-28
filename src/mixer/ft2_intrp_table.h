#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

#define CUBIC_WIDTH 4 /* number of taps */
#define CUBIC_WIDTH_BITS 2

#define CUBIC_PHASES 4096
#define CUBIC_PHASES_BITS 12

extern const uint32_t fCubicSplineTable[CUBIC_WIDTH * CUBIC_PHASES];

#define CUBIC_FSHIFT (MIXER_FRAC_BITS-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#define CUBIC_FMASK ((CUBIC_WIDTH*CUBIC_PHASES)-CUBIC_WIDTH)
#define CUBIC_QUANTSHIFT 15
