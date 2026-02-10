/* DIGI Booster (non-Pro) module loader
**
** Note: Data sanitation is done in the last stage
** of module loading, so you don't need to do that here.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

#ifdef _MSC_VER  // please don't mess with this struct!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct digiHdr_t
{
	char ID[20];
	char verStr[4];
	uint8_t version;
	uint8_t numChannels;
	uint8_t packedPatternsFlag;
	char reserved[19];
	uint8_t numPatterns;
	uint8_t numOrders;
	uint8_t orders[128];
	uint32_t smpLength[31];
	uint32_t smpLoopStart[31];
	uint32_t smpLoopLength[31];
	uint8_t smpVolume[31];
	uint8_t smpFinetune[31];
	char name[32];
	char smpName[31][30];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
digiHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static void readPatternNote(FILE *f, note_t *p);

bool loadDIGI(FILE *f, uint32_t filesize)
{
	int16_t i, j, k;
	sample_t *s;
	digiHdr_t hdr;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&hdr, 0, sizeof (hdr));
	if (fread(&hdr, 1, sizeof (hdr), f) != sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (hdr.numChannels < 1 || hdr.numChannels > 8)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	hdr.numOrders++;
	hdr.numPatterns++;

	if (hdr.numOrders < 1 || hdr.numOrders > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memcpy(songTmp.orders, hdr.orders, 128);
	memcpy(songTmp.name, hdr.name, 20);
	songTmp.numChannels = hdr.numChannels;
	songTmp.songLength = hdr.numOrders;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	// load pattern data
	for (i = 0; i < hdr.numPatterns; i++)
	{
		if (!allocateTmpPatt(i, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (hdr.packedPatternsFlag)
		{
			uint16_t pattSize;
			uint8_t bitMasks[64];

			fread(&pattSize, 2, 1, f); pattSize = SWAP16(pattSize);
			fread(bitMasks, 1, 64, f);

			for (j = 0; j < 64; j++)
			{
				uint8_t bit = 128;
				for (k = 0; k < songTmp.numChannels; k++, bit >>= 1)
				{
					note_t *p = &patternTmp[i][(j * MAX_CHANNELS) + k];
					if (bitMasks[j] & bit)
						readPatternNote(f, p);
				}
			}
		}
		else
		{
			for (j = 0; j < songTmp.numChannels; j++)
			{
				for (k = 0; k < 64; k++)
					readPatternNote(f, &patternTmp[i][(k * MAX_CHANNELS) + j]);
			}
		}
	}

	// pattern command handling
	for (i = 0; i < hdr.numPatterns; i++)
	{
		if (patternTmp[i] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.numChannels; k++)
			{
				note_t *p = &patternTmp[i][(j * MAX_CHANNELS) + k];

				if (p->efx == 0x8) // Robot effect (not supported)
				{
					p->efx = 0;
					p->efxData = 0;
				}
				else if (p->efx == 0xE)
				{
					switch (p->efxData >> 4)
					{
						case 0x3: p->efx = p->efxData = 0;    break; // backwards play (not supported)
						case 0x4:          p->efxData = 0xC0; break; // stop sample (convert to EC0)
						case 0x8: p->efx = p->efxData = 0;    break; // high sample offset (not supported)
						case 0x9: p->efx = p->efxData = 0;    break; // retrace (not supported)
						default: break;
					}
				}
			}
		}
	}

	// load sample data
	for (i = 0; i < 31; i++)
	{
		memcpy(songTmp.instrName[1+i], hdr.smpName[i], 22);
		if (hdr.smpLength[i] == 0)
			continue;

		if (!allocateTmpInstr(1+i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+i]);

		s = &instrTmp[1+i]->smp[0];
		memcpy(s->name, hdr.smpName[i], 22);

		s->length = SWAP32(hdr.smpLength[i]);
		s->finetune = FINETUNE_MOD2XM(hdr.smpFinetune[i]);
		s->volume = hdr.smpVolume[i];
		s->loopStart = SWAP32(hdr.smpLoopStart[i]);
		s->loopLength = SWAP32(hdr.smpLoopLength[i]);

		if (s->loopLength < 2)
			s->loopLength = 2;

		// fix overflown loop
		if (s->loopStart+s->loopLength > s->length)
		{
			if (s->loopStart >= s->length)
			{
				s->loopStart = 0;
				s->loopLength = 0;
			}
			else
			{
				s->loopLength = s->length - s->loopStart;
			}
		}

		if (s->loopStart+s->loopLength > 2)
		{
			s->flags |= LOOP_FWD; // enable loop
		}
		else
		{
			s->loopStart = 0;
			s->loopLength = 0;
		}

		if (!allocateSmpData(s, s->length, false))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		int32_t bytesRead = (int32_t)fread(s->dataPtr, 1, s->length, f);
		if (bytesRead < s->length)
		{
			int32_t bytesToClear = s->length - bytesRead;
			memset(&s->dataPtr[bytesRead], 0, bytesToClear);
		}
	}

	return true;
}

static void readPatternNote(FILE *f, note_t *p)
{
	uint8_t bytes[4];
	fread(bytes, 1, 4, f);

	// period to note
	uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
	for (uint8_t i = 0; i < 3*12; i++)
	{
		if (period >= ptPeriods[i])
		{
			p->note = 1 + (3*12) + i;
			break;
		}
	}

	p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
	p->efx = bytes[2] & 0x0F;
	p->efxData = bytes[3];
}
