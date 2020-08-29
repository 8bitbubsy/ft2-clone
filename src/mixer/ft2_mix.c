#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"
#include "ft2_mix_macros.h"
#include "ft2_center_mix.h"

/*
** -------------------- floating point audio channel mixer ---------------------
**            (Note: Mixing macros can be found in ft2_mix_macros.h)
**
** Specifications:
** - Fast 4-tap cubic interpolation through 32-bit float LUT (optional)
** - Linear volume ramping, matching FT2 (optional)
** - 32.32 fixed-point logic for position delta
** - 32-bit float logic for volumes/amplitudes
**
** This file has separate routines for EVERY possible sampling variation:
** Interpolation on/off, volumeramp on/off, 8-bit, 16-bit, no loop, loop, bidi.
** (24 mixing routines in total + another 24 for center-mixing)
**
** Every voice has a function pointer set to the according mixing routine on
** sample trigger (from replayer, but set in audio thread), using a function
** pointer look-up table. All voices & pointers are always thread-safely cleared
** when changing any of the above attributes from the GUI, to prevent possible
** thread-related issues.
**
** There's one problem with the 4-tap cubic spline resampling interpolation...
** On looped samples where loopStart>0, the splines are not correct when reading
** from the loopStart (or +1?) sample point. The difference in audio is very
** minor, so it's not a big problem. It just has to stay like this the way the
** mixer works. In cases where loopStart=0, the sample before index 0 (yes, we
** allocate enough data and pre-increment main pointer to support negative
** look-up), is already pre-fixed so that the splines will be correct.
** -----------------------------------------------------------------------------
*/

/* ----------------------------------------------------------------------- */
/*                          8-BIT MIXING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void mix8bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix8bLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix8bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix8bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_8BIT_SMP_INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_8BIT_SMP_INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
			RENDER_8BIT_SMP_INTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_8BIT_SMP_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix8bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix8bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix8bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_8BIT_SMP_INTRP
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

static void mix16bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix16bLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix16bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
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

static void mix16bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_16BIT_SMP_INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_16BIT_SMP_INTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
			RENDER_16BIT_SMP_INTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
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
			RENDER_16BIT_SMP_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix16bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix16bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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

static void mix16bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK
	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, fSample2, fSample3, fSample4, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolRDelta, fVolL, fVolR;
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
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
			VOLUME_RAMPING
			INC_POS_BIDI
			RENDER_16BIT_SMP_INTRP
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

const mixFunc mixFuncTab[48] =
{
	// normal mixing (this file)
	(mixFunc)mix8bNoLoop,
	(mixFunc)mix8bLoop,
	(mixFunc)mix8bBidiLoop,
	(mixFunc)mix8bNoLoopIntrp,
	(mixFunc)mix8bLoopIntrp,
	(mixFunc)mix8bBidiLoopIntrp,
	(mixFunc)mix16bNoLoop,
	(mixFunc)mix16bLoop,
	(mixFunc)mix16bBidiLoop,
	(mixFunc)mix16bNoLoopIntrp,
	(mixFunc)mix16bLoopIntrp,
	(mixFunc)mix16bBidiLoopIntrp,
	(mixFunc)mix8bRampNoLoop,
	(mixFunc)mix8bRampLoop,
	(mixFunc)mix8bRampBidiLoop,
	(mixFunc)mix8bRampNoLoopIntrp,
	(mixFunc)mix8bRampLoopIntrp,
	(mixFunc)mix8bRampBidiLoopIntrp,
	(mixFunc)mix16bRampNoLoop,
	(mixFunc)mix16bRampLoop,
	(mixFunc)mix16bRampBidiLoop,
	(mixFunc)mix16bRampNoLoopIntrp,
	(mixFunc)mix16bRampLoopIntrp,
	(mixFunc)mix16bRampBidiLoopIntrp,

	// center mixing (ft2_center_mix.c)
	(mixFunc)centerMix8bNoLoop,
	(mixFunc)centerMix8bLoop,
	(mixFunc)centerMix8bBidiLoop,
	(mixFunc)centerMix8bNoLoopIntrp,
	(mixFunc)centerMix8bLoopIntrp,
	(mixFunc)centerMix8bBidiLoopIntrp,
	(mixFunc)centerMix16bNoLoop,
	(mixFunc)centerMix16bLoop,
	(mixFunc)centerMix16bBidiLoop,
	(mixFunc)centerMix16bNoLoopIntrp,
	(mixFunc)centerMix16bLoopIntrp,
	(mixFunc)centerMix16bBidiLoopIntrp,
	(mixFunc)centerMix8bRampNoLoop,
	(mixFunc)centerMix8bRampLoop,
	(mixFunc)centerMix8bRampBidiLoop,
	(mixFunc)centerMix8bRampNoLoopIntrp,
	(mixFunc)centerMix8bRampLoopIntrp,
	(mixFunc)centerMix8bRampBidiLoopIntrp,
	(mixFunc)centerMix16bRampNoLoop,
	(mixFunc)centerMix16bRampLoop,
	(mixFunc)centerMix16bRampBidiLoop,
	(mixFunc)centerMix16bRampNoLoopIntrp,
	(mixFunc)centerMix16bRampLoopIntrp,
	(mixFunc)centerMix16bRampBidiLoopIntrp
};
