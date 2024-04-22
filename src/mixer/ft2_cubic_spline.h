#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define CUBIC_SPLINE_TAPS 4
#define CUBIC_SPLINE_WIDTH_BITS 2 // log2(CUBIC_SPLINE_TAPS)

// 8192 is a good compromise
#define CUBIC_SPLINE_PHASES 8192
#define CUBIC_SPLINE_PHASES_BITS 13 // log2(CUBIC_SPLINE_PHASES)

// do not change these!
#define CUBIC_SPLINE_FSHIFT (MIXER_FRAC_BITS-(CUBIC_SPLINE_PHASES_BITS+CUBIC_SPLINE_WIDTH_BITS))
#define CUBIC_SPLINE_FMASK ((CUBIC_SPLINE_TAPS*CUBIC_SPLINE_PHASES)-CUBIC_SPLINE_TAPS)

extern float *fCubicSplineLUT;

bool calcCubicSplineTable(void);
void freeCubicSplineTable(void);
