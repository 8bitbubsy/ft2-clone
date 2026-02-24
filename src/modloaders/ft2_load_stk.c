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
	sample_t *s;
	modSmpHdr_t *srcSmp;
	stkHdr_t header;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	bool veryLateSTKVerFlag = false; // "DFJ SoundTracker III" nad later
	bool lateSTKVerFlag = false; // "TJC SoundTracker II" and later

	if (filesize < sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&header, 0, sizeof (stkHdr_t));
	if (fread(&header, 1, sizeof (header), f) != sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (header.CIAVal == 0) // a CIA value of 0 results in 120
		header.CIAVal = 120;

	if (header.numOrders < 1 || header.numOrders > 128 || header.CIAVal > 220)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memcpy(songTmp.orders, header.orders, 128);

	songTmp.numChannels = 4;
	songTmp.songLength = header.numOrders;
	songTmp.BPM = 125;
	songTmp.speed = 6;

	srcSmp = header.smp;
	for (int32_t i = 0; i < 15; i++, srcSmp++)
	{
		memcpy(songTmp.instrName[1+i], srcSmp->name, 22);

		/* Only late versions of Ultimate SoundTracker supports samples larger than 9999 bytes.
		** If found, we know for sure that this is a late STK module.
		*/
		const int32_t sampleLen = 2*SWAP16(srcSmp->length);
		if (sampleLen > 9999)
			lateSTKVerFlag = true;
	}

	// jjk55.mod by Jesper Kyd has a bogus STK tempo value that should be ignored (hackish!)
	if (!strcmp("jjk55", header.name))
		header.CIAVal = 120;

	if (header.CIAVal != 120) // 120 is a special case and means 50Hz (125BPM)
	{
		if (header.CIAVal > 220)
			header.CIAVal = 220;

		// convert UST tempo to BPM
		uint16_t ciaPeriod = (240 - header.CIAVal) * 122;
		
		double dHz = 709379.0 / (ciaPeriod+1); // +1, CIA triggers on underflow

		songTmp.BPM = (uint16_t)((dHz * (125.0 / 50.0)) + 0.5);
	}

	memcpy(songTmp.name, header.name, 20);

	// count number of patterns
	int32_t numPatterns = 0;
	for (int32_t i = 0; i < 128; i++)
	{
		if (songTmp.orders[i] > numPatterns)
			numPatterns = songTmp.orders[i];
	}
	numPatterns++;

	// patterns

	for (int32_t i = 0; i < numPatterns; i++)
	{
		if (!allocateTmpPatt(i, 64))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		fread(tmpBuffer, 1, 64 * 4 * 4, f);
		uint8_t *pattPtr = tmpBuffer;

		for (int32_t row = 0; row < 64; row++)
		{
			for (int32_t ch = 0; ch < 4; ch++, pattPtr += 4)
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
	}

	// pattern command conversion
	for (int32_t i = 0; i < numPatterns; i++)
	{
		if (patternTmp[i] == NULL)
			continue;

		for (int32_t row = 0; row < 64; row++)
		{
			for (int32_t ch = 0; ch < songTmp.numChannels; ch++)
			{
				note_t *p = &patternTmp[i][(row * MAX_CHANNELS) + ch];

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

	// samples

	srcSmp = header.smp;
	for (int32_t i = 0; i < 15; i++, srcSmp++)
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

		memcpy(s->name, srcSmp->name, 22);

		s->volume = srcSmp->volume;
		s->length = 2 * SWAP16(srcSmp->length);
		s->loopStart = SWAP16(srcSmp->loopStart); // in STK, loopStart = bytes, not words
		s->loopLength = 2 * SWAP16(srcSmp->loopLength);

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
			s->flags |= LOOP_FORWARD; // enable loop
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
