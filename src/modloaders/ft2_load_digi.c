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

static void convertPatternEffect(note_t *p)
{
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

bool loadDIGI(FILE *f, uint32_t filesize)
{
	sample_t *s;
	digiHdr_t header;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&header, 0, sizeof (header));
	if (fread(&header, 1, sizeof (header), f) != sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (header.numChannels < 1 || header.numChannels > 8)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	header.numOrders++;
	header.numPatterns++;

	if (header.numOrders < 1 || header.numOrders > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memcpy(songTmp.orders, header.orders, 128);
	memcpy(songTmp.name, header.name, 20);
	songTmp.numChannels = header.numChannels;
	songTmp.songLength = header.numOrders;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	// patterns

	for (int32_t i = 0; i < header.numPatterns; i++)
	{
		if (!allocateTmpPatt(i, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (header.packedPatternsFlag)
		{
			// compressed pattern

			uint16_t pattSize;
			fread(&pattSize, 2, 1, f); pattSize = SWAP16(pattSize);

			if (pattSize >= 64)
			{
				uint8_t bitMasks[64];
				fread(bitMasks, 1, 64, f);

				fread(tmpBuffer, 1, pattSize-64, f);
				uint8_t *pattPtr = tmpBuffer;

				for (int32_t row = 0; row < 64; row++)
				{
					uint8_t bit = 128;
					for (int32_t ch = 0; ch < songTmp.numChannels; ch++, bit >>= 1)
					{
						note_t *p = &patternTmp[i][(row * MAX_CHANNELS) + ch];
						if (bitMasks[row] & bit)
						{
							// period to note
							uint16_t period = ((pattPtr[0] & 0x0F) << 8) | pattPtr[1];
							for (int32_t note = 0; note < 3*12; note++)
							{
								if (period >= ptPeriods[note])
								{
									p->note = 1 + (3*12) + (uint8_t)note;
									break;
								}
							}

							p->instr = (pattPtr[0] & 0xF0) | (pattPtr[2] >> 4);
							p->efx = pattPtr[2] & 0x0F;
							p->efxData = pattPtr[3];
							convertPatternEffect(p);

							pattPtr += 4;
						}
					}
				}
			}
		}
		else
		{
			// uncompressed pattern

			fread(tmpBuffer, 1, 64 * songTmp.numChannels * 4, f);
			uint8_t *pattPtr = tmpBuffer;

			for (int32_t ch = 0; ch < songTmp.numChannels; ch++)
			{
				for (int32_t row = 0; row < 64; row++, pattPtr += 4)
				{
					note_t *p = &patternTmp[i][(row * MAX_CHANNELS) + ch];

					// period to note
					uint16_t period = ((pattPtr[0] & 0x0F) << 8) | pattPtr[1];
					for (int32_t note = 0; note < 3*12; note++)
					{
						if (period >= ptPeriods[note])
						{
							p->note = 1 + (3*12) + (uint8_t)note;
							break;
						}
					}

					p->instr = (pattPtr[0] & 0xF0) | (pattPtr[2] >> 4);
					p->efx = pattPtr[2] & 0x0F;
					p->efxData = pattPtr[3];
					convertPatternEffect(p);
				}
			}
		}
	}

	// samples

	for (int32_t i = 0; i < 31; i++)
	{
		memcpy(songTmp.instrName[1+i], header.smpName[i], 22);
		if (header.smpLength[i] == 0)
			continue;

		if (!allocateTmpInstr(1+i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		setNoEnvelope(instrTmp[1+i]);
		s = &instrTmp[1+i]->smp[0];

		memcpy(s->name, header.smpName[i], 22);

		s->length = SWAP32(header.smpLength[i]);
		s->finetune = FINETUNE_MOD2XM(header.smpFinetune[i]);
		s->volume = header.smpVolume[i];
		s->loopStart = SWAP32(header.smpLoopStart[i]);
		s->loopLength = SWAP32(header.smpLoopLength[i]);

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
