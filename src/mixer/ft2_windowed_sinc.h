#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_audio.h" // voice_t
#include "ft2_mix.h" // MIXER_FRAC_BITS

#define SINC_KERNELS 4
#define SINC8_TAPS 8
#define SINC16_TAPS 16

// 256 phases + linear interpolation = near perfect (and low CPU cache usage)
#define SINC_OVERSAMPLING 256

// log2(SINC_OVERSAMPLING)
#define SINC_OVERSAMPLING_BITS 8

// log2(SINC8_TAPS)
#define SINC8_TAPS_BITS 3

// log2(SINC16_TAPS)
#define SINC16_TAPS_BITS 4

#define INTRP_PHASE_SHIFT (MIXER_FRAC_BITS-SINC_OVERSAMPLING_BITS)
#define INTRP_PHASE_SCALE (1L << INTRP_PHASE_SHIFT)
#define INTRP_PHASE_MASK (INTRP_PHASE_SCALE-1)

bool setupWindowedSincTables(void);
void freeWindowedSincTables(void);
void setWindowedSincIntrpTable(voice_t *v);
