#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define CUBIC4P_SPLINE_PHASES 8192
#define CUBIC4P_SPLINE_PHASES_BITS 13 // log2(CUBIC4P_SPLINE_PHASES)
#define CUBIC4P_SPLINE_FSHIFT (MIXER_FRAC_BITS-(CUBIC4P_SPLINE_PHASES_BITS+2))
#define CUBIC4P_SPLINE_FMASK ((4*CUBIC4P_SPLINE_PHASES)-4)

#define CUBIC6P_SPLINE_PHASES 8192
#define CUBIC6P_SPLINE_PHASES_BITS 13 // log2(CUBIC6P_SPLINE_PHASES)
#define CUBIC6P_SPLINE_FSHIFT (MIXER_FRAC_BITS-CUBIC6P_SPLINE_PHASES_BITS)

extern float *f4PointCubicSplineLUT, *f6PointCubicSplineLUT;

bool calcCubicSplineTables(void);
void freeCubicSplineTables(void);
