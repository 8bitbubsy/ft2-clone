/* Ultimate SoundTracker (or compatible) STK loader
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
typedef struct stkHdr_t
{
	char name[20];
	modSmpHdr_t smp[15];
	uint8_t numOrders, CIAVal, orders[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
stkHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

bool loadSTK(FILE *f, uint32_t filesize)
{
	uint8_t bytes[4];
	int16_t i, j, k;
	uint16_t a, b;
	sample_t *s;
	stkHdr_t h;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	bool veryLateSTKVerFlag = false; // "DFJ SoundTracker III" nad later
	bool lateSTKVerFlag = false; // "TJC SoundTracker II" and later

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&h, 0, sizeof (stkHdr_t));
	if (fread(&h, 1, sizeof (h), f) != sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h.CIAVal == 0) // a CIA value of 0 results in 120
		h.CIAVal = 120;

	if (h.numOrders < 1 || h.numOrders > 128 || h.CIAVal > 220)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memcpy(songTmp.orders, h.orders, 128);

	songTmp.numChannels = 4;
	songTmp.songLength = h.numOrders;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	for (a = 0; a < 15; a++)
	{
		modSmpHdr_t *modSmp = &h.smp[a];
		memcpy(songTmp.instrName[1+a], modSmp->name, 22);

		/* Only late versions of Ultimate SoundTracker supports samples larger than 9999 bytes.
		** If found, we know for sure that this is a late STK module.
		*/
		const int32_t sampleLen = 2*SWAP16(modSmp->length);
		if (sampleLen > 9999)
			lateSTKVerFlag = true;
	}

	// jjk55.mod by Jesper Kyd has a bogus STK tempo value that should be ignored (hackish!)
	if (!strcmp("jjk55", h.name))
		h.CIAVal = 120;

	if (h.CIAVal != 120) // 120 is a special case and means 50Hz (125BPM)
	{
		// convert UST tempo to BPM
		uint16_t ciaPeriod = (240 - h.CIAVal) * 122;
		double dHz = 709379.0 / ciaPeriod;
		int32_t BPM = (int32_t)((dHz * 2.5) + 0.5);

		songTmp.BPM = (uint16_t)BPM;
	}

	memcpy(songTmp.name, h.name, 20);

	// count number of patterns
	b = 0;
	for (a = 0; a < 128; a++)
	{
		if (songTmp.orders[a] > b)
			b = songTmp.orders[a];
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
			for (k = 0; k < songTmp.numChannels; k++)
			{
				note_t *p = &patternTmp[a][(j * MAX_CHANNELS) + k];

				if (fread(bytes, 1, 4, f) != 4)
				{
					loaderMsgBox("Error: This file is either not a module, or is not supported.");
					return false;
				}

				// period to note
				uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
				for (i = 0; i < 3*12; i++)
				{
					if (period >= ptPeriods[i])
					{
						p->note = 1 + (3*12) + (uint8_t)i;
						break;
					}
				}

				p->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
				p->efx = bytes[2] & 0x0F;
				p->efxData = bytes[3];

				if (p->efx == 0xC || p->efx == 0xD || p->efx == 0xE)
				{
					// "TJC SoundTracker II" and later
					lateSTKVerFlag = true;
				}

				if (p->efx == 0xF)
				{
					// "DFJ SoundTracker III" and later
					lateSTKVerFlag = true;
					veryLateSTKVerFlag = true;
				}

				if (p->efx == 0xC)
				{
					if (p->efxData > 64)
						p->efxData = 64;
				}
				else if (p->efx == 0x1)
				{
					if (p->efxData == 0)
						p->efxData = 0;
				}
				else if (p->efx == 0x2)
				{
					if (p->efxData == 0)
						p->efxData = 0;
				}
				else if (p->efx == 0x5)
				{
					if (p->efxData == 0)
						p->efxData = 0x3;
				}
				else if (p->efx == 0x6)
				{
					if (p->efxData == 0)
						p->efxData = 0x4;
				}
				else if (p->efx == 0xA)
				{
					if (p->efxData == 0)
						p->efxData = 0;
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

	// pattern command conversion for non-PT formats
	for (a = 0; a < b; a++)
	{
		if (patternTmp[a] == NULL)
			continue;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < songTmp.numChannels; k++)
			{
				note_t *p = &patternTmp[a][(j * MAX_CHANNELS) + k];

				// convert STK effects to PT effects

				if (!lateSTKVerFlag)
				{
					// old SoundTracker 1.x commands

					if (p->efx == 1)
					{
						// arpeggio
						p->efx = 0;
					}
					else if (p->efx == 2)
					{
						// pitch slide
						if (p->efxData & 0xF0)
						{
							// pitch slide down
							p->efx = 2;
							p->efxData >>= 4;
						}
						else if (p->efxData & 0x0F)
						{
							// pitch slide up
							p->efx = 1;
						}
					}
				}
				else
				{
					// "DFJ SoundTracker II" or later

					if (p->efx == 0xD)
					{
						if (veryLateSTKVerFlag) // "DFJ SoundTracker III" or later
						{
							// pattern break w/ no param (param must be cleared to fix some songs)
							p->efxData = 0;
						}
						else
						{
							// volume slide
							p->efx = 0xA;
						}
					}
				}

				// effect F with param 0x00 does nothing in UST/STK (I think)
				if (p->efx == 0xF && p->efxData == 0)
					p->efx = 0;
			}
		}
	}

	for (a = 0; a < 15; a++)
	{
		if (h.smp[a].length == 0)
			continue;

		if (!allocateTmpInstr(1+a))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		setNoEnvelope(instrTmp[1+a]);

		s = &instrTmp[1+a]->smp[0];
		s->volume = h.smp[a].volume;
		s->length = 2 * SWAP16(h.smp[a].length);
		s->loopStart = SWAP16(h.smp[a].loopStart); // in STK, loopStart = bytes, not words
		s->loopLength = 2 * SWAP16(h.smp[a].loopLength);

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
			s->loopLength = 0;
			s->loopStart = 0;
		}

		/* In STK, only the loop area of a looped sample is played.
		** Skip loading of eventual data present before loop start.
		*/
		if (s->loopStart > 0 && s->loopLength < s->length)
		{
			s->length -= s->loopStart;
			fseek(f, s->loopStart, SEEK_CUR);
			s->loopStart = 0;
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
