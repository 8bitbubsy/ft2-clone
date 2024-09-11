#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"

#define SCOPE_HEIGHT 36

#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE ((int64_t)1 << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

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
	bool wasCleared, sample16Bit;
	uint8_t loopType;
	int32_t volume, direction, loopStart, loopLength, sampleEnd, position;
	uint32_t drawDelta;
	uint64_t delta, positionFrac;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t smpNum, instrNum;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_CHANNELS];
