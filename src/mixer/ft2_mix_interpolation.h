#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define SINC_KERNELS 3
#define SINC8_TAPS 8
#define SINC16_TAPS 16
#define CUBIC_SPLINE_TAPS 4

// 8192 phases is optimal, aliasing should be below (or close to) 16-bit noise floor
#define INTRP_PHASES 8192

// log2(INTRP_PHASES)
#define INTRP_PHASES_BITS 13

// log2(CUBIC_SPLINE_TAPS)
#define CUBIC_SPLINE_TAPS_BITS 2

// log2(SINC8_TAPS)
#define SINC8_TAPS_BITS 3

// log2(SINC16_TAPS)
#define SINC16_TAPS_BITS 4

#define CUBIC_SPLINE_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+CUBIC_SPLINE_TAPS_BITS))
#define CUBIC_SPLINE_FRACMASK ((CUBIC_SPLINE_TAPS*INTRP_PHASES)-CUBIC_SPLINE_TAPS)
#define SINC8_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+SINC8_TAPS_BITS))
#define SINC8_FRACMASK ((SINC8_TAPS*INTRP_PHASES)-SINC8_TAPS)
#define SINC16_FRACSHIFT (MIXER_FRAC_BITS-(INTRP_PHASES_BITS+SINC16_TAPS_BITS))
#define SINC16_FRACMASK ((SINC16_TAPS*INTRP_PHASES)-SINC16_TAPS)

extern float *fCubicSplineLUT, *fSinc[SINC_KERNELS], *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];
extern uint64_t sincRatio1, sincRatio2;

bool setupMixerInterpolationTables(void);
void freeMixerInterpolationTables(void);
