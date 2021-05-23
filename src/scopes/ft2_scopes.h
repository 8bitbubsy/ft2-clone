#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_cpu.h"

#define SCOPE_HEIGHT 36

#if CPU_64BIT
#define SCOPE_FRAC_BITS 32
#else
/* Max safe amount of fracbits for uint32_t on scopes @ 64Hz.
** Since the scopes deltas are always high'ish, 13-bit
** fractional precision is OK.
*/
#define SCOPE_FRAC_BITS 13
#endif

#define SCOPE_FRAC_SCALE ((intCPUWord_t)1 << SCOPE_FRAC_BITS)
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
	uintCPUWord_t delta, positionFrac;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t smpNum, instrNum;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_CHANNELS];
