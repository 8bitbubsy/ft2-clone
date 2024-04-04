#pragma once

#include <stdint.h>

#define MAX_TAPS 16
#define MAX_LEFT_TAPS ((MAX_TAPS/2)-1)
#define MAX_RIGHT_TAPS (MAX_TAPS/2)

// the fractional bits are hardcoded, changing these will break things!
#define MIXER_FRAC_BITS 32

#define MIXER_FRAC_SCALE ((int64_t)1 << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

typedef void (*mixFunc)(void *, uint32_t, uint32_t);

extern const mixFunc mixFuncTab[]; // ft2_mix.c
