#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define GAUSSIAN_TAPS 4
#define GAUSSIAN_WIDTH_BITS 2 // log2(GAUSSIAN_TAPS)
#define GAUSSIAN_PHASES 8192 /* originally 256 on SNES/PSX, but more is better! */
#define GAUSSIAN_PHASES_BITS 13 // log2(GAUSSIAN_PHASES)
#define GAUSSIAN_FSHIFT (MIXER_FRAC_BITS-(GAUSSIAN_PHASES_BITS+GAUSSIAN_WIDTH_BITS))
#define GAUSSIAN_FMASK ((GAUSSIAN_TAPS*GAUSSIAN_PHASES)-GAUSSIAN_TAPS)

extern float *fGaussianLUT;

bool calcGaussianTable(void);
void freeGaussianTable(void);
