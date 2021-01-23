#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"
#include "ft2_mix_macros.h"
#include "ft2_center_mix.h"

/*
** ------------ double-precision floating-point audio channel mixer ------------
**            (Note: Mixing macros can be found in ft2_mix_macros.h)
**
** Specifications:
** - Either no interpolation, 2-tap linear interpolation (FT2) or 8-tap windowed-sinc interpolation
** - Linear volume ramping, matching FT2 (can be turned off)
** - 32.32 fixed-point logic for resampling delta
** - 64-bit doube-precision logic for mixing and interpolation
**
** This file has separate routines for EVERY possible sampling variation:
** Interpolation none/sinc/linear, volumeramp on/off, 8-bit, 16-bit, no loop, loop, bidi.
** (36 mixing routines in total + another 36 for center-mixing)
**
** Every voice has a function pointer set to the according mixing routine on
** sample trigger (from replayer, but set in audio thread), using a function
** pointer look-up table. All voices & pointers are always thread-safely cleared
** when changing any of the above attributes from the GUI, to prevent possible
** thread-related issues.
**
** -----------------------------------------------------------------------------
*/

/* ----------------------------------------------------------------------- */
/*                          8-BIT MIXING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void mix8bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
			RENDER_8BIT_SMP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			INC_POS_BIDI
			RENDER_8BIT_SMP
			INC_POS_BIDI
			RENDER_8BIT_SMP
			INC_POS_BIDI
			RENDER_8BIT_SMP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}
	SET_BACK_MIXER_POS
}

static void mix8bNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_SINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_SINTRP
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_BIDI
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_BIDI
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

/* ----------------------------------------------------------------------- */
/*                          16-BIT MIXING ROUTINES                         */
/* ----------------------------------------------------------------------- */

static void mix16bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
			RENDER_16BIT_SMP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			INC_POS_BIDI
			RENDER_16BIT_SMP
			INC_POS_BIDI
			RENDER_16BIT_SMP
			INC_POS_BIDI
			RENDER_16BIT_SMP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_SINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_SINTRP
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_BIDI
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_SINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_BIDI
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_SINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	double dSample, *dMixBufferL, *dMixBufferR;
	int32_t pos;
	double dVolLDelta, dVolRDelta, dVolL, dVolR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

// -----------------------------------------------------------------------

const mixFunc mixFuncTab[72] =
{
	/*
	** ---------------------------------
	** stereo mixing (this file)
	** ---------------------------------
	*/

	// no volume ramping

	// 8-bit
	(mixFunc)mix8bNoLoop,
	(mixFunc)mix8bLoop,
	(mixFunc)mix8bBidiLoop,
	(mixFunc)mix8bNoLoopSIntrp,
	(mixFunc)mix8bLoopSIntrp,
	(mixFunc)mix8bBidiLoopSIntrp,
	(mixFunc)mix8bNoLoopLIntrp,
	(mixFunc)mix8bLoopLIntrp,
	(mixFunc)mix8bBidiLoopLIntrp,

	// 16-bit
	(mixFunc)mix16bNoLoop,
	(mixFunc)mix16bLoop,
	(mixFunc)mix16bBidiLoop,
	(mixFunc)mix16bNoLoopSIntrp,
	(mixFunc)mix16bLoopSIntrp,
	(mixFunc)mix16bBidiLoopSIntrp,
	(mixFunc)mix16bNoLoopLIntrp,
	(mixFunc)mix16bLoopLIntrp,
	(mixFunc)mix16bBidiLoopLIntrp,

	// volume ramping

	// 8-bit
	(mixFunc)mix8bRampNoLoop,
	(mixFunc)mix8bRampLoop,
	(mixFunc)mix8bRampBidiLoop,
	(mixFunc)mix8bRampNoLoopSIntrp,
	(mixFunc)mix8bRampLoopSIntrp,
	(mixFunc)mix8bRampBidiLoopSIntrp,
	(mixFunc)mix8bRampNoLoopLIntrp,
	(mixFunc)mix8bRampLoopLIntrp,
	(mixFunc)mix8bRampBidiLoopLIntrp,

	// 16-bit
	(mixFunc)mix16bRampNoLoop,
	(mixFunc)mix16bRampLoop,
	(mixFunc)mix16bRampBidiLoop,
	(mixFunc)mix16bRampNoLoopSIntrp,
	(mixFunc)mix16bRampLoopSIntrp,
	(mixFunc)mix16bRampBidiLoopSIntrp,
	(mixFunc)mix16bRampNoLoopLIntrp,
	(mixFunc)mix16bRampLoopLIntrp,
	(mixFunc)mix16bRampBidiLoopLIntrp,

	/* 
	** ---------------------------------
	** center mixing (ft2_center_mix.c)
	** ---------------------------------
	*/ 

	// no volume ramping

	// 8-bit
	(mixFunc)centerMix8bNoLoop,
	(mixFunc)centerMix8bLoop,
	(mixFunc)centerMix8bBidiLoop,
	(mixFunc)centerMix8bNoLoopSIntrp,
	(mixFunc)centerMix8bLoopSIntrp,
	(mixFunc)centerMix8bBidiLoopSIntrp,
	(mixFunc)centerMix8bNoLoopLIntrp,
	(mixFunc)centerMix8bLoopLIntrp,
	(mixFunc)centerMix8bBidiLoopLIntrp,

	// 16-bit
	(mixFunc)centerMix16bNoLoop,
	(mixFunc)centerMix16bLoop,
	(mixFunc)centerMix16bBidiLoop,
	(mixFunc)centerMix16bNoLoopSIntrp,
	(mixFunc)centerMix16bLoopSIntrp,
	(mixFunc)centerMix16bBidiLoopSIntrp,
	(mixFunc)centerMix16bNoLoopLIntrp,
	(mixFunc)centerMix16bLoopLIntrp,
	(mixFunc)centerMix16bBidiLoopLIntrp,

	// volume ramping

	// 8-bit
	(mixFunc)centerMix8bRampNoLoop,
	(mixFunc)centerMix8bRampLoop,
	(mixFunc)centerMix8bRampBidiLoop,
	(mixFunc)centerMix8bRampNoLoopSIntrp,
	(mixFunc)centerMix8bRampLoopSIntrp,
	(mixFunc)centerMix8bRampBidiLoopSIntrp,
	(mixFunc)centerMix8bRampNoLoopLIntrp,
	(mixFunc)centerMix8bRampLoopLIntrp,
	(mixFunc)centerMix8bRampBidiLoopLIntrp,

	// 16-bit
	(mixFunc)centerMix16bRampNoLoop,
	(mixFunc)centerMix16bRampLoop,
	(mixFunc)centerMix16bRampBidiLoop,
	(mixFunc)centerMix16bRampNoLoopSIntrp,
	(mixFunc)centerMix16bRampLoopSIntrp,
	(mixFunc)centerMix16bRampBidiLoopSIntrp,
	(mixFunc)centerMix16bRampNoLoopLIntrp,
	(mixFunc)centerMix16bRampLoopLIntrp,
	(mixFunc)centerMix16bRampBidiLoopLIntrp
};
