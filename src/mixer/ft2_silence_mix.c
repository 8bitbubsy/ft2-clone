#include <stdint.h>
#include "../ft2_audio.h"
#include "ft2_silence_mix.h"

void silenceMixRoutine(voice_t *v, int32_t numSamples)
{
	int32_t realPos;
#if defined _WIN64 || defined __amd64__
	uint64_t pos;
#else
	uint32_t pos;
#endif

	SILENCE_MIX_INC_POS

	if (v->SLoopType == 0)
	{
		SILENCE_MIX_NO_LOOP
	}
	else if (v->SLoopType == 1)
	{
		SILENCE_MIX_LOOP
	}
	else
	{
		SILENCE_MIX_BIDI_LOOP
	}

	v->SPosDec = pos;
	v->SPos = realPos;
}
