#pragma once

#include <stdint.h>
#include "../ft2_header.h"
#include "ft2_scopes.h"

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

#define NO_INTERPOLATION8 \
{ \
	sample = s8[0] << 8; \
}

#define NO_INTERPOLATION16 \
{ \
	sample = s16[0]; \
}

// linear interpolation

#define LINEAR_INTERPOLATION(smp1, smp2, frac) \
{ \
	const int32_t frac15 = (uint32_t)(frac) >> (SCOPE_FRAC_BITS-15); \
	const int32_t s1 = smp1, s2 = smp2; \
	\
	sample = s1 + (((s2 - s1) * frac15) >> 15); \
}

#define LINEAR_INTERPOLATION8(frac) LINEAR_INTERPOLATION(s8[0] << 8, s8[1] << 8, frac)
#define LINEAR_INTERPOLATION16(frac) LINEAR_INTERPOLATION(s16[0], s16[1], frac)

/* 4-point cubic B-spline interpolation (32 phases with interpolation between them).
** The LUT has an extra inverted phase at the end for interpolation wrap-around.
*/

#define CUBIC_BSPLINE_INTERPOLATION(data, shift, frac) \
{ \
	const uint32_t fracReduced = (uint32_t)(frac) >> (SCOPE_FRAC_BITS-INTRP_BITS); \
	const uint32_t phaseInt5 = fracReduced >> INTRP_PHASE_SHIFT; \
	const int32_t phaseFrac15 = fracReduced & INTRP_PHASE_MASK; \
	const int16_t *t = scopeIntrpLUT + (phaseInt5 << SCOPE_INTRP_WIDTH_BITS); \
	\
	const int32_t s1 = ((data[-1] * t[0]) + \
	                    ( data[0] * t[1]) + \
	                    ( data[1] * t[2]) + \
	                    ( data[2] * t[3])) >> (shift); \
	\
	const int32_t s2 = ((data[-1] * t[SCOPE_INTRP_WIDTH+0]) + \
	                    ( data[0] * t[SCOPE_INTRP_WIDTH+1]) + \
	                    ( data[1] * t[SCOPE_INTRP_WIDTH+2]) + \
	                    ( data[2] * t[SCOPE_INTRP_WIDTH+3])) >> (shift); \
	\
	sample = s1 + (((s2 - s1) * phaseFrac15) >> INTRP_PHASE_SHIFT); \
}

#define CUBIC_BSPLINE_INTERPOLATION8(frac) CUBIC_BSPLINE_INTERPOLATION(s8, SCOPE_INTRP_SCALE_BITS-8, frac)
#define CUBIC_BSPLINE_INTERPOLATION16(frac) CUBIC_BSPLINE_INTERPOLATION(s16, SCOPE_INTRP_SCALE_BITS, frac)

#define CUBIC_BSPLINE_INTERPOLATION8_LOOP(pos, frac) \
{ \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s8 = s->leftEdgeTaps8 + (pos - s->loopStart); \
	\
	CUBIC_BSPLINE_INTERPOLATION(s8, SCOPE_INTRP_SCALE_BITS-8, frac) \
} \

#define CUBIC_BSPLINE_INTERPOLATION16_LOOP(pos, frac) \
{ \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s16 = s->leftEdgeTaps16 + (pos - s->loopStart); \
	\
	CUBIC_BSPLINE_INTERPOLATION(s16, SCOPE_INTRP_SCALE_BITS, frac) \
} \

#define INTERPOLATE_SMP8(pos, frac) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(frac) \
	else \
		CUBIC_BSPLINE_INTERPOLATION8(frac) \
	sample = (sample * s->volume) >> (16+9);

#define INTERPOLATE_SMP16(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(frac) \
	else \
		CUBIC_BSPLINE_INTERPOLATION16(frac) \
	sample = (sample * s->volume) >> (16+9);

#define INTERPOLATE_SMP8_LOOP(pos, frac) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(frac) \
	else \
		CUBIC_BSPLINE_INTERPOLATION8_LOOP(pos, frac) \
	sample = (sample * s->volume) >> (16+9);

#define INTERPOLATE_SMP16_LOOP(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(frac) \
	else \
		CUBIC_BSPLINE_INTERPOLATION16_LOOP(pos, frac) \
	sample = (sample * s->volume) >> (16+9);

#define SCOPE_GET_SMP8 \
	if (s->active) \
		sample = (s->base8[position] * s->volume) >> (8+9); \
	else \
		sample = 0;

#define SCOPE_GET_SMP16 \
	if (s->active) \
		sample = (s->base16[position] * s->volume) >> (16+9); \
	else \
		sample = 0;

#define SCOPE_GET_SMP8_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		sample = (s->base8[actualPos] * s->volume) >> (8+9); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		sample = (s->base16[actualPos] * s->volume) >> (16+9); \
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
		INTERPOLATE_SMP8_LOOP(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ (uint32_t)SCOPE_FRAC_MASK) : (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		INTERPOLATE_SMP16_LOOP(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ (uint32_t)SCOPE_FRAC_MASK) : (uint32_t)positionFrac) \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_UPDATE_READPOS \
	positionFrac += s->drawDelta; \
	position += positionFrac >> SCOPE_FRAC_BITS; \
	positionFrac &= SCOPE_FRAC_MASK;

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
