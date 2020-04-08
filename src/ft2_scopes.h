#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"

// 6 = log2(SCOPE_HZ) where SCOPE_HZ is 2^n
#define SCOPE_FRAC_BITS (MIXER_FRAC_BITS+6)

#define SCOPE_FRAC_SCALE (1L << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

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
	bool wasCleared, sample16Bit;
	uint8_t loopType;
	int32_t SPosDir, SRepS, SRepL, SLen, SPos;
	uint64_t SFrq, SPosDec;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t sampleNr, instrNr;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_VOICES];
