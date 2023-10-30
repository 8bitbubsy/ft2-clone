/* NoiseTracker/ProTracker (or compatible) MOD loader
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
	uint16_t a, b;
	sample_t *s;
	modHdr_t hdr;

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

	modFormat = getModType(&numChannels, hdr.ID);
	if (modFormat == FORMAT_UNKNOWN)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (modFormat == FORMAT_MK && hdr.numOrders == 129)
		hdr.numOrders = 127; // fixes a specific copy of beatwave.mod (FIXME: Do we want to keep this hack?)

	if (numChannels == 0 || hdr.numOrders < 1 || hdr.numOrders > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	bool tooManyChannels = numChannels > MAX_CHANNELS;

	songTmp.numChannels = tooManyChannels ? MAX_CHANNELS : numChannels;
	songTmp.songLength = hdr.numOrders;
	songTmp.songLoopStart = hdr.songLoopStart;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	memcpy(songTmp.orders, hdr.orders, 128);

	for (a = 0; a < 31; a++)
	{
		modSmpHdr_t *modSmp = &hdr.smp[a];

		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(songTmp.instrName[1+a], modSmp->name, 22);
	}

	memcpy(songTmp.name, hdr.name, 20);

	// count number of patterns
	b = 0;
	for (a = 0; a < 128; a++)
	{
		if (modFormat == FORMAT_FLT8)
			songTmp.orders[a] >>= 1;

		if (songTmp.orders[a] > b)
			b = songTmp.orders[a];
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
				for (k = 0; k < songTmp.numChannels; k++)
				{
					note_t *p = &patternTmp[a][(j * MAX_CHANNELS) + k];
					fread(bytes, 1, 4, f);

					// period to note
					uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					for (i = 0; i < 8*12; i++)
					{
						if (period >= modPeriods[i])
						{
							p->note = (uint8_t)i + 1;
							break;
						}
					}

					p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					p->efx = bytes[2] & 0x0F;
					p->efxData = bytes[3];
				}

				if (tooManyChannels)
				{
					int32_t remainingChans = numChannels-songTmp.numChannels;
					fseek(f, remainingChans*4, SEEK_CUR);
				}
			}

			if (tmpPatternEmpty(a))
			{
				if (patternTmp[a] != NULL)
				{
					free(patternTmp[a]);
					patternTmp[a] = NULL;
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
			int32_t pattNum = a >> 1;
			int32_t chnOffset = (a & 1) * 4;

			for (j = 0; j < 64; j++)
			{
				for (k = 0; k < 4; k++)
				{
					note_t *p = &patternTmp[pattNum][(j * MAX_CHANNELS) + (k+chnOffset)];
					fread(bytes, 1, 4, f);

					// period to note
					uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
					for (i = 0; i < 8*12; i++)
					{
						if (period >= modPeriods[i])
						{
							p->note = (uint8_t)i + 1;
							break;
						}
					}

					p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
					p->efx = bytes[2] & 0x0F;
					p->efxData = bytes[3];
				}
			}
		}

		for (a = 0; a < b; a++)
		{
			if (tmpPatternEmpty(a))
			{
				if (patternTmp[a] != NULL)
				{
					free(patternTmp[a]);
					patternTmp[a] = NULL;
				}
			}
		}
	}

	// pattern command conversion
	for (a = 0; a < b; a++)
	{
		if (patternTmp[a] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.numChannels; k++)
			{
				note_t *p = &patternTmp[a][(j * MAX_CHANNELS) + k];

				if (p->efx == 0xC)
				{
					if (p->efxData > 64)
						p->efxData = 64;
				}
				else if (p->efx == 0x1)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0x2)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0x5)
				{
					if (p->efxData == 0)
						p->efx = 0x3;
				}
				else if (p->efx == 0x6)
				{
					if (p->efxData == 0)
						p->efx = 0x4;
				}
				else if (p->efx == 0xA)
				{
					if (p->efxData == 0)
						p->efx = 0;
				}
				else if (p->efx == 0xE)
				{
					// check if certain E commands are empty
					if (p->efxData == 0x10 || p->efxData == 0x20 || p->efxData == 0xA0 || p->efxData == 0xB0)
					{
						p->efx = 0;
						p->efxData = 0;
					}
				}

				if (modFormat == FORMAT_NT || modFormat == FORMAT_HMNT)
				{
					// any Dxx == D00 in NT/HMNT
					if (p->efx == 0xD)
						p->efxData = 0;

					// effect F with param 0x00 does nothing in NT/HMNT
					if (p->efx == 0xF && p->efxData == 0)
						p->efx = 0;
				}
				else if (modFormat == FORMAT_FLT4 || modFormat == FORMAT_FLT8) // Startrekker
				{
					if (p->efx == 0xE) // remove unsupported "assembly macros" command
					{
						p->efx = 0;
						p->efxData = 0;
					}

					// Startrekker is always in vblank mode, and limits speed to 0x1F
					if (p->efx == 0xF && p->efxData > 0x1F)
						p->efxData = 0x1F;
				}
			}
		}
	}

	for (a = 0; a < 31; a++)
	{
		if (hdr.smp[a].length == 0)
			continue;

		if (!allocateTmpInstr(1+a))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+a]);

		s = &instrTmp[1+a]->smp[0];

		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(s->name, songTmp.instrName[1+a], 22);

		if (modFormat == FORMAT_HMNT) // finetune in "His Master's NoiseTracker" is different
			hdr.smp[a].finetune = (uint8_t)((-hdr.smp[a].finetune & 0x1F) >> 1); // one more bit of precision, + inverted

		s->length = 2 * SWAP16(hdr.smp[a].length);
		s->finetune = FINETUNE_MOD2XM(hdr.smp[a].finetune);
		s->volume = hdr.smp[a].volume;
		s->loopStart = 2 * SWAP16(hdr.smp[a].loopStart);
		s->loopLength = 2 * SWAP16(hdr.smp[a].loopLength);

		// fix for poorly converted STK (< v2.5) -> PT/NT modules
		if (s->loopLength > 2 && s->loopStart+s->loopLength > s->length)
		{
			if ((s->loopStart >> 1) + s->loopLength <= s->length)
				s->loopStart >>= 1;
		}

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
			s->flags |= LOOP_FWD; // enable loop

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

		if (GET_LOOPTYPE(s->flags) == LOOP_OFF) // clear loopLength and loopStart on non-looping samples...
		{
			s->loopLength = 0;
			s->loopStart = 0;
		}
	}

	if (tooManyChannels)
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
