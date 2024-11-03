#pragma once

#include <stdint.h>

enum
{
	// don't change the order of these! (yes, it looks off)
	INTERPOLATION_DISABLED = 0,
	INTERPOLATION_SINC8 = 1,
	INTERPOLATION_LINEAR = 2,
	INTERPOLATION_SINC16 = 3,
	INTERPOLATION_CUBIC = 4,
	INTERPOLATION_GAUSSIAN = 5,
	// ------

	NUM_INTERPOLATORS,
};

#define MAX_TAPS 16
#define MAX_LEFT_TAPS ((MAX_TAPS/2)-1)
#define MAX_RIGHT_TAPS (MAX_TAPS/2)

// the fractional bits are hardcoded, changing these will break things!
#define MIXER_FRAC_BITS 32

#define MIXER_FRAC_SCALE ((int64_t)1 << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

typedef void (*mixFunc)(void *, uint32_t, uint32_t);

extern const mixFunc mixFuncTab[]; // ft2_mix.c
