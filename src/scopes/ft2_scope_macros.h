#pragma once

#include <stdint.h>
#include "../ft2_header.h"
#include "../mixer/ft2_windowed_sinc.h"
#include "ft2_scopes.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS_NO_LOOP \
	const int32_t volume = s->volume * SCOPE_HEIGHT; \
	const int32_t sampleEnd = s->sampleEnd; \
	const uint64_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint64_t positionFrac = 0;

#define SCOPE_REGS_LOOP \
	const int32_t volume = s->volume * SCOPE_HEIGHT; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint64_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint64_t positionFrac = 0;

#define SCOPE_REGS_BIDI \
	const int32_t volume = s->volume * SCOPE_HEIGHT; \
	const int32_t sampleEnd = s->sampleEnd; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint64_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t actualPos, position = s->position; \
	uint64_t positionFrac = 0; \
	bool samplingBackwards = s->samplingBackwards;

#define LINED_SCOPE_REGS_NO_LOOP \
	SCOPE_REGS_NO_LOOP \
	int32_t smpY1, smpY2; \
	width--;

#define LINED_SCOPE_REGS_LOOP \
	SCOPE_REGS_LOOP \
	int32_t smpY1, smpY2; \
	width--;

#define LINED_SCOPE_REGS_BIDI \
	SCOPE_REGS_BIDI \
	int32_t smpY1, smpY2; \
	width--;

/* Note: Sample data already has fixed tap samples at the end of the sample,
** so that out-of-bounds reads get the correct interpolation tap data.
*/

#define INTERPOLATE_SMP8(pos, frac) \
	const int8_t *s8 = s->base8 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
	{ \
		sample = s8[0] << 8; \
	} \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
	{ \
		const int32_t f = (frac) >> (SCOPE_FRAC_BITS-15); \
		sample = (s8[0] << 8) + ((((s8[1] - s8[0]) << 8) * f) >> 15); \
	} \
	else /* interpolate scopes using 6-tap cubic B-spline */ \
	{ \
		const float *t = fScopeIntrpLUT + (((frac) >> (SCOPE_FRAC_BITS-SCOPE_INTRP_PHASES_BITS)) * SCOPE_INTRP_TAPS); \
		\
		/* get correct negative tap sample points */ \
		int32_t p1 = pos - 2; \
		int32_t p2 = pos - 1; \
		float fSample; \
		if (s->loopType != LOOP_DISABLED && s->hasLooped && (int32_t)pos-2 < (int32_t)s->loopStart) \
		{ \
			const int32_t overflow1 = (int32_t)s->loopStart - p1; \
			const int32_t overflow2 = (int32_t)s->loopStart - p2; \
			if (s->loopType == LOOP_BIDI) /* direction is always backwards at this point */ \
			{ \
				p1 = s->loopStart + overflow1; \
				if (overflow2 > 0) \
					p2 = s->loopStart + overflow2; \
			} \
			else \
			{ \
				p1 = s->loopEnd - overflow1; \
				if (overflow2 > 0) \
					p2 = s->loopEnd - overflow2; \
			} \
		} \
		\
		fSample = (s->base8[p1] * t[0]) + \
		          (s->base8[p2] * t[1]) + \
		          (       s8[0] * t[2]) + \
		          (       s8[1] * t[3]) + \
		          (       s8[2] * t[4]) + \
		          (       s8[3] * t[5]); \
		sample = (int32_t)(fSample * 256.0f); \
	}

#define INTERPOLATE_SMP16(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	if (config.interpolation == INTERPOLATION_DISABLED) \
	{ \
		sample = s16[0]; \
	} \
	else if (config.interpolation == INTERPOLATION_LINEAR) \
	{ \
		const int32_t f = (frac) >> (SCOPE_FRAC_BITS-15); \
		sample = s16[0] + (((s16[1] - s16[0]) * f) >> 15); \
	} \
	else /* interpolate scopes using 6-tap cubic B-spline */ \
	{ \
		const float *t = fScopeIntrpLUT + (((frac) >> (SCOPE_FRAC_BITS-SCOPE_INTRP_PHASES_BITS)) * SCOPE_INTRP_TAPS); \
		\
		/* get correct negative tap sample points */ \
		int32_t p1 = pos - 2; \
		int32_t p2 = pos - 1; \
		float fSample; \
		if (s->loopType != LOOP_DISABLED && s->hasLooped && (int32_t)pos-2 < (int32_t)s->loopStart) \
		{ \
			const int32_t overflow1 = (int32_t)s->loopStart - p1; \
			const int32_t overflow2 = (int32_t)s->loopStart - p2; \
			if (s->loopType == LOOP_BIDI) /* direction is always backwards at this point */ \
			{ \
				p1 = s->loopStart + overflow1; \
				if (overflow2 > 0) \
					p2 = s->loopStart + overflow2; \
			} \
			else \
			{ \
				p1 = s->loopEnd - overflow1; \
				if (overflow2 > 0) \
					p2 = s->loopEnd - overflow2; \
			} \
		} \
		\
		fSample = (s->base16[p1] * t[0]) + \
		          (s->base16[p2] * t[1]) + \
		          (       s16[0] * t[2]) + \
		          (       s16[1] * t[3]) + \
		          (       s16[2] * t[4]) + \
		          (       s16[3] * t[5]); \
		\
		sample = (int32_t)fSample; \
	}
#define SCOPE_GET_SMP8 \
	if (s->active) \
		sample = (s->base8[position] * volume) >> (8+7); \
	else \
		sample = 0;

#define SCOPE_GET_SMP16 \
	if (s->active) \
		sample = (s->base16[position] * volume) >> (16+7); \
	else \
		sample = 0;

#define SCOPE_GET_SMP8_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		sample = (s->base8[actualPos] * volume) >> (8+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		sample = (s->base16[actualPos] * volume) >> (16+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP8 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP8(position, (uint32_t)positionFrac) \
		sample = (sample * volume) >> (16+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP16(position, (uint32_t)positionFrac) \
		sample = (sample * volume) >> (16+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define GET_BIDI_POSITION \
	if (samplingBackwards) \
		actualPos = (sampleEnd - 1) - (position - loopStart); \
	else \
		actualPos = position;

#define SCOPE_GET_INTERPOLATED_SMP8_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		INTERPOLATE_SMP8(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : (uint32_t)positionFrac) \
		sample = (sample * volume) >> (16+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		INTERPOLATE_SMP16(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : (uint32_t)positionFrac) \
		sample = (sample * volume) >> (16+7); \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_UPDATE_READPOS \
	positionFrac += delta; \
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

#define LINED_SCOPE_PREPARE_SMP8_BIDI \
	SCOPE_GET_INTERPOLATED_SMP8_BIDI \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_PREPARE_SMP16_BIDI \
	SCOPE_GET_INTERPOLATED_SMP16_BIDI \
	smpY1 = lineY - sample; \
	SCOPE_UPDATE_READPOS

#define LINED_SCOPE_DRAW_SMP \
	smpY2 = lineY - sample; \
	scopeLine(x, smpY1, smpY2, color); \
	smpY1 = smpY2;

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
		\
		s->hasLooped = true; \
	}

#define SCOPE_HANDLE_POS_BIDI \
	if (position >= sampleEnd) \
	{ \
		if (loopLength >= 2) \
		{ \
			const uint32_t overflow = position - sampleEnd; \
			const uint32_t cycles = overflow / loopLength; \
			const uint32_t phase = overflow % loopLength; \
			position = loopStart + phase; \
			samplingBackwards ^= !(cycles & 1); \
		} \
		else \
		{ \
			position = loopStart; \
		} \
		\
		s->hasLooped = true; \
	}
