#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define SINC_KERNELS 3
#define SINC8_WIDTH 8
#define SINC16_WIDTH 16
#define CUBIC_SPLINE_WIDTH 4

// note: more than 4096 phases barely makes any audible difference
#define INTRP_PHASES 4096

// log2(INTRP_PHASES)
#define INTRP_PHASES_BITS 12

// log2(CUBIC_SPLINE_WIDTH)
#define CUBIC_SPLINE_WIDTH_BITS 2

// log2(SINC8_WIDTH)
#define SINC8_WIDTH_BITS 3

// log2(SINC16_WIDTH)
#define SINC16_WIDTH_BITS 4

#define CUBIC_SPLINE_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+CUBIC_SPLINE_WIDTH_BITS))
#define CUBIC_SPLINE_FRACMASK ((CUBIC_SPLINE_WIDTH*INTRP_PHASES)-CUBIC_SPLINE_WIDTH)
#define SINC8_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+SINC8_WIDTH_BITS))
#define SINC8_FRACMASK ((SINC8_WIDTH*INTRP_PHASES)-SINC8_WIDTH)
#define SINC16_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+SINC16_WIDTH_BITS))
#define SINC16_FRACMASK ((SINC16_WIDTH*INTRP_PHASES)-SINC16_WIDTH)

extern float *fCubicSplineLUT, *fSinc[SINC_KERNELS], *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];
extern uint64_t sincRatio1, sincRatio2;

bool setupMixerInterpolationTables(void);
void freeMixerInterpolationTables(void);
