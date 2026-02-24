#pragma once

#include <stdint.h>
#include "../ft2_header.h"
#include "ft2_scopes.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_INIT \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint64_t positionFrac = s->positionFrac;

#define SCOPE_INIT_PINGPONG \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t actualPos, position = s->position; \
	uint64_t positionFrac = s->positionFrac; \
	bool samplingBackwards = s->samplingBackwards;

#define LINED_SCOPE_INIT \
	SCOPE_INIT \
	int32_t smpY1, smpY2; \
	width--;

#define LINED_SCOPE_INIT_PINGPONG \
	SCOPE_INIT_PINGPONG \
	int32_t smpY1, smpY2; \
	width--;

/* Note: Sample data already has fixed tap samples at the end of the sample,
** so that out-of-bounds reads get the correct interpolation tap data.
*/

#define NEAREST_NEIGHGBOR8 \
{ \
	sample = s8[0] << 8; \
} \

#define LINEAR_INTERPOLATION8(frac) \
{ \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS-15); \
	sample = (s8[0] << 8) + ((((s8[1] - s8[0]) << 8) * f) >> 15); \
} \

#define NEAREST_NEIGHGBOR16 \
{ \
	sample = s16[0]; \
} \

#define LINEAR_INTERPOLATION16(frac) \
{ \
	const int32_t f = (frac) >> (SCOPE_FRAC_BITS-15); \
	sample = s16[0] + (((s16[1] - s16[0]) * f) >> 15); \
} \

#define CUBIC_SMP8(frac) \
	const int16_t *t = scopeIntrpLUT + (((frac) >> (SCOPE_FRAC_BITS-SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	\
	sample = ((s8[-1] * t[0]) + \
	          ( s8[0] * t[1]) + \
	          ( s8[1] * t[2]) + \
	          ( s8[2] * t[3])) >> (SCOPE_INTRP_SCALE_BITS-8);

#define CUBIC_SMP16(frac) \
	const int16_t *t = scopeIntrpLUT + (((frac) >> (SCOPE_FRAC_BITS-SCOPE_INTRP_PHASES_BITS)) << SCOPE_INTRP_WIDTH_BITS); \
	\
	sample = ((s16[-1] * t[0]) + \
	          ( s16[0] * t[1]) + \
	          ( s16[1] * t[2]) + \
	          ( s16[2] * t[3])) >> SCOPE_INTRP_SCALE_BITS;

#define CUBIC_INTERPOLATION8(frac) \
{ \
	CUBIC_SMP8(frac) \
} \

#define CUBIC_INTERPOLATION16(frac) \
{ \
	CUBIC_SMP16(frac) \
} \

#define CUBIC_INTERPOLATION8_LOOP(pos, frac) \
{ \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s8 = s->leftEdgeTaps8 + (pos - s->loopStart); \
	\
	CUBIC_SMP8(frac) \
} \

#define CUBIC_INTERPOLATION16_LOOP(pos, frac) \
{ \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s16 = s->leftEdgeTaps16 + (pos - s->loopStart); \
	\
	CUBIC_SMP16(frac) \
} \

#define INTERPOLATE_SMP8(pos, frac) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NEAREST_NEIGHGBOR8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(frac) \
	else \
		CUBIC_INTERPOLATION8(frac) \
	sample = (sample * s->volume) >> (16+2);

#define INTERPOLATE_SMP16(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NEAREST_NEIGHGBOR16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(frac) \
	else \
		CUBIC_INTERPOLATION16(frac) \
	sample = (sample * s->volume) >> (16+2);

#define INTERPOLATE_SMP8_LOOP(pos, frac) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NEAREST_NEIGHGBOR8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(frac) \
	else \
		CUBIC_INTERPOLATION8_LOOP(pos, frac) \
	sample = (sample * s->volume) >> (16+2);

#define INTERPOLATE_SMP16_LOOP(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NEAREST_NEIGHGBOR16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(frac) \
	else \
		CUBIC_INTERPOLATION16_LOOP(pos, frac) \
	sample = (sample * s->volume) >> (16+2);

#define SCOPE_GET_SMP8 \
	if (s->active) \
		sample = (s->base8[position] * s->volume) >> (8+2); \
	else \
		sample = 0;

#define SCOPE_GET_SMP16 \
	if (s->active) \
		sample = (s->base16[position] * s->volume) >> (16+2); \
	else \
		sample = 0;

#define SCOPE_GET_SMP8_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		sample = (s->base8[actualPos] * s->volume) >> (8+2); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		sample = (s->base16[actualPos] * s->volume) >> (16+2); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP8 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP8(position, (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP16(position, (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP8_LOOP \
	if (s->active) \
	{ \
		INTERPOLATE_SMP8_LOOP(position, (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16_LOOP \
	if (s->active) \
	{ \
		INTERPOLATE_SMP16_LOOP(position, (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define GET_PINGPONG_POSITION \
	if (samplingBackwards) \
		actualPos = (s->sampleEnd - 1) - (position - s->loopStart); \
	else \
		actualPos = position;

#define SCOPE_GET_INTERPOLATED_SMP8_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		INTERPOLATE_SMP8_LOOP(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		INTERPOLATE_SMP16_LOOP(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_UPDATE_READPOS \
	positionFrac += s->drawDelta; \
	position += positionFrac >> 32; \
	positionFrac &= UINT32_MAX;

#define SCOPE_DRAW_SMP \
	video.frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

#define LINED_SCOPE_PREPARE_SMP8 \
	SCOPE_GET_INTERPOLATED_SMP8 \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP16 \
	SCOPE_GET_INTERPOLATED_SMP16 \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP8_LOOP \
	SCOPE_GET_INTERPOLATED_SMP8_LOOP \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP16_LOOP \
	SCOPE_GET_INTERPOLATED_SMP16_LOOP \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP8_PINGPONG \
	SCOPE_GET_INTERPOLATED_SMP8_PINGPONG \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP16_PINGPONG \
	SCOPE_GET_INTERPOLATED_SMP16_PINGPONG \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_DRAW_SMP \
	smpY2 = lineY - sample; \
	scopeLine(x, smpY1, smpY2, color); \
	smpY1 = smpY2;

#define SCOPE_HANDLE_POS_NO_LOOP \
	if (position >= s->sampleEnd) \
		s->active = false;

#define SCOPE_HANDLE_POS_LOOP \
	if (position >= s->sampleEnd) \
	{ \
		if (s->loopLength >= 2) \
			position = s->loopStart + ((uint32_t)(position - s->sampleEnd) % (uint32_t)s->loopLength); \
		else \
			position = s->loopStart; \
		\
		s->hasLooped = true; \
	}

#define SCOPE_HANDLE_POS_PINGPONG \
	if (position >= s->sampleEnd) \
	{ \
		if (s->loopLength >= 2) \
		{ \
			const uint32_t overflow = position - s->sampleEnd; \
			const uint32_t cycles = overflow / (uint32_t)s->loopLength; \
			const uint32_t phase = overflow % (uint32_t)s->loopLength; \
			\
			position = s->loopStart + phase; \
			samplingBackwards ^= !(cycles & 1); \
		} \
		else \
		{ \
			position = s->loopStart; \
		} \
		\
		s->hasLooped = true; \
	}
