// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <unistd.h> // chdir()
#endif
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_sample_ed.h"
#include "ft2_diskop.h"
#include "ft2_mouse.h"
#include "ft2_structs.h"

typedef struct wavHeader_t
{
	uint32_t chunkID, chunkSize, format, subchunk1ID, subchunk1Size;
	uint16_t audioFormat, numChannels;
	uint32_t sampleRate, byteRate;
	uint16_t blockAlign, bitsPerSample;
	uint32_t subchunk2ID, subchunk2Size;
} wavHeader_t;

typedef struct sampleLoop_t
{
	uint32_t dwIdentifier, dwType, dwStart, dwEnd, dwFraction, dwPlayCount;
} sampleLoop_t;

typedef struct samplerChunk_t
{
	uint32_t chunkID, chunkSize, dwManufacturer, dwProduct, dwSamplePeriod;
	uint32_t dwMIDIUnityNote, dwMIDIPitchFraction, dwSMPTEFormat;
	uint32_t dwSMPTEOffset, cSampleLoops, cbSamplerData;
	sampleLoop_t loop;
} samplerChunk_t;

typedef struct mptExtraChunk_t
{
	uint32_t chunkID, chunkSize, flags;
	uint16_t defaultPan, defaultVolume, globalVolume, reserved;
	uint8_t vibratoType, vibratoSweep, vibratoDepth, vibratoRate;
} mptExtraChunk_t;

static const char *rangedDataStr = "Ranged data from FT2";

// thread data
static bool saveRangeFlag;
static SDL_Thread *thread;

// restores modified interpolation tap samples after loopEnd (for .RAW/.IFF/.WAV samples after save)
static void fileRestoreFixedSampleData(UNICHAR *filenameU, uint32_t sampleDataOffset, sample_t *s)
{
	if (!s->isFixed)
		return; // nothing to restore

	int32_t sampleFixPos = s->fixedPos;
	int32_t sampleFixOffset = 0;
	int32_t samplesToWrite = MAX_RIGHT_TAPS;

	if (saveRangeFlag)
	{
		const int32_t markStart = getSampleRangeStart();
		const int32_t markEnd = getSampleRangeEnd();

		if (markStart > sampleFixPos+MAX_RIGHT_TAPS || markEnd < sampleFixPos)
			return; // nothing to do here

		if (markStart > sampleFixPos)
		{
			sampleFixOffset += markStart-sampleFixPos;
			samplesToWrite -= markStart-sampleFixPos;
		}

		sampleFixPos -= markStart;

		if (sampleFixPos + samplesToWrite > markEnd)
			samplesToWrite = markEnd - sampleFixPos;

		if (samplesToWrite < 0 || samplesToWrite > MAX_RIGHT_TAPS || sampleFixPos < 0 || sampleFixOffset < 0 || sampleFixOffset >= MAX_RIGHT_TAPS)
			return;
	}

	FILE* f = UNICHAR_FOPEN(filenameU, "r+"); // open in read+update mode
	if (f == NULL)
		return;

	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	if (sample16Bit)
		fseek(f, sampleDataOffset + (sampleFixPos * 2), SEEK_SET);
	else
		fseek(f, sampleDataOffset + sampleFixPos, SEEK_SET);

	if (sample16Bit)
	{
		fwrite(&s->fixedSmp[sampleFixOffset], sizeof (int16_t), samplesToWrite, f);
	}
	else
	{
		for (int32_t i = 0; i < samplesToWrite; i++)
		{
			int8_t fixedSmp = (int8_t)s->fixedSmp[sampleFixOffset+i];
			if (editor.sampleSaveMode == SMP_SAVE_MODE_WAV) // on 8-bit WAVs the sample data is unsigned
				fixedSmp ^= 0x80; // signed -> unsigned

			fwrite(&fixedSmp, sizeof (int8_t), 1, f);
		}
	}

	fclose(f);
}

static bool saveRawSample(UNICHAR *filenameU, bool saveRangedData)
{
	int8_t *samplePtr;
	uint32_t sampleLen;

	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || ins->smp[editor.curSmp].dataPtr == NULL || ins->smp[editor.curSmp].length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &instr[editor.curInstr]->smp[editor.curSmp];
	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	if (fwrite(samplePtr, sampleLen, 1, f) != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving sample: General I/O error!", NULL);
		return false;
	}

	fclose(f);

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, 0, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static void iffWriteChunkHeader(FILE *f, char *chunkName, uint32_t chunkLen)
{
	fwrite(chunkName, sizeof (int32_t), 1, f);
	chunkLen = SWAP32(chunkLen);
	fwrite(&chunkLen, sizeof (int32_t), 1, f);
}

static void iffWriteUint32(FILE *f, uint32_t value)
{
	value = SWAP32(value);
	fwrite(&value, sizeof (int32_t), 1, f);
}

static void iffWriteUint16(FILE *f, uint16_t value)
{
	value = SWAP16(value);
	fwrite(&value, sizeof (int16_t), 1, f);
}

static void iffWriteUint8(FILE *f, const uint8_t value)
{
	fwrite(&value, sizeof (int8_t), 1, f);
}

static void iffWriteChunkData(FILE *f, const void *data, size_t length)
{
	fwrite(data, sizeof (int8_t), length, f);
	if (length & 1) fputc(0, f); // write pad byte if chunk size is uneven
}

static bool saveIFFSample(UNICHAR *filenameU, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint32_t sampleLen, smpNameLen, chunkLen;

	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || ins->smp[editor.curSmp].dataPtr == NULL || ins->smp[editor.curSmp].length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &instr[editor.curInstr]->smp[editor.curSmp];

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	// "FORM" chunk
	iffWriteChunkHeader(f, "FORM", 0); // "FORM" chunk size is overwritten later
	iffWriteUint32(f, sample16Bit ? 0x31365356 : 0x38535658); // bitdepth - "16SV" (16-bit) or "8SVX" (8-bit)

	// "VHDR" chunk
	iffWriteChunkHeader(f, "VHDR", 20);

	if (!saveRangedData && GET_LOOPTYPE(smp->flags) != LOOP_DISABLED)
	{
		iffWriteUint32(f, smp->loopStart << sample16Bit); // oneShotHiSamples
		iffWriteUint32(f, smp->loopLength << sample16Bit); // repeatHiSamples
	}
	else
	{
		iffWriteUint32(f, 0); // oneShotHiSamples
		iffWriteUint32(f, 0); // repeatHiSamples
	}

	iffWriteUint32(f, 0); // samplesPerHiCycle

	// samplesPerSec
	uint32_t tmp32 = getSampleMiddleCRate(smp);
	if (tmp32 == 0 || tmp32 > 65535) tmp32 = 16726;
	iffWriteUint16(f, (uint16_t)tmp32);

	iffWriteUint8(f, 1); // ctOctave (number of samples)
	iffWriteUint8(f, 0); // sCompression
	iffWriteUint32(f, smp->volume * 1024); // volume (max: 65536/0x10000)

	// "NAME" chunk

	if (saveRangedData)
	{
		smpNamePtr = (char *)rangedDataStr;
		smpNameLen = (uint32_t)strlen(rangedDataStr);
	}
	else
	{
		smpNamePtr = smp->name;

		smpNameLen = 0;
		while (smpNameLen < 22)
		{
			if (smpNamePtr[smpNameLen] == '\0')
				break;

			smpNameLen++;
		}
	}

	// "NAME" chunk
	chunkLen = smpNameLen;
	if (chunkLen > 0)
	{
		iffWriteChunkHeader(f, "NAME", chunkLen);
		iffWriteChunkData(f, smpNamePtr, chunkLen);
	}

	// "ANNO" chunk (we put the program name here)
	chunkLen = sizeof (PROG_NAME_STR) - 1;
	iffWriteChunkHeader(f, "ANNO", chunkLen);
	iffWriteChunkData(f, PROG_NAME_STR, chunkLen);

	// "BODY" chunk
	chunkLen = sampleLen << sample16Bit;
	iffWriteChunkHeader(f, "BODY", chunkLen);
	const uint32_t sampleDataPos = ftell(f);
	iffWriteChunkData(f, samplePtr, chunkLen);

	// go back and fill in "FORM" chunk size
	chunkLen = ftell(f) - 8;
	fseek(f, 4, SEEK_SET);
	iffWriteUint32(f, chunkLen);

	fclose(f);

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static bool saveWAVSample(UNICHAR *filenameU, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint32_t i, sampleLen, riffChunkSize, smpNameLen, tmpLen;
	wavHeader_t wavHeader;
	samplerChunk_t samplerChunk;
	mptExtraChunk_t mptExtraChunk;

	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || ins->smp[editor.curSmp].dataPtr == NULL || ins->smp[editor.curSmp].length == 0)
	{
		okBoxThreadSafe(0, "System message", "The sample is empty!", NULL);
		return false;
	}

	sample_t *smp = &ins->smp[editor.curSmp];

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	bool sample16Bit = !!(smp->flags & SAMPLE_16BIT);

	if (saveRangedData)
	{
		samplePtr = &smp->dataPtr[getSampleRangeStart() << sample16Bit];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->length;
		samplePtr = smp->dataPtr;
	}

	const uint8_t sampleBitDepth = sample16Bit ? 16 : 8;

	wavHeader.chunkID = 0x46464952; // "RIFF"
	wavHeader.chunkSize = 0; // is filled later
	wavHeader.format = 0x45564157; // "WAVE"
	wavHeader.subchunk1ID = 0x20746D66; // "fmt "
	wavHeader.subchunk1Size = 16;
	wavHeader.audioFormat = 1;
	wavHeader.numChannels = 1;
	wavHeader.sampleRate = getSampleMiddleCRate(smp);
	wavHeader.byteRate = (wavHeader.sampleRate * wavHeader.numChannels * sampleBitDepth) / 8;
	wavHeader.blockAlign = (wavHeader.numChannels * sampleBitDepth) / 8;
	wavHeader.bitsPerSample = sampleBitDepth;
	wavHeader.subchunk2ID = 0x61746164; // "data"
	wavHeader.subchunk2Size = sampleLen << sample16Bit;

	// write main header
	fwrite(&wavHeader, sizeof (wavHeader_t), 1, f);

	// write sample data
	const uint32_t sampleDataPos = ftell(f);
	if (sampleBitDepth == 16)
	{
		fwrite((int16_t *)samplePtr, sizeof (int16_t), sampleLen, f);
	}
	else
	{
		for (i = 0; i < sampleLen; i++)
			fputc(samplePtr[i] ^ 0x80, f); // write as unsigned 8-bit data
	}

	if (wavHeader.subchunk2Size & 1)
		fputc(0, f); // write pad byte if chunk size is uneven

	// write "smpl" chunk if loop is enabled
	if (!saveRangedData && GET_LOOPTYPE(smp->flags) != LOOP_DISABLED)
	{
		memset(&samplerChunk, 0, sizeof (samplerChunk));

		samplerChunk.chunkID = 0x6C706D73; // "smpl"
		samplerChunk.chunkSize = sizeof (samplerChunk) - 4 - 4;
		samplerChunk.dwSamplePeriod = 1000000000 / wavHeader.sampleRate;
		samplerChunk.dwMIDIUnityNote = 60; // 60 = MIDI middle-C
		samplerChunk.cSampleLoops = 1;
		samplerChunk.loop.dwType = GET_LOOPTYPE(smp->flags)-1; // 0 = forward, 1 = ping-pong
		samplerChunk.loop.dwStart = smp->loopStart;
		samplerChunk.loop.dwEnd = (smp->loopStart + smp->loopLength) - 1;

		fwrite(&samplerChunk, sizeof (samplerChunk), 1, f);
		if (samplerChunk.chunkSize & 1)
			fputc(0, f); // write pad byte if chunk size is uneven
	}

	// write modplug tracker "xtra" chunk
	if (!saveRangedData)
	{
		memset(&mptExtraChunk, 0, sizeof (mptExtraChunk));

		mptExtraChunk.chunkID = 0x61727478; // "xtra"
		mptExtraChunk.chunkSize = sizeof (mptExtraChunk) - 4 - 4;
		mptExtraChunk.flags = 0x20; // set pan flag
		mptExtraChunk.defaultPan = smp->panning; // 0..255
		mptExtraChunk.defaultVolume = smp->volume * 4; // 0..256
		mptExtraChunk.globalVolume = 64; // 0..64
		mptExtraChunk.vibratoType = ins->autoVibType; // 0..3    0 = sine, 1 = square, 2 = ramp up, 3 = ramp down
		mptExtraChunk.vibratoSweep = ins->autoVibSweep; // 0..255
		mptExtraChunk.vibratoDepth = ins->autoVibDepth; // 0..15
		mptExtraChunk.vibratoRate = ins->autoVibRate; // 0..63

		fwrite(&mptExtraChunk, sizeof (mptExtraChunk), 1, f);
		if (mptExtraChunk.chunkSize & 1)
			fputc(0, f); // write pad byte if chunk size is uneven
	}

	// write LIST->INFO->INAM chunk

	if (saveRangedData)
	{
		smpNamePtr = (char *)rangedDataStr;
		smpNameLen = (uint32_t)strlen(smpNamePtr);
	}
	else
	{
		smpNamePtr = smp->name;

		smpNameLen = 0;
		while (smpNameLen < 22)
		{
			if (smpNamePtr[smpNameLen] == '\0')
				break;

			smpNameLen++;
		}
	}

	const uint32_t progNameLen = sizeof (PROG_NAME_STR) - 1;

	tmpLen = 4 + (4 + 4) + (progNameLen + 1 + ((progNameLen + 1) & 1));
	if (smpNameLen > 0)
		tmpLen += ((4 + 4) + (progNameLen + 1 + ((progNameLen + 1) & 1)));

	fwrite("LIST", sizeof (int32_t), 1, f);
	fwrite(&tmpLen, sizeof (int32_t), 1, f);
	fwrite("INFO", sizeof (int32_t), 1, f);

	if (smpNameLen > 0)
	{
		tmpLen = smpNameLen + 1;
		fwrite("INAM", sizeof (int32_t), 1, f);
		fwrite(&tmpLen, sizeof (int32_t), 1, f);
		fwrite(smpNamePtr, 1, smpNameLen, f);
		fputc(0, f); // string termination
		if (tmpLen & 1)
			fputc(0, f); // pad byte
	}

	tmpLen = progNameLen + 1;
	fwrite("ISFT", sizeof (int32_t), 1, f);
	fwrite(&tmpLen, sizeof (int32_t), 1, f);
	fwrite(PROG_NAME_STR, 1, progNameLen, f);
	fputc(0, f); // string termination
	if (tmpLen & 1)
		fputc(0, f); // pad byte

	// go back and fill in "RIFF" chunk size
	riffChunkSize = ftell(f) - 8;
	fseek(f, 4, SEEK_SET);
	fwrite(&riffChunkSize, sizeof (int32_t), 1, f);

	fclose(f);

	// restore modified interpolation tap samples after loopEnd
	bool loopEnabled = GET_LOOPTYPE(smp->flags) != LOOP_DISABLED;
	if (loopEnabled && smp->length > smp->loopStart+smp->loopLength)
		fileRestoreFixedSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static int32_t SDLCALL saveSampleThread(void *ptr)
{
	if (editor.tmpFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
		return false;
	}

	const UNICHAR *oldPathU = getDiskOpCurPath();

	// in "save range mode", we must enter the sample directory
	if (saveRangeFlag)
		UNICHAR_CHDIR(getDiskOpSmpPath());

	switch (editor.sampleSaveMode)
	{
		         case SMP_SAVE_MODE_RAW: saveRawSample(editor.tmpFilenameU, saveRangeFlag); break;
		         case SMP_SAVE_MODE_IFF: saveIFFSample(editor.tmpFilenameU, saveRangeFlag); break;
		default: case SMP_SAVE_MODE_WAV: saveWAVSample(editor.tmpFilenameU, saveRangeFlag); break;
	}

	// set back old working directory if we changed it
	if (saveRangeFlag)
		UNICHAR_CHDIR(oldPathU);

	return true;

	(void)ptr;
}

void saveSample(UNICHAR *filenameU, bool saveAsRange)
{
	saveRangeFlag = saveAsRange;
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(saveSampleThread, "sample save thread", NULL);
	if (thread == NULL)
	{
		okBoxThreadSafe(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}
