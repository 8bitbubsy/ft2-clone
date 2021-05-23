#include <stdint.h>
#include "../ft2_audio.h"
#include "../ft2_cpu.h"

// used for the audio channel mixer when voice volume is zero

void silenceMixRoutine(voice_t *v, int32_t numSamples)
{
	const uint64_t samplesToMix = (uint64_t)v->delta * (uint32_t)numSamples; // fixed-point

	const uint32_t samples = (uint32_t)(samplesToMix >> MIXER_FRAC_BITS);
	const uintCPUWord_t samplesFrac = (samplesToMix & MIXER_FRAC_MASK) + v->positionFrac;

	uint32_t position = v->position + samples + (uint32_t)(samplesFrac >> MIXER_FRAC_BITS);
	uintCPUWord_t positionFrac = samplesFrac & MIXER_FRAC_MASK;

	if (position < (unsigned)v->sampleEnd) // we haven't reached the sample's end yet
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
