// Ultimate SoundTracker (or compatible) STK loader

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
typedef struct songMOD15HeaderTyp_t
{
	char name[20];
	songMODInstrHeaderTyp instr[15];
	uint8_t len, CIAVal, songTab[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMOD15HeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

bool loadSTK(FILE *f, uint32_t filesize)
{
	uint8_t bytes[4];
	int16_t i, j, k;
	uint16_t a, b, period;
	tonTyp *ton;
	sampleTyp *s;
	songMOD15HeaderTyp h_MOD15;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	bool veryLateSTKVerFlag = false; // "DFJ SoundTracker III" nad later
	bool lateSTKVerFlag = false; // "TJC SoundTracker II" and later

	if (filesize < sizeof (h_MOD15))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&h_MOD15, 0, sizeof (songMOD15HeaderTyp));
	if (fread(&h_MOD15, 1, sizeof (h_MOD15), f) != sizeof (h_MOD15))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h_MOD15.CIAVal == 0) // a CIA value of 0 results in 120
		h_MOD15.CIAVal = 120;

	songTmp.antChn = 4;
	songTmp.len = h_MOD15.len;
	songTmp.speed = 125;
	songTmp.initialTempo = songTmp.tempo = 6;
	memcpy(songTmp.songTab, h_MOD15.songTab, 128);

	if (songTmp.len < 1 || songTmp.len > 128 || h_MOD15.CIAVal > 220)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	for (a = 0; a < 15; a++)
	{
		songMODInstrHeaderTyp *smp = &h_MOD15.instr[a];
		memcpy(songTmp.instrName[1+a], smp->name, 22);

		/* Only late versions of Ultimate SoundTracker supports samples larger than 9999 bytes.
		** If found, we know for sure that this is a late STK module.
		*/
		const int32_t sampleLen = 2*SWAP16(smp->len);
		if (sampleLen > 9999)
			lateSTKVerFlag = true;
	}

	// jjk55.mod by Jesper Kyd has a bogus STK tempo value that should be ignored (hackish!)
	if (!strcmp("jjk55", h_MOD15.name))
		h_MOD15.CIAVal = 120;

	if (h_MOD15.CIAVal != 120) // 120 is a special case and means 50Hz (125BPM)
	{
		// convert UST tempo to BPM
		uint16_t ciaPeriod = (240 - h_MOD15.CIAVal) * 122;
		double dHz = 709379.0 / ciaPeriod;
		int32_t BPM = (int32_t)((dHz * 2.5) + 0.5);

		songTmp.speed = (uint16_t)BPM;
	}

	memcpy(songTmp.name, h_MOD15.name, 20);

	// count number of patterns
	b = 0;
	for (a = 0; a < 128; a++)
	{
		if (songTmp.songTab[a] > b)
			b = songTmp.songTab[a];
	}
	b++;

	for (a = 0; a < b; a++)
	{
		if (!allocateTmpPatt(a, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.antChn; k++)
			{
				ton = &pattTmp[a][(j * MAX_VOICES) + k];

				if (fread(bytes, 1, 4, f) != 4)
				{
					loaderMsgBox("Error: This file is either not a module, or is not supported.");
					return false;
				}

				// period to note
				period = ((bytes[0] & 0x0F) << 8) | bytes[1];
				for (i = 0; i < 3*12; i++)
				{
					if (period >= ptPeriods[i])
					{
						ton->ton = 1 + (3*12) + (uint8_t)i;
						break;
					}
				}

				ton->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
				ton->effTyp = bytes[2] & 0x0F;
				ton->eff = bytes[3];

				if (ton->effTyp == 0xC || ton->effTyp == 0xD || ton->effTyp == 0xE)
				{
					// "TJC SoundTracker II" and later
					lateSTKVerFlag = true;
				}

				if (ton->effTyp == 0xF)
				{
					// "DFJ SoundTracker III" and later
					lateSTKVerFlag = true;
					veryLateSTKVerFlag = true;
				}

				if (ton->effTyp == 0xC)
				{
					if (ton->eff > 64)
						ton->eff = 64;
				}
				else if (ton->effTyp == 0x1)
				{
					if (ton->eff == 0)
						ton->effTyp = 0;
				}
				else if (ton->effTyp == 0x2)
				{
					if (ton->eff == 0)
						ton->effTyp = 0;
				}
				else if (ton->effTyp == 0x5)
				{
					if (ton->eff == 0)
						ton->effTyp = 0x3;
				}
				else if (ton->effTyp == 0x6)
				{
					if (ton->eff == 0)
						ton->effTyp = 0x4;
				}
				else if (ton->effTyp == 0xA)
				{
					if (ton->eff == 0)
						ton->effTyp = 0;
				}
				else if (ton->effTyp == 0xE)
				{
					// check if certain E commands are empty
					if (ton->eff == 0x10 || ton->eff == 0x20 || ton->eff == 0xA0 || ton->eff == 0xB0)
					{
						ton->effTyp = 0;
						ton->eff = 0;
					}
				}
			}
		}

		if (tmpPatternEmpty(a))
		{
			if (pattTmp[a] != NULL)
			{
				free(pattTmp[a]);
				pattTmp[a] = NULL;
			}
		}
	}

	// pattern command conversion for non-PT formats
	for (a = 0; a < b; a++)
	{
		if (pattTmp[a] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.antChn; k++)
			{
				ton = &pattTmp[a][(j * MAX_VOICES) + k];

				// convert STK effects to PT effects

				if (!lateSTKVerFlag)
				{
					// old SoundTracker 1.x commands

					if (ton->effTyp == 1)
					{
						// arpeggio
						ton->effTyp = 0;
					}
					else if (ton->effTyp == 2)
					{
						// pitch slide
						if (ton->eff & 0xF0)
						{
							// pitch slide down
							ton->effTyp = 2;
							ton->eff >>= 4;
						}
						else if (ton->eff & 0x0F)
						{
							// pitch slide up
							ton->effTyp = 1;
						}
					}
				}
				else
				{
					// "DFJ SoundTracker II" or later

					if (ton->effTyp == 0xD)
					{
						if (veryLateSTKVerFlag) // "DFJ SoundTracker III" or later
						{
							// pattern break w/ no param (param must be cleared to fix some songs)
							ton->eff = 0;
						}
						else
						{
							// volume slide
							ton->effTyp = 0xA;
						}
					}
				}

				// effect F with param 0x00 does nothing in UST/STK (I think)
				if (ton->effTyp == 0xF && ton->eff == 0)
					ton->effTyp = 0;
			}
		}
	}

	for (a = 0; a < 15; a++)
	{
		if (h_MOD15.instr[a].len == 0)
			continue;

		if (!allocateTmpInstr(1+a))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+a]);

		s = &instrTmp[1+a]->samp[0];
		s->vol = h_MOD15.instr[a].vol;

		s->len  = 2 * SWAP16(h_MOD15.instr[a].len);
		s->repS =     SWAP16(h_MOD15.instr[a].repS); // in STK, loopStart = bytes, not words
		s->repL = 2 * SWAP16(h_MOD15.instr[a].repL);

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
			s->repL = 0;
			s->repS = 0;
		}

		/* In STK, only the loop area of a looped sample is played.
		** Skip loading of eventual data present before loop start.
		*/
		if (s->repS > 0 && s->repL < s->len)
		{
			s->len -= s->repS;
			fseek(f, s->repS, SEEK_CUR);
			s->repS = 0;
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
