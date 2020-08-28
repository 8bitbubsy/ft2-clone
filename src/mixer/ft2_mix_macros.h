#pragma once

#include <assert.h>
#include "../ft2_audio.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	const float fVolL = v->fVolL; \
	const float fVolR = v->fVolR; \

#define GET_VOL_MONO \
	const float fVolL = v->fVolL; \

#define GET_VOL_RAMP \
	fVolL = v->fVolL; \
	fVolR = v->fVolR; \

#define GET_VOL_MONO_RAMP \
	fVolL = v->fVolL; \

#define SET_VOL_BACK \
	v->fVolL = fVolL; \
	v->fVolR = fVolR; \

#define SET_VOL_BACK_MONO \
	v->fVolL = v->fVolR = fVolL; \

#define GET_MIXER_VARS \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL; \
	fMixBufferR = audio.fMixBufferR; \
	pos = v->pos; \
	posFrac = v->posFrac; \

#define GET_MIXER_VARS_RAMP \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL; \
	fMixBufferR = audio.fMixBufferR; \
	fVolLDelta = v->fVolDeltaL; \
	fVolRDelta = v->fVolDeltaR; \
	pos = v->pos; \
	posFrac = v->posFrac; \

#define GET_MIXER_VARS_MONO_RAMP \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL; \
	fMixBufferR = audio.fMixBufferR; \
	fVolLDelta = v->fVolDeltaL; \
	pos = v->pos; \
	posFrac = v->posFrac; \

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
	fVolL += fVolLDelta; \
	fVolR += fVolRDelta; \

#define VOLUME_RAMPING_MONO \
	fVolL += fVolLDelta; \

#define RENDER_8BIT_SMP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = *smpPtr * (1.0f / 128.0f); \
	*fMixBufferL++ += fSample * fVolL; \
	*fMixBufferR++ += fSample * fVolR; \

#define RENDER_8BIT_SMP_MONO \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = (*smpPtr * (1.0f / 128.0f)) * fVolL; \
	*fMixBufferL++ += fSample; \
	*fMixBufferR++ += fSample; \

#define RENDER_16BIT_SMP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = *smpPtr * (1.0f / 32768.0f); \
	*fMixBufferL++ += fSample * fVolL; \
	*fMixBufferR++ += fSample * fVolR; \

#define RENDER_16BIT_SMP_MONO \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = (*smpPtr * (1.0f / 32768.0f)) * fVolL; \
	*fMixBufferL++ += fSample; \
	*fMixBufferR++ += fSample; \

// 4-tap cubic spline interpolation

#define INTERPOLATE8(s0, s1, s2, s3, f) \
{ \
	const float *t = (float *)fCubicSplineTable + ((f >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	s0 = ((s0 * t[0]) + (s1 * t[1]) + (s2 * t[2]) + (s3 * t[3])) * (1.0f / 128.0f); \
} \

#define INTERPOLATE16(s0, s1, s2, s3, f) \
{ \
	const float *t = (float *)fCubicSplineTable + ((f >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	s0 = ((s0 * t[0]) + (s1 * t[1]) + (s2 * t[2]) + (s3 * t[3])) * (1.0f / 32768.0f); \
} \

/* 8bitbubsy: It may look like we are potentially going out of bounds by looking up sample point
** -1, 1 and 2, but the sample data is actually padded on both the left (negative) and right side,
** where correct samples are stored according to loop mode (or no loop).
**
** The only issue is that the -1 look-up gets wrong information if loopStart>0 on looped-samples,
** and the sample-position is at loopStart. The spline will get ever so slighty wrong because of this,
** but it's barely audible anyway. Doing it elsewise would require a refactoring of how the audio mixer
** works!
*/

#define RENDER_8BIT_SMP_INTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = smpPtr[-1]; \
	fSample2 = smpPtr[0]; \
	fSample3 = smpPtr[1]; \
	fSample4 = smpPtr[2]; \
	INTERPOLATE8(fSample, fSample2, fSample3, fSample4, posFrac) \
	*fMixBufferL++ += fSample * fVolL; \
	*fMixBufferR++ += fSample * fVolR; \

#define RENDER_8BIT_SMP_MONO_INTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = smpPtr[-1]; \
	fSample2 = smpPtr[0]; \
	fSample3 = smpPtr[1]; \
	fSample4 = smpPtr[2]; \
	INTERPOLATE8(fSample, fSample2, fSample3, fSample4, posFrac) \
	fSample *= fVolL; \
	*fMixBufferL++ += fSample; \
	*fMixBufferR++ += fSample; \

#define RENDER_16BIT_SMP_INTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = smpPtr[-1]; \
	fSample2 = smpPtr[0]; \
	fSample3 = smpPtr[1]; \
	fSample4 = smpPtr[2]; \
	INTERPOLATE16(fSample, fSample2, fSample3, fSample4, posFrac) \
	*fMixBufferL++ += fSample * fVolL; \
	*fMixBufferR++ += fSample * fVolR; \

#define RENDER_16BIT_SMP_MONO_INTRP \
	assert(smpPtr >= base && smpPtr < base+v->end); \
	fSample = smpPtr[-1]; \
	fSample2 = smpPtr[0]; \
	fSample3 = smpPtr[1]; \
	fSample4 = smpPtr[2]; \
	INTERPOLATE16(fSample, fSample2, fSample3, fSample4, posFrac) \
	fSample *= fVolL; \
	*fMixBufferL++ += fSample; \
	*fMixBufferR++ += fSample; \

/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#define LIMIT_MIX_NUM \
	i = (v->end - 1) - pos; \
	if (i > 65535) \
		i = 65535; \
	\
	i = (i << 16) | ((uint32_t)(posFrac >> 16) ^ 0xFFFF); \
	samplesToMix = ((int64_t)i * v->revDelta) >> 32; \
	samplesToMix++; \
	\
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
		fVolLDelta = 0; \
		fVolRDelta = 0; \
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
		fVolLDelta = 0; \
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
	while (pos >= v->end) \
		pos -= v->loopLength; \
	smpPtr = base + pos; \

#define WRAP_BIDI_LOOP \
	while (pos >= v->end) \
	{ \
		pos -= v->loopLength; \
		v->backwards ^= 1; \
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

