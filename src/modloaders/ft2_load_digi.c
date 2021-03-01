// DIGI Booster (non-Pro) module loader

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
typedef struct digiHeaderTyp_t
{
	char sig[20];
	char verStr[4];
	uint8_t ver;
	uint8_t numChannels;
	uint8_t packedPatternsFlag;
	char reserved[19];
	uint8_t numPats;
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
digiHeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static void readPatternNote(FILE *f, tonTyp *ton);

bool loadDIGI(FILE *f, uint32_t filesize)
{
	int16_t i, j, k;
	tonTyp *ton;
	sampleTyp *s;
	digiHeaderTyp h_DIGI;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (h_DIGI))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&h_DIGI, 0, sizeof (digiHeaderTyp));
	if (fread(&h_DIGI, 1, sizeof (h_DIGI), f) != sizeof (h_DIGI))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h_DIGI.numChannels < 1 || h_DIGI.numChannels > 8)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	h_DIGI.numOrders++;
	h_DIGI.numPats++;

	if (h_DIGI.numOrders < 1 || h_DIGI.numOrders > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	songTmp.antChn = h_DIGI.numChannels;
	songTmp.len = h_DIGI.numOrders;
	songTmp.initialTempo = songTmp.tempo = 6;
	songTmp.speed = 125;
	memcpy(songTmp.songTab, h_DIGI.orders, 128);

	// load pattern data
	for (i = 0; i < h_DIGI.numPats; i++)
	{
		if (!allocateTmpPatt(i, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (h_DIGI.packedPatternsFlag)
		{
			uint16_t pattSize;
			uint8_t bitMasks[64];

			fread(&pattSize, 2, 1, f); pattSize = SWAP16(pattSize);
			fread(bitMasks, 1, 64, f);

			for (j = 0; j < 64; j++)
			{
				uint8_t bit = 128;
				for (k = 0; k < songTmp.antChn; k++, bit >>= 1)
				{
					ton = &pattTmp[i][(j * MAX_VOICES) + k];
					if (bitMasks[j] & bit)
						readPatternNote(f, ton);
				}
			}
		}
		else
		{
			for (j = 0; j < songTmp.antChn; j++)
			{
				for (k = 0; k < 64; k++)
				{
					ton = &pattTmp[i][(k * MAX_VOICES) + j];
					readPatternNote(f, ton);
				}
			}
		}

		if (tmpPatternEmpty(i))
		{
			if (pattTmp[i] != NULL)
			{
				free(pattTmp[i]);
				pattTmp[i] = NULL;
			}
		}
	}

	// pattern command handling
	for (i = 0; i < h_DIGI.numPats; i++)
	{
		if (pattTmp[i] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.antChn; k++)
			{
				ton = &pattTmp[i][(j * MAX_VOICES) + k];

				if (ton->effTyp == 0x8) // Robot effect (not supported)
				{
					ton->effTyp = 0;
					ton->eff = 0;
				}
				else if (ton->effTyp == 0xE)
				{
					switch (ton->eff >> 4)
					{
						case 0x3: ton->effTyp = ton->eff = 0; break; // backwards play (not supported)
						case 0x4: ton->eff = 0xC0; break; // stop sample (convert to EC0)
						case 0x8: ton->effTyp = ton->eff = 0; break; // high sample offset (not supported)
						case 0x9: ton->effTyp = ton->eff = 0; break; // retrace (not supported)
						default: break;
					}
				}
			}
		}
	}

	// load sample data
	for (i = 0; i < 31; i++)
	{
		memcpy(songTmp.instrName[1+i], h_DIGI.smpName[i], 22);
		if (h_DIGI.smpLength[i] == 0)
			continue;

		if (!allocateTmpInstr(1+i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+i]);

		s = &instrTmp[1+i]->samp[0];
		memcpy(s->name, h_DIGI.smpName[i], 22);

		s->len = SWAP32(h_DIGI.smpLength[i]);
		s->fine = 8 * ((2 * ((h_DIGI.smpFinetune[i] & 0xF) ^ 8)) - 16);
		s->vol = h_DIGI.smpVolume[i];
		s->repS = SWAP32(h_DIGI.smpLoopStart[i]);
		s->repL = SWAP32(h_DIGI.smpLoopLength[i]);

		if (s->vol > 64)
			s->vol = 64;

		if (s->repL < 2)
			s->repL = 2;

		// fix overflown loop
		if (s->repS+s->repL > s->len)
		{
			if (s->repS >= s->len)
			{
				s->repS = 0;
				s->repL = 0;
			}
			else
			{
				s->repL = s->len - s->repS;
			}
		}

		if (s->repS+s->repL > 2)
		{
			s->typ = 1; // enable loop
		}
		else
		{
			s->repS = 0;
			s->repL = 0;
		}

		if (!allocateTmpSmpData(s, s->len))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		int32_t bytesRead = (int32_t)fread(s->pek, 1, s->len, f);
		if (bytesRead < s->len)
		{
			int32_t bytesToClear = s->len - bytesRead;
			memset(&s->pek[bytesRead], 0, bytesToClear);
		}
	}

	return true;
}

static void readPatternNote(FILE *f, tonTyp *ton)
{
	uint8_t bytes[4];
	fread(bytes, 1, 4, f);

	// period to note
	uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
	for (uint8_t i = 0; i < 3*12; i++)
	{
		if (period >= ptPeriods[i])
		{
			ton->ton = 1 + (3*12) + i;
			break;
		}
	}

	ton->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
	ton->effTyp = bytes[2] & 0x0F;
	ton->eff = bytes[3];
}
