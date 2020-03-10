#pragma once

#include "ft2_header.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	CDA_LVol = v->SLVol2; \
	CDA_RVol = v->SRVol2; \

#define SET_VOL_BACK \
	v->SLVol2 = CDA_LVol; \
	v->SRVol2 = CDA_RVol; \

#define GET_DELTA delta = v->SFrq;

#define GET_MIXER_VARS \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	mixInMono = (CDA_LVol == CDA_RVol); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

#define GET_MIXER_VARS_RAMP \
	audioMixL = audio.mixBufferL; \
	audioMixR = audio.mixBufferR; \
	CDA_LVolIP = v->SLVolIP; \
	CDA_RVolIP = v->SRVolIP; \
	mixInMono = (v->SLVol2 == v->SRVol2) && (CDA_LVolIP == CDA_RVolIP); \
	realPos = v->SPos; \
	pos = v->SPosDec; \

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
	pos += delta; \
	smpPtr += pos >> 16; \
	pos &= 0xFFFF; \

#define INC_POS_BIDI \
	pos += CDA_IPValL; \
	smpPtr += pos >> 16; \
	smpPtr += CDA_IPValH; \
	pos &= 0xFFFF; \

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

#ifndef LERPMIX

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

#else

// 2-tap linear interpolation (like FT2 - faster, but bad quality)

// in: int32_t s1,s2 = -128..127 | f = 0..65535 (frac) | out: s1 = -32768..32767
#define INTERPOLATE8(s1, s2, f) \
	s2 -= s1; \
	s2 = (s2 * (int32_t)f) >> (16 - 8); \
	s1 <<= 8; \
	s1 += s2; \

// in: int32_t s1,s2 = -32768..32767 | f = 0..65535 (frac) | out: s1 = -32768..32767
#define INTERPOLATE16(s1, s2, f) \
	s2 = (s2 - s1) >> 1; \
	s2 = (s2 * (int32_t)f) >> (16 - 1); \
	s1 += s2; \

#define RENDER_8BIT_SMP_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	INTERPOLATE8(sample, sample2, pos) \
	sample <<= 12; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_8BIT_SMP_MONO_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	INTERPOLATE8(sample, sample2, pos) \
	sample <<= 12; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

#define RENDER_16BIT_SMP_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	INTERPOLATE16(sample, sample2, pos) \
	sample <<= 12; \
	*audioMixL++ += ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixR++ += ((int64_t)sample * CDA_RVol) >> 32; \

#define RENDER_16BIT_SMP_MONO_INTRP \
	assert(smpPtr >= CDA_LinearAdr && smpPtr < CDA_LinearAdr+v->SLen); \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	INTERPOLATE16(sample, sample2, pos) \
	sample <<= 12; \
	sample = ((int64_t)sample * CDA_LVol) >> 32; \
	*audioMixL++ += sample; \
	*audioMixR++ += sample; \

#endif

/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#define LIMIT_MIX_NUM \
	i = (v->SLen - 1) - realPos; \
	if (i > 65535) \
		i = 65535; \
	\
	samplesToMix = (((((uint64_t)i << 16) | (pos ^ 0xFFFF)) * v->SFrqRev) >> 32) + 1; \
	if (samplesToMix > (uint32_t)CDA_BytesLeft) \
		samplesToMix = CDA_BytesLeft; \

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
		if (samplesToMix > (uint32_t)v->SVolIPLen) \
			samplesToMix = v->SVolIPLen; \
		\
		v->SVolIPLen -= samplesToMix; \
	} \

#define START_BIDI \
	if (v->backwards) \
	{ \
		delta = 0 - v->SFrq; \
		CDA_IPValH = (int32_t)delta >> 16; \
		assert(realPos >= v->SRepS && realPos < v->SLen); \
		realPos = ~realPos; \
		smpPtr = CDA_LinAdrRev + realPos; \
		pos ^= 0xFFFF; \
	} \
	else \
	{ \
		delta = v->SFrq; \
		CDA_IPValH = delta >> 16; \
		assert(realPos >= 0 && realPos < v->SLen); \
		smpPtr = CDA_LinearAdr + realPos; \
	} \
	\
	CDA_IPValL = delta & 0xFFFF; \

#define END_BIDI \
	if (v->backwards) \
	{ \
		pos ^= 0xFFFF; \
		realPos = (int32_t)(smpPtr - CDA_LinAdrRev); \
		realPos = ~realPos; \
	} \
	else \
	{ \
		realPos = (int32_t)(smpPtr - CDA_LinearAdr); \
	} \
	\

/* ----------------------------------------------------------------------- */
/*                     SAMPLE END/LOOP WRAPPING MACROS                     */
/* ----------------------------------------------------------------------- */

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

/* ----------------------------------------------------------------------- */
/*                       VOLUME=0 OPTIMIZATION MACROS                      */
/* ----------------------------------------------------------------------- */

#define VOL0_OPTIMIZATION_NO_LOOP \
	assert(numSamples <= 65536); \
	\
	pos = v->SPosDec + ((v->SFrq & 0xFFFF) * numSamples); \
	realPos = v->SPos + ((v->SFrq >> 16) * numSamples) + (pos >> 16); \
	\
	if (realPos >= v->SLen) \
	{ \
		v->mixRoutine = NULL; /* shut down voice */ \
		return; \
	} \
	\
	pos &= 0xFFFF; \
	SET_BACK_MIXER_POS

#define VOL0_OPTIMIZATION_LOOP \
	assert(numSamples <= 65536); \
	\
	pos = v->SPosDec + ((v->SFrq & 0xFFFF) * numSamples); \
	realPos = v->SPos + ((v->SFrq >> 16) * numSamples) + (pos >> 16); \
	\
	while (realPos >= v->SLen) \
		realPos -= v->SRepL; \
	\
	pos &= 0xFFFF; \
	SET_BACK_MIXER_POS

#define VOL0_OPTIMIZATION_BIDI_LOOP \
	assert(numSamples <= 65536); \
	\
	pos = v->SPosDec + ((v->SFrq & 0xFFFF) * numSamples); \
	realPos = v->SPos + ((v->SFrq >> 16) * numSamples) + (pos >> 16); \
	\
	while (realPos >= v->SLen) \
	{ \
		realPos -= v->SRepL; \
		v->backwards ^= 1; \
	} \
	\
	pos &= 0xFFFF; \
	SET_BACK_MIXER_POS
