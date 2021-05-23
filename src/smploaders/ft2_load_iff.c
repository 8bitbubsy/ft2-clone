/* IFF (Amiga/FT2) sample loader
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

bool loadIFF(FILE *f, uint32_t filesize)
{
	char hdr[4+1];
	uint32_t length, volume, loopStart, loopLength, sampleRate;
	sample_t *s = &tmpSmp;

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	fseek(f, 8, SEEK_SET);
	fread(hdr, 1, 4, f);
	hdr[4] = '\0';
	bool sample16Bit = !strncmp(hdr, "16SV", 4);

	uint32_t vhdrPtr = 0, vhdrLen = 0;
	uint32_t bodyPtr = 0, bodyLen = 0;
	uint32_t namePtr = 0, nameLen = 0;

	fseek(f, 12, SEEK_SET);
	while (!feof(f) && (uint32_t)ftell(f) < filesize-12)
	{
		uint32_t blockName, blockSize;
		fread(&blockName, 4, 1, f); if (feof(f)) break;
		fread(&blockSize, 4, 1, f); if (feof(f)) break;

		blockName = SWAP32(blockName);
		blockSize = SWAP32(blockSize);

		switch (blockName)
		{
			case 0x56484452: // VHDR
			{
				vhdrPtr = ftell(f);
				vhdrLen = blockSize;
			}
			break;

			case 0x4E414D45: // NAME
			{
				namePtr = ftell(f);
				nameLen = blockSize;
			}
			break;

			case 0x424F4459: // BODY
			{
				bodyPtr = ftell(f);
				bodyLen = blockSize;
			}
			break;

			default: break;
		}

		fseek(f, blockSize + (blockSize & 1), SEEK_CUR);
	}

	if (vhdrPtr == 0 || vhdrLen < 20 || bodyPtr == 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	// kludge for some really strange IFFs
	if (bodyLen == 0)
		bodyLen = filesize - bodyPtr;

	if (bodyPtr+bodyLen > (uint32_t)filesize)
		bodyLen = filesize - bodyPtr;

	fseek(f, vhdrPtr, SEEK_SET);
	fread(&loopStart,  4, 1, f); loopStart = SWAP32(loopStart);
	fread(&loopLength, 4, 1, f); loopLength = SWAP32(loopLength);
	fseek(f, 4, SEEK_CUR);
	fread(&sampleRate, 2, 1, f); sampleRate = SWAP16(sampleRate);
	fseek(f, 1, SEEK_CUR);

	if (fgetc(f) != 0) // sample type
	{
		loaderMsgBox("Error loading sample: The sample is not supported!");
		return false;
	}

	fread(&volume, 4, 1, f); volume = SWAP32(volume);
	if (volume > 65535)
		volume = 65535;

	volume = (volume + 512) / 1024; // rounded

	length = bodyLen;
	if (sample16Bit)
	{
		length >>= 1;
		loopStart >>= 1;
		loopLength >>= 1;
	}

	s->length = length;
	if (s->length > MAX_SAMPLE_LEN)
		s->length = MAX_SAMPLE_LEN;

	if (!allocateSmpData(s, length, sample16Bit))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	fseek(f, bodyPtr, SEEK_SET);
	if (fread(s->dataPtr, length << sample16Bit, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	s->loopStart = loopStart;
	s->loopLength = loopLength;

	if (s->loopLength > 0)
		s->flags |= LOOP_FWD;

	if (sample16Bit)
		s->flags |= SAMPLE_16BIT;

	s->volume = (uint8_t)volume;
	s->panning = 128;

	tuneSample(s, sampleRate, audio.linearPeriodsFlag);

	// set name
	if (namePtr != 0 && nameLen > 0)
	{
		fseek(f, namePtr, SEEK_SET);

		if (nameLen > 22)
			nameLen = 22;

		fread(s->name, 1, nameLen, f);
		s->name[22] = '\0';

		smpFilenameSet = true;
	}

	return true;
}
