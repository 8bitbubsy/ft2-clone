#pragma once

#include "ft2_header.h"
#include "ft2_audio.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	CDA_LVol = v->SLVol2; \
	CDA_RVol = v->SRVol2; \

#define SET_VOL_BACK \
	v->SLVol2 = CDA_LVol; \
	v->SRVol2 = CDA_RVol; \

#if defined _WIN64 || defined __amd64__

#define GET_MIXER_VARS \
	const uint64_t SFrq = v->SFrq; \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	const bool mixInMono = (CDA_LVol == CDA_RVol); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

#define GET_MIXER_VARS_RAMP \
	const uint64_t SFrq = v->SFrq; \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	CDA_LVolIP = v->SLVolIP; \
	CDA_RVolIP = v->SRVolIP; \
	const bool mixInMono = (v->SLVol2 == v->SRVol2) && (CDA_LVolIP == CDA_RVolIP); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

#else

#define GET_MIXER_VARS \
	const uint32_t SFrq = v->SFrq; \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	const bool mixInMono = (CDA_LVol == CDA_RVol); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

#define GET_MIXER_VARS_RAMP \
	const uint32_t SFrq = v->SFrq; \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	CDA_LVolIP = v->SLVolIP; \
	CDA_RVolIP = v->SRVolIP; \
	const bool mixInMono = (v->SLVol2 == v->SRVol2) && (CDA_LVolIP == CDA_RVolIP); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

#endif

#define SET_BASE8 \
	CDA_LinearAdr = v->SBase8; \
	smpPtr = CDA_LinearAdr + realPos; \

#define SET_BASE16 \
	CDA_LinearAdr = v->SBase16; \
	smpPtr = CDA_LinearAdr + realPos; \

#define SET_BASE8_BIDI \
	CDA_LinearAdr = v->SBase8; \
	CDA_LinAdrRev = v->SRevBase8; \

#define SET_BASE16_BIDI \
	CDA_LinearAdr = v->SBase16; \
	CDA_LinAdrRev = v->SRevBase16; \

#define INC_POS \
	pos += SFrq; \
	smpPtr += pos >> MIXER_FRAC_BITS; \
	pos &= MIXER_FRAC_MASK; \

#define INC_POS_BIDI \
	pos += CDA_IPValL; \
	smpPtr += pos >> MIXER_FRAC_BITS; \
	smpPtr += CDA_IPValH; \
	pos &= MIXER_FRAC_MASK; \

#define SET_BACK_MIXER_POS \
	v->SPosDec = pos; \
	v->SPos = realPos; \

/* ----------------------------------------------------------------------- */
/*                          SAMPLE RENDERING MACROS                        */
/* ----------------------------------------------------------------------- */

#define VOLUME_RAMPING \
	CDA_LVol += CDA_LVolIP; \
	CDA_RVol += CDA_RVolIP; \

// all the 64-bit MULs here convert to fast logic on most 32-bit CPUs

#define RENDER_8BIT_SMP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = *smpPtr << 20; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_8BIT_SMP_MONO \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = *smpPtr << 20; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

#define RENDER_16BIT_SMP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = *smpPtr << 12; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_16BIT_SMP_MONO \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = *smpPtr << 12; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

// 4-tap cubic spline interpolation (default - slower than linear, but better quality)

// in: int32_t s0,s1,s2,s3 = -128..127 | f = 0..65535 (frac) | out: 16-bit s0 (will exceed 16-bits because of overshoot)
#define INTERPOLATE8(s0, s1, s2, s3, f) \
{ \
	const int16_t *t = cubicSplineTable + ((f >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	s0 = ((s0 * t[0]) + (s1 * t[1]) + (s2 * t[2]) + (s3 * t[3])) >> (CUBIC_QUANTSHIFT-8); \
} \

// in: int32_t s0,s1,s2,s3 = -32768..32767 | f = 0..65535 (frac) | out: 16-bit s0 (will exceed 16-bits because of overshoot)
#define INTERPOLATE16(s0, s1, s2, s3, f) \
{ \
	const int16_t *t = cubicSplineTable + ((f >> CUBIC_FSHIFT) & CUBIC_FMASK); \
	s0 = ((s0 * t[0]) + (s1 * t[1]) + (s2 * t[2]) + (s3 * t[3])) >> CUBIC_QUANTSHIFT; \
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
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[-1]; \
	sample2 = smpPtr[0]; \
	sample3 = smpPtr[1]; \
	sample4 = smpPtr[2]; \
	INTERPOLATE8(sample, sample2, sample3, sample4, pos) \
	sample <<= 12; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_8BIT_SMP_MONO_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[-1]; \
	sample2 = smpPtr[0]; \
	sample3 = smpPtr[1]; \
	sample4 = smpPtr[2]; \
	INTERPOLATE8(sample, sample2, sample3, sample4, pos) \
	sample <<= 12; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

#define RENDER_16BIT_SMP_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[-1]; \
	sample2 = smpPtr[0]; \
	sample3 = smpPtr[1]; \
	sample4 = smpPtr[2]; \
	INTERPOLATE16(sample, sample2, sample3, sample4, pos) \
	sample <<= 12; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_16BIT_SMP_MONO_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[-1]; \
	sample2 = smpPtr[0]; \
	sample3 = smpPtr[1]; \
	sample4 = smpPtr[2]; \
	INTERPOLATE16(sample, sample2, sample3, sample4, pos) \
	sample <<= 12; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#if defined _WIN64 || defined __amd64__

#define LIMIT_MIX_NUM \
	samplesToMix = CDA_BytesLeft; \
	i = (v->SLen - 1) - realPos; \
	\
	if (SFrq > 0) \
	{ \
		uint64_t tmp64 = ((uint64_t)i << MIXER_FRAC_BITS) | (pos ^ MIXER_FRAC_MASK); \
		samplesToMix = (uint32_t)(tmp64 / SFrq) + 1; \
		if (samplesToMix > CDA_BytesLeft) \
			samplesToMix = CDA_BytesLeft; \
	} \
	\

#define START_BIDI \
	if (v->backwards) \
	{ \
		delta = 0 - SFrq; \
		assert(realPos >= v->SRepS && realPos < v->SLen); \
		realPos = ~realPos; \
		smpPtr = CDA_LinAdrRev + realPos; \
		pos ^= MIXER_FRAC_MASK; \
	} \
	else \
	{ \
		delta = SFrq; \
		assert(realPos >= 0 && realPos < v->SLen); \
		smpPtr = CDA_LinearAdr + realPos; \
	} \
	\
	const int32_t CDA_IPValH = (int64_t)delta >> MIXER_FRAC_BITS; \
	const uint32_t CDA_IPValL = delta & MIXER_FRAC_MASK; \

#else

#define LIMIT_MIX_NUM \
	i = (v->SLen - 1) - realPos; \
	if (i > (1UL << (32-MIXER_FRAC_BITS))) \
		i = 1UL << (32-MIXER_FRAC_BITS); \
	\
	i = (i << MIXER_FRAC_BITS) | (pos ^ MIXER_FRAC_MASK); \
	samplesToMix = ((int64_t)i * v->SFrqRev) >> 32; \
	samplesToMix++; \
	\
	if (samplesToMix > CDA_BytesLeft) \
		samplesToMix = CDA_BytesLeft; \

#define START_BIDI \
	if (v->backwards) \
	{ \
		delta = 0 - SFrq; \
		assert(realPos >= v->SRepS && realPos < v->SLen); \
		realPos = ~realPos; \
		smpPtr = CDA_LinAdrRev + realPos; \
		pos ^= MIXER_FRAC_MASK; \
	} \
	else \
	{ \
		delta = SFrq; \
		assert(realPos >= 0 && realPos < v->SLen); \
		smpPtr = CDA_LinearAdr + realPos; \
	} \
	\
	const int32_t CDA_IPValH = (int32_t)delta >> MIXER_FRAC_BITS; \
	const uint32_t CDA_IPValL = delta & MIXER_FRAC_MASK; \

#endif

#define LIMIT_MIX_NUM_RAMP \
	if (v->SVolIPLen == 0) \
	{ \
		CDA_LVolIP = 0; \
		CDA_RVolIP = 0; \
		\
		if (v->isFadeOutVoice) \
		{ \
			v->mixRoutine = NULL; /* fade out voice is done, shut it down */ \
			return; \
		} \
	} \
	else \
	{ \
		if (samplesToMix > v->SVolIPLen) \
			samplesToMix = v->SVolIPLen; \
		\
		v->SVolIPLen -= samplesToMix; \
	} \

#define HANDLE_SAMPLE_END \
	realPos = (int32_t)(smpPtr - CDA_LinearAdr); \
	if (realPos >= v->SLen) \
	{ \
		v->mixRoutine = NULL; \
		return; \
	} \

#define WRAP_LOOP \
	realPos = (int32_t)(smpPtr - CDA_LinearAdr); \
	while (realPos >= v->SLen) \
		realPos -= v->SRepL; \
	smpPtr = CDA_LinearAdr + realPos; \

#define WRAP_BIDI_LOOP \
	while (realPos >= v->SLen) \
	{ \
		realPos -= v->SRepL; \
		v->backwards ^= 1; \
	} \

#define END_BIDI \
	if (v->backwards) \
	{ \
		pos ^= MIXER_FRAC_MASK; \
		realPos = ~(int32_t)(smpPtr - CDA_LinAdrRev); \
	} \
	else \
	{ \
		realPos = (int32_t)(smpPtr - CDA_LinearAdr); \
	} \
	\

/* ----------------------------------------------------------------------- */
/*                          VOLUME=0 MIXING MACROS                         */
/* ----------------------------------------------------------------------- */


#if defined _WIN64 || defined __amd64__

#define	VOL0_INC_POS \
	const uint64_t newPos = v->SFrq * (uint64_t)numSamples; \
	const uint32_t addPos = (uint32_t)(newPos >> MIXER_FRAC_BITS); \
	uint64_t addFrac = newPos & MIXER_FRAC_MASK; \
	\
	addFrac += v->SPosDec; \
	realPos = v->SPos + addPos + (uint32_t)(addFrac >> MIXER_FRAC_BITS); \
	pos = addFrac & MIXER_FRAC_MASK; \

#define VOL0_MIXING_NO_LOOP \
	VOL0_INC_POS \
	if (realPos >= v->SLen) \
	{ \
		v->mixRoutine = NULL; /* shut down voice */ \
		return; \
	} \
	\
	SET_BACK_MIXER_POS

#define VOL0_MIXING_LOOP \
	VOL0_INC_POS \
	if (realPos >= v->SLen) \
	{ \
		if (v->SRepL >= 2) \
			realPos = v->SRepS + ((realPos - v->SLen) % v->SRepL); \
		else \
			realPos = v->SRepS; \
	} \
	\
	SET_BACK_MIXER_POS

#define VOL0_MIXING_BIDI_LOOP \
	VOL0_INC_POS \
	if (realPos >= v->SLen) \
	{ \
		if (v->SRepL >= 2) \
		{ \
			const int32_t overflow = realPos - v->SLen; \
			const int32_t cycles = overflow / v->SRepL; \
			const int32_t phase = overflow % v->SRepL; \
			\
			realPos = v->SRepS + phase; \
			v->backwards ^= !(cycles & 1); \
		} \
		else \
		{ \
			realPos = v->SRepS; \
		} \
	} \
	\
	SET_BACK_MIXER_POS

#else

#define	VOL0_INC_POS \
	assert(numSamples <= 65536); \
	\
	pos = v->SPosDec + ((v->SFrq & 0xFFFF) * numSamples); \
	realPos = v->SPos + ((v->SFrq >> 16) * numSamples) + (pos >> 16); \
	pos &= 0xFFFF; \

#define VOL0_MIXING_NO_LOOP \
	VOL0_INC_POS \
	if (realPos >= v->SLen) \
	{ \
		v->mixRoutine = NULL; /* shut down voice */ \
		return; \
	} \
	\
	SET_BACK_MIXER_POS

#define VOL0_MIXING_LOOP \
	VOL0_INC_POS \
	\
	while (realPos >= v->SLen) \
		realPos -= v->SRepL; \
	\
	SET_BACK_MIXER_POS

#define VOL0_MIXING_BIDI_LOOP \
	VOL0_INC_POS \
	while (realPos >= v->SLen) \
	{ \
		realPos -= v->SRepL; \
		v->backwards ^= 1; \
	} \
	\
	SET_BACK_MIXER_POS

#endif
