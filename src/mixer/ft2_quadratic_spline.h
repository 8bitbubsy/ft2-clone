#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define QUADRATIC_SPLINE_WIDTH 3
#define QUADRATIC_SPLINE_PHASES 8192
#define QUADRATIC_SPLINE_PHASES_BITS 13 /* log2(QUADRATIC_SPLINE_PHASES) */
#define QUADRATIC_SPLINE_FRACSHIFT (MIXER_FRAC_BITS-QUADRATIC_SPLINE_PHASES_BITS)

extern float *fQuadraticSplineLUT;

bool setupQuadraticSplineTable(void);
void freeQuadraticSplineTable(void);
