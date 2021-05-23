#pragma once

#include <stdint.h>
#include "../ft2_cpu.h"

#if CPU_64BIT
#define MIXER_FRAC_BITS 32
#else
#define MIXER_FRAC_BITS 16
#endif

#define MIXER_FRAC_SCALE ((intCPUWord_t)1 << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

typedef void (*mixFunc)(void *, uint32_t, uint32_t);

extern const mixFunc mixFuncTab[72]; // ft2_mix.c
