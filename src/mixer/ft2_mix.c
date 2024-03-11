#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"
#include "ft2_mix_macros.h"

/*
** ------------ 32-bit floating-point audio channel mixer ------------
**       (Note: Mixing macros can be found in ft2_mix_macros.h)
**
** Specifications:
** - Interpolation: None, 2-tap linear, 4-tap cubic spline, 8-tap windowed-sinc, 16-tap windowed-sinc
** - FT2-styled linear volume ramping (can be turned off)
** - 32.32 fixed-point precision for resampling delta/position
** - 32-bit floating-point precision for mixing and interpolation
**
** This file has separate routines for EVERY possible sampling variation:
** Interpolation none/sinc/linear/cubic, volumeramp on/off, 8-bit, 16-bit, no loop, loop, bidi.
** (48 mixing routines in total)
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

static void mix8bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix8bBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
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

static void mix8bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix8bNoLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix8bLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix8bBidiLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
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

static void mix8bBidiLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	int8_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
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

static void mix8bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix8bRampBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S8INTRP
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

static void mix8bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix8bRampNoLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix8bRampLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix8bRampBidiLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_S16INTRP
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

static void mix8bRampBidiLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_CINTRP
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

static void mix16bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix16bBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
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

static void mix16bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix16bNoLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix16bLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix16bBidiLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
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

static void mix16bBidiLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	int16_t *smpTapPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
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

static void mix16bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix16bRampBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S8INTRP
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

static void mix16bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t position;
	float fVolumeLDelta, fVolumeRDelta, fVolumeL, fVolumeR;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t positionFrac, tmpDelta;

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

static void mix16bRampNoLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix16bRampLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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

static void mix16bRampBidiLoopS32Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_S16INTRP
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

static void mix16bRampBidiLoopCIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples)
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
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP_TAP_FIX
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_CINTRP
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

// -----------------------------------------------------------------------

const mixFunc mixFuncTab[] =
{
	// no volume ramping

	// 8-bit
	(mixFunc)mix8bNoLoop,
	(mixFunc)mix8bLoop,
	(mixFunc)mix8bBidiLoop,
	(mixFunc)mix8bNoLoopS8Intrp,
	(mixFunc)mix8bLoopS8Intrp,
	(mixFunc)mix8bBidiLoopS8Intrp,
	(mixFunc)mix8bNoLoopLIntrp,
	(mixFunc)mix8bLoopLIntrp,
	(mixFunc)mix8bBidiLoopLIntrp,
	(mixFunc)mix8bNoLoopS32Intrp,
	(mixFunc)mix8bLoopS32Intrp,
	(mixFunc)mix8bBidiLoopS32Intrp,
	(mixFunc)mix8bNoLoopCIntrp,
	(mixFunc)mix8bLoopCIntrp,
	(mixFunc)mix8bBidiLoopCIntrp,

	// 16-bit
	(mixFunc)mix16bNoLoop,
	(mixFunc)mix16bLoop,
	(mixFunc)mix16bBidiLoop,
	(mixFunc)mix16bNoLoopS8Intrp,
	(mixFunc)mix16bLoopS8Intrp,
	(mixFunc)mix16bBidiLoopS8Intrp,
	(mixFunc)mix16bNoLoopLIntrp,
	(mixFunc)mix16bLoopLIntrp,
	(mixFunc)mix16bBidiLoopLIntrp,
	(mixFunc)mix16bNoLoopS32Intrp,
	(mixFunc)mix16bLoopS32Intrp,
	(mixFunc)mix16bBidiLoopS32Intrp,
	(mixFunc)mix16bNoLoopCIntrp,
	(mixFunc)mix16bLoopCIntrp,
	(mixFunc)mix16bBidiLoopCIntrp,

	// volume ramping

	// 8-bit
	(mixFunc)mix8bRampNoLoop,
	(mixFunc)mix8bRampLoop,
	(mixFunc)mix8bRampBidiLoop,
	(mixFunc)mix8bRampNoLoopS8Intrp,
	(mixFunc)mix8bRampLoopS8Intrp,
	(mixFunc)mix8bRampBidiLoopS8Intrp,
	(mixFunc)mix8bRampNoLoopLIntrp,
	(mixFunc)mix8bRampLoopLIntrp,
	(mixFunc)mix8bRampBidiLoopLIntrp,
	(mixFunc)mix8bRampNoLoopS32Intrp,
	(mixFunc)mix8bRampLoopS32Intrp,
	(mixFunc)mix8bRampBidiLoopS32Intrp,
	(mixFunc)mix8bRampNoLoopCIntrp,
	(mixFunc)mix8bRampLoopCIntrp,
	(mixFunc)mix8bRampBidiLoopCIntrp,

	// 16-bit
	(mixFunc)mix16bRampNoLoop,
	(mixFunc)mix16bRampLoop,
	(mixFunc)mix16bRampBidiLoop,
	(mixFunc)mix16bRampNoLoopS8Intrp,
	(mixFunc)mix16bRampLoopS8Intrp,
	(mixFunc)mix16bRampBidiLoopS8Intrp,
	(mixFunc)mix16bRampNoLoopLIntrp,
	(mixFunc)mix16bRampLoopLIntrp,
	(mixFunc)mix16bRampBidiLoopLIntrp,
	(mixFunc)mix16bRampNoLoopS32Intrp,
	(mixFunc)mix16bRampLoopS32Intrp,
	(mixFunc)mix16bRampBidiLoopS32Intrp,
	(mixFunc)mix16bRampNoLoopCIntrp,
	(mixFunc)mix16bRampLoopCIntrp,
	(mixFunc)mix16bRampBidiLoopCIntrp
};
