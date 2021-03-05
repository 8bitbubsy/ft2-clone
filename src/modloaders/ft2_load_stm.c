// Scream Tracker 2 STM loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct songSTMinstrHeaderTyp_t
{
	char name[12];
	uint8_t nul, insDisk;
	uint16_t reserved1, len, repS, repE;
	uint8_t vol, reserved2;
	uint16_t rate;
	int32_t reserved3;
	uint16_t paraLen;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songSTMinstrHeaderTyp;

typedef struct songSTMHeaderTyp_t
{
	char name[20], sig[8];
	uint8_t id1a, typ;
	uint8_t verMajor, verMinor;
	uint8_t tempo, ap, vol, reserved[13];
	songSTMinstrHeaderTyp instr[31];
	uint8_t songTab[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songSTMHeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static const uint8_t stmEff[16] = { 0, 0, 11, 0, 10, 2, 1, 3, 4, 7, 0, 5, 6, 0, 0, 0 };

static uint8_t stmTempoToBPM(uint8_t tempo);

bool loadSTM(FILE *f, uint32_t filesize)
{
	uint8_t typ, tempo, pattBuff[1024];
	int16_t i, j, k, ap, tmp;
	uint16_t a;
	tonTyp *ton;
	sampleTyp *s;
	songSTMHeaderTyp h_STM;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (h_STM))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&h_STM, 1, sizeof (h_STM), f) != sizeof (h_STM))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h_STM.verMinor == 0 || h_STM.typ != 2)
	{
		loaderMsgBox("Error loading STM: Incompatible module!");
		return false;
	}

	songTmp.antChn = 4;
	memcpy(songTmp.songTab, h_STM.songTab, 128);

	i = 0;
	while (i < 128 && songTmp.songTab[i] < 99) i++;
	songTmp.len = i + (i == 0);

	if (songTmp.len < 255)
		memset(&songTmp.songTab[songTmp.len], 0, 256 - songTmp.len);

	memcpy(songTmp.name, h_STM.name, 20);

	tempo = h_STM.tempo;
	if (h_STM.verMinor < 21)
		tempo = ((tempo / 10) << 4) + (tempo % 10);

	if (tempo == 0)
		tempo = 96;

	songTmp.initialTempo = songTmp.tempo = CLAMP(h_STM.tempo >> 4, 1, 31);
	songTmp.speed = stmTempoToBPM(tempo);

	if (h_STM.verMinor > 10)
		songTmp.globVol = MIN(h_STM.vol, 64);

	ap = h_STM.ap;
	for (i = 0; i < ap; i++)
	{
		if (!allocateTmpPatt(i, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		if (fread(pattBuff, 64 * 4 * 4, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading!");
			return false;
		}

		a = 0;
		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < 4; k++)
			{
				ton = &pattTmp[i][(j * MAX_VOICES) + k];
				
				if (pattBuff[a] == 254)
				{
					ton->ton = 97;
				}
				else if (pattBuff[a] < 96)
				{
					ton->ton = (12 * (pattBuff[a] >> 4)) + (25 + (pattBuff[a] & 0x0F));
					if (ton->ton > 96)
						ton->ton = 0;
				}
				else
				{
					ton->ton = 0;
				}

				ton->instr = pattBuff[a + 1] >> 3;
				typ = (pattBuff[a + 1] & 7) + ((pattBuff[a + 2] & 0xF0) >> 1);
				if (typ <= 64)
					ton->vol = typ + 0x10;

				ton->eff = pattBuff[a + 3];

				tmp = pattBuff[a + 2] & 0x0F;
				if (tmp == 1)
				{
					ton->effTyp = 15;

					if (h_STM.verMinor < 21)
						ton->eff = ((ton->eff / 10) << 4) + (ton->eff % 10);
					
					ton->eff >>= 4;
				}
				else if (tmp == 3)
				{
					ton->effTyp = 13;
					ton->eff = 0;
				}
				else if (tmp == 2 || (tmp >= 4 && tmp <= 12))
				{
					ton->effTyp = stmEff[tmp];
					if (ton->effTyp == 0xA)
					{
						if (ton->eff & 0x0F)
							ton->eff &= 0x0F;
						else
							ton->eff &= 0xF0;
					}
				}
				else
				{
					ton->eff = 0;
				}

				a += 4;
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

	for (i = 0; i < 31; i++)
	{
		memcpy(&songTmp.instrName[1+i], h_STM.instr[i].name, 12);

		if (h_STM.instr[i].len != 0 && h_STM.instr[i].reserved1 != 0)
		{
			allocateTmpInstr(1 + i);
			setNoEnvelope(instrTmp[i]);

			s = &instrTmp[1+i]->samp[0];

			if (!allocateTmpSmpData(s, h_STM.instr[i].len))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			s->len = h_STM.instr[i].len;
			s->vol = h_STM.instr[i].vol;
			s->repS = h_STM.instr[i].repS;
			s->repL = h_STM.instr[i].repE - h_STM.instr[i].repS;

			memcpy(s->name, h_STM.instr[i].name, 12);
			tuneSample(s, h_STM.instr[i].rate, tmpLinearPeriodsFlag);

			if (s->repS < s->len && h_STM.instr[i].repE > s->repS && h_STM.instr[i].repE != 0xFFFF)
			{
				if (s->repS+s->repL > s->len)
					s->repL = s->len - s->repS;

				s->typ = 1; // enable loop
			}
			else
			{
				s->repS = 0;
				s->repL = 0;
				s->typ = 0;
			}

			if (s->vol > 64)
				s->vol = 64;

			if (fread(s->pek, s->len, 1, f) != 1)
			{
				loaderMsgBox("General I/O error during loading! Possibly corrupt module?");
				return false;
			}
		}
	}

	return true;
}

static uint8_t stmTempoToBPM(uint8_t tempo) // ported from original ST2.3 replayer code
{
	const uint8_t slowdowns[16] = { 140, 50, 25, 15, 10, 7, 6, 4, 3, 3, 2, 2, 2, 2, 1, 1 };
	uint16_t hz = 50;

	hz -= ((slowdowns[tempo >> 4] * (tempo & 15)) >> 4); // can and will underflow

	const uint32_t bpm = (hz << 1) + (hz >> 1); // BPM = hz * 2.5
	return (uint8_t)CLAMP(bpm, 32, 255); // result can be slightly off, but close enough...
}
