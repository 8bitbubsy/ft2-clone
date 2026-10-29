#pragma once

#include <stdint.h>
#include "../ft2_header.h"
#include "ft2_scopes.h"

#define SCOPE_INIT \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample, position = s->position; \
	uint64_t positionFrac = s->positionFrac; \
	float fSample; \
	const float fVolume = (float)s->volume * (float)((SCOPE_HEIGHT / 2.0) / 32768.0);

#define SCOPE_INIT_PINGPONG \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample, actualPos, position = s->position; \
	uint64_t positionFrac = s->positionFrac; \
	bool samplingBackwards = s->samplingBackwards; \
	float fSample; \
	const float fVolume = (float)s->volume * (float)((SCOPE_HEIGHT / 2.0) / 32768.0);

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

#define NO_INTERPOLATION8 fSample = s8[0] * (1.0f / 128.0f);
#define NO_INTERPOLATION16 fSample = s16[0] * (1.0f / 32768.0f);

// 2-point linear interpolation

#define LINEAR_INTERPOLATION(s, f, smpBits) \
{ \
	const int32_t frac31 = (uint32_t)(f) >> (SCOPE_FRAC_BITS-31); \
	const float t = (float)frac31 * (float)(1.0 / (INT32_MAX+1.0)); \
	const float s1 = s[0], s2 = s[1]; \
	\
	fSample = (s1 + ((s2 - s1) * t)) * (float)(1.0 / (1 << (smpBits-1))); \
}

#define LINEAR_INTERPOLATION8(f) LINEAR_INTERPOLATION(s8, f, 8)
#define LINEAR_INTERPOLATION16(f) LINEAR_INTERPOLATION(s16, f, 16)

// 4-point cubic B-spline interpolation

#define CUBIC_BSPLINE_INTERPOLATION(s, f, smpBits) \
{ \
	const int32_t frac31 = (uint32_t)(f) >> (SCOPE_FRAC_BITS-31); \
	const float t = (float)frac31 * (float)(1.0 / (INT32_MAX+1.0)); \
	const float s1 = s[-1], s2 = s[0], s3 = s[1], s4 = s[2]; \
	\
	const float a = ((s4 - s1) * (1.0f / 6.0f)) + ((s2 - s3) * (3.0f / 6.0f)); \
	const float b = ((s1 + s3) * (3.0f / 6.0f)) - s2; \
	const float c =  (s3 - s1) * (3.0f / 6.0f); \
	const float d = ((s1 + s3) * (1.0f / 6.0f)) + (s2 * (4.0f / 6.0f)); \
	\
	fSample = (((a * t + b) * t + c) * t + d) * (float)(1.0 / (1 << (smpBits-1))); \
}

#define CUBIC_BSPLINE_INTERPOLATION8(f) CUBIC_BSPLINE_INTERPOLATION(s8, f, 8)
#define CUBIC_BSPLINE_INTERPOLATION16(f) CUBIC_BSPLINE_INTERPOLATION(s16, f, 16)

#define CUBIC_BSPLINE_INTERPOLATION8_LOOP(pos, f) \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s8 = s->leftEdgeTaps8 + (pos - s->loopStart); \
	\
	CUBIC_BSPLINE_INTERPOLATION(s8, f, 8) \

#define CUBIC_BSPLINE_INTERPOLATION16_LOOP(pos, f) \
	if (s->hasLooped && pos <= s->loopStart+MAX_LEFT_TAPS) \
		s16 = s->leftEdgeTaps16 + (pos - s->loopStart); \
	\
	CUBIC_BSPLINE_INTERPOLATION(s16, f, 16) \

/* Note:
** floorf() is needed for proper negative rounding, or else
** the scopes will not look correct.
*/

#define INTERPOLATE_SMP8(pos, f) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(f) \
	else \
		CUBIC_BSPLINE_INTERPOLATION8(f) \
	sample = (int32_t)floorf(fSample * fVolume);

#define INTERPOLATE_SMP16(pos, f) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(f) \
	else \
		CUBIC_BSPLINE_INTERPOLATION16(f) \
	sample = (int32_t)floorf(fSample * fVolume);

#define INTERPOLATE_SMP8_LOOP(pos, f) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION8 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION8(f) \
	else \
		CUBIC_BSPLINE_INTERPOLATION8_LOOP(pos, f) \
	sample = (int32_t)floorf(fSample * fVolume);

#define INTERPOLATE_SMP16_LOOP(pos, f) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
		NO_INTERPOLATION16 \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
		LINEAR_INTERPOLATION16(f) \
	else \
		CUBIC_BSPLINE_INTERPOLATION16_LOOP(pos, f) \
	sample = (int32_t)floorf(fSample * fVolume);

#define SCOPE_GET_SMP8 \
	if (s->active) \
	{ \
		fSample = s->base8[position] * (1.0f / 128.0f); \
		sample = (int32_t)floorf(fSample * fVolume); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16 \
	if (s->active) \
	{ \
		fSample = s->base16[position] * (1.0f / 32768.0f); \
		sample = (int32_t)floorf(fSample * fVolume); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP8_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		fSample = s->base8[actualPos] * (1.0f / 128.0f); \
		sample = (int32_t)floorf(fSample * fVolume); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16_PINGPONG \
	if (s->active) \
	{ \
		GET_PINGPONG_POSITION \
		fSample = s->base16[actualPos] * (1.0f / 32768.0f); \
		sample = (int32_t)floorf(fSample * fVolume); \
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
