#include <stdint.h>
#include "../ft2_audio.h"
#include "ft2_silence_mix.h"

void silenceMixRoutine(voice_t *v, int32_t numSamples)
{
	int32_t pos;
	uint64_t posFrac;

	SILENCE_MIX_INC_POS

	if (v->loopType == LOOP_DISABLED)
	{
		SILENCE_MIX_NO_LOOP
	}
	else if (v->loopType == LOOP_FORWARD)
	{
		SILENCE_MIX_LOOP
	}
	else // pingpong loop
	{
		SILENCE_MIX_BIDI_LOOP
	}

	v->posFrac = posFrac;
	v->pos = pos;
}
