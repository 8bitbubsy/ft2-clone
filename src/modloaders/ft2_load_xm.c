/* Fasttracker II (or compatible) XM loader
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

/* ModPlug Tracker & OpenMPT supports up to 32 samples per instrument for XMs -  we don't.
** For such modules, we use a temporary array here to store the extra sample data lengths
** we need to skip to be able to load the file (we lose the extra samples, though...).
*/
static uint32_t extraSampleLengths[32-MAX_SMP_PER_INST];

static bool loadInstrHeader(FILE *f, int32_t insNum);
static bool loadInstrSample(FILE *f, int32_t insNum);
static bool loadPatterns(FILE *f, int32_t numPatterns, uint16_t xmVersion);
static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t numChannels);
static void loadADPCMSample(FILE *f, sample_t *s); // ModPlug Tracker

bool loadXM(FILE *f, uint32_t filesize)
{
	xmHdr_t header;

	if (filesize < sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&header, 1, sizeof (header), f) != sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (header.version < 0x0102 || header.version > 0x0104)
	{
		loaderMsgBox("Error loading XM: Unsupported file version (v%01X.%02X).",
			(header.version >> 8) & 15, header.version & 0xFF);
		return false;
	}

	if (header.numOrders > MAX_ORDERS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 orders!");
		return false;
	}

	if (header.numPatterns > MAX_PATTERNS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 patterns!");
		return false;
	}

	if (header.numChannels == 0)
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	if (header.numInstr > 256) // if >128 instruments, we fake-load up to 128 extra instruments and discard them
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	fseek(f, 60 + header.headerSize, SEEK_SET);
	if (filesize != 336 && feof(f)) // 336 in length at this point = empty XM
	{
		loaderMsgBox("Error loading XM: The module is empty!");
		return false;
	}

	memcpy(songTmp.name, header.name, 20);
	songTmp.name[20] = '\0';

	songTmp.songLength = header.numOrders;
	songTmp.songLoopStart = header.songLoopStart;
	songTmp.numChannels = (uint8_t)header.numChannels;
	songTmp.BPM = header.BPM;
	songTmp.speed = header.speed;
	tmpLinearPeriodsFlag = !!(header.flags & 1);

	if (songTmp.songLength == 0)
		songTmp.songLength = 1; // songTmp.songTab is already empty
	else
		memcpy(songTmp.orders, header.orders, songTmp.songLength);

	// some strange XMs have the order list padded with 0xFF, remove them!
	for (int32_t i = 255; i >= 0; i--)
	{
		if (songTmp.orders[i] != 0xFF)
			break;

		if (songTmp.songLength > i)
			songTmp.songLength = (uint16_t)i;
	}

	// even though XM supports 256 orders, FT2 supports only 255...
	if (songTmp.songLength > 255)
		songTmp.songLength = 255;

	if (header.version < 0x0104)
	{
		// XM v1.02 and XM v1.03

		for (int32_t i = 1; i <= header.numInstr; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;
		}

		if (!loadPatterns(f, header.numPatterns, header.version))
			return false;

		for (int32_t i = 1; i <= header.numInstr; i++)
		{
			if (!loadInstrSample(f, i))
				return false;
		}
	}
	else
	{
		// XM v1.04 (latest version)

		if (!loadPatterns(f, header.numPatterns, header.version))
			return false;

		for (uint16_t i = 1; i <= header.numInstr; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;

			if (!loadInstrSample(f, i))
				return false;
		}
	}

	// if we temporarily loaded more than 128 instruments, clear the extra allocated memory
	if (header.numInstr > MAX_INST)
	{
		for (int32_t i = MAX_INST+1; i <= header.numInstr; i++)
		{
			if (instrTmp[i] != NULL)
			{
				free(instrTmp[i]);
				instrTmp[i] = NULL;
			}
		}
	}

	/* We support loading XMs with up to 32 samples per instrument (ModPlug/OpenMPT),
	** but only the first 16 will be loaded. Now make sure we set the number of samples
	** back to max 16 in the headers before loading is done.
	*/
	bool instrHasMoreThan16Samples = false;
	for (int32_t i = 1; i <= MAX_INST; i++)
	{
		if (instrTmp[i] != NULL && instrTmp[i]->numSamples > MAX_SMP_PER_INST)
		{
			instrHasMoreThan16Samples = true;
			instrTmp[i]->numSamples = MAX_SMP_PER_INST;
		}
	}

	if (songTmp.numChannels > MAX_CHANNELS)
	{
		songTmp.numChannels = MAX_CHANNELS;
		loaderMsgBox("Warning: Module contains >32 channels. The extra channels will be discarded!");
	}

	if (header.numInstr > MAX_INST)
		loaderMsgBox("Warning: Module contains >128 instruments. The extra instruments will be discarded!");

	if (instrHasMoreThan16Samples)
		loaderMsgBox("Warning: Module contains instrument(s) with >16 samples. The extra samples will be discarded!");

	return true;
}

static bool loadInstrHeader(FILE *f, int32_t insNum)
{
	uint32_t readSize;
	instr_t *ins;
	xmInsHdr_t ih;
	xmSmpHdr_t *src;
	sample_t *s;

	memset(extraSampleLengths, 0, sizeof (extraSampleLengths));
	memset(&ih, 0, sizeof (ih));

	fread(&readSize, 4, 1, f);
	fseek(f, -4, SEEK_CUR);

	// yes, some XMs can have a header size of 0, and it usually means 263 bytes (INSTR_HEADER_SIZE)
	if (readSize == 0 || readSize > INSTR_HEADER_SIZE)
		readSize = INSTR_HEADER_SIZE;

	if ((int32_t)readSize < 0)
	{
		loaderMsgBox("Error loading XM: This file is corrupt!");
		return false;
	}

	fread(&ih, readSize, 1, f); // read instrument header

	// FT2 bugfix: skip instrument header data if instrSize is above INSTR_HEADER_SIZE
	if (ih.instrSize > INSTR_HEADER_SIZE)
		fseek(f, ih.instrSize-INSTR_HEADER_SIZE, SEEK_CUR);

	if (ih.numSamples < 0 || ih.numSamples > 32)
	{
		loaderMsgBox("Error loading XM: This file is corrupt (or not supported)!");
		return false;
	}

	if (insNum <= MAX_INST) // copy over instrument name
		memcpy(songTmp.instrName[insNum], ih.name, 22);

	if (ih.numSamples > 0 && ih.numSamples <= 32)
	{
		if (!allocateTmpInstr(insNum))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		ins = instrTmp[insNum];

		// copy instrument header elements to our instrument struct

		memcpy(ins->note2SampleLUT, ih.note2SampleLUT, 96);
		memcpy(ins->volEnvPoints, ih.volEnvPoints, 12*2*sizeof(int16_t));
		memcpy(ins->panEnvPoints, ih.panEnvPoints, 12*2*sizeof(int16_t));
		ins->volEnvLength = ih.volEnvLength;
		ins->panEnvLength = ih.panEnvLength;
		ins->volEnvSustain = ih.volEnvSustain;
		ins->volEnvLoopStart = ih.volEnvLoopStart;
		ins->volEnvLoopEnd = ih.volEnvLoopEnd;
		ins->panEnvSustain = ih.panEnvSustain;
		ins->panEnvLoopStart = ih.panEnvLoopStart;
		ins->panEnvLoopEnd = ih.panEnvLoopEnd;
		ins->volEnvFlags = ih.volEnvFlags;
		ins->panEnvFlags = ih.panEnvFlags;
		ins->autoVibType = ih.vibType;
		ins->autoVibSweep = ih.vibSweep;
		ins->autoVibDepth = ih.vibDepth;
		ins->autoVibRate = ih.vibRate;
		ins->fadeout = ih.fadeout;
		ins->midiOn = (ih.midiOn == 1) ? true : false;
		ins->midiChannel = ih.midiChannel;
		ins->midiProgram = ih.midiProgram;
		ins->midiBend = ih.midiBend;
		ins->mute = (ih.mute == 1) ? true : false; // correct logic, don't change this!
		ins->numSamples = ih.numSamples; // used in loadInstrSample()

		int32_t sampleHeadersToRead = ih.numSamples;
		if (sampleHeadersToRead > MAX_SMP_PER_INST)
			sampleHeadersToRead = MAX_SMP_PER_INST;

		if (fread(ih.smp, sampleHeadersToRead * sizeof (xmSmpHdr_t), 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading!");
			return false;
		}

		// if instrument contains more than 16 sample headers (unsupported), skip them
		if (ih.numSamples > MAX_SMP_PER_INST) // can only be 0..32 at this point
		{
			const int32_t samplesToSkip = ih.numSamples-MAX_SMP_PER_INST;
			for (int32_t j = 0; j < samplesToSkip; j++)
			{
				fread(&extraSampleLengths[j], 4, 1, f); // used for skipping data in loadInstrSample()
				fseek(f, sizeof (xmSmpHdr_t)-4, SEEK_CUR);
			}
		}

		s = instrTmp[insNum]->smp;
		src = ih.smp;
		for (int32_t j = 0; j < sampleHeadersToRead; j++, s++, src++)
		{
			// copy sample header elements to our sample struct

			s->length = src->length;
			s->loopStart = src->loopStart;
			s->loopLength = src->loopLength;
			s->volume = src->volume;
			s->finetune = src->finetune;
			s->flags = src->flags;
			s->panning = src->panning;
			s->relativeNote = src->relativeNote;

			/* If the sample is 8-bit mono and nameLength (reserved) is 0xAD,
			** then this is a 4-bit ADPCM compressed sample (ModPlug Tracker).
			*/
			if (src->nameLength == 0xAD && !(src->flags & (SAMPLE_16BIT | SAMPLE_STEREO)))
				s->flags |= SAMPLE_ADPCM;

			memcpy(s->name, src->name, 22);

			// dst->dataPtr is set up later
		}
	}

	return true;
}

static bool loadInstrSample(FILE *f, int32_t insNum)
{
	if (instrTmp[insNum] == NULL)
		return true; // empty instrument, let's just pretend it got loaded successfully

	int32_t numSamples = instrTmp[insNum]->numSamples;
	if (numSamples > MAX_SMP_PER_INST)
		numSamples = MAX_SMP_PER_INST;

	sample_t *s = instrTmp[insNum]->smp;

	if (insNum > MAX_INST) // insNum > 128, just skip sample data
	{
		for (int32_t i = 0; i < numSamples; i++, s++)
		{
			if (s->length > 0)
				fseek(f, s->length, SEEK_CUR);
		}
	}
	else
	{
		for (int32_t i = 0; i < numSamples; i++, s++)
		{
			if (s->length <= 0)
			{
				s->length = 0;
				s->loopStart = 0;
				s->loopLength = 0;
				s->flags = 0;
			}
			else
			{
				const int32_t lengthInFile = s->length;

				bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
				bool stereoSample = !!(s->flags & SAMPLE_STEREO);
				bool adpcmSample = !!(s->flags & SAMPLE_ADPCM); // ModPlug Tracker

				if (sample16Bit) // we use units of samples (not bytes like in FT2)
				{
					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}

				if (s->length > MAX_SAMPLE_LEN)
					s->length = MAX_SAMPLE_LEN;

				if (!allocateSmpData(s, s->length, sample16Bit))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				if (adpcmSample)
				{
					loadADPCMSample(f, s);
				}
				else
				{
					if (sample16Bit)
						fread(s->dataPtr, 2, s->length, f);
					else
						fread(s->dataPtr, 1, s->length, f);

					const int32_t sampleLengthInBytes = SAMPLE_LENGTH_BYTES(s);
					if (sampleLengthInBytes < lengthInFile)
						fseek(f, lengthInFile-sampleLengthInBytes, SEEK_CUR);

					delta2Samp(s->dataPtr, s->length, s->flags);

					if (stereoSample) // stereo sample - already downmixed to mono in delta2samp()
					{
						s->length >>= 1;
						s->loopStart >>= 1;
						s->loopLength >>= 1;

						reallocateSmpData(s, s->length, sample16Bit); // dealloc unused memory
					}
				}
			}

			// remove stereo flag if present (already handled)
			if (s->flags & SAMPLE_STEREO)
				s->flags &= ~SAMPLE_STEREO;
		}
	}

	// skip sample headers if we have more than 16 samples in instrument
	if (instrTmp[insNum]->numSamples > MAX_SMP_PER_INST)
	{
		const int32_t samplesToSkip = instrTmp[insNum]->numSamples-MAX_SMP_PER_INST;
		for (int32_t i = 0; i < samplesToSkip; i++)
		{
			if (extraSampleLengths[i] > 0)
				fseek(f, extraSampleLengths[i], SEEK_CUR); 
		}
	}

	return true;
}

static bool loadPatterns(FILE *f, int32_t numPatterns, uint16_t xmVersion)
{
	uint8_t tmpLen;
	xmPatHdr_t ph;

	bool pattLenWarn = false;
	for (int32_t i = 0; i < numPatterns; i++)
	{
		if (fread(&ph.headerSize, 4, 1, f) != 1)
			goto pattCorrupt;

		if (fread(&ph.type, 1, 1, f) != 1)
			goto pattCorrupt;

		ph.numRows = 0;
		if (xmVersion == 0x0102)
		{
			if (fread(&tmpLen, 1, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataSize, 2, 1, f) != 1)
				goto pattCorrupt;

			ph.numRows = tmpLen + 1; // +1 in v1.02

			if (ph.headerSize > 8)
				fseek(f, ph.headerSize - 8, SEEK_CUR);
		}
		else
		{
			if (fread(&ph.numRows, 2, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataSize, 2, 1, f) != 1)
				goto pattCorrupt;

			if (ph.headerSize > 9)
				fseek(f, ph.headerSize - 9, SEEK_CUR);
		}

		if (feof(f))
			goto pattCorrupt;

		patternNumRowsTmp[i] = ph.numRows;
		if (patternNumRowsTmp[i] > MAX_PATT_LEN)
		{
			patternNumRowsTmp[i] = MAX_PATT_LEN;
			pattLenWarn = true;
		}

		if (ph.dataSize > 0)
		{
			if (!allocateTmpPatt(i, patternNumRowsTmp[i]))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			if (fread(tmpBuffer, 1, ph.dataSize, f) != ph.dataSize)
				goto pattCorrupt;

			unpackPatt((uint8_t *)patternTmp[i], tmpBuffer, patternNumRowsTmp[i], songTmp.numChannels);
			clearUnusedChannels(patternTmp[i], patternNumRowsTmp[i], songTmp.numChannels);
		}

		if (tmpPatternEmpty(i))
		{
			if (patternTmp[i] != NULL)
			{
				free(patternTmp[i]);
				patternTmp[i] = NULL;
			}

			patternNumRowsTmp[i] = 64;
		}
	}

	if (pattLenWarn)
		loaderMsgBox("This module contains pattern(s) with a length above 256! They will be truncated.");

	return true;

pattCorrupt:
	loaderMsgBox("Error loading XM: This file is corrupt!");
	return false;
}

static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t numChannels)
{
	if (dst == NULL)
		return;

	const int32_t srcEnd = len * (sizeof (note_t) * numChannels);
	int32_t srcIdx = 0;

	int32_t readChannels = numChannels;
	if (readChannels > MAX_CHANNELS)
		readChannels = MAX_CHANNELS;

	const int32_t pitch = sizeof (note_t) * (MAX_CHANNELS - numChannels);
	for (int32_t i = 0; i < len; i++)
	{
		int32_t j;
		for (j = 0; j < readChannels; j++)
		{
			if (srcIdx >= srcEnd)
				return; // error!

			const uint8_t note = *src++;
			if (note & 0x80)
			{
				*dst++ = (note & 0x01) ? *src++ : 0;
				*dst++ = (note & 0x02) ? *src++ : 0;
				*dst++ = (note & 0x04) ? *src++ : 0;
				*dst++ = (note & 0x08) ? *src++ : 0;
				*dst++ = (note & 0x10) ? *src++ : 0;
			}
			else
			{
				*dst++ = note;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
			}

			srcIdx += sizeof (note_t);
		}

		// if more than 32 channels, skip rest of the channels for this row
		for (; j < numChannels; j++)
		{
			if (srcIdx >= srcEnd)
				return; // error!

			const uint8_t note = *src++;
			if (note & 0x80)
			{
				if (note & 0x01) src++;
				if (note & 0x02) src++;
				if (note & 0x04) src++;
				if (note & 0x08) src++;
				if (note & 0x10) src++;
			}
			else
			{
				src++;
				src++;
				src++;
				src++;
			}

			srcIdx += sizeof (note_t);
		}

		// if song has <32 channels, align pointer to next row (skip unused channels)
		if (numChannels < MAX_CHANNELS)
			dst += pitch;
	}
}

static void loadADPCMSample(FILE *f, sample_t *s) // ModPlug Tracker
{
	int8_t deltaLUT[16];
	fread(deltaLUT, 1, 16, f);

	int8_t *dataPtr = s->dataPtr;
	const int32_t dataLength = (s->length + 1) / 2;

	int8_t currSample = 0;
	for (int32_t i = 0; i < dataLength; i++)
	{
		const uint8_t nibbles = (uint8_t)fgetc(f);

		currSample += deltaLUT[nibbles & 0x0F];
		*dataPtr++ = currSample;

		currSample += deltaLUT[nibbles >> 4];
		*dataPtr++ = currSample;
	}
}
