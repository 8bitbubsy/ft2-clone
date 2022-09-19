#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"

// 8 or 16. 8 allows more unwanted frequencies, which may be preferred for lo-fi samples
#define SINC_TAPS 8

// log2(SINC_TAPS)
#define SINC_TAPS_BITS 3

#define SINC_PHASES 8192

// log2(SINC_PHASES)
#define SINC_PHASES_BITS 13

#define SINC_LUT_LEN (SINC_TAPS * SINC_PHASES)
#define SINC_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC_TAPS_BITS))
#define SINC_FMASK ((SINC_TAPS*SINC_PHASES)-SINC_TAPS)

#define SINC_CENTER_TAP (SINC_TAPS/2)
#define SINC_LEFT_TAPS ((SINC_TAPS/2)-1)
#define SINC_RIGHT_TAPS (SINC_TAPS/2)

// for LUT calculation
#define SINC_MID_TAP ((SINC_TAPS/2)*SINC_PHASES)

extern float *fKaiserSinc, *fDownSample1, *fDownSample2;

bool calcWindowedSincTables(void);
void freeWindowedSincTables(void);
