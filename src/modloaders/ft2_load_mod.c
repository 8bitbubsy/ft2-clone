// NoiseTracker/ProTracker (or compatible) MOD loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

enum // supported 31-sample .MOD types
{
	FORMAT_MK,   // ProTracker or compatible
	FORMAT_FLT4, // StarTrekker (4ch modules)
	FORMAT_FLT8, // StarTrekker (8ch modules)
	FORMAT_FT2,  // FT2 or compatible (multi-channel)
	FORMAT_NT,   // NoiseTracker
	FORMAT_HMNT, // His Master's NoiseTracker (special one)

	FORMAT_UNKNOWN
};

static uint8_t getModType(uint8_t *numChannels, const char *id);

bool loadMOD(FILE *f, uint32_t filesize)
{
	uint8_t bytes[4], modFormat, numChannels;
	int16_t i, j, k;
	uint16_t a, b, period;
	tonTyp *ton;
	sampleTyp *s;
	songMOD31HeaderTyp h_MOD31;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (h_MOD31))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&h_MOD31, 0, sizeof (songMOD31HeaderTyp));
	if (fread(&h_MOD31, 1, sizeof (h_MOD31), f) != sizeof (h_MOD31))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	modFormat = getModType(&numChannels, h_MOD31.sig);
	if (modFormat == FORMAT_UNKNOWN)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	bool hasMoreThan32Chans = numChannels > 32;

	songTmp.antChn = hasMoreThan32Chans ? 32 : numChannels;
	songTmp.len = h_MOD31.len;
	songTmp.repS = h_MOD31.repS;
	songTmp.initialTempo = songTmp.tempo = 6;
	songTmp.speed = 125;

	memcpy(songTmp.songTab, h_MOD31.songTab, 128);

	if (modFormat == FORMAT_MK && songTmp.len == 129)
		songTmp.len = 127; // fixes a specific copy of beatwave.mod (FIXME: Do we want to keep this hack?)

	if (songTmp.repS >= songTmp.len)
		songTmp.repS = 0;

	if (songTmp.antChn == 0 || songTmp.len < 1 || songTmp.len > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	for (a = 0; a < 31; a++)
	{
		songMODInstrHeaderTyp *smp = &h_MOD31.instr[a];

		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(songTmp.instrName[1+a], smp->name, 22);
	}

	memcpy(songTmp.name, h_MOD31.name, 20);

	// count number of patterns
	b = 0;
	for (a = 0; a < 128; a++)
	{
		if (modFormat == FORMAT_FLT8)
			songTmp.songTab[a] >>= 1;

		if (songTmp.songTab[a] > b)
			b = songTmp.songTab[a];
	}
	b++;

	// load pattern data
	if (modFormat != FORMAT_FLT8)
	{
		// normal pattern loading

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
					fread(bytes, 1, 4, f);

					// period to note
					period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					for (i = 0; i < 8*12; i++)
					{
						if (period >= amigaPeriod[i])
						{
							ton->ton = (uint8_t)i + 1;
							break;
						}
					}

					ton->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					ton->effTyp = bytes[2] & 0x0F;
					ton->eff = bytes[3];
				}

				if (hasMoreThan32Chans)
				{
					int32_t remainingChans = numChannels-songTmp.antChn;
					fseek(f, remainingChans*4, SEEK_CUR);
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
	}
	else
	{
		// FLT8 pattern loading

		for (a = 0; a < b; a++)
		{
			if (!allocateTmpPatt(a, 64))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}
		}

		for (a = 0; a < b*2; a++)
		{
			int32_t pattern = a >> 1;
			int32_t chnOffset = (a & 1) * 4;

			for (j = 0; j < 64; j++)
			{
				for (k = 0; k < 4; k++)
				{
					ton = &pattTmp[pattern][(j * MAX_VOICES) + (k+chnOffset)];
					fread(bytes, 1, 4, f);

					// period to note
					period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					for (i = 0; i < 8*12; i++)
					{
						if (period >= amigaPeriod[i])
						{
							ton->ton = (uint8_t)i + 1;
							break;
						}
					}

					ton->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					ton->effTyp = bytes[2] & 0x0F;
					ton->eff = bytes[3];
				}
			}
		}

		for (a = 0; a < b; a++)
		{
			if (tmpPatternEmpty(a))
			{
				if (pattTmp[a] != NULL)
				{
					free(pattTmp[a]);
					pattTmp[a] = NULL;
				}
			}
		}
	}

	// pattern command conversion
	for (a = 0; a < b; a++)
	{
		if (pattTmp[a] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.antChn; k++)
			{
				ton = &pattTmp[a][(j * MAX_VOICES) + k];

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

				if (modFormat == FORMAT_NT || modFormat == FORMAT_HMNT)
				{
					// any Dxx == D00 in NT/HMNT
					if (ton->effTyp == 0xD)
						ton->eff = 0;

					// effect F with param 0x00 does nothing in NT/HMNT
					if (ton->effTyp == 0xF && ton->eff == 0)
						ton->effTyp = 0;
				}
				else if (modFormat == FORMAT_FLT4 || modFormat == FORMAT_FLT8) // Startrekker
				{
					if (ton->effTyp == 0xE) // remove unsupported "assembly macros" command
					{
						ton->effTyp = 0;
						ton->eff = 0;
					}

					// Startrekker is always in vblank mode, and limits speed to 0x1F
					if (ton->effTyp == 0xF && ton->eff > 0x1F)
						ton->eff = 0x1F;
				}
			}
		}
	}

	for (a = 0; a < 31; a++)
	{
		if (h_MOD31.instr[a].len == 0)
			continue;

		if (!allocateTmpInstr(1+a))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+a]);

		s = &instrTmp[1+a]->samp[0];

		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(s->name, songTmp.instrName[1+a], 22);

		if (modFormat == FORMAT_HMNT) // finetune in "His Master's NoiseTracker" is different
			h_MOD31.instr[a].fine = (uint8_t)((-h_MOD31.instr[a].fine & 0x1F) >> 1); // one more bit of precision, + inverted

		s->len = 2 * SWAP16(h_MOD31.instr[a].len);
		s->fine = 8 * ((2 * ((h_MOD31.instr[a].fine & 0xF) ^ 8)) - 16);
		s->vol = h_MOD31.instr[a].vol;
		s->repS = 2 * SWAP16(h_MOD31.instr[a].repS);
		s->repL = 2 * SWAP16(h_MOD31.instr[a].repL);

		if (s->vol > 64)
			s->vol = 64;

		if (s->repL < 2)
			s->repL = 2;

		// fix for poorly converted STK (< v2.5) -> PT/NT modules (FIXME: Worth keeping or not?)
		if (s->repL > 2 && s->repS+s->repL > s->len)
		{
			if ((s->repS>>1)+s->repL <= s->len)
				s->repS >>= 1;
		}

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
			s->typ = 1; // enable loop

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

		if (s->typ == 0) // clear repL and repS on non-looping samples...
		{
			s->repL = 0;
			s->repS = 0;
		}
	}

	if (hasMoreThan32Chans)
		loaderMsgBox("Warning: Module contains >32 channels. The extra channels will be discarded!");

	return true;
}

static uint8_t getModType(uint8_t *numChannels, const char *id)
{
#define IS_ID(s, b) !strncmp(s, b, 4)

	uint8_t modFormat = FORMAT_UNKNOWN;
	*numChannels = 4;

	if (IS_ID("M.K.", id) || IS_ID("M!K!", id) || IS_ID("NSMS", id) || IS_ID("LARD", id) || IS_ID("PATT", id))
	{
		modFormat = FORMAT_MK; // ProTracker or compatible
	}
	else if (isdigit(id[0]) && id[1] == 'C' && id[2] == 'H' && id[3] == 'N') // xCHN
	{
		modFormat = FORMAT_FT2;
		*numChannels = id[0] - '0';
	}
	else if (isdigit(id[0]) && isdigit(id[1]) && id[2] == 'C' && id[3] == 'H') // xxCH
	{
		modFormat = FORMAT_FT2;
		*numChannels = ((id[0] - '0') * 10) + (id[1] - '0');
	}
	else if (isdigit(id[0]) && isdigit(id[1]) && id[2] == 'C' && id[3] == 'N') // xxCN (load as xxCH)
	{
		modFormat = FORMAT_FT2;
		*numChannels = ((id[0] - '0') * 10) + (id[1] - '0');
	}
	else if (IS_ID("CD61", id) || IS_ID("CD81", id)) // Octalyser (Atari)
	{
		modFormat = FORMAT_FT2;
		*numChannels = id[2] - '0';
	}
	else if (id[0] == 'F' && id[1] == 'A' && id[2] == '0' && id[3] >= '4' && id[3] <= '8') // FA0x (Digital Tracker, Atari)
	{
		modFormat = FORMAT_FT2;
		*numChannels = id[3] - '0';
	}
	else if (IS_ID("OKTA", id) || IS_ID("OCTA", id)) // Oktalyzer (as .MOD format)
	{
		modFormat = FORMAT_FT2;
		*numChannels = 8;
	}
	else if (IS_ID("FLT4", id) || IS_ID("EXO4", id)) // StarTrekker 4ch
	{
		modFormat = FORMAT_FLT4;
	}
	else if (IS_ID("FLT8", id) || IS_ID("EXO8", id)) // StarTrekker 8ch
	{
		modFormat = FORMAT_FLT8;
		*numChannels = 8;
	}
	else if (IS_ID("N.T.", id))
	{
		modFormat = FORMAT_NT; // NoiseTracker
	}
	else if (IS_ID("M&K!", id) || IS_ID("FEST", id))
	{
		modFormat = FORMAT_HMNT; // His Master's NoiseTracker (special one)
	}

	return modFormat;
}
