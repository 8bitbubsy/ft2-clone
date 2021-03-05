// Fasttracker II (or compatible) XM loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

static uint8_t packedPattData[65536];

/* ModPlug Tracker & OpenMPT supports up to 32 samples per instrument for XMs -  we don't.
** For such modules, we use a temporary array here to store the extra sample data lengths
** we need to skip to be able to load the file (we lose the extra samples, though...).
*/
static uint32_t extraSampleLengths[32-MAX_SMP_PER_INST];

static bool loadInstrHeader(FILE *f, uint16_t i);
static bool loadInstrSample(FILE *f, uint16_t i);
static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn);
static bool loadPatterns(FILE *f, uint16_t antPtn);
static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn);
static void sanitizeInstrument(instrTyp *ins);

bool loadXM(FILE *f, uint32_t filesize)
{
	uint16_t i;
	songHeaderTyp h;

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&h, 1, sizeof (h), f) != sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h.ver < 0x0102 || h.ver > 0x0104)
	{
		loaderMsgBox("Error loading XM: Unsupported file version (v%01X.%02X).", (h.ver >> 8) & 15, h.ver & 0xFF);
		return false;
	}

	if (h.len > MAX_ORDERS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 orders!");
		return false;
	}

	if (h.antPtn > MAX_PATTERNS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 patterns!");
		return false;
	}

	if (h.antChn == 0)
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	if (h.antInstrs > 256) // if >128 instruments, we fake-load up to 128 extra instruments and discard them
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	fseek(f, 60 + h.headerSize, SEEK_SET);
	if (filesize != 336 && feof(f)) // 336 in length at this point = empty XM
	{
		loaderMsgBox("Error loading XM: The module is empty!");
		return false;
	}

	memcpy(songTmp.name, h.name, 20);

	songTmp.len = h.len;
	songTmp.repS = h.repS;
	songTmp.antChn = (uint8_t)h.antChn;
	songTmp.speed = h.defSpeed;
	songTmp.tempo = h.defTempo;
	songTmp.ver = h.ver;
	tmpLinearPeriodsFlag = h.flags & 1;

	// non-FT2: clamp to max numbers that are okay for GUI
	songTmp.speed = CLAMP(songTmp.speed, 1, 999);
	songTmp.initialTempo = songTmp.tempo = CLAMP(songTmp.tempo, 1, 99);

	if (songTmp.len == 0)
		songTmp.len = 1; // songTmp.songTab is already empty
	else
		memcpy(songTmp.songTab, h.songTab, songTmp.len);

	// some strange XMs have the order list padded with 0xFF, remove them!
	for (int16_t j = 255; j >= 0; j--)
	{
		if (songTmp.songTab[j] != 0xFF)
			break;

		if (songTmp.len > j)
			songTmp.len = j;
	}

	// even though XM supports 256 orders, FT2 supports only 255...
	if (songTmp.len > 0xFF)
		songTmp.len = 0xFF;

	if (songTmp.ver < 0x0104)
	{
		// XM v1.02 and XM v1.03

		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;
		}

		if (!loadPatterns(f, h.antPtn))
			return false;

		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrSample(f, i))
				return false;
		}
	}
	else
	{
		// XM v1.04 (latest version)

		if (!loadPatterns(f, h.antPtn))
			return false;

		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;

			if (!loadInstrSample(f, i))
				return false;
		}
	}

	// if we temporarily loaded more than 128 instruments, clear the extra allocated memory
	if (h.antInstrs > MAX_INST)
	{
		for (i = MAX_INST+1; i <= h.antInstrs; i++)
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
	for (i = 1; i <= MAX_INST; i++)
	{
		if (instrTmp[i] != NULL && instrTmp[i]->antSamp > MAX_SMP_PER_INST)
		{
			instrHasMoreThan16Samples = true;
			instrTmp[i]->antSamp = MAX_SMP_PER_INST;
		}
	}

	if (songTmp.antChn > MAX_VOICES)
	{
		songTmp.antChn = MAX_VOICES;
		loaderMsgBox("Warning: Module contains >32 channels. The extra channels will be discarded!");
	}

	if (h.antInstrs > MAX_INST)
		loaderMsgBox("Warning: Module contains >128 instruments. The extra instruments will be discarded!");

	if (instrHasMoreThan16Samples)
		loaderMsgBox("Warning: Module contains instrument(s) with >16 samples. The extra samples will be discarded!");

	return true;
}

static bool loadInstrHeader(FILE *f, uint16_t i)
{
	uint8_t j;
	uint32_t readSize;
	instrHeaderTyp ih;
	instrTyp *ins;
	sampleHeaderTyp *src;
	sampleTyp *s;

	memset(extraSampleLengths, 0, sizeof (extraSampleLengths));
	memset(&ih, 0, sizeof (ih));
	fread(&ih.instrSize, 4, 1, f);

	readSize = ih.instrSize;

	// yes, some XMs can have a header size of 0, and it usually means 263 bytes (INSTR_HEADER_SIZE)
	if (readSize == 0 || readSize > INSTR_HEADER_SIZE)
		readSize = INSTR_HEADER_SIZE;

	if (readSize < 4)
	{
		loaderMsgBox("Error loading XM: This file is corrupt (or not supported)!");
		return false;
	}

	// load instrument data into temp buffer
	fread(ih.name, readSize-4, 1, f); // -4 = skip ih.instrSize

	// FT2 bugfix: skip instrument header data if instrSize is above INSTR_HEADER_SIZE
	if (ih.instrSize > INSTR_HEADER_SIZE)
		fseek(f, ih.instrSize-INSTR_HEADER_SIZE, SEEK_CUR);

	if (ih.antSamp < 0 || ih.antSamp > 32)
	{
		loaderMsgBox("Error loading XM: This file is corrupt (or not supported)!");
		return false;
	}

	if (i <= MAX_INST) // copy over instrument names
		memcpy(songTmp.instrName[i], ih.name, 22);

	if (ih.antSamp > 0)
	{
		if (!allocateTmpInstr(i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		// copy instrument header elements to our instrument struct

		ins = instrTmp[i];
		memcpy(ins->ta, ih.ta, 96);
		memcpy(ins->envVP, ih.envVP, 12*2*sizeof(int16_t));
		memcpy(ins->envPP, ih.envPP, 12*2*sizeof(int16_t));
		ins->envVPAnt = ih.envVPAnt;
		ins->envPPAnt = ih.envPPAnt;
		ins->envVSust = ih.envVSust;
		ins->envVRepS = ih.envVRepS;
		ins->envVRepE = ih.envVRepE;
		ins->envPSust = ih.envPSust;
		ins->envPRepS = ih.envPRepS;
		ins->envPRepE = ih.envPRepE;
		ins->envVTyp = ih.envVTyp;
		ins->envPTyp = ih.envPTyp;
		ins->vibTyp = ih.vibTyp;
		ins->vibSweep = ih.vibSweep;
		ins->vibDepth = ih.vibDepth;
		ins->vibRate = ih.vibRate;
		ins->fadeOut = ih.fadeOut;
		ins->midiOn = (ih.midiOn == 1) ? true : false;
		ins->midiChannel = ih.midiChannel;
		ins->midiProgram = ih.midiProgram;
		ins->midiBend = ih.midiBend;
		ins->mute = (ih.mute == 1) ? true : false; // correct logic, don't change this!
		ins->antSamp = ih.antSamp; // used in loadInstrSample()

		sanitizeInstrument(ins);

		int32_t sampleHeadersToRead = ih.antSamp;
		if (sampleHeadersToRead > MAX_SMP_PER_INST)
			sampleHeadersToRead = MAX_SMP_PER_INST;

		if (fread(ih.samp, sampleHeadersToRead * sizeof (sampleHeaderTyp), 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading!");
			return false;
		}

		// if instrument contains more than 16 sample headers (unsupported), skip them
		if (ih.antSamp > MAX_SMP_PER_INST) // can only be 0..32 at this point
		{
			const int32_t samplesToSkip = ih.antSamp-MAX_SMP_PER_INST;
			for (j = 0; j < samplesToSkip; j++)
			{
				fread(&extraSampleLengths[j], 4, 1, f); // used for skipping data in loadInstrSample()
				fseek(f, sizeof (sampleHeaderTyp)-4, SEEK_CUR);
			}
		}

		for (j = 0; j < sampleHeadersToRead; j++)
		{
			s = &instrTmp[i]->samp[j];
			src = &ih.samp[j];

			// copy sample header elements to our sample struct

			s->len = src->len;
			s->repS = src->repS;
			s->repL = src->repL;
			s->vol = src->vol;
			s->fine = src->fine;
			s->typ = src->typ;
			s->pan = src->pan;
			s->relTon = src->relTon;
			memcpy(s->name, src->name, 22);

			// dst->pek is set up later

			// sanitize stuff broken/unsupported samples (FT2 doesn't do this, but we do!)
			if (s->vol > 64)
				s->vol = 64;

			s->relTon = CLAMP(s->relTon, -48, 71);
		}
	}

	return true;
}

static bool loadInstrSample(FILE *f, uint16_t i)
{
	if (instrTmp[i] == NULL)
		return true; // empty instrument, let's just pretend it got loaded successfully

	uint16_t k = instrTmp[i]->antSamp;
	if (k > MAX_SMP_PER_INST)
		k = MAX_SMP_PER_INST;

	sampleTyp *s = instrTmp[i]->samp;

	if (i > MAX_INST) // insNum > 128, just skip sample data
	{
		for (uint16_t j = 0; j < k; j++, s++)
		{
			if (s->len > 0)
				fseek(f, s->len, SEEK_CUR);
		}
	}
	else
	{
		for (uint16_t j = 0; j < k; j++, s++)
		{
			// FT2: a sample with both forward loop and pingpong loop set results in pingpong
			if ((s->typ & 3) == 3)
				s->typ &= 0xFE; // remove forward loop flag

			int32_t l = s->len;
			if (l <= 0)
			{
				s->len = 0;
				s->repL = 0;
				s->repS = 0;

				if (s->typ & 32) // remove stereo flag if present
					s->typ &= ~32;
			}
			else
			{
				int32_t bytesToSkip = 0;
				if (l > MAX_SAMPLE_LEN)
				{
					bytesToSkip = l - MAX_SAMPLE_LEN;
					l = MAX_SAMPLE_LEN;
				}

				if (!allocateTmpSmpData(s, l))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				const int32_t bytesRead = (int32_t)fread(s->pek, 1, l, f);
				if (bytesRead < l)
				{
					const int32_t bytesToClear = l - bytesRead;
					memset(&s->pek[bytesRead], 0, bytesToClear);
				}

				if (bytesToSkip > 0)
					fseek(f, bytesToSkip, SEEK_CUR);

				delta2Samp(s->pek, l, s->typ);

				if (s->typ & 32) // stereo sample - already downmixed to mono in delta2samp()
				{
					s->typ &= ~32; // remove stereo flag

					s->len >>= 1;
					s->repL >>= 1;
					s->repS >>= 1;

					reallocateTmpSmpData(s, s->len); // dealloc unused memory
				}
			}

			// NON-FT2 FIX: Align to 2-byte if 16-bit sample
			if (s->typ & 16)
			{
				s->repL &= 0xFFFFFFFE;
				s->repS &= 0xFFFFFFFE;
				s->len &= 0xFFFFFFFE;
			}
		}
	}

	if (instrTmp[i]->antSamp > MAX_SMP_PER_INST)
	{
		const int32_t samplesToSkip = instrTmp[i]->antSamp-MAX_SMP_PER_INST;
		for (i = 0; i < samplesToSkip; i++)
		{
			if (extraSampleLengths[i] > 0)
				fseek(f, extraSampleLengths[i], SEEK_CUR); 
		}
	}

	return true;
}

static bool loadPatterns(FILE *f, uint16_t antPtn)
{
	uint8_t tmpLen;
	patternHeaderTyp ph;

	bool pattLenWarn = false;
	for (uint16_t i = 0; i < antPtn; i++)
	{
		if (fread(&ph.patternHeaderSize, 4, 1, f) != 1)
			goto pattCorrupt;

		if (fread(&ph.typ, 1, 1, f) != 1)
			goto pattCorrupt;

		ph.pattLen = 0;
		if (songTmp.ver == 0x0102)
		{
			if (fread(&tmpLen, 1, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataLen, 2, 1, f) != 1)
				goto pattCorrupt;

			ph.pattLen = tmpLen + 1; // +1 in v1.02

			if (ph.patternHeaderSize > 8)
				fseek(f, ph.patternHeaderSize - 8, SEEK_CUR);
		}
		else
		{
			if (fread(&ph.pattLen, 2, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataLen, 2, 1, f) != 1)
				goto pattCorrupt;

			if (ph.patternHeaderSize > 9)
				fseek(f, ph.patternHeaderSize - 9, SEEK_CUR);
		}

		if (feof(f))
			goto pattCorrupt;

		pattLensTmp[i] = ph.pattLen;
		if (pattLensTmp[i] > MAX_PATT_LEN)
		{
			pattLensTmp[i] = MAX_PATT_LEN;
			pattLenWarn = true;
		}

		if (ph.dataLen > 0)
		{
			if (!allocateTmpPatt(i, pattLensTmp[i]))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			if (fread(packedPattData, 1, ph.dataLen, f) != ph.dataLen)
				goto pattCorrupt;

			unpackPatt((uint8_t *)pattTmp[i], packedPattData, pattLensTmp[i], songTmp.antChn);
			clearUnusedChannels(pattTmp[i], pattLensTmp[i], songTmp.antChn);
		}

		if (tmpPatternEmpty(i))
		{
			if (pattTmp[i] != NULL)
			{
				free(pattTmp[i]);
				pattTmp[i] = NULL;
			}

			pattLensTmp[i] = 64;
		}
	}

	if (pattLenWarn)
		loaderMsgBox("This module contains pattern(s) with a length above 256! They will be truncated.");

	return true;

pattCorrupt:
	loaderMsgBox("Error loading XM: This file is corrupt!");
	return false;
}

static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn)
{
	int32_t j;

	if (dst == NULL)
		return;

	const int32_t srcEnd = len * (sizeof (tonTyp) * antChn);
	int32_t srcIdx = 0;

	int32_t numChannels = antChn;
	if (numChannels > MAX_VOICES)
		numChannels = MAX_VOICES;

	const int32_t pitch = sizeof (tonTyp) * (MAX_VOICES - antChn);
	for (int32_t i = 0; i < len; i++)
	{
		for (j = 0; j < numChannels; j++)
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

			srcIdx += sizeof (tonTyp);
		}

		// if more than 32 channels, skip rest of the channels for this row
		for (; j < antChn; j++)
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

			srcIdx += sizeof (tonTyp);
		}

		// if song has <32 channels, align pointer to next row (skip unused channels)
		if (antChn < MAX_VOICES)
			dst += pitch;
	}
}

static void sanitizeInstrument(instrTyp *ins) // FT2 doesn't do this, but we do!
{
	ins->midiProgram = CLAMP(ins->midiProgram, 0, 127);
	ins->midiBend = CLAMP(ins->midiBend, 0, 36);

	if (ins->midiChannel > 15) ins->midiChannel = 15;
	if (ins->vibDepth > 0x0F) ins->vibDepth = 0x0F;
	if (ins->vibRate > 0x3F) ins->vibRate = 0x3F;
	if (ins->vibTyp > 3) ins->vibTyp = 0;

	for (int32_t i = 0; i < 96; i++)
	{
		if (ins->ta[i] >= MAX_SMP_PER_INST)
			ins->ta[i] = MAX_SMP_PER_INST-1;
	}

	if (ins->envVPAnt > 12) ins->envVPAnt = 12;
	if (ins->envVRepS > 11) ins->envVRepS = 11;
	if (ins->envVRepE > 11) ins->envVRepE = 11;
	if (ins->envVSust > 11) ins->envVSust = 11;
	if (ins->envPPAnt > 12) ins->envPPAnt = 12;
	if (ins->envPRepS > 11) ins->envPRepS = 11;
	if (ins->envPRepE > 11) ins->envPRepE = 11;
	if (ins->envPSust > 11) ins->envPSust = 11;

	for (int32_t  i= 0; i < 12; i++)
	{
		if ((uint16_t)ins->envVP[i][0] > 32767) ins->envVP[i][0] = 32767;
		if ((uint16_t)ins->envPP[i][0] > 32767) ins->envPP[i][0] = 32767;
		if ((uint16_t)ins->envVP[i][1] > 64) ins->envVP[i][1] = 64;
		if ((uint16_t)ins->envPP[i][1] > 63) ins->envPP[i][1] = 63;
	}
}
