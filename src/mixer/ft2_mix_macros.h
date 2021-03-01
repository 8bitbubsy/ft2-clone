#pragma once

#include <assert.h>
#include "../ft2_audio.h"
#include "ft2_windowed_sinc.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	const double dVolL = v->dVolL; \
	const double dVolR = v->dVolR; \

#define GET_VOL_MONO \
	const double dVolL = v->dVolL; \

#define GET_VOL_RAMP \
	dVolL = v->dVolL; \
	dVolR = v->dVolR; \

#define GET_VOL_MONO_RAMP \
	dVolL = v->dVolL; \

#define SET_VOL_BACK \
	v->dVolL = dVolL; \
	v->dVolR = dVolR; \

#define SET_VOL_BACK_MONO \
	v->dVolL = v->dVolR = dVolL; \

#define GET_MIXER_VARS \
	const uint64_t delta = v->delta; \
	dMixBufferL = audio.dMixBufferL + bufferPos; \
	dMixBufferR = audio.dMixBufferR + bufferPos; \
	pos = v->pos; \
	posFrac = v->posFrac; \

#define GET_MIXER_VARS_RAMP \
	const uint64_t delta = v->delta; \
	dMixBufferL = audio.dMixBufferL + bufferPos; \
	dMixBufferR = audio.dMixBufferR + bufferPos; \
	dVolLDelta = v->dVolDeltaL; \
	dVolRDelta = v->dVolDeltaR; \
	pos = v->pos; \
	posFrac = v->posFrac; \

#define GET_MIXER_VARS_MONO_RAMP \
	const uint64_t delta = v->delta; \
	dMixBufferL = audio.dMixBufferL + bufferPos; \
	dMixBufferR = audio.dMixBufferR + bufferPos; \
	dVolLDelta = v->dVolDeltaL; \
	pos = v->pos; \
	posFrac = v->posFrac; \

#define PREPARE_TAP_FIX8 \
	const int8_t *loopStartPtr = &v->base8[v->loopStart]; \
	const int8_t *leftEdgePtr = loopStartPtr+SINC_LEFT_TAPS; \

#define PREPARE_TAP_FIX16 \
	const int16_t *loopStartPtr = &v->base16[v->loopStart]; \
	const int16_t *leftEdgePtr = loopStartPtr+SINC_LEFT_TAPS; \

#define SET_BASE8 \
	base = v->base8; \
	smpPtr = base + pos; \

#define SET_BASE16 \
	base = v->base16; \
	smpPtr = base + pos; \

#define SET_BASE8_BIDI \
	base = v->base8; \
	revBase = v->revBase8; \

#define SET_BASE16_BIDI \
	base = v->base16; \
	revBase = v->revBase16; \

#define INC_POS \
	posFrac += delta; \
	smpPtr += posFrac >> MIXER_FRAC_BITS; \
	posFrac &= MIXER_FRAC_MASK; \

#define INC_POS_BIDI \
	posFrac += deltaLo; \
	smpPtr += posFrac >> MIXER_FRAC_BITS; \
	smpPtr += deltaHi; \
	posFrac &= MIXER_FRAC_MASK; \

#define SET_BACK_MIXER_POS \
	v->posFrac = posFrac; \
	v->pos = pos; \

/* ----------------------------------------------------------------------- */
/*                          SAMPLE RENDERING MACROS                        */
/* ----------------------------------------------------------------------- */

#define VOLUME_RAMPING \
	dVolL += dVolLDelta; \
	dVolR += dVolRDelta; \

#define VOLUME_RAMPING_MONO \
	dVolL += dVolLDelta; \

#define RENDER_8BIT_SMP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	dSample = *smpPtr * (1.0 / 128.0); \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_8BIT_SMP_MONO \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	dSample = (*smpPtr * (1.0 / 128.0)) * dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

#define RENDER_16BIT_SMP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	dSample = *smpPtr * (1.0 / 32768.0); \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_16BIT_SMP_MONO \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	dSample = (*smpPtr * (1.0 / 32768.0)) * dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

// 2-tap linear interpolation (like FT2)

#define LINEAR_INTERPOLATION(s, f, scale) \
{ \
	/* uint32_t -> int32_t for less SIMD overhead when doing int->double conversion */ \
	const int32_t frac = (uint32_t)(f) >> 1; /* (2^32)-1 -> (2^31)-1 */ \
	\
	const double dFrac = (double)(frac * (1.0 / (INT32_MAX+1.0))); /* 0.0 .. 0.999999999 */ \
	dSample = ((s[0] + (s[1]-s[0]) * dFrac)) * (1.0 / scale); \
} \

#define RENDER_8BIT_SMP_LINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	LINEAR_INTERPOLATION(smpPtr, posFrac, 128) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_8BIT_SMP_MONO_LINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	LINEAR_INTERPOLATION(smpPtr, posFrac, 128) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

#define RENDER_16BIT_SMP_LINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	LINEAR_INTERPOLATION(smpPtr, posFrac, 32768) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_16BIT_SMP_MONO_LINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	LINEAR_INTERPOLATION(smpPtr, posFrac, 32768) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

// 8-tap windowed-sinc interpolation (better quality, through LUT: mixer/ft2_windowed_sinc.c)

/* 8bitbubsy: It may look like we are potentially going out of bounds while looking up the sample points,
** but the sample data is actually padded on both the left (negative) and right side, where correct tap
** samples are stored according to loop mode (or no loop).
**
** There is also a second special case for the left edge (negative taps) after the sample has looped once.
**
*/

#define WINDOWED_SINC_INTERPOLATION(s, f, scale) \
{ \
	const double *t = v->dSincLUT + (((uint32_t)f >> SINC_FSHIFT) & SINC_FMASK); \
	dSample = ((s[-3] * t[0]) + \
	           (s[-2] * t[1]) + \
	           (s[-1] * t[2]) + \
	           ( s[0] * t[3]) + \
	           ( s[1] * t[4]) + \
	           ( s[2] * t[5]) + \
	           ( s[3] * t[6]) + \
	           ( s[4] * t[7])) * (1.0 / scale); \
} \

#define RENDER_8BIT_SMP_SINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	WINDOWED_SINC_INTERPOLATION(smpPtr, posFrac, 128) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_8BIT_SMP_MONO_SINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	WINDOWED_SINC_INTERPOLATION(smpPtr, posFrac, 128) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

#define RENDER_16BIT_SMP_SINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	WINDOWED_SINC_INTERPOLATION(smpPtr, posFrac, 32768) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_16BIT_SMP_MONO_SINTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	WINDOWED_SINC_INTERPOLATION(smpPtr, posFrac, 32768) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

/* Special left-edge case mixers to get proper tap data after one loop cycle.
** These are only used with sinc interpolation on looped samples.
*/

#define RENDER_8BIT_SMP_SINTRP_TAP_FIX  \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC_INTERPOLATION(smpTapPtr, posFrac, 128) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_8BIT_SMP_MONO_SINTRP_TAP_FIX \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC_INTERPOLATION(smpTapPtr, posFrac, 128) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

#define RENDER_16BIT_SMP_SINTRP_TAP_FIX \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC_INTERPOLATION(smpTapPtr, posFrac, 32768) \
	*dMixBufferL++ += dSample * dVolL; \
	*dMixBufferR++ += dSample * dVolR; \

#define RENDER_16BIT_SMP_MONO_SINTRP_TAP_FIX \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC_INTERPOLATION(smpTapPtr, posFrac, 32768) \
	dSample *= dVolL; \
	*dMixBufferL++ += dSample; \
	*dMixBufferR++ += dSample; \

/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#define LIMIT_MIX_NUM \
	i = (v->end - 1) - pos; \
	const uint64_t tmp64 = ((uint64_t)i << 32) | ((uint32_t)posFrac ^ 0xFFFFFFFF); \
	\
	samplesToMix = (uint32_t)(tmp64 / (uint64_t)v->delta) + 1; /* this can be slow on 32-bit systems... */ \
	if (samplesToMix > samplesLeft) \
		samplesToMix = samplesLeft; \

#define START_BIDI \
	if (v->backwards) \
	{ \
		tmpDelta = 0 - delta; \
		assert(pos >= v->loopStart && pos < v->end); \
		pos = ~pos; \
		smpPtr = revBase + pos; \
		posFrac ^= MIXER_FRAC_MASK; \
	} \
	else \
	{ \
		tmpDelta = delta; \
		assert(pos >= 0 && pos < v->end); \
		smpPtr = base + pos; \
	} \
	\
	const int32_t deltaHi = (int64_t)tmpDelta >> MIXER_FRAC_BITS; \
	const uint32_t deltaLo = tmpDelta & MIXER_FRAC_MASK; \

#define LIMIT_MIX_NUM_RAMP \
	if (v->volRampSamples == 0) \
	{ \
		dVolLDelta = 0.0; \
		dVolRDelta = 0.0; \
		\
		if (v->isFadeOutVoice) \
		{ \
			v->active = false; /* volume ramp fadeout-voice is done, shut it down */ \
			return; \
		} \
	} \
	else \
	{ \
		if (samplesToMix > v->volRampSamples) \
			samplesToMix = v->volRampSamples; \
		\
		v->volRampSamples -= samplesToMix; \
	} \

#define LIMIT_MIX_NUM_MONO_RAMP \
	if (v->volRampSamples == 0) \
	{ \
		dVolLDelta = 0.0; \
		if (v->isFadeOutVoice) \
		{ \
			v->active = false; /* volume ramp fadeout-voice is done, shut it down */ \
			return; \
		} \
	} \
	else \
	{ \
		if (samplesToMix > v->volRampSamples) \
			samplesToMix = v->volRampSamples; \
		\
		v->volRampSamples -= samplesToMix; \
	} \

#define HANDLE_SAMPLE_END \
	pos = (int32_t)(smpPtr - base); \
	if (pos >= v->end) \
	{ \
		v->active = false; \
		return; \
	} \

#define WRAP_LOOP \
	pos = (int32_t)(smpPtr - base); \
	if (pos >= v->end) \
	{ \
		do \
		{ \
			pos -= v->loopLength; \
		} \
		while (pos >= v->end); \
		\
		smpPtr = base + pos; \
		\
		v->hasLooped = true; \
	} \

#define WRAP_BIDI_LOOP \
	if (pos >= v->end) \
	{ \
		do \
		{ \
			pos -= v->loopLength; \
			v->backwards ^= 1; \
		} \
		while (pos >= v->end); \
		v->hasLooped = true; \
	} \

#define END_BIDI \
	if (v->backwards) \
	{ \
		posFrac ^= MIXER_FRAC_MASK; \
		pos = ~(int32_t)(smpPtr - revBase); \
	} \
	else \
	{ \
		pos = (int32_t)(smpPtr - base); \
	} \

