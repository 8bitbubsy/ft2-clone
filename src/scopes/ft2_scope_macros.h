#pragma once

#include <stdint.h>
#include "../ft2_header.h"
#include "../mixer/ft2_windowed_sinc.h"
#include "ft2_scopes.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS_NO_LOOP \
	const int32_t volume = s->volume; \
	const int32_t sampleEnd = s->sampleEnd; \
	const uint64_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	uint32_t width = x + w; \
	int32_t sample; \
	int32_t position = s->position; \
	uint64_t positionFrac = 0;

#define SCOPE_REGS_LOOP \
	const int32_t volume = s->volume; \
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
	const int32_t volume = s->volume; \
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
	const int16_t *t = scopeGaussianLUT + (((frac) >> (SCOPE_FRAC_BITS-8)) << 2); \
	sample = ((s8[0] * t[0]) + \
	          (s8[1] * t[1]) + \
	          (s8[2] * t[2]) + \
	          (s8[3] * t[3])) >> (15-8);

#define INTERPOLATE_SMP16(pos, frac) \
	const int16_t *s16 = s->base16 + pos; \
	const int16_t *t = scopeGaussianLUT + (((frac) >> (SCOPE_FRAC_BITS-8)) << 2); \
	sample = ((s16[0] * t[0]) + \
	          (s16[1] * t[1]) + \
	          (s16[2] * t[2]) + \
	          (s16[3] * t[3])) >> 15;

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

#define SCOPE_GET_SMP8_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		sample = (s->base8[actualPos] * volume) >> 8; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_SMP16_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		sample = (s->base16[actualPos] * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP8 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP8(position, (uint32_t)positionFrac) \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16 \
	if (s->active) \
	{ \
		INTERPOLATE_SMP16(position, (uint32_t)positionFrac) \
		sample = (sample * volume) >> 16; \
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
		INTERPOLATE_SMP8(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : positionFrac) \
		sample = (sample * volume) >> 16; \
	} \
	else \
	{ \
		sample = 0; \
	}

#define SCOPE_GET_INTERPOLATED_SMP16_BIDI \
	if (s->active) \
	{ \
		GET_BIDI_POSITION \
		INTERPOLATE_SMP16(actualPos, samplingBackwards ? ((uint32_t)positionFrac ^ UINT32_MAX) : positionFrac) \
		sample = (sample * volume) >> 16; \
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
	}
