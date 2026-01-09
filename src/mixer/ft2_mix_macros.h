#pragma once

#include "../ft2_audio.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	const float fVolumeL = v->fCurrVolumeL; \
	const float fVolumeR = v->fCurrVolumeR;

#define GET_VOL_RAMP \
	fVolumeL = v->fCurrVolumeL; \
	fVolumeR = v->fCurrVolumeR;

#define SET_VOL_BACK \
	v->fCurrVolumeL = fVolumeL; \
	v->fCurrVolumeR = fVolumeR;

#define GET_MIXER_VARS \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL + bufferPos; \
	fMixBufferR = audio.fMixBufferR + bufferPos; \
	position = v->position; \
	positionFrac = v->positionFrac;

#define GET_MIXER_VARS_RAMP \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL + bufferPos; \
	fMixBufferR = audio.fMixBufferR + bufferPos; \
	fVolumeLDelta = v->fVolumeLDelta; \
	fVolumeRDelta = v->fVolumeRDelta; \
	position = v->position; \
	positionFrac = v->positionFrac;

#define PREPARE_TAP_FIX8 \
	const int8_t *loopStartPtr = &v->base8[v->loopStart]; \
	const int8_t *leftEdgePtr = loopStartPtr+MAX_LEFT_TAPS;

#define PREPARE_TAP_FIX16 \
	const int16_t *loopStartPtr = &v->base16[v->loopStart]; \
	const int16_t *leftEdgePtr = loopStartPtr+MAX_LEFT_TAPS;

#define SET_BASE8 \
	base = v->base8; \
	smpPtr = base + position;

#define SET_BASE16 \
	base = v->base16; \
	smpPtr = base + position;

#define SET_BASE8_BIDI \
	base = v->base8; \
	revBase = v->revBase8;

#define SET_BASE16_BIDI \
	base = v->base16; \
	revBase = v->revBase16;

#define INC_POS \
	positionFrac += delta; \
	smpPtr += positionFrac >> MIXER_FRAC_BITS; \
	positionFrac &= MIXER_FRAC_MASK;

#define INC_POS_BIDI \
	positionFrac += deltaLo; \
	smpPtr += positionFrac >> MIXER_FRAC_BITS; \
	smpPtr += deltaHi; \
	positionFrac &= MIXER_FRAC_MASK;

#define SET_BACK_MIXER_POS \
	v->positionFrac = positionFrac; \
	v->position = position;

#define VOLUME_RAMPING \
	fVolumeL += fVolumeLDelta; \
	fVolumeR += fVolumeRDelta;

/* It may look like we are potentially going out of bounds while looking up the sample points,
** but the sample data is actually padded on both the left (negative) and right side, where correct tap
** samples are stored according to loop mode (or no loop).
**
** There is also a second special case for the left edge (negative taps) after the sample has looped once.
*/

/* ----------------------------------------------------------------------- */
/*                  NO INTERPOLATION (NEAREST NEIGHBOR)                    */
/* ----------------------------------------------------------------------- */

#define RENDER_8BIT_SMP \
	fSample = *smpPtr * (1.0f / 128.0f); \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP \
	fSample = *smpPtr * (1.0f / 32768.0f); \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                          LINEAR INTERPOLATION                           */
/* ----------------------------------------------------------------------- */

#define LINEAR_INTERPOLATION(s, f, scale) \
{ \
	const int32_t frac = (uint32_t)(f) >> 1; /* uint32 -> int32 range, faster int->float conv. (x86/x86_64) */ \
	const float fFrac = (int32_t)frac * (1.0f / (MIXER_FRAC_SCALE/2)); /* 0.0f .. 0.9999999f */ \
	fSample = (s[0] + ((s[1] - s[0]) * fFrac)) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_LINTRP \
	LINEAR_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_LINTRP \
	LINEAR_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                     QUADRATIC SPLINE INTERPOLATION                      */
/* ----------------------------------------------------------------------- */

// through LUT: mixer/ft2_quadratic_spline.c

#define QUADRATIC_SPLINE_INTERPOLATION(s, f, scale) \
{ \
	const float *t = fQuadraticSplineLUT + (((uint32_t)(f) >> QUADRATIC_SPLINE_FRACSHIFT) * QUADRATIC_SPLINE_WIDTH); \
	fSample = ((s[0] * t[0]) + \
	           (s[1] * t[1]) + \
	           (s[2] * t[2])) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_QINTRP \
	QUADRATIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_QINTRP \
	QUADRATIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                       CUBIC SPLINE INTERPOLATION                        */
/* ----------------------------------------------------------------------- */

// through LUT: mixer/ft2_cubic_spline.c

#define CUBIC_SPLINE_INTERPOLATION(s, f, scale) \
{ \
	const float *t = fCubicSplineLUT + (((uint32_t)(f) >> CUBIC_SPLINE_FRACSHIFT) & CUBIC_SPLINE_FRACMASK); \
	fSample = ((s[-1] * t[0]) + \
	           ( s[0] * t[1]) + \
	           ( s[1] * t[2]) + \
	           ( s[2] * t[3])) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* Special left-edge case mixers to get proper tap data after one loop cycle.
** These are only used on looped samples.
*/

#define RENDER_8BIT_SMP_CINTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	CUBIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_CINTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	CUBIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                       WINDOWED-SINC INTERPOLATION                       */
/* ----------------------------------------------------------------------- */

// through LUTs: mixer/ft2_windowed_sinc.c

#define WINDOWED_SINC8_INTERPOLATION(s, f, scale) \
{ \
	const float *t = v->fSincLUT + (((uint32_t)(f) >> SINC8_FRACSHIFT) & SINC8_FRACMASK); \
	fSample = ((s[-3] * t[0]) + \
	           (s[-2] * t[1]) + \
	           (s[-1] * t[2]) + \
	           ( s[0] * t[3]) + \
	           ( s[1] * t[4]) + \
	           ( s[2] * t[5]) + \
	           ( s[3] * t[6]) + \
	           ( s[4] * t[7])) * (1.0f / scale); \
}

#define WINDOWED_SINC16_INTERPOLATION(s, f, scale) \
{ \
	const float *t = v->fSincLUT + (((uint32_t)(f) >> SINC16_FRACSHIFT) & SINC16_FRACMASK); \
	fSample = (( s[-7] * t[0]) + \
	           ( s[-6] * t[1]) + \
	           ( s[-5] * t[2]) + \
	           ( s[-4] * t[3]) + \
	           ( s[-3] * t[4]) + \
	           ( s[-2] * t[5]) + \
	           ( s[-1] * t[6]) + \
	           (  s[0] * t[7]) + \
	           (  s[1] * t[8]) + \
	           (  s[2] * t[9]) + \
	           (  s[3] * t[10]) + \
	           (  s[4] * t[11]) + \
	           (  s[5] * t[12]) + \
	           (  s[6] * t[13]) + \
	           (  s[7] * t[14]) + \
	           (  s[8] * t[15])) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_8BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

/* Special left-edge case mixers to get proper tap data after one loop cycle.
** These are only used on looped samples.
*/

#define RENDER_8BIT_SMP_S8INTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC8_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S8INTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC8_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_8BIT_SMP_S16INTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC16_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S16INTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC16_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#define LIMIT_MIX_NUM \
	samplesToMix = INT32_MAX; \
	if (v->delta != 0) \
	{ \
		i = (v->sampleEnd - 1) - position; \
		const uint64_t dividend = ((uint64_t)i << MIXER_FRAC_BITS) | ((uint32_t)positionFrac ^ MIXER_FRAC_MASK); \
		samplesToMix = (uint32_t)(dividend / (uint64_t)v->delta) + 1; \
	} \
	\
	if (samplesToMix > samplesLeft) \
		samplesToMix = samplesLeft;

#define START_BIDI \
	if (v->samplingBackwards) \
	{ \
		tmpDelta = 0 - delta; \
		position = ~position; \
		smpPtr = revBase + position; \
		positionFrac ^= MIXER_FRAC_MASK; \
	} \
	else \
	{ \
		tmpDelta = delta; \
		smpPtr = base + position; \
	} \
	\
	const int32_t deltaHi = (int64_t)tmpDelta >> MIXER_FRAC_BITS; \
	const uint32_t deltaLo = tmpDelta & MIXER_FRAC_MASK;

#define LIMIT_MIX_NUM_RAMP \
	if (v->volumeRampLength == 0) \
	{ \
		fVolumeLDelta = 0.0f; \
		fVolumeRDelta = 0.0f; \
		\
		if (v->isFadeOutVoice) \
		{ \
			v->active = false; /* volume ramp fadeout-voice is done, shut it down */ \
			return; \
		} \
	} \
	else \
	{ \
		if (samplesToMix > v->volumeRampLength) \
			samplesToMix = v->volumeRampLength; \
		\
		v->volumeRampLength -= samplesToMix; \
	}

#define HANDLE_SAMPLE_END \
	position = (int32_t)(smpPtr - base); \
	if (position >= v->sampleEnd) \
	{ \
		v->active = false; \
		return; \
	}

#define WRAP_LOOP \
	position = (int32_t)(smpPtr - base); \
	if (position >= v->sampleEnd) \
	{ \
		do \
		{ \
			position -= v->loopLength; \
		} \
		while (position >= v->sampleEnd); \
		\
		smpPtr = base + position; \
		\
		v->hasLooped = true; \
	}

#define WRAP_BIDI_LOOP \
	if (position >= v->sampleEnd) \
	{ \
		do \
		{ \
			position -= v->loopLength; \
			v->samplingBackwards ^= 1; \
		} \
		while (position >= v->sampleEnd); \
		v->hasLooped = true; \
	}

#define END_BIDI \
	if (v->samplingBackwards) \
	{ \
		positionFrac ^= MIXER_FRAC_MASK; \
		position = ~(int32_t)(smpPtr - revBase); \
	} \
	else \
	{ \
		position = (int32_t)(smpPtr - base); \
	}
