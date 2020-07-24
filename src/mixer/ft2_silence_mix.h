#pragma once

#include <stdint.h>
#include <assert.h>
#include "../ft2_audio.h"

#define SILENCE_MIX_NO_LOOP \
	if (realPos >= v->SLen) \
	{ \
		v->active = false; /* shut down voice */ \
		return; \
	} \

#if defined _WIN64 || defined __amd64__

#define SILENCE_MIX_INC_POS \
	const uint64_t newPos = v->SFrq * (uint64_t)numSamples; \
	const uint32_t addPos = (uint32_t)(newPos >> MIXER_FRAC_BITS); \
	uint64_t addFrac = newPos & MIXER_FRAC_MASK; \
	\
	addFrac += v->SPosDec; \
	realPos = v->SPos + addPos + (uint32_t)(addFrac >> MIXER_FRAC_BITS); \
	pos = addFrac & MIXER_FRAC_MASK; \


#define SILENCE_MIX_LOOP \
	if (realPos >= v->SLen) \
	{ \
		if (v->SRepL >= 2) \
			realPos = v->SRepS + ((realPos - v->SLen) % v->SRepL); \
		else \
			realPos = v->SRepS; \
	} \

#define SILENCE_MIX_BIDI_LOOP \
	if (realPos >= v->SLen) \
	{ \
		if (v->SRepL >= 2) \
		{ \
			const int32_t overflow = realPos - v->SLen; \
			const int32_t cycles = overflow / v->SRepL; \
			const int32_t phase = overflow % v->SRepL; \
			\
			realPos = v->SRepS + phase; \
			v->backwards ^= !(cycles & 1); \
		} \
		else \
		{ \
			realPos = v->SRepS; \
		} \
	} \

#else

#define SILENCE_MIX_INC_POS \
	assert(numSamples <= 65536); \
	\
	pos = v->SPosDec + ((v->SFrq & 0xFFFF) * numSamples); \
	realPos = v->SPos + ((v->SFrq >> 16) * numSamples) + (pos >> 16); \
	pos &= 0xFFFF; \

#define SILENCE_MIX_LOOP \
	while (realPos >= v->SLen) \
		realPos -= v->SRepL; \

#define SILENCE_MIX_BIDI_LOOP \
	while (realPos >= v->SLen) \
	{ \
		realPos -= v->SRepL; \
		v->backwards ^= 1; \
	} \

#endif

void silenceMixRoutine(voice_t *v, int32_t numSamples);
