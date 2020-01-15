#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"

void resetOldScopeRates(void);
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
	uint32_t SFrq, SPosDec;
} scope_t;

typedef struct lastChInstr_t
{
	uint8_t sampleNr, instrNr;
} lastChInstr_t;

extern lastChInstr_t lastChInstr[MAX_VOICES];
