#pragma once

#include <stdint.h>
#include <assert.h>
#include "../ft2_audio.h"

#define SILENCE_MIX_NO_LOOP \
	if (pos >= v->end) \
	{ \
		v->active = false; /* shut down voice */ \
		return; \
	} \

#define SILENCE_MIX_INC_POS \
	const uint64_t newPos = v->delta * (uint64_t)numSamples; \
	const uint32_t addPos = (uint32_t)(newPos >> MIXER_FRAC_BITS); \
	uint64_t addFrac = newPos & MIXER_FRAC_MASK; \
	\
	addFrac += v->posFrac; \
	pos = v->pos + addPos + (uint32_t)(addFrac >> MIXER_FRAC_BITS); \
	posFrac = addFrac & MIXER_FRAC_MASK; \

#define SILENCE_MIX_LOOP \
	if (pos >= v->end) \
	{ \
		if (v->loopLength >= 2) \
			pos = v->loopStart + ((pos - v->end) % v->loopLength); \
		else \
			pos = v->loopStart; \
		\
		v->hasLooped = true; \
	} \

#define SILENCE_MIX_BIDI_LOOP \
	if (pos >= v->end) \
	{ \
		if (v->loopLength >= 2) \
		{ \
			const int32_t overflow = pos - v->end; \
			const int32_t cycles = overflow / v->loopLength; \
			const int32_t phase = overflow % v->loopLength; \
			\
			pos = v->loopStart + phase; \
			v->backwards ^= !(cycles & 1); \
		} \
		else \
		{ \
			pos = v->loopStart; \
		} \
		v->hasLooped = true; \
	} \

void silenceMixRoutine(voice_t *v, int32_t numSamples);
