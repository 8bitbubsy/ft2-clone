#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define SINC_KERNELS 3
#define SINC_PHASES 8192
#define SINC_PHASES_BITS 13 /* log2(SINC_PHASES) */

#define SINC8_WIDTH_BITS 3 /* log2(8) */
#define SINC8_FRACSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC8_WIDTH_BITS))
#define SINC8_FRACMASK ((8*SINC_PHASES)-8)

#define SINC16_WIDTH_BITS 4 /* log2(16) */
#define SINC16_FRACSHIFT (MIXER_FRAC_BITS-(SINC_PHASES_BITS+SINC16_WIDTH_BITS))
#define SINC16_FRACMASK ((16*SINC_PHASES)-16)

extern float *fSinc[SINC_KERNELS], *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];
extern uint64_t sincRatio1, sincRatio2;

bool setupWindowedSincTables(void);
void freeWindowedSincTables(void);
