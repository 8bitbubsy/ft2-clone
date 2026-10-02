#pragma once

#include "../ft2_audio.h"
#include "ft2_windowed_sinc.h"

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

#define SET_BASE8_PINGPONG \
	base = v->base8; \
	revBase = v->revBase8;

#define SET_BASE16_PINGPONG \
	base = v->base16; \
	revBase = v->revBase16;

#define INC_POS \
	positionFrac += delta; \
	smpPtr += positionFrac >> MIXER_FRAC_BITS; \
	positionFrac &= MIXER_FRAC_MASK;

#define INC_POS_PINGPONG \
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
/*                            NO INTERPOLATION                             */
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
/*                      2-POINT LINEAR INTERPOLATION                       */
/* ----------------------------------------------------------------------- */

#define LINEAR_INTERPOLATION(s, f, scale) \
{ \
	const int32_t frac24 = (uint32_t)f >> (MIXER_FRAC_BITS-24); \
	const float t = (float)frac24 * (1.0f / (1 << 24)); \
	\
	const float s1 = s[0]; \
	const float s2 = s[1]; \
	\
	fSample = (s1 + ((s2 - s1) * t)) * (1.0f / scale); \
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
/*                 3-POINT QUADRATIC SPLINE INTERPOLATION                  */
/* ----------------------------------------------------------------------- */

#define QUADRATIC_SPLINE_INTERPOLATION(s, f, scale) \
{ \
	const int32_t frac24 = (uint32_t)f >> (MIXER_FRAC_BITS-24); \
	const float t = (float)frac24 * (1.0f / (1 << 24)); \
	\
	const float s1 = s[-1]; \
	const float s2 =  s[0]; \
	const float s3 =  s[1]; \
	\
	const float a = ((s1 + s3) * 0.5f) - s2; \
	const float b =  (s3 - s1) * 0.5f; \
	\
	fSample = ((a * t + b) * t + s2) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_QINTRP \
	QUADRATIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_QINTRP \
	QUADRATIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

/* The TAP_FIX macros are for special left-edge cases get proper tap data after one loop cycle.
** These are only used on looped samples.
*/

#define RENDER_8BIT_SMP_QINTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	QUADRATIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_QINTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	QUADRATIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                   4-POINT CUBIC SPLINE INTERPOLATION                    */
/* ----------------------------------------------------------------------- */

// Catmull-Rom algorithm
#define CUBIC_SPLINE_INTERPOLATION(s, f, scale) \
{ \
	const int32_t frac24 = (uint32_t)f >> (MIXER_FRAC_BITS-24); \
	const float t = (float)frac24 * (1.0f / (1 << 24)); \
	\
	const float s1 = s[-1]; \
	const float s2 =  s[0]; \
	const float s3 =  s[1]; \
	const float s4 =  s[2]; \
	\
	const float a = ((s4 - s1) * 0.5f) + ((s2 - s3) * 1.5f); \
	const float b = ((s1 - (s2 * 2.5f)) + (s3 * 2.0f)) - (s4 * 0.5f); \
	const float c = (s3 - s1) * 0.5f; \
	\
	fSample = (((a * t + b) * t + c) * t + s2) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

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
/*                   8-POINT WINDOWED-SINC INTERPOLATION                   */
/* ----------------------------------------------------------------------- */

/* Uses linear interpolation between the phases in the kernels to achieve
** very good precision at a low pre-computed phase count.
**
** It may look like we go out of bounds for fSinc_2[] and s[], but we have
** extra data at the correct places to account for this.
*/

#define WINDOWED_SINC8_INTERPOLATION(s, f, scale) \
{ \
	const uint32_t frac32 = (uint32_t)f; \
	const uint32_t lutPhase = frac32 >> INTRP_PHASE_SHIFT; \
	const float fIntrpFrac = (int32_t)(frac32 & INTRP_PHASE_MASK) * (1.0f / INTRP_PHASE_SCALE); \
	\
	const float *fSinc_1 = v->fSincLUT + ( lutPhase    << SINC8_TAPS_BITS); \
	const float *fSinc_2 = v->fSincLUT + ((lutPhase+1) << SINC8_TAPS_BITS); \
	\
	float fSum = 0.0f; \
	for (int32_t j = 0; j < SINC8_TAPS; j++) \
	{ \
		const float y1 = fSinc_1[j]; \
		const float y2 = fSinc_2[j]; \
		\
		fSum += s[j-((SINC8_TAPS/2)-1)] * (y1 + ((y2 - y1) * fIntrpFrac)); \
	} \
	\
	fSample = fSum * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

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


/* ----------------------------------------------------------------------- */
/*                  16-POINT WINDOWED-SINC INTERPOLATION                   */
/* ----------------------------------------------------------------------- */

#define WINDOWED_SINC16_INTERPOLATION(s, f, scale) \
{ \
	const uint32_t frac32 = (uint32_t)f; \
	const uint32_t lutPhase = frac32 >> INTRP_PHASE_SHIFT; \
	const float fIntrpFrac = (int32_t)(frac32 & INTRP_PHASE_MASK) * (1.0f / INTRP_PHASE_SCALE); \
	\
	const float *fSinc_1 = v->fSincLUT + ( lutPhase    << SINC16_TAPS_BITS); \
	const float *fSinc_2 = v->fSincLUT + ((lutPhase+1) << SINC16_TAPS_BITS); \
	\
	float fSum = 0.0f; \
	for (int32_t j = 0; j < SINC16_TAPS; j++) \
	{ \
		const float y1 = fSinc_1[j]; \
		const float y2 = fSinc_2[j]; \
		\
		fSum += s[j-((SINC16_TAPS/2)-1)] * (y1 + ((y2 - y1) * fIntrpFrac)); \
	} \
	\
	fSample = fSum * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 32768) \
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

#define START_PINGPONG \
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
		fVolumeLDelta = fVolumeRDelta = 0.0f; \
		if (v->isFadeOutVoice) \
		{ \
			v->active = false; \
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

#define WRAP_PINGPONG_LOOP \
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

#define END_PINGPONG \
	if (v->samplingBackwards) \
	{ \
		positionFrac ^= MIXER_FRAC_MASK; \
		position = ~(int32_t)(smpPtr - revBase); \
	} \
	else \
	{ \
		position = (int32_t)(smpPtr - base); \
	}
