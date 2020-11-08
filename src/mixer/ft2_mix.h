#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

typedef void (*mixFunc)(void *, uint32_t, uint32_t);

extern const mixFunc mixFuncTab[72]; // ft2_mix.c
