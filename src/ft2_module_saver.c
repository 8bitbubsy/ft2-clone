// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_sample_ed.h"
#include "ft2_module_loader.h"
#include "ft2_tables.h"
#include "ft2_structs.h"

static int8_t smpChunkBuf[1024];
static uint8_t packedPattData[65536], modPattData[64*32*4];
static SDL_Thread *thread;

static const char modIDs[32][5] =
{
	"1CHN", "2CHN", "3CHN", "4CHN", "5CHN", "6CHN", "7CHN", "8CHN",
	"9CHN", "10CH", "11CH", "12CH", "13CH", "14CH", "15CH", "16CH",
	"17CH", "18CH", "19CH", "20CH", "21CH", "22CH", "23CH", "24CH",
	"25CH", "26CH", "27CH", "28CH", "29CH", "30CH", "31CH", "32CH"
};

static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows);

bool saveXM(UNICHAR *filenameU)
{
	int16_t i, j, k, a;
	size_t result;
	xmHdr_t h;
	xmPatHdr_t ph;
	instr_t *ins;
	xmInsHdr_t ih;
	sample_t *s;
	xmSmpHdr_t *dst;

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "Error opening file for saving, is it in use?", NULL);
		return false;
	}

	memcpy(h.ID, "Extended Module: ", 17);

	// song name
	int32_t nameLength = (int32_t)strlen(song.name);
	if (nameLength > 20)
		nameLength = 20;

	memset(h.name, ' ', 20); // yes, FT2 pads the name with spaces
	if (nameLength > 0)
		memcpy(h.name, song.name, nameLength);

	h.x1A = 0x1A;

	// program/tracker name
	nameLength = (int32_t)strlen(PROG_NAME_STR);
	if (nameLength > 20)
		nameLength = 20;

	memset(h.progName, ' ', 20); // yes, FT2 pads the name with spaces
	if (nameLength > 0)
		memcpy(h.progName, PROG_NAME_STR, nameLength);

	h.version = 0x0104;
	h.headerSize = 20 + 256;
	h.numOrders = song.songLength;
	h.songLoopStart = song.songLoopStart;
	h.numChannels = (uint16_t)song.numChannels;
	h.speed = song.speed;
	h.BPM = song.BPM;

	// count number of patterns
	i = MAX_PATTERNS;
	do
	{
		if (patternEmpty(i-1))
			i--;
		else
			break;
	}
	while (i > 0);
	h.numPatterns = i;

	// count number of instruments
	i = 128;
	while (i > 0 && getUsedSamples(i) == 0 && song.instrName[i][0] == '\0')
		i--;
	h.numInstr = i;

	h.flags = audio.linearPeriodsFlag;
	memcpy(h.orders, song.orders, 256);

	if (fwrite(&h, sizeof (h), 1, f) != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
		return false;
	}

	for (i = 0; i < h.numPatterns; i++)
	{
		if (patternEmpty(i))
		{
			if (pattern[i] != NULL)
			{
				free(pattern[i]);
				pattern[i] = NULL;
			}

			patternNumRows[i] = 64;
		}

		ph.headerSize = sizeof (xmPatHdr_t);
		ph.numRows = patternNumRows[i];
		ph.type = 0;

		if (pattern[i] == NULL)
		{
			ph.dataSize = 0;
			if (fwrite(&ph, ph.headerSize, 1, f) != 1)
			{
				fclose(f);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
				return false;
			}
		}
		else
		{
			ph.dataSize = packPatt(packedPattData, (uint8_t *)pattern[i], patternNumRows[i]);

			result = fwrite(&ph, ph.headerSize, 1, f);
			result += fwrite(packedPattData, ph.dataSize, 1, f);

			if (result != 2) // write was not OK
			{
				fclose(f);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
				return false;
			}
		}
	}

	memset(&ih, 0, sizeof (ih)); // important, clears reserved stuff

	for (i = 1; i <= h.numInstr; i++)
	{
		if (instr[i] == NULL)
			j = 0;
		else
			j = i;

		a = getUsedSamples(i);

		nameLength = (int32_t)strlen(song.instrName[i]);
		if (nameLength > 22)
			nameLength = 22;

		memset(ih.name, 0, 22); // pad with zero
		if (nameLength > 0)
			memcpy(ih.name, song.instrName[i], nameLength);

		ih.type = 0;
		ih.numSamples = a;
		ih.sampleSize = sizeof (xmSmpHdr_t);

		if (a > 0)
		{
			ins = instr[j];

			memcpy(ih.note2SampleLUT, ins->note2SampleLUT, 96);
			memcpy(ih.volEnvPoints, ins->volEnvPoints, 12*2*sizeof(int16_t));
			memcpy(ih.panEnvPoints, ins->panEnvPoints, 12*2*sizeof(int16_t));
			ih.volEnvLength = ins->volEnvLength;
			ih.panEnvLength = ins->panEnvLength;
			ih.volEnvSustain = ins->volEnvSustain;
			ih.volEnvLoopStart = ins->volEnvLoopStart;
			ih.volEnvLoopEnd = ins->volEnvLoopEnd;
			ih.panEnvSustain = ins->panEnvSustain;
			ih.panEnvLoopStart = ins->panEnvLoopStart;
			ih.panEnvLoopEnd = ins->panEnvLoopEnd;
			ih.volEnvFlags = ins->volEnvFlags;
			ih.panEnvFlags = ins->panEnvFlags;
			ih.vibType = ins->autoVibType;
			ih.vibSweep = ins->autoVibSweep;
			ih.vibDepth = ins->autoVibDepth;
			ih.vibRate = ins->autoVibRate;
			ih.fadeout = ins->fadeout;
			ih.midiOn = ins->midiOn ? 1 : 0;
			ih.midiChannel = ins->midiChannel;
			ih.midiProgram = ins->midiProgram;
			ih.midiBend = ins->midiBend;
			ih.mute = ins->mute ? 1 : 0;
			ih.instrSize = INSTR_HEADER_SIZE;
			
			for (k = 0; k < a; k++)
			{
				s = &instr[j]->smp[k];
				dst = &ih.smp[k];

				bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

				dst->length = s->length;
				dst->loopStart = s->loopStart;
				dst->loopLength = s->loopLength;

				if (sample16Bit)
				{
					dst->length <<= 1;
					dst->loopStart <<= 1;
					dst->loopLength <<= 1;
				}

				dst->volume = s->volume;
				dst->finetune = s->finetune;
				dst->flags = s->flags;
				dst->panning = s->panning;
				dst->relativeNote = s->relativeNote;

				nameLength = (int32_t)strlen(s->name);
				if (nameLength > 22)
					nameLength = 22;

				dst->nameLength = (uint8_t)nameLength;

				memset(dst->name, ' ', 22); // yes, FT2 pads the name with spaces
				if (nameLength > 0)
					memcpy(dst->name, s->name, nameLength);

				if (s->dataPtr == NULL)
					dst->length = 0;
			}
		}
		else
		{
			ih.instrSize = 22 + 11;
		}

		if (fwrite(&ih, ih.instrSize + (a * sizeof (xmSmpHdr_t)), 1, f) != 1)
		{
			fclose(f);
			okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
			return false;
		}

		for (k = 1; k <= a; k++)
		{
			s = &instr[j]->smp[k-1];
			if (s->dataPtr != NULL)
			{
				unfixSample(s);
				samp2Delta(s->dataPtr, s->length, s->flags);

				result = fwrite(s->dataPtr, 1, SAMPLE_LENGTH_BYTES(s), f);

				delta2Samp(s->dataPtr, s->length, s->flags);
				fixSample(s);

				if (result != (size_t)SAMPLE_LENGTH_BYTES(s)) // write not OK
				{
					fclose(f);
					okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
					return false;
				}
			}
		}
	}

	removeSongModifiedFlag();

	fclose(f);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static bool saveMOD(UNICHAR *filenameU)
{
	int16_t i;
	int32_t j, k;
	instr_t *ins;
	sample_t *smp;
	modHdr_t hdr;

	// Commented out. This one was probably confusing to many people...
	/*
	if (audio.linearPeriodsFlag)
		okBoxThreadSafe(0, "System message", "Warning: \"Frequency slides\" is not set to Amiga!", NULL);
	*/

	int32_t songLength = song.songLength;
	if (songLength > 128)
	{
		songLength = 128;
		okBoxThreadSafe(0, "System message", "Warning: Song length is above 128!", NULL);
	}
	
	// calculate number of patterns referenced (max 128 orders)
	int32_t numPatterns = 0;
	for (i = 0; i < songLength; i++)
	{
		if (song.orders[i] > numPatterns)
			numPatterns = song.orders[i];
	}
	numPatterns++;

	if (numPatterns > 100)
	{
		numPatterns = 100;
		okBoxThreadSafe(0, "System message", "Warning: Song has more than 100 patterns!", NULL);
	}

	// check if song has more than 31 instruments
	for (i = 32; i <= 128; i++)
	{
		if (getRealUsedSamples(i) > 0)
		{
			okBoxThreadSafe(0, "System message", "Warning: Song has more than 31 instruments!", NULL);
			break;
		}
	}

	// check if the first 31 samples have a length above 65534 samples
	bool test = false;
	bool test2 = false;
	for (i = 1; i <= 31; i++)
	{
		ins = instr[i];
		if (ins == NULL)
			continue;

		smp = &ins->smp[0];

		if (smp->length > 131070)
			test = true;
		else if (smp->length > 65534)
			test2 = true;
	}
	if (test) okBoxThreadSafe(0, "System message", "Warning: Song has sample lengths that are too long for the MOD format!", NULL);
	else if (test2) okBoxThreadSafe(0, "System message", "Warning: Song has sample lengths above 65534! Not all MOD players support this.", NULL);

	// check if XM instrument features are being used
	test = false;
	for (i = 1; i <= 31; i++)
	{
		ins = instr[i];
		if (ins == NULL)
			continue;

		smp = &ins->smp[0];

		j = getRealUsedSamples(i);
		if (j > 1)
		{
			test = true;
			break;
		}

		if (j == 1)
		{
			if (ins->fadeout != 0 || ins->volEnvFlags != 0 || ins->panEnvFlags != 0 || ins->autoVibRate > 0 ||
				GET_LOOPTYPE(smp->flags) == LOOP_BIDI || smp->relativeNote != 0 || ins->midiOn)
			{
				test = true;
				break;
			}
		}
	}
	if (test) okBoxThreadSafe(0, "System message", "Warning: Song is using XM instrument features!", NULL);

	bool tooLongPatterns = false;
	bool tooManyInstr = false;
	bool incompatEfx = false;
	bool noteUnderflow = false;

	for (i = 0; i < numPatterns; i++)
	{
		if (pattern[i] == NULL)
			continue;

		if (patternNumRows[i] < 64)
		{
			okBoxThreadSafe(0, "System message", "Error: Pattern lengths can't be below 64! Module wasn't saved.", NULL);
			return false;
		}

		if (patternNumRows[i] > 64)
			tooLongPatterns = true;

		for (j = 0; j < 64; j++)
		{
			for (k = 0; k < song.numChannels; k++)
			{
				note_t *p = &pattern[i][(j * MAX_CHANNELS) + k];

				if (p->instr > 31)
					tooManyInstr = true;

				if (p->efx > 0xF || p->vol != 0)
					incompatEfx = true;

				// added security that wasn't present in FT2
				if (p->note > 0 && p->note < 10)
					noteUnderflow = true;
			}
		}
	}

	if (tooLongPatterns) okBoxThreadSafe(0, "System message", "Warning: Song has pattern lengths above 64!", NULL);
	if (tooManyInstr) okBoxThreadSafe(0, "System message", "Warning: Patterns have instrument numbers above 31!", NULL);
	if (incompatEfx) okBoxThreadSafe(0, "System message", "Warning: Patterns have incompatible effects!", NULL);
	if (noteUnderflow) okBoxThreadSafe(0, "System message", "Warning: Patterns have notes below A-0!", NULL);

	// save module now

	memset(&hdr, 0, sizeof (hdr));

	// song name
	int32_t nameLength = (int32_t)strlen(song.name);
	if (nameLength > 20)
		nameLength = 20;

	memset(hdr.name, 0, 20); // pad with zeroes
	if (nameLength > 0)
		memcpy(hdr.name, song.name, nameLength);

	hdr.numOrders = (uint8_t)songLength; // pre-clamped to 0..128

	hdr.songLoopStart = (uint8_t)song.songLoopStart;
	if (hdr.songLoopStart >= hdr.numOrders) // repeat-point must be lower than the song length
		hdr.songLoopStart = 0;

	memcpy(hdr.orders, song.orders, hdr.numOrders);

	if (song.numChannels == 4)
		memcpy(hdr.ID, (numPatterns > 64) ? "M!K!" : "M.K.", 4);
	else
		memcpy(hdr.ID, modIDs[song.numChannels-1], 4);

	// fill MOD sample headers
	for (i = 1; i <= 31; i++)
	{
		modSmpHdr_t *modSmp = &hdr.smp[i-1];

		nameLength = (int32_t)strlen(song.instrName[i]);
		if (nameLength > 22)
			nameLength = 22;

		memset(modSmp->name, 0, 22); // pad with zeroes
		if (nameLength > 0)
			memcpy(modSmp->name, song.instrName[i], nameLength);

		if (instr[i] != NULL && getRealUsedSamples(i) != 0)
		{
			smp = &instr[i]->smp[0];

			int32_t length = smp->length >> 1;
			int32_t loopStart = smp->loopStart >> 1;
			int32_t loopLength = smp->loopLength >> 1;

			// length/loopStart/loopLength are now in units of words

			if (length > UINT16_MAX)
				length = UINT16_MAX;

			if (GET_LOOPTYPE(smp->flags) == LOOP_OFF)
			{
				loopStart = 0;
				loopLength = 1;
			}
			else // looped sample
			{
				if (loopLength == 0) // ProTracker hates loopLengths of zero
					loopLength = 1;

				if (loopStart+loopLength > length)
				{
					loopStart = 0;
					loopLength = 1;
				}
			}

			modSmp->length = (uint16_t)SWAP16(length);
			modSmp->finetune = FINETUNE_XM2MOD(smp->finetune);
			modSmp->volume = smp->volume;
			modSmp->loopStart = (uint16_t)SWAP16(loopStart);
			modSmp->loopLength = (uint16_t)SWAP16(loopLength);
		}
	}

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "Error opening file for saving, is it in use?", NULL);
		return false;
	}

	// write header
	if (fwrite(&hdr, 1, sizeof (hdr), f) != sizeof (hdr))
	{
		okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
		goto modSaveError;
	}

	// write pattern data
	const int32_t patternBytes = song.numChannels * 64 * 4;
	for (i = 0; i < numPatterns; i++)
	{
		if (pattern[i] == NULL) // empty pattern
		{
			memset(modPattData, 0, patternBytes);
		}
		else
		{
			int32_t offs = 0;
			for (j = 0; j < 64; j++)
			{
				for (k = 0; k < song.numChannels; k++)
				{
					note_t *p = &pattern[i][(j * MAX_CHANNELS) + k];

					uint8_t inst = p->instr;
					uint8_t note = p->note;

					// FT2 bugfix: prevent overflow
					if (inst > 31)
						inst = 0;

					// FT2 bugfix: convert note-off into no note for MOD saving
					if (note == NOTE_OFF)
						note = 0;

					// FT2 bugfix: clamp notes below 10 (A-0) to prevent 12-bit period overflow
					if (note > 0 && note < 10)
						note = 10;

					if (note == 0)
					{
						modPattData[offs+0] = inst & 0xF0;
						modPattData[offs+1] = 0;
					}
					else
					{
						modPattData[offs+0] = (inst & 0xF0) | ((modPeriods[note-1] >> 8) & 0x0F);
						modPattData[offs+1] = modPeriods[note-1] & 0xFF;
					}

					// FT2 bugfix: if effect is overflowing (0xF in .MOD), set effect and param to 0
					if (p->efx > 0x0F)
					{
						modPattData[offs+2] = (inst & 0x0F) << 4;
						modPattData[offs+3] = 0;
					}
					else
					{
						modPattData[offs+2] = ((inst & 0x0F) << 4) | (p->efx & 0x0F);
						modPattData[offs+3] = p->efxData;
					}

					offs += 4;
				}
			}
		}

		if (fwrite(modPattData, 1, patternBytes, f) != (size_t)patternBytes)
		{
			okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
			goto modSaveError;
		}
	}

	// write sample data
	for (i = 1; i <= 31; i++)
	{
		if (instr[i] == NULL || getRealUsedSamples(i) == 0)
			continue;

		smp = &instr[i]->smp[0];
		if (smp->dataPtr == NULL || smp->length <= 0)
			continue;

		modSmpHdr_t *modSmp = &hdr.smp[i-1];

		unfixSample(smp);

		int32_t sampleBytes = SWAP16(modSmp->length) * 2;

		if (smp->flags & SAMPLE_16BIT) // 16-bit sample (convert to 8-bit)
		{
			int8_t *dstPtr = (int8_t *)smpChunkBuf;
			int32_t writeLen = sampleBytes;
			int32_t samplesWritten = 0;

			while (samplesWritten < writeLen) // write in chunks
			{
	 			int32_t samplesToWrite = sizeof (smpChunkBuf);
				if (samplesWritten+samplesToWrite > writeLen)
					samplesToWrite = writeLen - samplesWritten;

				int16_t *srcPtr16 = (int16_t *)smp->dataPtr + samplesWritten;
				for (j = 0; j < samplesToWrite; j++)
					dstPtr[j] = srcPtr16[j] >> 8; // convert 16-bit to 8-bit

				if (fwrite(dstPtr, 1, samplesToWrite, f) != (size_t)samplesToWrite)
				{
					fixSample(smp);
					okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
					goto modSaveError;
				}

				samplesWritten += samplesToWrite;
			}
		}
		else // 8-bit sample
		{
			if (fwrite(smp->dataPtr, 1, sampleBytes, f) != (size_t)sampleBytes)
			{
				fixSample(smp);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!", NULL);
				goto modSaveError;
			}
		}

		fixSample(smp);
	}

	fclose(f);
	removeSongModifiedFlag();

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;

modSaveError:
	fclose(f);
	return false;
}

static int32_t SDLCALL saveMusicThread(void *ptr)
{
	assert(editor.tmpFilenameU != NULL);
	if (editor.tmpFilenameU == NULL)
		return false;

	pauseAudio();

	if (editor.moduleSaveMode == 1)
		saveXM(editor.tmpFilenameU);
	else
		saveMOD(editor.tmpFilenameU);

	resumeAudio();
	return true;

	(void)ptr;
}

void saveMusic(UNICHAR *filenameU)
{
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(saveMusicThread, NULL, NULL);
	if (thread == NULL)
	{
		okBoxThreadSafe(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows)
{
	uint8_t bytes[5];

	if (pattPtr == NULL)
		return 0;

	uint16_t totalPackLen = 0;

	const int32_t pitch = sizeof (note_t) * (MAX_CHANNELS - song.numChannels);
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < song.numChannels; chn++)
		{
			bytes[0] = *pattPtr++;
			bytes[1] = *pattPtr++;
			bytes[2] = *pattPtr++;
			bytes[3] = *pattPtr++;
			bytes[4] = *pattPtr++;

			uint8_t *firstBytePtr = writePtr++;

			uint8_t packBits = 0;
			if (bytes[0] > 0) { packBits |= 1; *writePtr++ = bytes[0]; } // note
			if (bytes[1] > 0) { packBits |= 2; *writePtr++ = bytes[1]; } // instrument
			if (bytes[2] > 0) { packBits |= 4; *writePtr++ = bytes[2]; } // volume column
			if (bytes[3] > 0) { packBits |= 8; *writePtr++ = bytes[3]; } // effect

			if (packBits == 15) // first four bits set?
			{
				// no packing needed, write pattern data as is

				// point to first byte (and overwrite data)
				writePtr = firstBytePtr;

				*writePtr++ = bytes[0];
				*writePtr++ = bytes[1];
				*writePtr++ = bytes[2];
				*writePtr++ = bytes[3];
				*writePtr++ = bytes[4];

				totalPackLen += 5;
				continue;
			}

			if (bytes[4] > 0) { packBits |= 16; *writePtr++ = bytes[4]; } // effect parameter

			*firstBytePtr = packBits | 128; // write pack bits byte
			totalPackLen += (uint16_t)(writePtr - firstBytePtr); // bytes writen
		}

		// skip unused channels (unpacked patterns always have 32 channels)
		pattPtr += pitch;
	}

	return totalPackLen;
}
