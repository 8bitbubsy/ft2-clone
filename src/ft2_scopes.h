#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"

#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE (1ULL << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

// just about max safe bits, don't mess with it!
#define SCOPE_DRAW_FRAC_BITS 20
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
	const int8_t *sampleData8;
	const int16_t *sampleData16;
	int8_t SVol;
	bool wasCleared, sampleIs16Bit, backwards;
	uint8_t loopType;
	int32_t SRepS, SRepL, SLen, SPos;
	uint32_t DFrq;
	uint64_t SFrq, SPosDec;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t sampleNr, instrNr;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_VOICES];
