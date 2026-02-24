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

static void convertPatternEffect(note_t *p, uint8_t modFormat)
{
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

bool loadMOD(FILE *f, uint32_t filesize)
{
	uint8_t numChannels;
	sample_t *s;
	modSmpHdr_t *srcSmp;
	modHdr_t header;

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

	uint8_t modFormat = getModType(&numChannels, header.ID);
	if (modFormat == FORMAT_UNKNOWN)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (modFormat == FORMAT_MK && header.numOrders == 129)
		header.numOrders = 127; // fixes a specific copy of beatwave.mod (FIXME: Do we want to keep this hack?)

	if (numChannels == 0 || header.numOrders < 1 || header.numOrders > 128)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	bool tooManyChannels = numChannels > MAX_CHANNELS;

	songTmp.numChannels = tooManyChannels ? MAX_CHANNELS : numChannels;
	songTmp.songLength = header.numOrders;
	songTmp.songLoopStart = header.songLoopStart;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	memcpy(songTmp.orders, header.orders, 128);

	srcSmp = header.smp;
	for (int32_t i = 0; i < 31; i++, srcSmp++)
	{
		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(songTmp.instrName[1+i], srcSmp->name, 22);
	}

	memcpy(songTmp.name, header.name, 20);

	// count number of patterns
	int32_t numPatterns = 0;
	for (int32_t i = 0; i < 128; i++)
	{
		if (modFormat == FORMAT_FLT8)
			songTmp.orders[i] >>= 1;

		if (songTmp.orders[i] > numPatterns)
			numPatterns = songTmp.orders[i];
	}
	numPatterns++;

	// patterns

	if (modFormat != FORMAT_FLT8)
	{
		// normal pattern loading

		for (int32_t i = 0; i < numPatterns; i++)
		{
			if (!allocateTmpPatt(i, 64))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			fread(tmpBuffer, 1, 64 * numChannels * 4, f);
			uint8_t *pattPtr = tmpBuffer;

			for (int32_t row = 0; row < 64; row++)
			{
				for (int32_t ch = 0; ch < songTmp.numChannels; ch++, pattPtr += 4)
				{
					note_t *p = &patternTmp[i][(row * MAX_CHANNELS) + ch];

					// period to note
					uint16_t period = ((pattPtr[0] & 0x0F) << 8) | pattPtr[1];
					for (int32_t note = 0; note < 8*12; note++)
					{
						if (period >= modPeriods[note])
						{
							p->note = (uint8_t)note + 1;
							break;
						}
					}

					p->instr = (pattPtr[0] & 0xF0) | (pattPtr[2] >> 4);
					p->efx = pattPtr[2] & 0x0F;
					p->efxData = pattPtr[3];
					convertPatternEffect(p, modFormat);
				}

				if (tooManyChannels)
					pattPtr += (numChannels - MAX_CHANNELS) * 4;
			}
		}
	}
	else
	{
		// FLT8 pattern loading

		for (int32_t i = 0; i < numPatterns; i++)
		{
			if (!allocateTmpPatt(i, 64))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}
		}

		for (int32_t i = 0; i < numPatterns * 2; i++)
		{
			int32_t pattNum = i >> 1;
			int32_t chnOffset = (i & 1) * 4;

			fread(tmpBuffer, 1, 64 * 4 * 4, f);
			uint8_t *pattPtr = tmpBuffer;

			for (int32_t row = 0; row < 64; row++)
			{
				for (int32_t ch = 0; ch < 4; ch++, pattPtr += 4)
				{
					note_t *p = &patternTmp[pattNum][(row * MAX_CHANNELS) + (ch+chnOffset)];

					// period to note
					uint16_t period = ((pattPtr[0] & 0x0F) << 8) | pattPtr[1];
					for (int32_t note = 0; note < 8*12; note++)
					{
						if (period >= modPeriods[note])
						{
							p->note = (uint8_t)note + 1;
							break;
						}
					}

					p->instr = (pattPtr[0] & 0xF0) | (pattPtr[2] >> 4);
					p->efx = pattPtr[2] & 0x0F;
					p->efxData = pattPtr[3];
					convertPatternEffect(p, modFormat);
				}
			}
		}
	}

	// samples

	srcSmp = header.smp;
	for (int32_t i = 0; i < 31; i++, srcSmp++)
	{
		if (srcSmp->length == 0)
			continue;

		if (!allocateTmpInstr(1+i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		setNoEnvelope(instrTmp[1+i]);
		s = &instrTmp[1+i]->smp[0];

		// copy over sample name if format isn't "His Master's Noisetracker" (junk sample names)
		if (modFormat != FORMAT_HMNT)
			memcpy(s->name, songTmp.instrName[1+i], 22);

		if (modFormat == FORMAT_HMNT) // finetune in "His Master's NoiseTracker" is different
			srcSmp->finetune = (uint8_t)((-srcSmp->finetune & 0x1F) >> 1); // one more bit of precision, + inverted

		s->length = 2 * SWAP16(srcSmp->length);
		s->finetune = FINETUNE_MOD2XM(srcSmp->finetune);
		s->volume = srcSmp->volume;
		s->loopStart = 2 * SWAP16(srcSmp->loopStart);
		s->loopLength = 2 * SWAP16(srcSmp->loopLength);

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
			s->flags |= LOOP_FORWARD; // enable loop

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

		if (GET_LOOPTYPE(s->flags) == LOOP_DISABLED) // clear loopLength and loopStart on non-looping samples...
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
