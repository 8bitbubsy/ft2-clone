#pragma once

#include <stdint.h>
#include "ft2_scopes.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS_NO_LOOP \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0;

#define SCOPE_REGS_LOOP \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0;

#define SCOPE_REGS_PINGPONG \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0; \
	int32_t direction = s->direction;

#define LINED_SCOPE_REGS_NO_LOOP \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const uint32_t delta = (uint32_t)(s->delta >> (SCOPE_FRAC_BITS-10)); \
	const uint32_t width = (x + w) - 1; \
	int32_t sample, sample2; \
	int32_t y1, y2; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0;

#define LINED_SCOPE_REGS_LOOP \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = (uint32_t)(s->delta >> (SCOPE_FRAC_BITS-10)); \
	const uint32_t width = (x + w) - 1; \
	int32_t sample, sample2; \
	int32_t y1, y2; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0;

#define LINED_SCOPE_REGS_PINGPONG \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = (uint32_t)(s->delta >> (SCOPE_FRAC_BITS-10)); \
	const uint32_t width = (x + w) - 1; \
	int32_t sample, sample2; \
	int32_t y1, y2; \
	int32_t position = s->position; \
	uint32_t positionFrac = 0; \
	int32_t direction = s->direction;

/* Note: Sample data already has a fixed tap samples at the end of the sample,
** so that an out-of-bounds read is OK and reads the correct interpolation tap.
*/

#define COLLECT_LERP_SAMPLES8 \
		sample = s->base8[position+0] << 8; \
		sample2 = s->base8[position+1] << 8;

#define COLLECT_LERP_SAMPLES16 \
		sample = s->base16[position+0]; \
		sample2 = s->base16[position+1];

#define DO_LERP \
		const int32_t frac = (uint32_t)positionFrac >> 1; /* 0..32767 */ \
		sample2 -= sample; \
		sample2 = (sample2 * frac) >> 15; \
		sample += sample2; \

#define DO_LERP_BIDI \
		int32_t frac = (uint32_t)positionFrac >> 1; /* 0..32767 */ \
		if (direction == -1) frac ^= 32767; /* negate frac */ \
		sample2 -= sample; \
		sample2 = (sample2 * frac) >> 15; \
		sample += sample2;

#define GET_LERP_SMP8 \
		COLLECT_LERP_SAMPLES8 \
		DO_LERP

#define GET_LERP_SMP16 \
		COLLECT_LERP_SAMPLES16 \
		DO_LERP

#define GET_LERP_SMP8_BIDI \
		COLLECT_LERP_SAMPLES8 \
		DO_LERP_BIDI

#define GET_LERP_SMP16_BIDI  \
		COLLECT_LERP_SAMPLES16 \
		DO_LERP_BIDI

#define SCOPE_GET_SMP8 \
	if (s->active) \
		sample = (s->base8[position] * volume) >> 8; \
	else \
		sample = 0;

#define SCOPE_GET_SMP16 \
	if (s->active) \
		sample = (s->base16[position] * volume) >> 16; \
	else \
		sample = 0;

#define SCOPE_GET_LERP_SMP8 \
	if (s->active) \
	{ \
		GET_LERP_SMP8 \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_LERP_SMP16 \
	if (s->active) \
	{ \
		GET_LERP_SMP16 \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_LERP_SMP8_BIDI \
	if (s->active) \
	{ \
		GET_LERP_SMP8_BIDI \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_LERP_SMP16_BIDI \
	if (s->active) \
	{ \
		GET_LERP_SMP16_BIDI \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_UPDATE_DRAWPOS \
	positionFrac += delta; \
	position += positionFrac >> 16; \
	positionFrac &= 0xFFFF;

#define SCOPE_UPDATE_DRAWPOS_PINGPONG \
	positionFrac += delta; \
	position += (int32_t)(positionFrac >> 16) * direction; \
	positionFrac &= 0xFFFF;

#define SCOPE_DRAW_SMP \
	video.frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

#define LINED_SCOPE_PREPARE_LERP_SMP8 \
	SCOPE_GET_LERP_SMP8 \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_PREPARE_LERP_SMP16 \
	SCOPE_GET_LERP_SMP16 \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_PREPARE_LERP_SMP8_BIDI \
	SCOPE_GET_LERP_SMP8_BIDI \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_PREPARE_LERP_SMP16_BIDI \
	SCOPE_GET_LERP_SMP16_BIDI \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_DRAW_SMP \
	y2 = lineY - sample; \
	scopeLine(x, y1, y2); \
	y1 = y2; \

#define SCOPE_HANDLE_POS_NO_LOOP \
	if (position >= sampleEnd) \
		s->active = false;

#define SCOPE_HANDLE_POS_LOOP \
	if (position >= sampleEnd) \
	{ \
		if (loopLength >= 2) \
			position = loopStart + ((position - sampleEnd) % loopLength); \
		else \
			position = loopStart; \
	}

#define SCOPE_HANDLE_POS_PINGPONG \
	if (direction == -1 && position < loopStart) \
	{ \
		direction = 1; /* change direction to forwards */ \
		\
		if (loopLength >= 2) \
			position = loopStart + ((loopStart - position - 1) % loopLength); \
		else \
			position = loopStart; \
	} \
	else if (position >= sampleEnd) \
	{ \
		direction = -1; /* change direction to backwards */ \
		\
		if (loopLength >= 2) \
			position = (sampleEnd - 1) - ((position - sampleEnd) % loopLength); \
		else \
			position = sampleEnd - 1; \
	}
