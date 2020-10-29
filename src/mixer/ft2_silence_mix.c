#include <stdint.h>
#include "../ft2_audio.h"

// used for the audio channel mixer when voice volume is zero

void silenceMixRoutine(voice_t *v, int32_t numSamples)
{
	const uint64_t newPos = v->delta * (uint64_t)numSamples;
	const uint32_t addPos = (uint32_t)(newPos >> MIXER_FRAC_BITS);
	uint64_t addFrac = newPos & MIXER_FRAC_MASK;

	addFrac += v->posFrac;
	int32_t pos = v->pos + addPos + (uint32_t)(addFrac >> MIXER_FRAC_BITS);
	uint64_t  posFrac = addFrac & MIXER_FRAC_MASK;

	if (pos < v->end) // we haven't reached the sample's end yet
	{
		v->posFrac = posFrac;
		v->pos = pos;
		return;
	}

	// end of sample (or loop) reached

	if (v->loopType == LOOP_DISABLED)
	{
		v->active = false; // shut down voice
		return;
	}

	if (v->loopType == LOOP_FORWARD)
	{
		if (v->loopLength >= 2)
			pos = v->loopStart + ((pos - v->end) % v->loopLength);
		else
			pos = v->loopStart;
	}
	else // pingpong loop
	{
		if (v->loopLength >= 2)
		{
			const int32_t overflow = pos - v->end;
			const int32_t cycles = overflow / v->loopLength;
			const int32_t phase = overflow % v->loopLength;

			pos = v->loopStart + phase;
			v->backwards ^= !(cycles & 1);
		}
		else
		{
			pos = v->loopStart;
		}
	}

	v->hasLooped = true;
	v->posFrac = posFrac;
	v->pos = pos;
}
