#pragma once

#include <stdint.h>
#include "ft2_audio.h"

typedef void (*mixRoutine)(void *, int32_t);

extern const mixRoutine mixRoutineTable[24]; // ft2_mix.c
