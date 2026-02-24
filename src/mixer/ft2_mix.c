#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h" // CLAMP16()
#include "ft2_mix.h"
#include "ft2_mix_macros.h"

/*
** ------------ 32-bit floating-point audio channel mixer ------------
**       (Note: Mixing macros can be found in ft2_mix_macros.h)
**
** Specifications:
** - Interpolation: None, 2-tap linear, 3-tap quadratic spline, 4-tap cubic spline, 8-tap/16-tap windowed-sinc
** - FT2-styled linear volume ramping (can be turned off)
** - 32.32 fixed-point precision for resampling delta/position
** - 32-bit floating-point precision for mixing and interpolation
**
** This file has separate routines for EVERY possible sampling variation:
** Interpolation type, volume ramp on/off, 8-bit/16-bit sample, loop type.
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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix8bPingpongLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}
	SET_BACK_MIXER_POS
}

static void mix8bNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_S8INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_S8INTRP
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bPingpongLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix8bPingpongLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_S16INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_S16INTRP
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bPingpongLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bNoLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_CINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_CINTRP
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bPingpongLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bNoLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_QINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_QINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bPingpongLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_QINTRP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix8bRampPingpongLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampPingpongLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix8bRampPingpongLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampPingpongLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampPingpongLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}
	
	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampPingpongLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE8_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_8BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix16bPingpongLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_S8INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_S8INTRP
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bPingpongLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix16bPingpongLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_S16INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_S16INTRP
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bPingpongLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_CINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_CINTRP
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bPingpongLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_QINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_QINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			INC_POS
		}
		
		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bPingpongLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_QINTRP
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix16bRampPingpongLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S8INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampPingpongLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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

static void mix16bRampPingpongLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_LINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_S16INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampPingpongLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_CINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampPingpongLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		if (v->hasLooped) // the negative interpolation taps need a special case after the sample has looped once
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_PINGPONG
			}
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac;

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
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampPingpongLoopQIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

	GET_VOL_RAMP
	GET_MIXER_VARS_RAMP
	SET_BASE16_PINGPONG

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		samplesLeft -= samplesToMix;

		START_PINGPONG
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
			RENDER_16BIT_SMP_QINTRP
			VOLUME_RAMPING
			INC_POS_PINGPONG
		}
		END_PINGPONG

		WRAP_PINGPONG_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

// -----------------------------------------------------------------------

const mixFunc mixFuncTab[] =
{
	// no volume ramping

	// 8-bit
	(mixFunc)mix8bNoLoop,
	(mixFunc)mix8bLoop,
	(mixFunc)mix8bPingpongLoop,
	(mixFunc)mix8bNoLoopS8Intrp,
	(mixFunc)mix8bLoopS8Intrp,
	(mixFunc)mix8bPingpongLoopS8Intrp,
	(mixFunc)mix8bNoLoopLIntrp,
	(mixFunc)mix8bLoopLIntrp,
	(mixFunc)mix8bPingpongLoopLIntrp,
	(mixFunc)mix8bNoLoopS16Intrp,
	(mixFunc)mix8bLoopS16Intrp,
	(mixFunc)mix8bPingpongLoopS16Intrp,
	(mixFunc)mix8bNoLoopCIntrp,
	(mixFunc)mix8bLoopCIntrp,
	(mixFunc)mix8bPingpongLoopCIntrp,
	(mixFunc)mix8bNoLoopQIntrp,
	(mixFunc)mix8bLoopQIntrp,
	(mixFunc)mix8bPingpongLoopQIntrp,

	// 16-bit
	(mixFunc)mix16bNoLoop,
	(mixFunc)mix16bLoop,
	(mixFunc)mix16bPingpongLoop,
	(mixFunc)mix16bNoLoopS8Intrp,
	(mixFunc)mix16bLoopS8Intrp,
	(mixFunc)mix16bPingpongLoopS8Intrp,
	(mixFunc)mix16bNoLoopLIntrp,
	(mixFunc)mix16bLoopLIntrp,
	(mixFunc)mix16bPingpongLoopLIntrp,
	(mixFunc)mix16bNoLoopS16Intrp,
	(mixFunc)mix16bLoopS16Intrp,
	(mixFunc)mix16bPingpongLoopS16Intrp,
	(mixFunc)mix16bNoLoopCIntrp,
	(mixFunc)mix16bLoopCIntrp,
	(mixFunc)mix16bPingpongLoopCIntrp,
	(mixFunc)mix16bNoLoopQIntrp,
	(mixFunc)mix16bLoopQIntrp,
	(mixFunc)mix16bPingpongLoopQIntrp,

	// volume ramping

	// 8-bit
	(mixFunc)mix8bRampNoLoop,
	(mixFunc)mix8bRampLoop,
	(mixFunc)mix8bRampPingpongLoop,
	(mixFunc)mix8bRampNoLoopS8Intrp,
	(mixFunc)mix8bRampLoopS8Intrp,
	(mixFunc)mix8bRampPingpongLoopS8Intrp,
	(mixFunc)mix8bRampNoLoopLIntrp,
	(mixFunc)mix8bRampLoopLIntrp,
	(mixFunc)mix8bRampPingpongLoopLIntrp,
	(mixFunc)mix8bRampNoLoopS16Intrp,
	(mixFunc)mix8bRampLoopS16Intrp,
	(mixFunc)mix8bRampPingpongLoopS16Intrp,
	(mixFunc)mix8bRampNoLoopCIntrp,
	(mixFunc)mix8bRampLoopCIntrp,
	(mixFunc)mix8bRampPingpongLoopCIntrp,
	(mixFunc)mix8bRampNoLoopQIntrp,
	(mixFunc)mix8bRampLoopQIntrp,
	(mixFunc)mix8bRampPingpongLoopQIntrp,

	// 16-bit
	(mixFunc)mix16bRampNoLoop,
	(mixFunc)mix16bRampLoop,
	(mixFunc)mix16bRampPingpongLoop,
	(mixFunc)mix16bRampNoLoopS8Intrp,
	(mixFunc)mix16bRampLoopS8Intrp,
	(mixFunc)mix16bRampPingpongLoopS8Intrp,
	(mixFunc)mix16bRampNoLoopLIntrp,
	(mixFunc)mix16bRampLoopLIntrp,
	(mixFunc)mix16bRampPingpongLoopLIntrp,
	(mixFunc)mix16bRampNoLoopS16Intrp,
	(mixFunc)mix16bRampLoopS16Intrp,
	(mixFunc)mix16bRampPingpongLoopS16Intrp,
	(mixFunc)mix16bRampNoLoopCIntrp,
	(mixFunc)mix16bRampLoopCIntrp,
	(mixFunc)mix16bRampPingpongLoopCIntrp,
	(mixFunc)mix16bRampNoLoopQIntrp,
	(mixFunc)mix16bRampLoopQIntrp,
	(mixFunc)mix16bRampPingpongLoopQIntrp
};
