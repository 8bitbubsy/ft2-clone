/* Apple AIFF sample loader
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

static uint32_t getAIFFSampleRate(uint8_t *in);
static bool aiffIsStereo(FILE *f); // only ran on files that are confirmed to be AIFFs

bool loadAIFF(FILE *f, uint32_t filesize)
{
	char compType[4];
	int8_t *audioDataS8;
	uint8_t sampleRateBytes[10], *audioDataU8;
	int16_t *audioDataS16, smp16;
	uint16_t numChannels, bitDepth;
	int32_t *audioDataS32;
	uint32_t i, blockName, blockSize;
	uint32_t offset, len32;
	sample_t *s = &tmpSmp;

	fseek(f, 8, SEEK_SET);
	fread(compType, 1, 4, f);
	rewind(f);

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	uint32_t commPtr = 0, commLen = 0;
	uint32_t ssndPtr = 0, ssndLen = 0;

	fseek(f, 12, SEEK_SET);
	while (!feof(f) && (uint32_t)ftell(f) < filesize-12)
	{
		fread(&blockName, 4, 1, f); if (feof(f)) break;
		fread(&blockSize, 4, 1, f); if (feof(f)) break;

		blockName = SWAP32(blockName);
		blockSize = SWAP32(blockSize);

		switch (blockName)
		{
			case 0x434F4D4D: // "COMM"
			{
				commPtr = ftell(f);
				commLen = blockSize;
			}
			break;

			case 0x53534E44: // "SSND"
			{
				ssndPtr = ftell(f);
				ssndLen = blockSize;
			}
			break;

			default: break;
		}

		fseek(f, blockSize + (blockSize & 1), SEEK_CUR);
	}

	if (commPtr == 0 || commLen < 18 || ssndPtr == 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	// kludge for strange AIFFs
	if (ssndLen == 0)
		ssndLen = filesize - ssndPtr;

	if (ssndPtr+ssndLen > (uint32_t)filesize)
		ssndLen = filesize - ssndPtr;

	fseek(f, commPtr, SEEK_SET);
	fread(&numChannels, 2, 1, f); numChannels = SWAP16(numChannels);
	fseek(f, 4, SEEK_CUR);
	fread(&bitDepth, 2, 1, f); bitDepth = SWAP16(bitDepth);
	fread(sampleRateBytes, 1, 10, f);

	if (numChannels != 1 && numChannels != 2)
	{
		loaderMsgBox("Error loading sample: Unsupported amounts of channels!");
		return false;
	}

	// read compression type (if present)
	bool signedSample = true;
	bool floatSample = false;
	if (commLen > 18)
	{
		fread(&compType, 1, 4, f);

		if (!memcmp(compType, "raw ", 4))
		{
			signedSample = false;
		}
		else if (!memcmp(compType, "FL32", 4) || !memcmp(compType, "fl32", 4) || !memcmp(compType, "FL64", 4) || !memcmp(compType, "fl64", 4))
		{
			floatSample = true;
		}
		else
		{
			loaderMsgBox("Error loading sample: Unsupported AIFF type!");
			return false;
		}
	}

	if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32 &&
		!(floatSample && bitDepth == 64) && !(floatSample && bitDepth == 32))
	{
		loaderMsgBox("Error loading sample: Unsupported AIFF type!");
		return false;
	}

	uint32_t sampleRate = getAIFFSampleRate(sampleRateBytes);

	// sample data chunk

	fseek(f, ssndPtr, SEEK_SET);

	fread(&offset, 4, 1, f);
	if (offset > 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	fseek(f, 4, SEEK_CUR);

	ssndLen -= 8; // don't include offset and blockSize datas

	uint32_t sampleLength = ssndLen;

	int16_t stereoSampleLoadMode = -1;
	if (aiffIsStereo(f))
		stereoSampleLoadMode = loaderSysReq(5, "System request", "This is a stereo sample...");

	// read sample data

	if (!floatSample && bitDepth == 8) // 8-BIT INTEGER SAMPLE
	{
		if (!allocateSmpData(s, sampleLength, false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		audioDataS8 = (int8_t *)s->dataPtr;
		if (!signedSample)
		{
			for (i = 0; i < sampleLength; i++)
				audioDataS8[i] ^= 0x80;
		}

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;

			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					for (i = 1; i < sampleLength; i++)
						audioDataS8[i] = audioDataS8[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS8[i] = audioDataS8[(i * 2) + 1];

					audioDataS8[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
					{
						smp16 = (audioDataS8[(i * 2) + 0] + audioDataS8[(i * 2) + 1]) >> 1;
						audioDataS8[i] = (int8_t)smp16;
					}

					audioDataS8[i] = 0;
				}
				break;
			}
		}
	}
	else if (!floatSample && bitDepth == 16) // 16-BIT INTEGER SAMPLE
	{
		sampleLength /= sizeof (int16_t);
		if (!allocateSmpData(s, sampleLength * sizeof (int16_t), false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, sizeof (int16_t), f) != sizeof (int16_t))
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// change endianness
		audioDataS16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS16[i] = SWAP16(audioDataS16[i]);

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;

			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					for (i = 1; i < sampleLength; i++)
						audioDataS16[i] = audioDataS16[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS16[i] = audioDataS16[(i * 2) + 1];

					audioDataS16[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
					{
						int32_t smp32 = (audioDataS16[(i * 2) + 0] + audioDataS16[(i * 2) + 1]) >> 1;
						audioDataS16[i] = (int16_t)smp32;
					}

					audioDataS16[i] = 0;
				}
				break;
			}
		}

		s->flags |= SAMPLE_16BIT;
	}
	else if (!floatSample && bitDepth == 24) // 24-BIT INTEGER SAMPLE
	{
		sampleLength /= 3;
		if (!allocateSmpData(s, sampleLength * sizeof (int32_t), false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(&s->dataPtr[sampleLength], sampleLength, 3, f) != 3)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		audioDataS32 = (int32_t *)s->dataPtr;

		// convert to 32-bit
		audioDataU8 = (uint8_t *)s->dataPtr + sampleLength;
		for (i = 0; i < sampleLength; i++)
		{
			audioDataS32[i] = (audioDataU8[0] << 24) | (audioDataU8[1] << 16) | (audioDataU8[2] << 8);
			audioDataU8 += 3;
		}

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;

			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					for (i = 1; i < sampleLength; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 1];

					audioDataS32[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
					{
						int64_t smp64 = audioDataS32[(i * 2) + 0];
						smp64 += audioDataS32[(i * 2) + 1];
						smp64 >>= 1;

						audioDataS32[i] = (int32_t)smp64;
					}

					audioDataS32[i] = 0;
				}
				break;
			}
		}

		normalizeSigned32Bit(audioDataS32, sampleLength);

		audioDataS16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS16[i] = audioDataS32[i] >> 16;

		s->flags |= SAMPLE_16BIT;
	}
	else if (!floatSample && bitDepth == 32) // 32-BIT INTEGER SAMPLE
	{
		sampleLength /= sizeof (int32_t);
		if (!allocateSmpData(s, sampleLength * sizeof (int32_t), false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, sizeof (int32_t), f) != sizeof (int32_t))
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// change endianness
		audioDataS32 = (int32_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS32[i] = SWAP32(audioDataS32[i]);

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;

			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					for (i = 1; i < sampleLength; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 1];

					audioDataS32[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
					{
						int64_t smp64 = audioDataS32[(i * 2) + 0];
						smp64 += audioDataS32[(i * 2) + 1];
						smp64 >>= 1;

						audioDataS32[i] = (int32_t)smp64;
					}

					audioDataS32[i] = 0;
				}
				break;
			}
		}

		normalizeSigned32Bit(audioDataS32, sampleLength);

		audioDataS16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS16[i] = audioDataS32[i] >> 16;

		s->flags |= SAMPLE_16BIT;
	}
	else if (floatSample && bitDepth == 32) // 32-BIT FLOAT SAMPLE
	{
		sampleLength /= sizeof (float);
		if (!allocateSmpData(s, sampleLength * sizeof (float), false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, sizeof (float), f) != sizeof (float))
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// change endianness
		audioDataS32 = (int32_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS32[i] = SWAP32(audioDataS32[i]);

		float *fAudioDataFloat = (float *)s->dataPtr;

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;
			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					// remove right channel data
					for (i = 1; i < sampleLength; i++)
						fAudioDataFloat[i] = fAudioDataFloat[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						fAudioDataFloat[i] = fAudioDataFloat[(i * 2) + 1];

					fAudioDataFloat[i] = 0.0f;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						fAudioDataFloat[i] = (fAudioDataFloat[(i * 2) + 0] + fAudioDataFloat[(i * 2) + 1]) * 0.5f;

					fAudioDataFloat[i] = 0.0f;
				}
				break;
			}
		}

		normalize32BitFloatToSigned16Bit(fAudioDataFloat, sampleLength);

		int16_t *ptr16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
		{
			const int32_t smp32 = (const int32_t)fAudioDataFloat[i];
			ptr16[i] = (int16_t)smp32;
		}

		s->flags |= SAMPLE_16BIT;
	}
	else if (floatSample && bitDepth == 64) // 64-BIT FLOAT SAMPLE
	{
		sampleLength /= sizeof (double);
		if (!allocateSmpData(s, sampleLength * sizeof (double), false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, sizeof (double), f) != sizeof (double))
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// change endianness
		int64_t *audioDataS64 = (int64_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			audioDataS64[i] = SWAP64(audioDataS64[i]);

		double *dAudioDataDouble = (double *)s->dataPtr;

		// stereo conversion
		if (numChannels == 2)
		{
			sampleLength /= 2;
			switch (stereoSampleLoadMode)
			{
				case STEREO_SAMPLE_READ_LEFT:
				{
					// remove right channel data
					for (i = 1; i < sampleLength; i++)
						dAudioDataDouble[i] = dAudioDataDouble[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						dAudioDataDouble[i] = dAudioDataDouble[(i * 2) + 1];

					dAudioDataDouble[i] = 0.0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						dAudioDataDouble[i] = (dAudioDataDouble[(i * 2) + 0] + dAudioDataDouble[(i * 2) + 1]) * 0.5;

					dAudioDataDouble[i] = 0.0;
				}
				break;
			}
		}

		normalize64BitFloatToSigned16Bit(dAudioDataDouble, sampleLength);

		int16_t *ptr16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
		{
			const int32_t smp32 = (const int32_t)dAudioDataDouble[i];
			ptr16[i] = (int16_t)smp32;
		}

		s->flags |= SAMPLE_16BIT;
	}

	if (sampleLength > MAX_SAMPLE_LEN)
		sampleLength = MAX_SAMPLE_LEN;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	reallocateSmpData(s, sampleLength, sample16Bit); // readjust memory needed

	s->length = sampleLength;
	s->volume = 64;
	s->panning = 128;

	tuneSample(s, sampleRate, audio.linearPeriodsFlag);

	return true;
}

static uint32_t getAIFFSampleRate(uint8_t *in)
{
	/* 80-bit IEEE-754 to unsigned 32-bit integer (rounded).
	** Sign bit is ignored.
	*/

#define EXP_BIAS 16383

	const uint16_t exp15 = ((in[0] & 0x7F) << 8) | in[1];
	const uint64_t mantissaBits = *(uint64_t *)&in[2];
	const uint64_t mantissa63 = SWAP64(mantissaBits) & INT64_MAX;

	double dExp = exp15 - EXP_BIAS;
	double dMantissa = mantissa63 / (INT64_MAX+1.0);

	double dResult = (1.0 + dMantissa) * exp2(dExp);
	return (uint32_t)round(dResult);
}

static bool aiffIsStereo(FILE *f) // only ran on files that are confirmed to be AIFFs
{
	uint16_t numChannels;
	uint32_t chunkID, chunkSize;

	uint32_t oldPos = ftell(f);

	fseek(f, 0, SEEK_END);
	int32_t filesize = ftell(f);

	if (filesize < 12)
	{
		fseek(f, oldPos, SEEK_SET);
		return false;
	}

	fseek(f, 12, SEEK_SET);

	uint32_t commPtr = 0;
	uint32_t commLen = 0;

	int32_t bytesRead = 0;
	while (!feof(f) && bytesRead < filesize-12)
	{
		fread(&chunkID, 4, 1, f); chunkID = SWAP32(chunkID); if (feof(f)) break;
		fread(&chunkSize, 4, 1, f); chunkSize = SWAP32(chunkSize); if (feof(f)) break;

		int32_t endOfChunk = (ftell(f) + chunkSize) + (chunkSize & 1);
		switch (chunkID)
		{
			case 0x434F4D4D: // "COMM"
			{
				commPtr = ftell(f);
				commLen = chunkSize;
			}
			break;

			default: break;
		}

		bytesRead += (chunkSize + (chunkSize & 1));
		fseek(f, endOfChunk, SEEK_SET);
	}

	if (commPtr == 0 || commLen < 2)
	{
		fseek(f, oldPos, SEEK_SET);
		return false;
	}

	fseek(f, commPtr, SEEK_SET);
	fread(&numChannels, 2, 1, f); numChannels = SWAP16(numChannels);
	fseek(f, oldPos, SEEK_SET);

	return (numChannels == 2);
}
