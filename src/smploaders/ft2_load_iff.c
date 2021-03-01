// IFF (Amiga/FT2) sample loader

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
	uint32_t sampleLength, sampleVol, sampleLoopStart, sampleLoopLength, sampleRate;

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	fseek(f, 8, SEEK_SET);
	fread(hdr, 1, 4, f);
	hdr[4] = '\0';
	bool is16Bit = !strncmp(hdr, "16SV", 4);

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
	fread(&sampleLoopStart,  4, 1, f); sampleLoopStart = SWAP32(sampleLoopStart);
	fread(&sampleLoopLength, 4, 1, f); sampleLoopLength = SWAP32(sampleLoopLength);
	fseek(f, 4, SEEK_CUR);
	fread(&sampleRate, 2, 1, f); sampleRate = SWAP16(sampleRate);
	fseek(f, 1, SEEK_CUR);

	if (fgetc(f) != 0) // sample type
	{
		loaderMsgBox("Error loading sample: The sample is not supported!");
		return false;
	}

	fread(&sampleVol, 4, 1, f); sampleVol = SWAP32(sampleVol);
	if (sampleVol > 65535)
		sampleVol = 65535;

	sampleVol = (sampleVol + 512) / 1024; // rounded

	sampleLength = bodyLen;
	if (sampleLength > MAX_SAMPLE_LEN)
		sampleLength = MAX_SAMPLE_LEN;

	if (sampleLoopStart >= MAX_SAMPLE_LEN || sampleLoopLength > MAX_SAMPLE_LEN)
	{
		sampleLoopStart = 0;
		sampleLoopLength = 0;
	}

	if (sampleLoopStart+sampleLoopLength > sampleLength)
	{
		sampleLoopStart = 0;
		sampleLoopLength = 0;
	}

	if (sampleLoopStart > sampleLength-2)
	{
		sampleLoopStart = 0;
		sampleLoopLength = 0;
	}

	if (!allocateTmpSmpData(&tmpSmp, sampleLength))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	fseek(f, bodyPtr, SEEK_SET);
	if (fread(tmpSmp.pek, sampleLength, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	tmpSmp.len = sampleLength;

	if (sampleLoopLength > 2)
	{
		tmpSmp.repS = sampleLoopStart;
		tmpSmp.repL = sampleLoopLength;
		tmpSmp.typ |= 1;
	}

	if (is16Bit)
	{
		tmpSmp.len  &= 0xFFFFFFFE;
		tmpSmp.repS &= 0xFFFFFFFE;
		tmpSmp.repL &= 0xFFFFFFFE;
		tmpSmp.typ |= 16;
	}

	tmpSmp.vol = (uint8_t)sampleVol;
	tmpSmp.pan = 128;

	tuneSample(&tmpSmp, sampleRate, audio.linearPeriodsFlag);

	// set name
	if (namePtr != 0 && nameLen > 0)
	{
		fseek(f, namePtr, SEEK_SET);
		if (nameLen > 21)
		{
			fread(tmpSmp.name, 1, 21, f);
			tmpSmp.name[21] = '\0';
		}
		else
		{
			memset(tmpSmp.name, 0, 22);
			fread(tmpSmp.name, 1, nameLen, f);
		}

		smpFilenameSet = true;
	}

	return true;
}
