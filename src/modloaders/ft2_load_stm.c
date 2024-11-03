/* Scream Tracker 2 STM loader
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

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct stmSmpHdr_t
{
	char name[12];
	uint8_t nul, junk1;
	uint16_t junk2, length, loopStart, loopEnd;
	uint8_t volume, junk3;
	uint16_t midCFreq;
	int32_t junk4;
	uint16_t paraLen;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
stmSmpHdr_t;

typedef struct stmHdr_t
{
	char name[20], sig[8];
	uint8_t x1A, type;
	uint8_t verMajor, verMinor;
	uint8_t tempo, numPatterns, volume, reserved[13];
	stmSmpHdr_t smp[31];
	uint8_t orders[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
stmHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static const uint8_t stmEfx[16] = { 0, 0, 11, 0, 10, 2, 1, 3, 4, 7, 0, 5, 6, 0, 0, 0 };
static uint8_t pattBuff[64*4*4];

static uint16_t stmTempoToBPM(uint8_t tempo);

bool loadSTM(FILE *f, uint32_t filesize)
{
	int16_t i, j, k;
	stmHdr_t hdr;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&hdr, 1, sizeof (hdr), f) != sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (hdr.verMinor == 0 || hdr.type != 2)
	{
		loaderMsgBox("Error loading STM: Incompatible module!");
		return false;
	}

	songTmp.numChannels = 4;
	memcpy(songTmp.orders, hdr.orders, 128);

	i = 0;
	while (i < 128 && songTmp.orders[i] < 99)
		i++;
	songTmp.songLength = i + (i == 0);

	if (songTmp.songLength < 255)
		memset(&songTmp.orders[songTmp.songLength], 0, 256 - songTmp.songLength);

	memcpy(songTmp.name, hdr.name, 20);

	uint8_t tempo = hdr.tempo;
	if (hdr.verMinor < 21)
		tempo = ((tempo / 10) << 4) + (tempo % 10);

	if (tempo == 0)
		tempo = 96;

	songTmp.BPM = stmTempoToBPM(tempo);
	songTmp.speed = hdr.tempo >> 4;

	for (i = 0; i < hdr.numPatterns; i++)
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

		uint8_t *pattPtr = pattBuff;
		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < 4; k++, pattPtr += 4)
			{
				note_t *p = &patternTmp[i][(j * MAX_CHANNELS) + k];
				
				if (pattPtr[0] == 254)
				{
					p->note = NOTE_OFF;
				}
				else if (pattPtr[0] < 96)
				{
					p->note = (12 * (pattPtr[0] >> 4)) + (25 + (pattPtr[0] & 0x0F));
					if (p->note > 96)
						p->note = 0;
				}
				else
				{
					p->note = 0;
				}

				p->instr = pattPtr[1] >> 3;

				uint8_t vol = (pattPtr[1] & 7) + ((pattPtr[2] & 0xF0) >> 1);
				if (vol <= 64)
					p->vol = 0x10 + vol;

				p->efxData = pattPtr[3];

				uint8_t efx = pattPtr[2] & 0x0F;
				if (efx == 1)
				{
					p->efx = 15;

					if (hdr.verMinor < 21)
						p->efxData = ((p->efxData / 10) << 4) + (p->efxData % 10);
					
					p->efxData >>= 4;
				}
				else if (efx == 3)
				{
					p->efx = 13;
					p->efxData = 0;
				}
				else if (efx == 2 || (efx >= 4 && efx <= 12))
				{
					p->efx = stmEfx[efx];
					if (p->efx == 0xA)
					{
						if (p->efxData & 0x0F)
							p->efxData &= 0x0F;
						else
							p->efxData &= 0xF0;
					}
				}
				else
				{
					p->efxData = 0;
				}
			}
		}

		if (tmpPatternEmpty(i))
		{
			if (patternTmp[i] != NULL)
			{
				free(patternTmp[i]);
				patternTmp[i] = NULL;
			}
		}
	}

	for (i = 0; i < 31; i++)
	{
		memcpy(&songTmp.instrName[1+i], hdr.smp[i].name, 12);

		if (hdr.smp[i].length > 0)
		{
			allocateTmpInstr(1 + i);
			setNoEnvelope(instrTmp[i]);

			sample_t *s = &instrTmp[1+i]->smp[0];

			if (!allocateSmpData(s, hdr.smp[i].length, false))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			s->length = hdr.smp[i].length;
			s->volume = hdr.smp[i].volume;
			s->loopStart = hdr.smp[i].loopStart;
			s->loopLength = hdr.smp[i].loopEnd - hdr.smp[i].loopStart;

			memcpy(s->name, hdr.smp[i].name, 12);
			setSampleC4Hz(s, hdr.smp[i].midCFreq);

			if (s->loopStart < s->length && hdr.smp[i].loopEnd > s->loopStart && hdr.smp[i].loopEnd != 0xFFFF)
			{
				if (s->loopStart+s->loopLength > s->length)
					s->loopLength = s->length - s->loopStart;

				s->flags |= LOOP_FWD; // enable loop
			}
			else
			{
				s->loopStart = 0;
				s->loopLength = 0;
			}

			if (fread(s->dataPtr, s->length, 1, f) != 1)
			{
				loaderMsgBox("General I/O error during loading! Possibly corrupt module?");
				return false;
			}
		}
	}

	return true;
}

static uint16_t stmTempoToBPM(uint8_t tempo) // ported from original ST2.3 replayer code
{
	const uint8_t slowdowns[16] = { 140, 50, 25, 15, 10, 7, 6, 4, 3, 3, 2, 2, 2, 2, 1, 1 };
	uint16_t hz = 50;

	hz -= ((slowdowns[tempo >> 4] * (tempo & 15)) >> 4); // can and will underflow

	const uint16_t bpm = (hz << 1) + (hz >> 1); // BPM = hz * 2.5
	return bpm; // result can be slightly off, but close enough...
}
