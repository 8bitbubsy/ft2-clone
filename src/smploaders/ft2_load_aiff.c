// Apple AIFF sample loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

static int32_t getAIFFRate(uint8_t *in);
static bool aiffIsStereo(FILE *f); // only ran on files that are confirmed to be AIFFs

bool loadAIFF(FILE *f, uint32_t filesize)
{
	char compType[4];
	int8_t *audioDataS8;
	uint8_t sampleRateBytes[10], *audioDataU8;
	int16_t *audioDataS16, *audioDataS16_2, smp16;
	uint16_t numChannels, bitDepth;
	int32_t smp32, *audioDataS32;
	uint32_t i, blockName, blockSize;
	uint32_t offset, len32;

	fseek(f, 8, SEEK_SET);
	fread(compType, 1, 4, f);
	rewind(f);
	if (!memcmp(compType, "AIFC", 4))
	{
		loaderMsgBox("Error loading sample: This AIFF type (AIFC) is not supported!");
		return false;
	}

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

	if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	// read compression type (if present)
	if (commLen > 18)
	{
		fread(&compType, 1, 4, f);
		if (memcmp(compType, "NONE", 4) != 0)
		{
			loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
			return false;
		}
	}

	uint32_t sampleRate = getAIFFRate(sampleRateBytes);

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
	if (sampleLength > MAX_SAMPLE_LEN)
		sampleLength = MAX_SAMPLE_LEN;

	int16_t stereoSampleLoadMode = -1;
	if (aiffIsStereo(f))
		stereoSampleLoadMode = loaderSysReq(5, "System request", "This is a stereo sample...");

	// read sample data

	if (bitDepth == 8)
	{
		// 8-BIT SIGNED PCM

		if (!allocateTmpSmpData(&tmpSmp, sampleLength))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(tmpSmp.pek, sampleLength, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		audioDataS8 = (int8_t *)tmpSmp.pek;

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
	else if (bitDepth == 16)
	{
		// 16-BIT SIGNED PCM

		sampleLength /= 2;
		if (!allocateTmpSmpData(&tmpSmp, sampleLength*2))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(tmpSmp.pek, sampleLength, 2, f) != 2)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// fix endianness
		audioDataS16 = (int16_t *)tmpSmp.pek;
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
						smp32 = (audioDataS16[(i * 2) + 0] + audioDataS16[(i * 2) + 1]) >> 1;
						audioDataS16[i] = (int16_t)smp32;
					}

					audioDataS16[i] = 0;
				}
				break;
			}
		}

		sampleLength *= 2;
		tmpSmp.typ |= 16;
	}
	else if (bitDepth == 24)
	{
		// 24-BIT SIGNED PCM

		sampleLength /= 3;
		if (!allocateTmpSmpData(&tmpSmp, (sampleLength * 4) * 2))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(&tmpSmp.pek[sampleLength], sampleLength, 3, f) != 3)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		audioDataS32 = (int32_t *)tmpSmp.pek;

		// convert to 32-bit
		audioDataU8 = (uint8_t *)tmpSmp.pek + sampleLength;
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

		// downscale to 16-bit (ultra fast method!)

		audioDataS16 = (int16_t *)tmpSmp.pek;
		audioDataS16_2 = (int16_t *)tmpSmp.pek + 1; // yes, this is aligned to words

		for (i = 0; i < sampleLength; i++, audioDataS16_2++)
			audioDataS16[i] = audioDataS16_2[i];

		sampleLength *= 2;
		tmpSmp.typ |= 16;
	}
	else if (bitDepth == 32)
	{
		// 32-BIT SIGNED PCM

		sampleLength /= 4;
		if (!allocateTmpSmpData(&tmpSmp, sampleLength * 4))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(tmpSmp.pek, sampleLength, 4, f) != 4)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		// fix endianness
		audioDataS32 = (int32_t *)tmpSmp.pek;
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

		// downscale to 16-bit (ultra fast method!)

		audioDataS16 = (int16_t *)tmpSmp.pek;
		audioDataS16_2 = (int16_t *)tmpSmp.pek + 1; // yes, this is aligned to words

		for (i = 0; i < sampleLength; i++, audioDataS16_2++)
			audioDataS16[i] = audioDataS16_2[i];

		sampleLength *= 2;
		tmpSmp.typ |= 16;
	}

	reallocateTmpSmpData(&tmpSmp, sampleLength); // readjust memory needed

	tmpSmp.len = sampleLength;
	tmpSmp.vol = 64;
	tmpSmp.pan = 128;

	tuneSample(&tmpSmp, sampleRate, audio.linearPeriodsFlag);

	return true;
}

static int32_t getAIFFRate(uint8_t *in)
{
	int32_t exp = (int32_t)(((in[0] & 0x7F) << 8) | in[1]);
	uint32_t lo = (in[2] << 24) | (in[3] << 16) | (in[4] << 8) | in[5];
	uint32_t hi = (in[6] << 24) | (in[7] << 16) | (in[8] << 8) | in[9];

	if (exp == 0 && lo == 0 && hi == 0)
		return 0;

	exp -= 16383;

	double dOut = ldexp(lo, -31 + exp) + ldexp(hi, -63 + exp);
	return (int32_t)(dOut + 0.5); // rounded
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
