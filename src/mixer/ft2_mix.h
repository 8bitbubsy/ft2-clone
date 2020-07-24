#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

typedef void (*mixFunc)(void *, int32_t);

extern const mixFunc mixFuncTab[48]; // ft2_mix.c
