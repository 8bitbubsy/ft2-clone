#pragma once

#include <stdint.h>

// number of bits for fractional sample position in audio mixer
#define FRAC_BITS 16

#define CUBIC_WIDTH 4
#define CUBIC_WIDTH_BITS 2
#define CUBIC_PHASES 4096
#define CUBIC_PHASES_BITS 12

#define CUBIC_FSHIFT (FRAC_BITS-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#define CUBIC_FMASK ((CUBIC_WIDTH*CUBIC_PHASES)-CUBIC_WIDTH)
#define CUBIC_QUANTSHIFT 15

extern const int16_t cubicSplineTable[CUBIC_WIDTH * CUBIC_PHASES];
