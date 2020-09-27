#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix_macros.h"

/* Check out ft2_mix.c for comments on how this works.
** These are duplicates for center-mixing (slightly faster when it can be used).
*/

/* ----------------------------------------------------------------------- */
/*                      8-BIT CENTER MIXING ROUTINES                       */
/* ----------------------------------------------------------------------- */

void centerMix8bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix8bLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
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
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bNoLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_CINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix8bLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bBidiLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8_BIDI
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
			}
		}

		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bNoLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix8bLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bBidiLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
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
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampNoLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampBidiLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8_BIDI
	PREPARE_TAP_FIX8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampNoLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampBidiLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

/* ----------------------------------------------------------------------- */
/*                      16-BIT CENTER MIXING ROUTINES                      */
/* ----------------------------------------------------------------------- */

void centerMix16bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix16bLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
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
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bNoLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_CINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix16bLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bBidiLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16_BIDI
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bNoLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix16bLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bBidiLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO
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
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}


void centerMix16bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;
	
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampNoLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_CINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampBidiLoopCIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float s0, s1, s2, s3, fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16_BIDI
	PREPARE_TAP_FIX16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		if (v->hasLooped) // the interpolation's -1 tap needs a special case from now on
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP_TAP_FIX
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
		}
		else
		{
			for (i = 0; i < (samplesToMix & 3); i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
			samplesToMix >>= 2;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_CINTRP
				VOLUME_RAMPING_MONO
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampNoLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampBidiLoopLIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *base, *revBase, *smpPtr;
	float fSample, *fMixBufferL, *fMixBufferR;
	int32_t pos;
	float fVolLDelta, fVolL;
	uint32_t i, samplesToMix, samplesLeft;
	uint64_t posFrac, tmpDelta;

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16_BIDI

	samplesLeft = numSamples;
	while (samplesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		samplesLeft -= samplesToMix;

		START_BIDI
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_LINTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}
