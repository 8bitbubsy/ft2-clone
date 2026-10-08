#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_audio.h" // voice_t
#include "ft2_mix.h" // MIXER_FRAC_BITS

// 256 phases + linear interpolation = near perfect (and low CPU cache usage)
#define SINC_OVERSAMPLING 256

#define SINC_KERNELS 4
#define SINC_OVERSAMPLING_BITS 8 /* log2(SINC_OVERSAMPLING) */
#define SINC8_TAPS 8
#define SINC8_TAPS_BITS 3 /* log2(SINC8_TAPS) */
#define SINC8_CENTER_TAP ((SINC8_TAPS/2)-1)
#define SINC16_TAPS 16
#define SINC16_TAPS_BITS 4 /* log2(SINC16_TAPS) */
#define SINC16_CENTER_TAP ((SINC16_TAPS/2)-1)

#define SINC_PHASE_SHIFT (MIXER_FRAC_BITS-SINC_OVERSAMPLING_BITS)
#define SINC_PHASE_SCALE (1L << SINC_PHASE_SHIFT)
#define SINC_PHASE_MASK (SINC_PHASE_SCALE-1)

bool setupWindowedSincTables(void);
void freeWindowedSincTables(void);
void setWindowedSincIntrpTable(voice_t *v);
