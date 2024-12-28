#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"

/* Scopes must be clocked slightly higher than the nominal vblank rate
** to prevent update/draw racing issues. Setting it too high will
** cause more issues!
*/
#define SCOPE_HZ 64

#define SCOPE_HEIGHT 36

#define SCOPE_FRAC_BITS 32
#define SCOPE_FRAC_SCALE ((int64_t)1 << SCOPE_FRAC_BITS)
#define SCOPE_FRAC_MASK (SCOPE_FRAC_SCALE-1)

#define SCOPE_INTRP_TAPS 6
#define SCOPE_INTRP_SCALE 32768
#define SCOPE_INTRP_SCALE_BITS 15 /* log2(SCOPE_INTRP_SCALE) */
#define SCOPE_INTRP_PHASES 512 /* plentiful for FT2-styled scopes */
#define SCOPE_INTRP_PHASES_BITS 9 /* log2(SCOPE_INTRP_PHASES) */

int32_t getSamplePositionFromScopes(uint8_t ch);
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
	bool wasCleared, sample16Bit, samplingBackwards, hasLooped;
	uint8_t loopType;
	int32_t volume, loopStart, loopLength, loopEnd, sampleEnd, position;
	uint64_t delta, drawDelta, positionFrac;

	// if (loopEnabled && hasLooped && samplingPos <= loopStart+MAX_LEFT_TAPS) readFixedTapsFromThisPointer();
	const int8_t *leftEdgeTaps8;
	const int16_t *leftEdgeTaps16;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t smpNum, instrNum;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_CHANNELS];
