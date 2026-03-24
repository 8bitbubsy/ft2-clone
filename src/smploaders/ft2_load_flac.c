/* FLAC sample loader (using miniflac)
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

// hide miniflac compiler warnings
#ifdef _MSC_VER
#pragma warning(disable: 4146)
#pragma warning(disable: 4201)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#pragma warning(disable: 4267)
#pragma warning(disable: 4334)
#define MINIFLAC_IMPLEMENTATION
#include "miniflac.h"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch-default"
#define MINIFLAC_IMPLEMENTATION
#include "miniflac.h"
#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_mouse.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

#define MAX_FLAC_BLOCK_SIZE 65535

#define INC_BUFFER \
	bytesLeft -= bytesHandled; \
	bufferPtr += bytesHandled;

static uint8_t numChannels, bitDepth;
static int16_t stereoAction = STEREO_SAMPLE_MIX_TO_MONO;
static sample_t *s;

static bool writeSamples(int64_t sampleIndex, int32_t **samples, int32_t numSamples);

bool detectFLAC(FILE *f)
{
	uint8_t h[4];
	memset(h, 0, sizeof (h));
	fread(h, 1, 4, f);

	bool result = (memcmp(h, "fLaC", 4) == 0);

	rewind(f);
	return result;
}

bool loadFLAC(FILE *f, uint32_t filesize)
{
	int32_t *samples[2] = { NULL };
	uint32_t sampleRate = 44100;
	uint64_t totalSamples = 0;
	miniflac_t decoder;

	s = &tmpSmp;

	uint8_t *fileBuffer = (uint8_t *)malloc(filesize);
	if (fileBuffer == NULL)
		goto oomError;

	samples[0] = (int32_t *)malloc(sizeof (int32_t) * MAX_FLAC_BLOCK_SIZE);
	samples[1] = (int32_t *)malloc(sizeof (int32_t) * MAX_FLAC_BLOCK_SIZE);
	if (samples[0] == NULL || samples[1] == NULL)
		goto oomError;

	if (fread(fileBuffer, 1, filesize, f) != filesize)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		goto errorCleanup;
	}

	uint8_t *bufferPtr = fileBuffer;
	uint32_t bytesHandled, bytesLeft = filesize;

	miniflac_init(&decoder, MINIFLAC_CONTAINER_NATIVE);
	if (miniflac_sync(&decoder, bufferPtr, bytesLeft, &bytesHandled) != MINIFLAC_OK) goto decodeError;
	INC_BUFFER

	s->volume = 64;
	s->panning = 128;

	while (decoder.state == MINIFLAC_METADATA)
	{
		if (decoder.metadata.header.type == MINIFLAC_METADATA_STREAMINFO)
		{
			if (miniflac_streaminfo_sample_rate(&decoder, bufferPtr, bytesLeft, &bytesHandled, &sampleRate) != MINIFLAC_OK) goto decodeError;
			INC_BUFFER
			if (miniflac_streaminfo_channels(&decoder, bufferPtr, bytesLeft, &bytesHandled, &numChannels) != MINIFLAC_OK) goto decodeError;
			INC_BUFFER
			if (miniflac_streaminfo_bps(&decoder, bufferPtr, bytesLeft, &bytesHandled, &bitDepth) != MINIFLAC_OK) goto decodeError;
			INC_BUFFER
			if (miniflac_streaminfo_total_samples(&decoder, bufferPtr, bytesLeft, &bytesHandled, &totalSamples) != MINIFLAC_OK) goto decodeError;
			INC_BUFFER
		}
		else if (decoder.metadata.header.type == MINIFLAC_METADATA_APPLICATION && !memcmp(bufferPtr, "riff", 4))
		{
			const uint8_t *data = bufferPtr + 4;

			uint32_t chunkID  = *(uint32_t *)data; data += 4;
			uint32_t chunkLen = *(uint32_t *)data; data += 4;

			if (chunkID == 0x61727478 && chunkLen >= 8) // "xtra"
			{
				uint32_t xtraFlags = *(uint32_t *)data; data += 4;

				// panning (0..256)
				if (xtraFlags & 0x20) // set panning flag
				{
					uint16_t tmpPan = *(uint16_t *)data;
					if (tmpPan > 255)
						tmpPan = 255;

					s->panning = (uint8_t)tmpPan;
				}
				data += 2;

				// volume (0..256)
				uint16_t tmpVol = *(uint16_t *)data;
				if (tmpVol > 256)
					tmpVol = 256;

				s->volume = (uint8_t)((tmpVol + 2) / 4); // 0..256 -> 0..64 (rounded)
			}

			if (chunkID == 0x6C706D73 && chunkLen > 52) // "smpl"
			{
				data += 28; // seek to first wanted byte

				uint32_t numLoops = *(uint32_t *)data; data += 4;
				if (numLoops == 1)
				{
					data += 4+4; // skip "samplerData" and "identifier"

					uint32_t loopType  = *(uint32_t *)data; data += 4;
					uint32_t loopStart = *(uint32_t *)data; data += 4;
					uint32_t loopEnd   = *(uint32_t *)data; data += 4;

					s->loopStart = loopStart;
					s->loopLength = (loopEnd+1) - loopStart;
					s->flags |= (loopType == 0) ? LOOP_FORWARD : LOOP_PINGPONG;
				}
			}
		}

		if (miniflac_sync_native(&decoder, bufferPtr, bytesLeft, &bytesHandled) != MINIFLAC_OK) break;
		INC_BUFFER
	}

	if (numChannels > 2)
	{
		loaderMsgBox("Error loading sample: Only mono/stereo FLACs are supported!");
		goto errorCleanup;
	}

	if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24)
	{
		loaderMsgBox("Error loading sample: Only FLACs with a bitdepth of 8/16/24 are supported!");
		goto errorCleanup;
	}

	stereoAction = -1;
	if (numChannels == 2)
	{
		stereoAction = loaderSysReq(4, "System request", "This is a stereo sample. Which channel do you want to read?", NULL);
		setMouseBusy(true);
	}

	bool sample16Bit = (bitDepth >= 16);
	if (sample16Bit)
		s->flags |= SAMPLE_16BIT;

	if (totalSamples > MAX_SAMPLE_LEN)
		totalSamples = MAX_SAMPLE_LEN;

	s->length = (uint32_t)totalSamples;
	setSampleC4Hz(s, sampleRate);

	if (!allocateSmpData(s, s->length, sample16Bit))
		goto oomError;

	int64_t sampleIndex = 0;
	while (true)
	{
		if (miniflac_decode(&decoder, bufferPtr, bytesLeft, &bytesHandled, samples) != MINIFLAC_OK) break;
		INC_BUFFER

		const int32_t numSamples = decoder.frame.header.block_size;
		if (!writeSamples(sampleIndex, samples, numSamples)) break;
		sampleIndex += numSamples;

		if (miniflac_sync_native(&decoder, bufferPtr, bytesLeft, &bytesHandled) != MINIFLAC_OK) break;
		INC_BUFFER
	}

	if (fileBuffer != NULL) free(fileBuffer);
	if (samples[0] != NULL) free(samples[0]);
	if (samples[1] != NULL) free(samples[1]);

	return true;

	(void)filesize;

oomError:
	loaderMsgBox("Not enough memory!");
	goto errorCleanup;

decodeError:
	loaderMsgBox("Error loading sample: The sample is empty or corrupt!");
errorCleanup:
	if (fileBuffer != NULL) free(fileBuffer);
	if (samples[0] != NULL) free(samples[0]);
	if (samples[1] != NULL) free(samples[1]);

	return false;
}

static bool writeSamples(int64_t sampleIndex, int32_t **samples, int32_t numSamples)
{
	if (sampleIndex >= s->length)
		return false;

	int32_t samplesTodo = numSamples;
	if (sampleIndex+samplesTodo > s->length)
		samplesTodo = s->length - (int32_t)sampleIndex;

	if (numChannels == 1)
	{
		int32_t *src32 = samples[0];

		if (bitDepth == 8)
		{
			int8_t *ptr8 = (int8_t *)s->dataPtr + sampleIndex;
			for (int32_t i = 0; i < samplesTodo; i++)
				ptr8[i] = (int8_t)src32[i];
		}
		else
		{
			int16_t *ptr16 = (int16_t *)s->dataPtr + sampleIndex;
			const int8_t shift = bitDepth - 16;

			for (int32_t i = 0; i < samplesTodo; i++)
				ptr16[i] = (int16_t)(src32[i] >> shift);
		}
	}
	else
	{
		if (stereoAction == STEREO_SAMPLE_MIX_TO_MONO)
		{
			int32_t *src32L = samples[0], *src32R = samples[1];

			if (bitDepth == 8)
			{
				int8_t *ptr8 = (int8_t *)s->dataPtr + sampleIndex;
				for (int32_t i = 0; i < samplesTodo; i++)
					ptr8[i] = (int8_t)((src32L[i] + src32R[i]) >> 1);
			}
			else
			{
				int16_t *ptr16 = (int16_t *)s->dataPtr + sampleIndex;
				const int8_t shift = (bitDepth - 16) + 1;

				for (int32_t i = 0; i < samplesTodo; i++)
					ptr16[i] = (int16_t)((src32L[i] + src32R[i]) >> shift);
			}
		}
		else
		{
			const int32_t *src32 = (stereoAction == STEREO_SAMPLE_READ_RIGHT_CHANNEL) ? samples[1] : samples[0];

			if (bitDepth == 8)
			{
				int8_t *ptr8 = (int8_t *)s->dataPtr + sampleIndex;
				for (int32_t i = 0; i < samplesTodo; i++)
					ptr8[i] = (int8_t)src32[i];
			}
			else
			{
				int16_t *ptr16 = (int16_t *)s->dataPtr + sampleIndex;
				const int8_t shift = bitDepth - 16;

				for (int32_t i = 0; i < samplesTodo; i++)
					ptr16[i] = (int16_t)(src32[i] >> shift);
			}
		}
	}

	return true;
}
