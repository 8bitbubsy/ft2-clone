/* OGG sample loader (using minivorbis)
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// hide minivorbis compiler warnings
#ifdef _MSC_VER
#pragma warning(disable: 4100)
#pragma warning(disable: 4232)
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#pragma warning(disable: 4456)
#pragma warning(disable: 4457)
#pragma warning(disable: 4459)
#pragma warning(disable: 4706)
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"
#pragma GCC diagnostic pop
#endif

#include "../ft2_header.h"
#include "../ft2_mouse.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

#define SAMPLE_BUFFER_SIZE 524208

bool detectOGG(FILE *f)
{
	uint8_t h[4];
	memset(h, 0, sizeof (h));
	fread(h, 1, 4, f);

	bool result = (memcmp(h, "OggS", 4) == 0);

	rewind(f);
	return result;
}

bool loadOGG(FILE *f, uint32_t filesize)
{
	OggVorbis_File vorbis;
	sample_t *s = &tmpSmp;

	if (ov_open_callbacks(f, &vorbis, NULL, 0, OV_CALLBACKS_DEFAULT) != 0)
	{
		loaderMsgBox("Error loading sample: The sample is empty or corrupt!");
		return false;
	}

	int32_t numChannels = vorbis.vi->channels;
	int32_t sampleRate = vorbis.vi->rate;

	if (numChannels != 1 && numChannels != 2)
	{
		ov_clear(&vorbis);
		loaderMsgBox("Error loading sample: Only mono/stereo OGGs are supported!");
		return false;
	}

	uint8_t *smpBuffer = (uint8_t *)malloc(SAMPLE_BUFFER_SIZE);
	if (smpBuffer == NULL)
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	int16_t stereoAction = -1;
	if (numChannels == 2)
	{
		stereoAction = loaderSysReq(4, "System request", "This is a stereo sample. Which channel do you want to read?", NULL);
		setMouseBusy(true);
	}

	const int32_t shift = (numChannels == 2) ? 2 : 1;
	const int32_t bufferSamples = SAMPLE_BUFFER_SIZE >> shift;
	int32_t sampleCounter = 0, sampleLength = 0;

	int8_t *bufPtr = (int8_t *)smpBuffer;
	while (true)
	{
		int32_t section, numBytes = ov_read(&vorbis, (char *)bufPtr, SAMPLE_BUFFER_SIZE, 0, 2, 1, &section);
		bool endOfSample = (numBytes <= 0);

		sampleCounter += numBytes;
		bufPtr += numBytes;

		if (endOfSample || sampleCounter >= bufferSamples)
		{
			bufPtr = (int8_t *)smpBuffer;

			int32_t numSamples = sampleCounter >> shift;
			if (!reallocateSmpData(s, sampleLength + numSamples, true))
			{
				free(smpBuffer);
				ov_clear(&vorbis);
				loaderMsgBox("Not enough memory!");
				return false;
			}

			int16_t *dst16 = (int16_t *)s->dataPtr + sampleLength;

			if (numChannels == 1)
			{
				memcpy(dst16, smpBuffer, numSamples << shift);
			}
			else
			{
				if (stereoAction == STEREO_SAMPLE_MIX_TO_MONO)
				{
					int16_t *src16L = (int16_t *)smpBuffer + 0, *src16R = (int16_t *)smpBuffer + 1;
					for (int32_t i = 0; i < numSamples; i++, src16L += 2, src16R += 2)
						dst16[i] = (*src16L + *src16R) >> 1;
				}
				else
				{
					const int16_t *src16;
					if (stereoAction == STEREO_SAMPLE_READ_LEFT_CHANNEL)
						src16 = (int16_t *)smpBuffer + 0;
					else
						src16 = (int16_t *)smpBuffer + 1;

					for (int32_t i = 0; i < numSamples; i++, src16 += 2)
						dst16[i] = *src16;
				}
			}

			sampleLength += numSamples;
			sampleCounter = 0;
		}

		if (endOfSample)
			break;
	}

	free(smpBuffer);
	ov_clear(&vorbis);

	s->length = sampleLength;
	s->volume = 64;
	s->panning = 128;
	s->flags |= SAMPLE_16BIT;
	setSampleC4Hz(s, sampleRate);

	return true;

	(void)filesize;
}
