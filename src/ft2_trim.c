// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_sample_ed.h"
#include "ft2_gui.h"
#include "scopes/ft2_scopes.h"
#include "ft2_pattern_ed.h"
#include "ft2_replayer.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_structs.h"

// this is truly a mess, but it works...

static char byteFormatBuffer[64], tmpInstrName[1 + MAX_INST][22 + 1], tmpInstName[MAX_INST][22 + 1];
static bool removePatt, removeInst, removeSamp, removeChans, removeSmpDataAfterLoop, convSmpsTo8Bit;
static uint8_t instrUsed[MAX_INST], instrOrder[MAX_INST], pattUsed[MAX_PATTERNS], pattOrder[MAX_PATTERNS];
static int16_t oldPattLens[MAX_PATTERNS], tmpPattLens[MAX_PATTERNS];
static int64_t xmSize64 = -1, xmAfterTrimSize64 = -1, spaceSaved64 = -1;
static note_t *oldPatts[MAX_PATTERNS], *tmpPatt[MAX_PATTERNS];
static instr_t *tmpInstr[1 + MAX_INST], *tmpInst[MAX_INST]; // tmpInstr[x] = copy of instr[x] for "after trim" size calculation
static SDL_Thread *trimThread;

void pbTrimCalc(void);

static void freeTmpInstruments(void)
{
	for (int32_t i = 0; i <= MAX_INST; i++)
	{
		if (tmpInstr[i] != NULL)
		{
			// don't free samples, as the pointers are shared with main instruments...

			free(tmpInstr[i]);
			tmpInstr[i] = NULL;
		}
	}
}

static bool setTmpInstruments(void)
{
	freeTmpInstruments();

	for (int32_t i = 0; i <= MAX_INST; i++)
	{
		if (instr[i] != NULL)
		{
			tmpInstr[i] = (instr_t *)malloc(sizeof (instr_t));
			if (tmpInstr[i] == NULL)
			{
				freeTmpInstruments();
				return false;
			}

			*tmpInstr[i] = *instr[i];
		}
	}

	return true;
}

static void remapInstrInSong(uint8_t src, uint8_t dst, int32_t ap)
{
	for (int32_t i = 0; i < ap; i++)
	{
		note_t *pattPtr = pattern[i];
		if (pattPtr == NULL)
			continue;

		const int32_t readLen = patternNumRows[i] * MAX_CHANNELS;

		note_t *p = pattPtr;
		for (int32_t j = 0; j < readLen; j++, p++)
		{
			if (p->instr == src)
				p->instr = dst;
		}
	}
}

static int16_t getUsedTempSamples(uint16_t insNum)
{
	if (tmpInstr[insNum] == NULL)
		return 0;

	instr_t *ins = tmpInstr[insNum];

	int16_t i = 16 - 1;
	while (i >= 0 && ins->smp[i].dataPtr == NULL && ins->smp[i].name[0] == '\0')
		i--;

	/* Yes, 'i' can be -1 here, and will be set to at least 0
	** because of ins->ta values. Possibly an FT2 bug...
	**/
	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->note2SampleLUT[j] > i)
			i = ins->note2SampleLUT[j];
	}

	return i+1;
}

static int64_t getTempInsAndSmpSize(void)
{
	int16_t j;

	int16_t ai = MAX_INST;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && tmpInstrName[ai][0] == '\0')
		ai--;

	int64_t currSize64 = 0;

	// count instrument and sample data size in song
	for (int16_t i = 1; i <= ai; i++)
	{
		if (tmpInstr[i] == NULL)
			j = 0;
		else
			j = i;

		const int16_t a = getUsedTempSamples(i);
		if (a > 0)
			currSize64 += INSTR_HEADER_SIZE + (a * sizeof (xmSmpHdr_t));
		else
			currSize64 += 22+11;

		instr_t *ins = tmpInstr[j];
		for (int16_t k = 0; k < a; k++)
		{
			sample_t *s = &ins->smp[k];
			if (s->dataPtr != NULL && s->length > 0)
			{
				if (s->flags & SAMPLE_16BIT)
					currSize64 += s->length << 1;
				else
					currSize64 += s->length;
			}
		}
	}

	return currSize64;
}

static void wipeInstrUnused(bool testWipeSize, int16_t *ai, int32_t ap, int32_t antChn)
{
	int16_t numRows;
	int32_t i, j, k;
	note_t *p;

	int32_t numInsts = *ai;

	// calculate what instruments are used
	memset(instrUsed, 0, numInsts);
	for (i = 0; i < ap; i++)
	{
		if (testWipeSize)
		{
			p = tmpPatt[i];
			numRows = tmpPattLens[i];
		}
		else
		{
			p = pattern[i];
			numRows = patternNumRows[i];
		}

		if (p == NULL)
			continue;

		for (j = 0; j < numRows; j++)
		{
			for (k = 0; k < antChn; k++)
			{
				uint8_t ins = p[(j * MAX_CHANNELS) + k].instr;
				if (ins > 0 && ins <= MAX_INST)
					instrUsed[ins-1] = true;
			}
		}
	}

	int16_t instToDel = 0;
	uint8_t newInst = 0;
	int16_t newNumInsts = 0;

	memset(instrOrder, 0, numInsts);
	for (i = 0; i < numInsts; i++)
	{
		if (instrUsed[i])
		{
			newNumInsts++;
			instrOrder[i] = newInst++;
		}
		else
		{
			instToDel++;
		}
	}

	if (instToDel == 0)
		return;

	if (testWipeSize)
	{
		for (i = 0; i < numInsts; i++)
		{
			if (!instrUsed[i] && tmpInstr[1+i] != NULL)
			{
				free(tmpInstr[1+i]);
				tmpInstr[1+i] = NULL;
			}
		}

		// relocate instruments

		memcpy(tmpInstName, &tmpInstrName[1], MAX_INST * sizeof (song.instrName[0]));
		memcpy(tmpInst, &tmpInstr[1], MAX_INST * sizeof (instr[0]));

		memset(&tmpInstr[1], 0, numInsts * sizeof (tmpInstr[0]));
		memset(&tmpInstrName[1], 0, numInsts * sizeof (tmpInstrName[0]));

		for (i = 0; i < numInsts; i++)
		{
			if (instrUsed[i])
			{
				newInst = instrOrder[i];

				memcpy(&tmpInstr[1+newInst], &tmpInst[i], sizeof (tmpInst[0]));
				strcpy(tmpInstrName[1+newInst], tmpInstName[i]);
			}
		}

		*ai = newNumInsts;
		return;
	}

	// clear unused instruments
	for (i = 0; i < numInsts; i++)
	{
		if (!instrUsed[i])
			freeInstr(1 + i);
	}

	// relocate instruments

	memcpy(tmpInstName, &song.instrName[1], MAX_INST * sizeof (song.instrName[0]));
	memcpy(tmpInst, &instr[1], MAX_INST * sizeof (instr[0]));

	memset(&instr[1], 0, numInsts * sizeof (instr[0]));
	memset(song.instrName[1], 0, numInsts * sizeof (song.instrName[0])); // XXX: Is this safe?

	for (i = 0; i < numInsts; i++)
	{
		if (instrUsed[i])
		{
			newInst = instrOrder[i];
			remapInstrInSong(1 + (uint8_t)i, 1 + newInst, ap);

			memcpy(&instr[1+newInst], &tmpInst[i], sizeof (instr[0]));
			strcpy(song.instrName[1+newInst], tmpInstName[i]);
		}
	}

	*ai = newNumInsts;

	setTmpInstruments();
}

static void wipePattsUnused(bool testWipeSize, int16_t *ap)
{
	uint8_t newPatt;
	int16_t i, *pLens;
	note_t **p;

	int16_t usedPatts = *ap;
	memset(pattUsed, 0, usedPatts);

	int16_t newUsedPatts = 0;
	for (i = 0; i < song.songLength; i++)
	{
		newPatt = song.orders[i];
		if (newPatt < usedPatts && !pattUsed[newPatt])
		{
			pattUsed[newPatt] = true;
			newUsedPatts++;
		}
	}

	if (newUsedPatts == 0 || newUsedPatts == usedPatts)
		return; // nothing to do!

	newPatt = 0;
	memset(pattOrder, 0, usedPatts);
	for (i = 0; i < usedPatts; i++)
	{
		if (pattUsed[i])
			pattOrder[i] = newPatt++;
	}

	if (testWipeSize)
	{
		p = tmpPatt;
		pLens = tmpPattLens;
	}
	else
	{
		p = pattern;
		pLens = patternNumRows;
	}

	memcpy(oldPatts, p, usedPatts * sizeof (note_t *));
	memcpy(oldPattLens, pLens, usedPatts * sizeof (int16_t));
	memset(p, 0, usedPatts * sizeof (note_t *));
	memset(pLens, 0, usedPatts * sizeof (int16_t));

	// relocate patterns
	for (i = 0; i < usedPatts; i++)
	{
		p[i] = NULL;

		if (!pattUsed[i])
		{
			if (!testWipeSize && oldPatts[i] != NULL)
			{
				free(oldPatts[i]);
				oldPatts[i] = NULL;
			}
		}
		else
		{
			newPatt = pattOrder[i];
			p[newPatt] = oldPatts[i];
			pLens[newPatt] = oldPattLens[i];
		}
	}

	if (!testWipeSize)
	{
		for (i = 0; i < MAX_PATTERNS; i++)
		{
			if (pattern[i] == NULL)
				patternNumRows[i] = 64;
		}

		// reorder order list (and clear unused entries)
		for (i = 0; i < 256; i++)
		{
			if (i < song.songLength)
				song.orders[i] = pattOrder[song.orders[i]];
			else
				song.orders[i] = 0;
		}
	}

	*ap = newUsedPatts;
}

static void wipeSamplesUnused(bool testWipeSize, int16_t ai)
{
	uint8_t smpUsed[16], smpOrder[16];
	int16_t j, k, l;
	instr_t *ins;
	sample_t tempSamples[16];

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (instr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = instr[l];
			l = getUsedSamples(i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = tmpInstr[l];
			l = getUsedTempSamples(i);
		}

		memset(smpUsed, 0, l);
		if (l > 0)
		{
			sample_t *s = ins->smp;
			for (j = 0; j < l; j++, s++)
			{
				// check if sample is referenced in instrument
				for (k = 0; k < 96; k++)
				{
					if (ins->note2SampleLUT[k] == j)
					{
						smpUsed[j] = true;
						break; // sample is used
					}
				}

				if (k == 96)
				{
					// sample is unused

					if (s->dataPtr != NULL && !testWipeSize)
						freeSmpData(s);

					memset(s, 0, sizeof (sample_t));
				}
			}

			// create re-order list
			uint8_t newSamp = 0;
			memset(smpOrder, 0, l);
			for (j = 0; j < l; j++)
			{
				if (smpUsed[j])
					smpOrder[j] = newSamp++;
			}

			// re-order samples

			memcpy(tempSamples, ins->smp, l * sizeof (sample_t));
			memset(ins->smp, 0, l * sizeof (sample_t));

			for (j = 0; j < l; j++)
			{
				if (smpUsed[j])
					ins->smp[smpOrder[j]] = tempSamples[j];
			}

			// re-order note->sample list
			for (j = 0; j < 96; j++)
			{
				newSamp = ins->note2SampleLUT[j];
				if (smpUsed[newSamp])
					ins->note2SampleLUT[j] = smpOrder[newSamp];
				else
					ins->note2SampleLUT[j] = 0;
			}
		}
	}
}

static void wipeSmpDataAfterLoop(bool testWipeSize, int16_t ai)
{
	int16_t l;
	instr_t *ins;

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (instr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = instr[l];
			l = getUsedSamples(i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				l = 0;
			else
				l = i;

			ins = tmpInstr[l];
			l = getUsedTempSamples(i);
		}

		sample_t *s = ins->smp;
		for (int16_t j = 0; j < l; j++, s++)
		{
			if (s->dataPtr != NULL && GET_LOOPTYPE(s->flags) != LOOP_OFF && s->length > 0 && s->length > s->loopStart+s->loopLength)
			{
				if (!testWipeSize)
					unfixSample(s);

				s->length = s->loopStart + s->loopLength;
				if (!testWipeSize)
				{
					if (s->length <= 0)
					{
						s->length = 0;
						freeSmpData(s);
					}
					else
					{
						reallocateSmpData(s, s->length, !!(s->flags & SAMPLE_16BIT));
					}
				}

				if (!testWipeSize)
					fixSample(s);
			}
		}
	}
}

static void convertSamplesTo8bit(bool testWipeSize, int16_t ai)
{
	int16_t k;
	instr_t *ins;

	for (int16_t i = 1; i <= ai; i++)
	{
		if (!testWipeSize)
		{
			if (instr[i] == NULL)
				k = 0;
			else
				k = i;

			ins = instr[k];
			k = getUsedSamples(i);
		}
		else
		{
			if (tmpInstr[i] == NULL)
				k = 0;
			else
				k = i;

			ins = tmpInstr[k];
			k = getUsedTempSamples(i);
		}

		sample_t *s = ins->smp;
		for (int16_t j = 0; j < k; j++, s++)
		{
			if (s->dataPtr != NULL && s->length > 0 && (s->flags & SAMPLE_16BIT))
			{
				if (testWipeSize)
				{
					s->flags &= ~SAMPLE_16BIT;
				}
				else
				{
					unfixSample(s);

					const int16_t *src16 = (const int16_t *)s->dataPtr;
					int8_t *dst8 = s->dataPtr;

					for (int32_t a = 0; a < s->length; a++)
						dst8[a] = src16[a] >> 8;

					s->flags &= ~SAMPLE_16BIT;

					reallocateSmpData(s, s->length, true);
					fixSample(s);
				}
			}
		}
	}
}

static uint16_t getPackedPattSize(note_t *p, int32_t numRows, int32_t antChn)
{
	uint8_t bytes[sizeof (note_t)];

	uint16_t totalPackLen = 0;
	uint8_t *pattPtr = (uint8_t *)p;
	uint8_t *writePtr = pattPtr;

	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < antChn; chn++)
		{
			bytes[0] = *pattPtr++;
			bytes[1] = *pattPtr++;
			bytes[2] = *pattPtr++;
			bytes[3] = *pattPtr++;
			bytes[4] = *pattPtr++;

			uint8_t *firstBytePtr = writePtr++;

			uint8_t packBits = 0;
			if (bytes[0] > 0) { packBits |= 1; writePtr++; } // note
			if (bytes[1] > 0) { packBits |= 2; writePtr++; } // instrument
			if (bytes[2] > 0) { packBits |= 4; writePtr++; } // volume column
			if (bytes[3] > 0) { packBits |= 8; writePtr++; } // effect

			if (packBits == 15) // first four bits set?
			{
				// no packing needed, write pattern data as is
				totalPackLen += 5;
				writePtr += 5;
				continue;
			}

			if (bytes[4] > 0) writePtr++; // effect parameter

			totalPackLen += (uint16_t)(writePtr - firstBytePtr); // bytes writen
		}

		// skip unused channels
		pattPtr += sizeof (note_t) * (MAX_CHANNELS - antChn);
	}

	return totalPackLen;
}

static bool tmpPatternEmpty(uint16_t pattNum, int32_t numChannels)
{
	if (tmpPatt[pattNum] == NULL)
		return true;

	uint8_t *scanPtr = (uint8_t *)tmpPatt[pattNum];
	int32_t scanLen = numChannels * sizeof (note_t);
	int32_t numRows = tmpPattLens[pattNum];

	for (int32_t i = 0; i < numRows; i++, scanPtr += TRACK_WIDTH)
	{
		for (int32_t j = 0; j < scanLen; j++)
		{
			if (scanPtr[j] != 0)
				return false;
		}
	}

	return true;
}

static int64_t calculateXMSize(void)
{
	// count header size in song
	int64_t currSize64 = sizeof (xmHdr_t);

	// count number of patterns that would be saved
	int16_t ap = MAX_PATTERNS;
	do
	{
		if (patternEmpty(ap - 1))
			ap--;
		else
			break;
	}
	while (ap > 0);

	// count number of instruments
	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(ai) == 0 && song.instrName[ai][0] == '\0')
		ai--;

	// count packed pattern data size in song
	for (int16_t i = 0; i < ap; i++)
	{
		currSize64 += sizeof (xmPatHdr_t);
		if (!patternEmpty(i))
			currSize64 += getPackedPattSize(pattern[i], patternNumRows[i], song.numChannels);
	}

	// count instrument and sample data size in song
	for (int16_t i = 1; i <= ai; i++)
	{
		int16_t j;
		if (instr[i] == NULL)
			j = 0;
		else
			j = i;

		const int16_t a = getUsedSamples(i);
		if (a > 0)
			currSize64 += INSTR_HEADER_SIZE + (a * sizeof (xmSmpHdr_t));
		else
			currSize64 += 22+11;

		instr_t *ins = instr[j];
		for (int16_t k = 0; k < a; k++)
		{
			sample_t* s = &ins->smp[k];
			if (s->dataPtr != NULL && s->length > 0)
			{
				if (s->flags & SAMPLE_16BIT)
					currSize64 += s->length << 1;
				else
					currSize64 += s->length;
			}
		}
	}

	return currSize64;
}

static int64_t calculateTrimSize(void)
{
	int16_t i, j, k;

	int32_t numChannels = song.numChannels;
	int32_t pattDataLen = 0;
	int32_t newPattDataLen = 0;
	int64_t bytes64 = 0;
	int64_t oldInstrSize64 = 0;

	// copy over temp data
	memcpy(tmpPatt, pattern, sizeof (tmpPatt));
	memcpy(tmpPattLens, patternNumRows, sizeof (tmpPattLens));
	memcpy(tmpInstrName, song.instrName, sizeof (tmpInstrName));

	if (!setTmpInstruments())
	{
		okBox(0, "System message", "Not enough memory!");
		return 0;
	}

	// get current size of all instruments and their samples
	if (removeInst || removeSamp || removeSmpDataAfterLoop || convSmpsTo8Bit)
		oldInstrSize64 = getTempInsAndSmpSize();

	// count number of patterns that would be saved
	int16_t ap = MAX_PATTERNS;
	do
	{
		if (tmpPatternEmpty(ap - 1, numChannels))
			ap--;
		else
			break;
	}
	while (ap > 0);

	// count number of instruments that would be saved
	int16_t ai = MAX_INST;
	while (ai > 0 && getUsedTempSamples(ai) == 0 && tmpInstrName[ai][0] == '\0')
		ai--;

	// calculate "remove unused samples" size
	if (removeSamp) wipeSamplesUnused(true, ai);

	// calculate "remove sample data after loop" size
	if (removeSmpDataAfterLoop) wipeSmpDataAfterLoop(true, ai);

	// calculate "convert samples to 8-bit" size
	if (convSmpsTo8Bit) convertSamplesTo8bit(true, ai);

	// get old pattern data length
	if (removeChans || removePatt)
	{
		for (i = 0; i < ap; i++)
		{
			pattDataLen += sizeof (xmPatHdr_t);
			if (!tmpPatternEmpty(i, numChannels))
				pattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}
	}

	// calculate "remove unused channels" size
	if (removeChans)
	{
		// get real number of channels
		int16_t highestChan = -1;
		for (i = 0; i < ap; i++)
		{
			note_t *pattPtr = tmpPatt[i];
			if (pattPtr == NULL)
				continue;

			const int16_t numRows = tmpPattLens[i];
			for (j = 0; j < numRows; j++)
			{
				for (k = 0; k < numChannels; k++)
				{
					note_t *p = &pattPtr[(j * MAX_CHANNELS) + k];
					if (p->note > 0 || p->instr > 0 || p->vol > 0 || p->efx > 0 || p->efxData > 0)
					{
						if (k > highestChan)
							highestChan = k;
					}
				}
			}
		}

		// set new number of channels (and make it an even number)
		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1)
				highestChan++;

			numChannels = (uint8_t)(CLAMP(highestChan, 2, numChannels));
		}
	}

	// calculate "remove unused patterns" size
	if (removePatt) wipePattsUnused(true, &ap);

	// calculate new pattern data size
	if (removeChans || removePatt)
	{
		for (i = 0; i < ap; i++)
		{
			newPattDataLen += sizeof (xmPatHdr_t);
			if (!tmpPatternEmpty(i, numChannels))
				newPattDataLen += getPackedPattSize(tmpPatt[i], tmpPattLens[i], numChannels);
		}

		assert(pattDataLen >= newPattDataLen);

		if (pattDataLen > newPattDataLen)
			bytes64 += (pattDataLen - newPattDataLen);
	}

	// calculate "remove unused instruments" size
	if (removeInst) wipeInstrUnused(true, &ai, ap, numChannels);

	// calculate new instruments and samples size
	if (removeInst || removeSamp || removeSmpDataAfterLoop || convSmpsTo8Bit)
	{
		int64_t newInstrSize64 = getTempInsAndSmpSize();

		assert(oldInstrSize64 >= newInstrSize64);
		if (oldInstrSize64 > newInstrSize64)
			bytes64 += (oldInstrSize64 - newInstrSize64);
	}

	freeTmpInstruments();
	return bytes64;
}

static int32_t SDLCALL trimThreadFunc(void *ptr)
{
	int16_t i, j, k;

	if (!setTmpInstruments())
	{
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	// audio callback is not running now, so we're safe

	// count number of patterns
	int16_t ap = MAX_PATTERNS;
	do
	{
		if (patternEmpty(ap - 1))
			ap--;
		else
			break;
	}
	while (ap > 0);

	// count number of instruments
	int16_t ai = MAX_INST;
	while (ai > 0 && getUsedSamples(ai) == 0 && song.instrName[ai][0] == '\0')
		ai--;

	// remove unused samples
	if (removeSamp)
		wipeSamplesUnused(false, ai);

	// remove sample data after loop
	if (removeSmpDataAfterLoop)
		wipeSmpDataAfterLoop(false, ai);

	// convert samples to 8-bit
	if (convSmpsTo8Bit)
		convertSamplesTo8bit(false, ai);

	// removed unused channels
	if (removeChans)
	{
		// count used channels
		int16_t highestChan = -1;
		for (i = 0; i < ap; i++)
		{
			note_t *pattPtr = pattern[i];
			if (pattPtr == NULL)
				continue;

			const int16_t numRows = patternNumRows[i];
			for (j = 0; j < numRows; j++)
			{
				for (k = 0; k < song.numChannels; k++)
				{
					note_t *p = &pattPtr[(j * MAX_CHANNELS) + k];
					if (p->note > 0 || p->vol > 0 || p->instr > 0 || p->efx > 0 || p->efxData > 0)
					{
						if (k > highestChan)
							highestChan = k;
					}
				}
			}
		}

		// set new 'channels used' number
		if (highestChan >= 0)
		{
			highestChan++;
			if (highestChan & 1)
				highestChan++;

			song.numChannels = (uint8_t)(CLAMP(highestChan, 2, song.numChannels));
		}

		// clear potentially unused channel data
		if (song.numChannels < MAX_CHANNELS)
		{
			for (i = 0; i < MAX_PATTERNS; i++)
			{
				note_t *p = pattern[i];
				if (p == NULL)
					continue;

				const int16_t numRows = patternNumRows[i];
				for (j = 0; j < numRows; j++)
					memset(&p[(j * MAX_CHANNELS) + song.numChannels], 0, sizeof (note_t) * (MAX_CHANNELS - song.numChannels));
			}
		}
	}

	// clear unused patterns
	if (removePatt)
		wipePattsUnused(false, &ap);

	// remove unused instruments
	if (removeInst)
		wipeInstrUnused(false, &ai, ap, song.numChannels);

	freeTmpInstruments();
	editor.trimThreadWasDone = true;

	return true;
	(void)ptr;
}

void trimThreadDone(void)
{
	if (removePatt)
		setPos(song.songPos, song.row, false);

	if (removeInst)
	{
		editor.currVolEnvPoint = 0;
		editor.currPanEnvPoint = 0;
	}

	updateTextBoxPointers();

	hideTopScreen();
	showTopScreen(true);
	showBottomScreen();

	if (removeChans)
	{
		if (ui.patternEditorShown)
		{
			if (ui.channelOffset > song.numChannels-ui.numChannelsShown)
				setScrollBarPos(SB_CHAN_SCROLL, song.numChannels - ui.numChannelsShown, true);
		}

		if (cursor.ch >= ui.channelOffset+ui.numChannelsShown)
			cursor.ch = ui.channelOffset+ui.numChannelsShown - 1;
	}

	checkMarkLimits();

	if (removeSamp || convSmpsTo8Bit)
		updateSampleEditorSample();

	pbTrimCalc();
	setSongModifiedFlag();
	resumeAudio();
	setMouseBusy(false);
}

static char *formatBytes(uint64_t bytes, bool roundUp)
{
	double dBytes;

	if (bytes == 0)
	{
		strcpy(byteFormatBuffer, "0");
		return byteFormatBuffer;
	}

	bytes %= 1000ULL*1024*1024*999; // wrap around gigabytes in case of overflow
	if (bytes >= 1024ULL*1024*1024*9)
	{
		// gigabytes
		dBytes = bytes / (1024.0*1024.0*1024.0);
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fGB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dGB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*1024*9)
	{
		// megabytes
		dBytes = bytes / (1024.0*1024.0);
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fMB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dMB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else if (bytes >= 1024*9)
	{
		// kilobytes
		dBytes = bytes / 1024.0;
		if (dBytes < 100)
			sprintf(byteFormatBuffer, "%.1fkB", dBytes);
		else
			sprintf(byteFormatBuffer, "%dkB", roundUp ? (int32_t)ceil(dBytes) : (int32_t)dBytes);
	}
	else
	{
		// bytes
		sprintf(byteFormatBuffer, "%d", (int32_t)bytes);
	}

	return byteFormatBuffer;
}

void drawTrimScreen(void)
{
	char sizeBuf[16];

	drawFramework(0,   92, 136, 81, FRAMEWORK_TYPE1);
	drawFramework(136, 92, 155, 81, FRAMEWORK_TYPE1);

	textOutShadow(4,    95, PAL_FORGRND, PAL_DSKTOP2, "What to remove:");
	textOutShadow(19,  109, PAL_FORGRND, PAL_DSKTOP2, "Unused patterns");
	textOutShadow(19,  122, PAL_FORGRND, PAL_DSKTOP2, "Unused instruments");
	textOutShadow(19,  135, PAL_FORGRND, PAL_DSKTOP2, "Unused samples");
	textOutShadow(19,  148, PAL_FORGRND, PAL_DSKTOP2, "Unused channels");
	textOutShadow(19,  161, PAL_FORGRND, PAL_DSKTOP2, "Smp. dat. after loop");

	textOutShadow(155,  96, PAL_FORGRND, PAL_DSKTOP2, "Conv. samples to 8-bit");
	textOutShadow(140, 111, PAL_FORGRND, PAL_DSKTOP2, ".xm size before");
	textOutShadow(140, 124, PAL_FORGRND, PAL_DSKTOP2, ".xm size after");
	textOutShadow(140, 137, PAL_FORGRND, PAL_DSKTOP2, "Bytes to save");

	if (xmSize64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(xmSize64, true));
		textOut(287 - textWidth(sizeBuf), 111, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(287 - textWidth("Unknown"), 111, PAL_FORGRND, "Unknown");
	}

	if (xmAfterTrimSize64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(xmAfterTrimSize64, true));
		textOut(287 - textWidth(sizeBuf), 124, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(287 - textWidth("Unknown"), 124, PAL_FORGRND, "Unknown");
	}

	if (spaceSaved64 > -1)
	{
		sprintf(sizeBuf, "%s", formatBytes(spaceSaved64, false));
		textOut(287 - textWidth(sizeBuf), 137, PAL_FORGRND, sizeBuf);
	}
	else
	{
		textOut(287 - textWidth("Unknown"), 137, PAL_FORGRND, "Unknown");
	}

	showCheckBox(CB_TRIM_PATT);
	showCheckBox(CB_TRIM_INST);
	showCheckBox(CB_TRIM_SAMP);
	showCheckBox(CB_TRIM_CHAN);
	showCheckBox(CB_TRIM_SMPD);
	showCheckBox(CB_TRIM_CONV);
	showPushButton(PB_TRIM_CALC);
	showPushButton(PB_TRIM_TRIM);
}

void hideTrimScreen(void)
{
	hideCheckBox(CB_TRIM_PATT);
	hideCheckBox(CB_TRIM_INST);
	hideCheckBox(CB_TRIM_SAMP);
	hideCheckBox(CB_TRIM_CHAN);
	hideCheckBox(CB_TRIM_SMPD);
	hideCheckBox(CB_TRIM_CONV);
	hidePushButton(PB_TRIM_CALC);
	hidePushButton(PB_TRIM_TRIM);

	ui.trimScreenShown = false;
	ui.scopesShown = true;
	drawScopeFramework();
}

void showTrimScreen(void)
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.trimScreenShown = true;
	ui.scopesShown = false;

	drawTrimScreen();
}

void toggleTrimScreen(void)
{
	if (ui.trimScreenShown)
		hideTrimScreen();
	else
		showTrimScreen();
}

void setInitialTrimFlags(void)
{
	removePatt = true;
	removeInst = true;
	removeSamp = true;
	removeChans = true;
	removeSmpDataAfterLoop = true;
	convSmpsTo8Bit = false;

	checkBoxes[CB_TRIM_PATT].checked = true;
	checkBoxes[CB_TRIM_INST].checked = true;
	checkBoxes[CB_TRIM_SAMP].checked = true;
	checkBoxes[CB_TRIM_CHAN].checked = true;
	checkBoxes[CB_TRIM_SMPD].checked = true;
	checkBoxes[CB_TRIM_CONV].checked = false;
}

void cbTrimUnusedPatt(void)
{
	removePatt ^= 1;
}

void cbTrimUnusedInst(void)
{
	removeInst ^= 1;
}

void cbTrimUnusedSamp(void)
{
	removeSamp ^= 1;
}

void cbTrimUnusedChans(void)
{
	removeChans ^= 1;
}

void cbTrimUnusedSmpData(void)
{
	removeSmpDataAfterLoop ^= 1;
}

void cbTrimSmpsTo8Bit(void)
{
	convSmpsTo8Bit ^= 1;
}

void pbTrimCalc(void)
{
	xmSize64 = calculateXMSize();
	spaceSaved64 = calculateTrimSize();

	xmAfterTrimSize64 = xmSize64 - spaceSaved64;
	if (xmAfterTrimSize64 < 0)
		xmAfterTrimSize64 = 0;

	if (ui.trimScreenShown)
		drawTrimScreen();
}

void pbTrimDoTrim(void)
{
	if (!removePatt && !removeInst && !removeSamp && !removeChans && !removeSmpDataAfterLoop && !convSmpsTo8Bit)
		return; // nothing to trim...

	if (okBox(2, "System request", "Are you sure you want to trim the song? Making a backup of the song first is recommended.") != 1)
		return;

	mouseAnimOn();
	pauseAudio();

	trimThread = SDL_CreateThread(trimThreadFunc, NULL, NULL);
	if (trimThread == NULL)
	{
		resumeAudio();
		mouseAnimOff();
		return;
	}

	SDL_DetachThread(trimThread);
}

void resetTrimSizes(void)
{
	xmSize64 = -1;
	xmAfterTrimSize64 = -1;
	spaceSaved64 = -1;

	if (ui.trimScreenShown)
		drawTrimScreen();
}
