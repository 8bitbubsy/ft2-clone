/* WAV sample loader
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

enum
{
	WAV_FORMAT_PCM = 1,
	WAV_FORMAT_IEEE_FLOAT = 3
};

static bool wavIsStereo(FILE *f);

bool loadWAV(FILE *f, uint32_t filesize)
{
	uint8_t *audioDataU8;
	int16_t *audioDataS16, *ptr16;
	uint16_t audioFormat, numChannels, bitsPerSample;
	int32_t *audioDataS32;
	uint32_t i, sampleRate, sampleLength;
	uint32_t len32;
	float *fAudioDataFloat;
	double *dAudioDataDouble;
	sample_t *s = &tmpSmp;

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	uint32_t fmtPtr  = 0, fmtLen  = 0;
	uint32_t dataPtr = 0, dataLen = 0;
	uint32_t inamPtr = 0, inamLen = 0;
	uint32_t xtraPtr = 0, xtraLen = 0;
	uint32_t smplPtr = 0, smplLen = 0;

	// look for wanted chunks and set up pointers + lengths
	fseek(f, 12, SEEK_SET);

	uint32_t bytesRead = 0;
	while (!feof(f) && bytesRead < filesize-12)
	{
		uint32_t chunkID, chunkSize;
		fread(&chunkID, 4, 1, f); if (feof(f)) break;
		fread(&chunkSize, 4, 1, f); if (feof(f)) break;

		uint32_t endOfChunk = (ftell(f) + chunkSize) + (chunkSize & 1);
		switch (chunkID)
		{
			case 0x20746D66: // "fmt "
			{
				fmtPtr = ftell(f);
				fmtLen = chunkSize;
			}
			break;

			case 0x61746164: // "data"
			{
				dataPtr = ftell(f);
				dataLen = chunkSize;
			}
			break;

			case 0x5453494C: // "LIST"
			{
				if (chunkSize >= 4)
				{
					fread(&chunkID, 4, 1, f);
					if (chunkID == 0x4F464E49) // "INFO"
					{
						bytesRead = 0;
						while (!feof(f) && bytesRead < chunkSize)
						{
							fread(&chunkID, 4, 1, f);
							fread(&chunkSize, 4, 1, f);

							switch (chunkID)
							{
								case 0x4D414E49: // "INAM"
								{
									inamPtr = ftell(f);
									inamLen = chunkSize;
								}
								break;

								default: break;
							}

							bytesRead += (chunkSize + (chunkSize & 1));
						}
					}
				}
			}
			break;

			case 0x61727478: // "xtra"
			{
				xtraPtr = ftell(f);
				xtraLen = chunkSize;
			}
			break;

			case 0x6C706D73: // "smpl"
			{
				smplPtr = ftell(f);
				smplLen = chunkSize;
			}
			break;

			default: break;
		}

		bytesRead += (chunkSize + (chunkSize & 1));
		fseek(f, endOfChunk, SEEK_SET);
	}

	// we need at least "fmt " and "data" - check if we found them sanely
	if (fmtPtr == 0 || fmtLen < 16 || dataPtr == 0 || dataLen == 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	// ---- READ "fmt " CHUNK ----
	fseek(f, fmtPtr, SEEK_SET);
	fread(&audioFormat, 2, 1, f);
	fread(&numChannels, 2, 1, f);
	fread(&sampleRate,  4, 1, f);
	fseek(f, 6, SEEK_CUR); // unneeded
	fread(&bitsPerSample, 2, 1, f);
	sampleLength = dataLen;
	// ---------------------------

	// test if the WAV is compatible with our loader

	if (sampleRate == 0 || sampleLength == 0 || sampleLength >= filesize)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	if (audioFormat != WAV_FORMAT_PCM && audioFormat != WAV_FORMAT_IEEE_FLOAT)
	{
		loaderMsgBox("Error loading sample: The sample is not supported!");
		return false;
	}

	if (numChannels == 0 || numChannels > 2)
	{
		loaderMsgBox("Error loading sample: Unsupported number of channels!");
		return false;
	}

	if (audioFormat == WAV_FORMAT_IEEE_FLOAT && bitsPerSample != 32 && bitsPerSample != 64)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32 && bitsPerSample != 64)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	// ---- READ SAMPLE DATA ----
	fseek(f, dataPtr, SEEK_SET);

	int16_t stereoSampleLoadMode = -1;
	if (wavIsStereo(f))
		stereoSampleLoadMode = loaderSysReq(5, "System request", "This is a stereo sample...");

	if (bitsPerSample == 8) // 8-BIT INTEGER SAMPLE
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

		audioDataU8 = (uint8_t *)s->dataPtr;

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
						audioDataU8[i] = audioDataU8[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataU8[i] = audioDataU8[(i * 2) + 1];

					audioDataU8[i] = 128;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataU8[i] = (audioDataU8[(i * 2) + 0] + audioDataU8[(i * 2) + 1]) >> 1;

					audioDataU8[i] = 128;
				}
				break;
			}
		}

		// convert from unsigned to signed
		for (i = 0; i < sampleLength; i++)
			s->dataPtr[i] ^= 0x80;
	}
	else if (bitsPerSample == 16) // 16-BIT INTEGER SAMPLE
	{
		sampleLength /= 2;
		if (!allocateSmpData(s, sampleLength, true))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(s->dataPtr, sampleLength, 2, f) != 2)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		audioDataS16 = (int16_t *)s->dataPtr;

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
						audioDataS16[i] = audioDataS16[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS16[i] = audioDataS16[(i * 2) + 1];

					audioDataS16[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
					{
						int32_t smp32 = audioDataS16[(i * 2) + 0] + audioDataS16[(i * 2) + 1];
						audioDataS16[i] = (int16_t)(smp32 >> 1);
					}

					audioDataS16[i] = 0;
				}
				break;
			}
		}

		s->flags |= SAMPLE_16BIT;
	}
	else if (bitsPerSample == 24) // 24-BIT INTEGER SAMPLE
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
			audioDataS32[i] = (audioDataU8[2] << 24) | (audioDataU8[1] << 16) | (audioDataU8[0] << 8);
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
					// remove right channel data
					for (i = 1; i < sampleLength; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 1];

					audioDataS32[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
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

		ptr16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			ptr16[i] = audioDataS32[i] >> 16;

		s->flags |= SAMPLE_16BIT;
	}
	else if (audioFormat == WAV_FORMAT_PCM && bitsPerSample == 32) // 32-BIT INTEGER SAMPLE
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

		audioDataS32 = (int32_t *)s->dataPtr;

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
						audioDataS32[i] = audioDataS32[(i * 2) + 0];
				}
				break;

				case STEREO_SAMPLE_READ_RIGHT:
				{
					// remove left channel data
					len32 = sampleLength - 1;
					for (i = 0; i < len32; i++)
						audioDataS32[i] = audioDataS32[(i * 2) + 1];

					audioDataS32[i] = 0;
				}
				break;

				default:
				case STEREO_SAMPLE_CONVERT:
				{
					// mix stereo to mono
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

		ptr16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
			ptr16[i] = audioDataS32[i] >> 16;

		s->flags |= SAMPLE_16BIT;
	}
	else if (audioFormat == WAV_FORMAT_IEEE_FLOAT && bitsPerSample == 32) // 32-BIT FLOATING POINT SAMPLE
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

		fAudioDataFloat = (float *)s->dataPtr;

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

		ptr16 = (int16_t *)s->dataPtr;
		for (i = 0; i < sampleLength; i++)
		{
			const int32_t smp32 = (const int32_t)fAudioDataFloat[i];
			ptr16[i] = (int16_t)smp32;
		}

		s->flags |= SAMPLE_16BIT;
	}
	else if (audioFormat == WAV_FORMAT_IEEE_FLOAT && bitsPerSample == 64) // 64-BIT FLOATING POINT SAMPLE
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

		dAudioDataDouble = (double *)s->dataPtr;

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

		ptr16 = (int16_t *)s->dataPtr;
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

	tuneSample(s, sampleRate, audio.linearPeriodsFlag);

	s->volume = 64;
	s->panning = 128;
	s->length = sampleLength;

	// ---- READ "smpl" chunk ----
	if (smplPtr != 0 && smplLen > 52)
	{
		uint32_t numLoops, loopType, loopStart, loopEnd;

		fseek(f, smplPtr+28, SEEK_SET); // seek to first wanted byte

		fread(&numLoops, 4, 1, f);
		if (numLoops == 1)
		{
			fseek(f, 4+4, SEEK_CUR); // skip "samplerData" and "identifier"

			fread(&loopType, 4, 1, f);
			fread(&loopStart, 4, 1, f);
			fread(&loopEnd, 4, 1, f);

			loopEnd++;
			if (loopEnd <= sampleLength)
			{
				s->loopStart = loopStart;
				s->loopLength = loopEnd - loopStart;
				s->flags |= (loopType == 0) ? LOOP_FWD : LOOP_BIDI;
			}
		}
	}
	// ---------------------------

	// ---- READ "xtra" chunk ----
	if (xtraPtr != 0 && xtraLen >= 8)
	{
		uint16_t tmpPan, tmpVol;
		uint32_t xtraFlags;

		fseek(f, xtraPtr, SEEK_SET);
		fread(&xtraFlags, 4, 1, f); // flags

		// panning (0..256)
		if (xtraFlags & 0x20) // set panning flag
		{
			fread(&tmpPan, 2, 1, f);
			if (tmpPan > 255)
				tmpPan = 255;

			s->panning = (uint8_t)tmpPan;
		}
		else
		{
			// don't read panning, skip it
			fseek(f, 2, SEEK_CUR);
		}

		// volume (0..256)
		fread(&tmpVol, 2, 1, f);
		if (tmpVol > 256)
			tmpVol = 256;

		s->volume = (uint8_t)((tmpVol + 2) / 4); // 0..256 -> 0..64 (rounded)
	}
	// ---------------------------

	// ---- READ "INAM" chunk ----
	if (inamPtr != 0 && inamLen > 0)
	{
		fseek(f, inamPtr, SEEK_SET);
		if (inamLen > 22)
			inamLen = 22;

		fread(s->name, 1, inamLen, f);
		s->name[22] = '\0';

		smpFilenameSet = true;
	}

	return true;
}

static bool wavIsStereo(FILE *f)
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

	uint32_t fmtPtr = 0;
	uint32_t fmtLen = 0;

	int32_t bytesRead = 0;
	while (!feof(f) && bytesRead < filesize-12)
	{
		fread(&chunkID, 4, 1, f); if (feof(f)) break;
		fread(&chunkSize, 4, 1, f); if (feof(f)) break;

		int32_t endOfChunk = (ftell(f) + chunkSize) + (chunkSize & 1);
		switch (chunkID)
		{
			case 0x20746D66: // "fmt "
			{
				fmtPtr = ftell(f);
				fmtLen = chunkSize;
			}
			break;

			default: break;
		}

		bytesRead += (chunkSize + (chunkSize & 1));
		fseek(f, endOfChunk, SEEK_SET);
	}

	if (fmtPtr == 0 || fmtLen < 4)
	{
		fseek(f, oldPos, SEEK_SET);
		return false;
	}

	fseek(f, fmtPtr + 2, SEEK_SET);
	fread(&numChannels, 2, 1, f);

	fseek(f, oldPos, SEEK_SET);
	return (numChannels == 2);
}
