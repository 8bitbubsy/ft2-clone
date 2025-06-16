#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define CUBIC_SPLINE_WIDTH 4
#define CUBIC_SPLINE_WIDTH_BITS 2 /* log2(CUBIC_SPLINE_WIDTH) */
#define CUBIC_SPLINE_PHASES 8192
#define CUBIC_SPLINE_PHASES_BITS 13 /* log2(CUBIC_SPLINE_PHASES) */
#define CUBIC_SPLINE_FRACSHIFT (MIXER_FRAC_BITS-(CUBIC_SPLINE_PHASES_BITS+CUBIC_SPLINE_WIDTH_BITS))
#define CUBIC_SPLINE_FRACMASK ((CUBIC_SPLINE_WIDTH*CUBIC_SPLINE_PHASES)-CUBIC_SPLINE_WIDTH)

extern float *fCubicSplineLUT;

bool setupCubicSplineTable(void);
void freeCubicSplineTable(void);
