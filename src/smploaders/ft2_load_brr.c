/* Super Nintendo BRR sample loader (based on work by _astriid_, but heavily modified)
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

#define BRR_RATIO(x) (((x) * 16) / 9)

static int16_t s1, s2;

bool detectBRR(FILE *f)
{
	if (f == NULL)
		return false;

	uint32_t oldPos = (uint32_t)ftell(f);
	fseek(f, 0, SEEK_END);
	uint32_t filesize = (uint32_t)ftell(f);

	const uint32_t filesizeMod9 = filesize % 9;

	if (filesize < 11 || filesize > 65536 || (filesizeMod9 != 0 && filesizeMod9 != 2))
		goto error; // definitely not a BRR file

	rewind(f);

	uint32_t blockBytes = filesize;
	if (filesizeMod9 == 2) // skip loop block word
	{
		fseek(f, 2, SEEK_CUR);
		blockBytes -= 2;
	}

	uint32_t numBlocks = blockBytes / 9;

	// if the first block is the last, this is very unlikely to be a real BRR sample
	uint8_t header = (uint8_t)fgetc(f);
	if (header & 1)
		goto error;

	/* Test the shift range of a few blocks.
	** While it's possible to have a shift value above 12 (illegal),
	** it's rare in dumped BRR samples. I have personally seen 13,
	** but never above, so let's test for >13 then.
	*/
	uint32_t blocksToTest = 8;
	if (blocksToTest > numBlocks)
		blocksToTest = numBlocks;

	for (uint32_t i = 0; i < blocksToTest; i++)
	{
		const uint8_t shift = header >> 4;
		if (shift > 13)
			goto error;

		fseek(f, 8, SEEK_CUR);
		header = (uint8_t)fgetc(f);
	}

	fseek(f, oldPos, SEEK_SET);
	return true;

error:
	fseek(f, oldPos, SEEK_SET);
	return false;
}

static int16_t decodeSample(int8_t nybble, int32_t shift, int32_t filter)
{
	int32_t smp = (nybble << shift) >> 1;
	if (shift >= 13)
		smp &= ~2047; // invalid shift clamping

	switch (filter)
	{
		default: break;

		case 1:
			smp += (s1 * 15) >> 4;
			break;

		case 2:
			smp += (s1 * 61) >> 5;
			smp -= (s2 * 15) >> 4;
			break;

		case 3:
			smp += (s1 * 115) >> 6;
			smp -= (s2 *  13) >> 4;
			break;
	}

	// clamp as 16-bit, even if we decode into a 15-bit sample
	smp = CLAMP(smp, -32768, 32767);

	// 15-bit clip
	if (smp & 16384)
		smp |= ~16383;
	else
		smp &= 16383;

	// shuffle last samples (store as 15-bit sample)
	s2 = s1;
	s1 = (int16_t)smp;

	return (int16_t)(smp << 1); // multiply by two to get 16-bit scale
}

bool loadBRR(FILE *f, uint32_t filesize)
{
	sample_t *s = &tmpSmp;

	uint32_t blockBytes = filesize, loopStart = 0;
	if ((filesize % 9) == 2) // loop header present
	{
		uint16_t loopStartBlock;
		fread(&loopStartBlock, 2, 1, f);
		loopStart = BRR_RATIO(loopStartBlock);
		blockBytes -= 2;
	}

	uint32_t sampleLength = BRR_RATIO(blockBytes);
	if (!allocateSmpData(s, sampleLength, true))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	uint32_t shift = 0, filter = 0;
	bool loopFlag = false, endFlag = false;

	s1 = s2 = 0; // clear last BRR samples (for decoding)

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (uint32_t i = 0; i < blockBytes; i++)
	{
		const uint32_t blockOffset = i % 9;
		const uint8_t byte = (uint8_t)fgetc(f);

		if (blockOffset == 0) // this byte is the BRR header
		{
			shift = byte >> 4;
			filter = (byte & 0x0C) >> 2;
			loopFlag = !!(byte & 0x02);
			endFlag = !!(byte & 0x01);
			continue;
		}

		// decode samples
		*ptr16++ = decodeSample((int8_t)byte >> 4, shift, filter);
		*ptr16++ = decodeSample((int8_t)(byte << 4) >> 4, shift, filter);

		if (endFlag && blockOffset == 8)
		{
			sampleLength = BRR_RATIO(i+1);
			break;
		}
	}

	s->volume = 64;
	s->panning = 128;
	s->flags |= SAMPLE_16BIT;
	s->length = sampleLength;

	if (loopFlag) // XXX: Maybe this is not how to do it..?
	{
		s->flags |= LOOP_FORWARD;
		s->loopStart = loopStart;
		s->loopLength = sampleLength - loopStart;
	}

	return true;
}
