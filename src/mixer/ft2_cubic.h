#pragma once

#include <stdbool.h>
#include "../ft2_audio.h" // MIXER_FRAC_BITS definition

// if you change this, also change CUBIC_PHASES_BITS
#define CUBIC_PHASES 4096
#define CUBIC_PHASES_BITS 12 /* log2(CUBIC_PHASES) */

// don't change these!

#define CUBIC_WIDTH 4
#define CUBIC_WIDTH_BITS 2
#define CUBIC_LUT_LEN (CUBIC_WIDTH * CUBIC_PHASES)
#define CUBIC_FSHIFT (MIXER_FRAC_BITS-(CUBIC_PHASES_BITS+CUBIC_WIDTH_BITS))
#define CUBIC_FMASK ((CUBIC_WIDTH*CUBIC_PHASES)-CUBIC_WIDTH)

extern float *fCubicLUT8;
extern float *fCubicLUT16;

bool calcCubicTable(void);
void freeCubicTable(void);
