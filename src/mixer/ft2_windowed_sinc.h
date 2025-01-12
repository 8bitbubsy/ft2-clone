#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define SINC1_WIDTH 8
#define SINC1_WIDTH_BITS 3 // log2(SINC1_WIDTH)
#define SINC1_PHASES 8192
#define SINC1_PHASES_BITS 13 // log2(SINC1_PHASES)
#define SINC1_FRACSHIFT (MIXER_FRAC_BITS-(SINC1_PHASES_BITS+SINC1_WIDTH_BITS))
#define SINC1_FRACMASK ((SINC1_WIDTH*SINC1_PHASES)-SINC1_WIDTH)

#define SINC2_WIDTH 16
#define SINC2_WIDTH_BITS 4 // log2(SINC2_WIDTH)
#define SINC2_PHASES 8192
#define SINC2_PHASES_BITS 13 // log2(SINC2_PHASES)
#define SINC2_FRACSHIFT (MIXER_FRAC_BITS-(SINC2_PHASES_BITS+SINC2_WIDTH_BITS))
#define SINC2_FRACMASK ((SINC2_WIDTH*SINC2_PHASES)-SINC2_WIDTH)

extern float *fSinc8_1, *fSinc8_2, *fSinc8_3;
extern float *fSinc16_1, *fSinc16_2, *fSinc16_3;
extern float *fSinc_1, *fSinc_2, *fSinc_3;
extern uint64_t sincRatio1, sincRatio2;

bool setupWindowedSincTables(void);
void freeWindowedSincTables(void);
