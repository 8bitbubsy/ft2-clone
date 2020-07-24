#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix_macros.h"
#include "ft2_intrp_table.h"

/* Check out ft2_mix.c for comments on how this works.
** These are duplicates for center-mixing (slightly faster when it can be used).
*/

/* ----------------------------------------------------------------------- */
/*                      8-BIT CENTER MIXING ROUTINES                       */
/* ----------------------------------------------------------------------- */

void centerMix8bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int8_t *CDA_LinearAdr, *smpPtr;;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
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

void centerMix8bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix8bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix8bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
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

void centerMix8bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix8bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_8BIT_SMP_MONO_INTRP
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
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
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

void centerMix16bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

void centerMix16bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO
	GET_MIXER_VARS
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

void centerMix16bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;
	
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
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
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
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

void centerMix16bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		HANDLE_SAMPLE_END
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS
		}

		WRAP_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}

void centerMix16bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_LVol;
	uint32_t i, samplesToMix, CDA_BytesLeft;
#if defined _WIN64 || defined __amd64__
	uint64_t pos, delta;
#else
	uint32_t pos, delta;
#endif

	GET_VOL_MONO_RAMP
	GET_MIXER_VARS_MONO_RAMP
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_MONO_RAMP
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		if (samplesToMix & 1)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		samplesToMix >>= 1;
		for (i = 0; i < samplesToMix; i++)
		{
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
			RENDER_16BIT_SMP_MONO_INTRP
			VOLUME_RAMPING_MONO
			INC_POS_BIDI
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_VOL_BACK_MONO
	SET_BACK_MIXER_POS
}
