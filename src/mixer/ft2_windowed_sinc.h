#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef MIXER_FRAC_BITS
#define MIXER_FRAC_BITS 32
#endif

// if you change this, also change SINC_PHASES_BITS (>4096 makes little sense)
#define SINC_PHASES 4096
#define SINC_PHASES_BITS 12 /* log2(SINC_PHASES) */

// don't change these!

#define SINC_TAPS 8
#define SINC_TAPS_BITS 3
#define SINC_LUT_LEN (SINC_TAPS * SINC_PHASES)
#define SINC_FSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC_TAPS_BITS))
#define SINC_FMASK ((SINC_TAPS*SINC_PHASES)-SINC_TAPS)

#define SINC_LEFT_TAPS ((SINC_TAPS/2)-1)
#define SINC_RIGHT_TAPS (SINC_TAPS/2)

// for LUT calculation
#define SINC_MID_TAP ((SINC_TAPS/2)*SINC_PHASES)

extern double *gKaiserSinc;
extern double *gDownSample1;
extern double *gDownSample2;

bool calcWindowedSincTables(void);
void freeWindowedSincTables(void);
