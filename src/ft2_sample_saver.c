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

static const char rangedDataStr[] = "Ranged data from FT2";

// thread data
static bool saveRangeFlag;
static SDL_Thread *thread;

// used to restore mixer interpolation fix .RAW/.IFF/.WAV files after save
static bool fileRestoreSampleData(UNICHAR *filenameU, int32_t sampleDataOffset, sampleTyp *smp)
{
	int8_t fixSpar8;
	FILE *f;

	if (!smp->fixed)
		return false; // nothing to fix

	f = UNICHAR_FOPEN(filenameU, "r+"); // open in read+update mode
	if (f == NULL)
		return false;

	if (smp->typ & 16)
	{
		// 16-bit sample
		if (smp->fixedPos < smp->len/2)
		{
			fseek(f, sampleDataOffset + (smp->fixedPos * 2), SEEK_SET);
			fwrite(&smp->fixedSmp1, sizeof (int16_t), 1, f);
		}

		if (smp->fixedPos+2 < smp->len/2)
		{
			fseek(f, sampleDataOffset + ((smp->fixedPos + 2) * 2), SEEK_SET);
			fwrite(&smp->fixedSmp2, sizeof (int16_t), 1, f);
		}
	}
	else
	{
		// 8-bit sample
		if (smp->fixedPos < smp->len)
		{
			fseek(f, sampleDataOffset + smp->fixedPos, SEEK_SET);

			fixSpar8 = (int8_t)smp->fixedSmp1;
			if (editor.sampleSaveMode == SMP_SAVE_MODE_WAV) // on 8-bit WAVs the sample data is unsigned
				fixSpar8 ^= 0x80;

			fwrite(&fixSpar8, sizeof (int8_t), 1, f);
		}

		if (smp->fixedPos+1 < smp->len)
		{
			fseek(f, sampleDataOffset + (smp->fixedPos + 1), SEEK_SET);

			fixSpar8 = (int8_t)smp->fixedSmp2;
			if (editor.sampleSaveMode == SMP_SAVE_MODE_WAV) // on 8-bit WAVs the sample data is unsigned
				fixSpar8 ^= 0x80;

			fwrite(&fixSpar8, sizeof (int8_t), 1, f);
		}
	}

	fclose(f);
	return true;
}

static bool saveRawSample(UNICHAR *filenameU, bool saveRangedData)
{
	int8_t *samplePtr;
	uint32_t sampleLen;
	FILE *f;
	sampleTyp *smp;

	if (instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].len == 0)
	{
		okBoxThreadSafe(0, "System message", "Error saving sample: The sample is empty!");
		return false;
	}

	smp = &instr[editor.curInstr]->samp[editor.curSmp];

	if (saveRangedData)
	{
		samplePtr = &smp->pek[getSampleRangeStart()];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->len;
		samplePtr = smp->pek;
	}

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	if (fwrite(samplePtr, sampleLen, 1, f) != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving sample: General I/O error!");
		return false;
	}

	fclose(f);

	// restore mixer interpolation fix
	fileRestoreSampleData(filenameU, 0, smp);

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

static bool saveIFFSample(UNICHAR *filenameU, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint32_t sampleLen, smpNameLen, chunkLen, tmp32, sampleDataPos;
	FILE *f;
	sampleTyp *smp;

	if (instr[editor.curInstr] == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].pek == NULL ||
		instr[editor.curInstr]->samp[editor.curSmp].len == 0)
	{
		okBoxThreadSafe(0, "System message", "Error saving sample: The sample is empty!");
		return false;
	}

	smp = &instr[editor.curInstr]->samp[editor.curSmp];

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	if (saveRangedData)
	{
		samplePtr = &smp->pek[getSampleRangeStart()];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->len;
		samplePtr = smp->pek;
	}

	// "FORM" chunk
	iffWriteChunkHeader(f, "FORM", 0); // "FORM" chunk size is overwritten later
	iffWriteUint32(f, (smp->typ & 16) ? 0x31365356 : 0x38535658); // bitdepth - "16SV" (16-bit) or "8SVX" (8-bit)

	// "VHDR" chunk
	iffWriteChunkHeader(f, "VHDR", 20);

	if (!saveRangedData && (smp->typ & 3)) // loop enabled?
	{
		iffWriteUint32(f, smp->repS); // oneShotHiSamples
		iffWriteUint32(f, smp->repL); // repeatHiSamples
	}
	else
	{
		iffWriteUint32(f, 0); // oneShotHiSamples
		iffWriteUint32(f, 0); // repeatHiSamples
	}

	iffWriteUint32(f, 0); // samplesPerHiCycle

	// samplesPerSec
	tmp32 = getSampleMiddleCRate(smp);
	if (tmp32 == 0 || tmp32 > 65535)
		tmp32 = 16726;
	iffWriteUint16(f, tmp32 & 0xFFFF);

	fputc(1, f); // ctOctave (number of samples)
	fputc(0, f); // sCompression
	iffWriteUint32(f, smp->vol * 1024); // volume (max: 65536/0x10000)

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

	if (smpNameLen > 0)
	{
		chunkLen = smpNameLen;
		iffWriteChunkHeader(f, "NAME", chunkLen);
		fwrite(smpNamePtr, 1, chunkLen, f);
		if (chunkLen & 1) fputc(0, f); // write pad byte if chunk size is uneven
	}

	// "ANNO" chunk (we put the program name here)
	if (PROG_NAME_STR[0] != '\0')
	{
		chunkLen = sizeof (PROG_NAME_STR) - 1;
		iffWriteChunkHeader(f, "ANNO", chunkLen);
		fwrite(PROG_NAME_STR, 1, chunkLen, f);
		if (chunkLen & 1) fputc(0, f); // write pad byte if chunk size is uneven
	}

	// "BODY" chunk
	chunkLen = sampleLen;
	iffWriteChunkHeader(f, "BODY", chunkLen);
	sampleDataPos = ftell(f);
	fwrite(samplePtr, 1, chunkLen, f);
	if (chunkLen & 1) fputc(0, f); // write pad byte if chunk size is uneven

	// go back and fill in "FORM" chunk size
	tmp32 = ftell(f) - 8;
	fseek(f, 4, SEEK_SET);
	iffWriteUint32(f, tmp32);

	fclose(f);

	// restore mixer interpolation fix
	fileRestoreSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static bool saveWAVSample(UNICHAR *filenameU, bool saveRangedData)
{
	char *smpNamePtr;
	int8_t *samplePtr;
	uint8_t sampleBitDepth;
	uint32_t i, sampleLen, riffChunkSize, smpNameLen, tmpLen, progNameLen, sampleDataPos;
	FILE *f;
	sampleTyp *smp;
	instrTyp *ins;
	wavHeader_t wavHeader;
	samplerChunk_t samplerChunk;
	mptExtraChunk_t mptExtraChunk;

	ins = instr[editor.curInstr];
	if (ins == NULL)
	{
		okBoxThreadSafe(0, "System message", "Error saving sample: The sample is empty!");
		return false;
	}

	smp = &ins->samp[editor.curSmp];
	if (smp->pek == NULL || smp->len == 0)
	{
		okBoxThreadSafe(0, "System message", "Error saving sample: The sample is empty!");
		return false;
	}

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	if (saveRangedData)
	{
		samplePtr = &smp->pek[getSampleRangeStart()];
		sampleLen = getSampleRangeLength();
	}
	else
	{
		sampleLen = smp->len;
		samplePtr = smp->pek;
	}

	sampleBitDepth = (smp->typ & 16) ? 16 : 8;

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
	wavHeader.subchunk2Size = sampleLen;

	// write main header
	fwrite(&wavHeader, sizeof (wavHeader_t), 1, f);

	// write sample data
	sampleDataPos = ftell(f);
	if (sampleBitDepth == 16)
	{
		fwrite((int16_t *)samplePtr, sizeof (int16_t), sampleLen / 2, f);
	}
	else
	{
		for (i = 0; i < sampleLen; i++)
			fputc(samplePtr[i] ^ 0x80, f); // write as unsigned 8-bit data
	}

	if (wavHeader.subchunk2Size & 1)
		fputc(0, f); // write pad byte if chunk size is uneven

	// write "smpl" chunk if loop is enabled
	if (!saveRangedData && (smp->typ & 3))
	{
		memset(&samplerChunk, 0, sizeof (samplerChunk));

		samplerChunk.chunkID = 0x6C706D73; // "smpl"
		samplerChunk.chunkSize = sizeof (samplerChunk) - 4 - 4;
		samplerChunk.dwSamplePeriod = 1000000000 / wavHeader.sampleRate;
		samplerChunk.dwMIDIUnityNote = 60; // 60 = C-4
		samplerChunk.cSampleLoops = 1;
		samplerChunk.loop.dwType = (smp->typ & 3) - 1; // 0 = forward, 1 = ping-pong

		if (sampleBitDepth == 16)
		{
			// divide loop points by 2 to get samples insetad of bytes
			samplerChunk.loop.dwStart = smp->repS / 2;
			samplerChunk.loop.dwEnd = ((smp->repS + smp->repL) / 2) - 1;
		}
		else
		{
			// 8-bit sample
			samplerChunk.loop.dwStart = smp->repS;
			samplerChunk.loop.dwEnd = (smp->repS + smp->repL) - 1;
		}

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
		mptExtraChunk.flags = 0x20; // set pan flag - used when loading .WAVs in OpenMPT
		mptExtraChunk.defaultPan = smp->pan; // 0..255
		mptExtraChunk.defaultVolume = smp->vol * 4; // 0..256
		mptExtraChunk.globalVolume = 64; // 0..64
		mptExtraChunk.vibratoType = ins->vibTyp; // 0..3    0 = sine, 1 = square, 2 = ramp up, 3 = ramp down
		mptExtraChunk.vibratoSweep = ins->vibSweep; // 0..255
		mptExtraChunk.vibratoDepth = ins->vibDepth; // 0..15
		mptExtraChunk.vibratoRate= ins->vibRate; // 0..63

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

	progNameLen = sizeof (PROG_NAME_STR) - 1;

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

	// restore mixer interpolation fix
	fileRestoreSampleData(filenameU, sampleDataPos, smp);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

static int32_t SDLCALL saveSampleThread(void *ptr)
{
	const UNICHAR *oldPathU;

	(void)ptr;

	if (editor.tmpFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	oldPathU = getDiskOpCurPath();

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
}

void saveSample(UNICHAR *filenameU, bool saveAsRange)
{
	saveRangeFlag = saveAsRange;
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(saveSampleThread, NULL, NULL);
	if (thread == NULL)
	{
		okBoxThreadSafe(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}
