#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"

#define SCOPE_HEIGHT 36

#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE (1ULL << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

// *absolute* max safe bits (Amiga periods, period 1), don't mess with it!
#define SCOPE_DRAW_FRAC_BITS 19
#define SCOPE_DRAW_FRAC_SCALE (1UL << SCOPE_DRAW_FRAC_BITS)
#define SCOPE_DRAW_FRAC_MASK (SCOPE_DRAW_FRAC_SCALE-1)

void resetCachedScopeVars(void);
int32_t getSamplePosition(uint8_t ch);
void stopAllScopes(void);
void refreshScopes(void);
bool testScopesMouseDown(void);
void drawScopes(void);
void drawScopeFramework(void);
bool initScopes(void);

// actual scope data
typedef struct scope_t
{
	volatile bool active;
	const int8_t *base8;
	const int16_t *base16;
	bool wasCleared, sampleIs16Bit;
	uint8_t loopType;
	int32_t vol, loopStart, loopLength, end, pos, direction;
	uint32_t drawDelta, oldDrawDelta;
	uint64_t delta, oldDelta, posFrac;

	double dOldHz;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t sampleNr, instrNr;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_VOICES];
