#pragma once

#include <stdint.h>
#include "ft2_scopes.h"

typedef void (*scopeDrawRoutine)(const scope_t *, uint32_t, uint32_t, uint32_t);

extern const scopeDrawRoutine scopeDrawRoutineTable[12]; // ft2_scopedraw.c
