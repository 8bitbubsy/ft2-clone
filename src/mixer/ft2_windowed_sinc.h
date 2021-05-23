#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"

// if you change this, also change SINC_PHASES_BITS (>8192 gives very little improvement)
#define SINC_PHASES 8192
#define SINC_PHASES_BITS 13 /* log2(SINC_PHASES) */

// don't change these!

#define SINC_TAPS 8
#define SINC_TAPS_BITS 3
#define SINC_LUT_LEN (SINC_TAPS * SINC_PHASES)
#define SINC_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC_TAPS_BITS))
#define SINC_FMASK ((SINC_TAPS*SINC_PHASES)-SINC_TAPS)

#define SINC_CENTER_TAP (SINC_TAPS/2)
#define SINC_LEFT_TAPS ((SINC_TAPS/2)-1)
#define SINC_RIGHT_TAPS (SINC_TAPS/2)

// for LUT calculation
#define SINC_MID_TAP ((SINC_TAPS/2)*SINC_PHASES)

extern float *fKaiserSinc;
extern float *fDownSample1;
extern float *fDownSample2;

bool calcWindowedSincTables(void);
void freeWindowedSincTables(void);
