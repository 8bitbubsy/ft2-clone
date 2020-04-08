#include <stdint.h>
#include <stdbool.h>
#include "ft2_mix.h"
#include "ft2_mix_macros.h"
#include "ft2_intrp_table.h"

/*
** --------------------- 32-bit fixed-point audio channel mixer ---------------------
**              (Note: Mixing macros can be found in ft2_mix_macros.h)
**
** 8bitbubsy: This is mostly ported from the i386-asm 32-bit mixer that was introduced in
** FT2.08 (MS-DOS). It has been changed and improved quite a bit, though...
**
** This file has separate routines for EVERY possible sampling variation:
** Interpolation on/off, volume ramping on/off, 8-bit, 16-bit, no loop, loop, pingpong.
** (24 mixing routines in total)
**
** Every voice has a function pointer set to the according mixing routine on sample
** trigger (from replayer, but set in audio thread), using a function pointer look-up
** table. All voices & pointers are always thread-safely cleared when changing any
** of the above attributes from the GUI, to prevent possible thread-related issues.
**
** There's one problem with the 4-tap cubic spline resampling interpolation...
** On looped samples where loopStart>0, the splines are not correct when reading
** from the loopStart (or +1?) sample point. The difference in audio is very minor,
** so it's not a big problem. It just has to stay like this the way the mixer works.
** In cases where loopStart=0, the sample before index 0 (yes, we allocate enough
** data and pre-increment main pointer to support negative look-up), is already
** pre-fixed so that the splines will be correct.
** ----------------------------------------------------------------------------------
*/

/* ----------------------------------------------------------------------- */
/*                          8-BIT MIXING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void mix8bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				INC_POS
				RENDER_8BIT_SMP
				INC_POS
			}
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				INC_POS
				RENDER_8BIT_SMP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				INC_POS_BIDI
				RENDER_8BIT_SMP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}
	SET_BACK_MIXER_POS
}

static void mix8bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS
				RENDER_8BIT_SMP_INTRP
				INC_POS
			}
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS
				RENDER_8BIT_SMP_INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				INC_POS_BIDI
				RENDER_8BIT_SMP_INTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI
		SET_VOL_BACK

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE8_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_8BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI
		SET_VOL_BACK

		WRAP_BIDI_LOOP
	}
	SET_BACK_MIXER_POS
}



/* ----------------------------------------------------------------------- */
/*                          16-BIT MIXING ROUTINES                         */
/* ----------------------------------------------------------------------- */

static void mix16bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				INC_POS
				RENDER_16BIT_SMP
				INC_POS
			}
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				INC_POS
				RENDER_16BIT_SMP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				INC_POS_BIDI
				RENDER_16BIT_SMP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS
				RENDER_16BIT_SMP_INTRP
				INC_POS
			}
		}

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol| CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS
				RENDER_16BIT_SMP_INTRP
				INC_POS
			}
		}

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
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
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				INC_POS_BIDI
				RENDER_16BIT_SMP_INTRP
				INC_POS_BIDI
			}
		}
		END_BIDI

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoop(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI
		SET_VOL_BACK

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampNoLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		HANDLE_SAMPLE_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS
			}
		}
		SET_VOL_BACK

		WRAP_LOOP
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples)
{
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev, *smpPtr;
	int32_t realPos, sample, sample2, sample3, sample4, *audioMixL, *audioMixR;
	int32_t CDA_LVolIP, CDA_RVolIP, CDA_LVol, CDA_RVol;
	uint32_t pos, delta, i, samplesToMix, CDA_BytesLeft;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_BIDI_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	SET_BASE16_BIDI

	CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL
		START_BIDI
		if (mixInMono)
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_MONO_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		else
		{
			if (samplesToMix & 1)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
			samplesToMix >>= 1;
			for (i = 0; i < samplesToMix; i++)
			{
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
				RENDER_16BIT_SMP_INTRP
				VOLUME_RAMPING
				INC_POS_BIDI
			}
		}
		END_BIDI
		SET_VOL_BACK

		WRAP_BIDI_LOOP
	}

	SET_BACK_MIXER_POS
}

// -----------------------------------------------------------------------

const mixRoutine mixRoutineTable[24] =
{
	(mixRoutine)mix8bNoLoop,
	(mixRoutine)mix8bLoop,
	(mixRoutine)mix8bBidiLoop,
	(mixRoutine)mix8bNoLoopIntrp,
	(mixRoutine)mix8bLoopIntrp,
	(mixRoutine)mix8bBidiLoopIntrp,
	(mixRoutine)mix8bRampNoLoop,
	(mixRoutine)mix8bRampLoop,
	(mixRoutine)mix8bRampBidiLoop,
	(mixRoutine)mix8bRampNoLoopIntrp,
	(mixRoutine)mix8bRampLoopIntrp,
	(mixRoutine)mix8bRampBidiLoopIntrp,
	(mixRoutine)mix16bNoLoop,
	(mixRoutine)mix16bLoop,
	(mixRoutine)mix16bBidiLoop,
	(mixRoutine)mix16bNoLoopIntrp,
	(mixRoutine)mix16bLoopIntrp,
	(mixRoutine)mix16bBidiLoopIntrp,
	(mixRoutine)mix16bRampNoLoop,
	(mixRoutine)mix16bRampLoop,
	(mixRoutine)mix16bRampBidiLoop,
	(mixRoutine)mix16bRampNoLoopIntrp,
	(mixRoutine)mix16bRampLoopIntrp,
	(mixRoutine)mix16bRampBidiLoopIntrp
};
