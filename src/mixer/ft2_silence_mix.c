#include <assert.h>
#include <stdint.h>
#include "../ft2_audio.h"

// used for the audio channel mixer when voice volume is zero

void silenceMixRoutine(voice_t *v, int32_t numSamples)
{
	assert((uint32_t)numSamples <= UINT16_MAX);

	const uint32_t samplesInt = (uint32_t)(v->delta >> MIXER_FRAC_BITS) * (uint32_t)numSamples;
	const uint64_t samplesFrac = (uint64_t)(v->delta & MIXER_FRAC_SCALE) * (uint32_t)numSamples;

	uint32_t position = v->position + samplesInt + (uint32_t)(samplesFrac >> MIXER_FRAC_BITS);
	uint32_t positionFrac = samplesFrac & MIXER_FRAC_MASK;

	if (position < (uint32_t)v->sampleEnd) // we haven't reached the sample's end yet
	{
		v->positionFrac = positionFrac;
		v->position = position;
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
			position = v->loopStart + ((position - v->sampleEnd) % v->loopLength);
		else
			position = v->loopStart;
	}
	else // pingpong loop
	{
		if (v->loopLength >= 2)
		{
			// wrap as forward loop (position is inverted if sampling backwards, when needed)

			const uint32_t overflow = position - v->sampleEnd;
			const uint32_t cycles = overflow / v->loopLength;
			const uint32_t phase = overflow % v->loopLength;

			position = v->loopStart + phase;
			v->samplingBackwards ^= !(cycles & 1);
		}
		else
		{
			position = v->loopStart;
		}
	}

	v->hasLooped = true;
	v->positionFrac = positionFrac;
	v->position = position;
}
