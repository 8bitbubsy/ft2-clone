#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_mix.h"
#include "ft2_mix_macros.h"

/*
   --------------------- fixed-point audio channel mixer ---------------------

   This file has separate routines for EVERY possible sampling variation:
   Interpolation, volume ramping, 8-bit, 16-bit, no loop, loop, bidi loop.
   24 mixing routines in total.

   Every voice has a function pointer set to the according mixing routine on
   sample trigger (from replayer, but set in audio thread), using a function
   pointer look-up table.
   All voices are always cleared (thread safe) when changing any of the above
   states from the GUI, so no problem there with deprecated cached function
   pointers.

   Mixing macros can be found in ft2_mix_macros.h.

   Yes, this is a HUGE mess, and I hope you don't need to modify it.
   If it's not broken, don't try to fix it!
*/

/* ----------------------------------------------------------------------- */
/*                          8-BIT MIXING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void mix8bNoLoop(voice_t *v, uint32_t numSamples)
{
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;

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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;

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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	const int8_t *CDA_LinearAdr;
	bool mixInMono;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int8_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int8_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;

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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	GET_VOL

	if ((CDA_LVol | CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	GET_VOL

	if ((CDA_LVol| CDA_RVol) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;

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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t  CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_NO_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol;
	register uint32_t pos, delta;
	uint32_t i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

	if ((v->SLVol1 | v->SRVol1 | v->SLVol2 | v->SRVol2) == 0)
	{
		VOL0_OPTIMIZATION_LOOP
		return;
	}

	GET_MIXER_VARS_RAMP
	GET_DELTA
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
	bool mixInMono;
	const int16_t *CDA_LinearAdr, *CDA_LinAdrRev;
	int32_t realPos, sample, sample2, *audioMixL, *audioMixR, CDA_BytesLeft, CDA_LVolIP, CDA_RVolIP;
	register const int16_t *smpPtr;
	register int32_t CDA_LVol, CDA_RVol, CDA_IPValL, CDA_IPValH;
	register uint32_t pos;
	uint32_t delta, i, samplesToMix;
#ifndef LERPMIX
	int32_t sample3;
#endif

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
