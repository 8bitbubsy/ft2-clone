#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

// 8192 is a good compromise
#define SINC_PHASES 8192
#define SINC_PHASES_BITS 13 // log2(SINC_PHASES)

// do not change these!

#define SINC1_TAPS 8
#define SINC8_WIDTH_BITS 3 // log2(SINC1_TAPS)
#define SINC8_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC8_WIDTH_BITS))
#define SINC8_FMASK ((SINC1_TAPS*SINC_PHASES)-SINC1_TAPS)

#define SINC2_TAPS 32
#define SINC32_WIDTH_BITS 5 // log2(SINC2_TAPS)
#define SINC32_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC32_WIDTH_BITS))
#define SINC32_FMASK ((SINC2_TAPS*SINC_PHASES)-SINC2_TAPS)

extern float *fKaiserSinc_8, *fDownSample1_8, *fDownSample2_8;
extern float *fKaiserSinc_32, *fDownSample1_32, *fDownSample2_32;

extern float *fKaiserSinc, *fDownSample1, *fDownSample2;

bool calcWindowedSincTables(void);
void freeWindowedSincTables(void);
