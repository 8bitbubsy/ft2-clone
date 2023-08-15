#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

// 8192 is a good compromise
#define SINC_PHASES 8192
#define SINC_PHASES_BITS 13 // log2(SINC_PHASES)

// do not change these!
#define SINC8_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+3))
#define SINC8_FMASK ((8*SINC_PHASES)-8)
#define SINC16_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+4))
#define SINC16_FMASK ((16*SINC_PHASES)-16)
#define SINC_MAX_TAPS 16
#define SINC_MAX_LEFT_TAPS ((SINC_MAX_TAPS/2)-1)
#define SINC_MAX_RIGHT_TAPS (SINC_MAX_TAPS/2)

extern float *fKaiserSinc_8, *fDownSample1_8, *fDownSample2_8;
extern float *fKaiserSinc_16, *fDownSample1_16, *fDownSample2_16;

extern float *fKaiserSinc, *fDownSample1, *fDownSample2;

bool calcWindowedSincTables(void);
void freeWindowedSincTables(void);
