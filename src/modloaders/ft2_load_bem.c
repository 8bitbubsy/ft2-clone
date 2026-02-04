/* BEM (UN05, MikMod) loader. Supports modules converted from XM only!
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
#include "../ft2_sysreqs.h"

#define MAX_TRACKS (256*32)

#define FLAG_XMPERIODS 1
#define FLAG_LINEARSLIDES 2

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct bemHdr_t
{
	char id[4];
	uint8_t numchn;
	uint16_t numpos;
	uint16_t reppos;
	uint16_t numpat;
	uint16_t numtrk;
	uint16_t numins;
	uint8_t initspeed;
	uint8_t inittempo;
	uint8_t positions[256];
	uint8_t panning[32];
	uint8_t flags;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
bemHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

enum
{
	UNI_NOTE = 1,
	UNI_INSTRUMENT,
	UNI_PTEFFECT0,
	UNI_PTEFFECT1,
	UNI_PTEFFECT2,
	UNI_PTEFFECT3,
	UNI_PTEFFECT4,
	UNI_PTEFFECT5,
	UNI_PTEFFECT6,
	UNI_PTEFFECT7,
	UNI_PTEFFECT8,
	UNI_PTEFFECT9,
	UNI_PTEFFECTA,
	UNI_PTEFFECTB,
	UNI_PTEFFECTC,
	UNI_PTEFFECTD,
	UNI_PTEFFECTE,
	UNI_PTEFFECTF,
	UNI_S3MEFFECTA,
	UNI_S3MEFFECTD,
	UNI_S3MEFFECTE,
	UNI_S3MEFFECTF,
	UNI_S3MEFFECTI,
	UNI_S3MEFFECTQ,
	UNI_S3MEFFECTT,
	UNI_XMEFFECTA,
	UNI_XMEFFECTG,
	UNI_XMEFFECTH,
	UNI_XMEFFECTP,

	UNI_LAST
};

static const uint8_t xmEfxTab[] = { 10, 16, 17, 25 }; // A, G, H, P
static uint16_t trackList[256*32];
static note_t *decodedTrack[MAX_TRACKS];

static char *readString(FILE *f)
{
	uint16_t length;
	fread(&length, 2, 1, f);

	char *out = (char *)malloc(length+1);
	if (out == NULL)
		return NULL;

	fread(out, 1, length, f);
	out[length] = '\0';

	return out;
}

bool detectBEM(FILE *f)
{
	if (f == NULL) return false;

	uint32_t oldPos = (uint32_t)ftell(f);

	fseek(f, 0, SEEK_SET);
	char ID[64];
	memset(ID, 0, sizeof (ID));
	fread(ID, 1, 4, f);
	if (memcmp(ID, "UN05", 4) != 0)
		goto error;

	fseek(f, 0x131, SEEK_SET);
	if (feof(f))
		goto error;

	uint8_t flags = (uint8_t)fgetc(f);
	if ((flags & FLAG_XMPERIODS) == 0)
		goto error;

	fseek(f, 0x132, SEEK_SET);
	if (feof(f))
		goto error;

	uint16_t strLength = 0;
	fread(&strLength, 2, 1, f);
	if (strLength == 0 || strLength > 512)
		goto error;

	fseek(f, strLength+2, SEEK_CUR);
	if (feof(f))
		goto error;

	fread(ID, 1, 64, f);
	if (memcmp(ID, "FastTracker v2.00", 17) != 0)
		goto error;

	fseek(f, oldPos, SEEK_SET);
	return true;

error:
	fseek(f, oldPos, SEEK_SET);
	return false;
}

bool loadBEM(FILE *f, uint32_t filesize)
{
	bemHdr_t h;

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	fread(&h, 1, sizeof (bemHdr_t), f);

	char *songName = readString(f);
	if (songName == NULL)
		return false;

	strcpy(songTmp.name, songName);
	free(songName);
	uint16_t strLength;
	fread(&strLength, 2, 1, f);
	fseek(f, strLength, SEEK_CUR);
	fread(&strLength, 2, 1, f);
	fseek(f, strLength, SEEK_CUR);

	if (h.numpos > 256 || h.numpat > 256 || h.numchn > 32 || h.numtrk > MAX_TRACKS)
	{
		loaderMsgBox("Error loading BEM: The module is corrupt!");
		return false;
	}

	tmpLinearPeriodsFlag = !!(h.flags & FLAG_LINEARSLIDES);

	songTmp.numChannels = h.numchn;
	songTmp.songLength = h.numpos;
	songTmp.songLoopStart = h.reppos;
	songTmp.BPM = h.inittempo;
	songTmp.speed = h.initspeed;
	
	memcpy(songTmp.orders, h.positions, 256);

	// load instruments
	for (int16_t i = 0; i < h.numins; i++)
	{
		if (!allocateTmpInstr(1 + i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		instr_t *ins = instrTmp[1 + i];

		ins->numSamples = (uint8_t)fgetc(f);
		fread(ins->note2SampleLUT, 1, 96, f);

		ins->volEnvFlags = (uint8_t)fgetc(f);
		ins->volEnvLength = (uint8_t)fgetc(f);
		ins->volEnvSustain = (uint8_t)fgetc(f);
		ins->volEnvLoopStart = (uint8_t)fgetc(f);
		ins->volEnvLoopEnd = (uint8_t)fgetc(f);
		fread(ins->volEnvPoints, 2, 12*2, f);

		ins->panEnvFlags = (uint8_t)fgetc(f);
		ins->panEnvLength = (uint8_t)fgetc(f);
		ins->panEnvSustain = (uint8_t)fgetc(f);
		ins->panEnvLoopStart = (uint8_t)fgetc(f);
		ins->panEnvLoopEnd = (uint8_t)fgetc(f);
		fread(ins->panEnvPoints, 2, 12*2, f);

		ins->autoVibType = (uint8_t)fgetc(f);
		ins->autoVibSweep = (uint8_t)fgetc(f);
		ins->autoVibDepth = (uint8_t)fgetc(f);
		ins->autoVibRate = (uint8_t)fgetc(f);
		fread(&ins->fadeout, 2, 1, f);

		char *insName = readString(f);
		if (insName == NULL)
			return false;

		uint32_t insNameLen = (uint32_t)strlen(insName);
		if (insNameLen > 22)
			insNameLen = 22;

		memcpy(songTmp.instrName[1+i], insName, insNameLen);
		free(insName);

		for (int32_t j = 0; j < ins->numSamples; j++)
		{
			sample_t *s = &ins->smp[j];

			s->finetune = (int8_t)fgetc(f) ^ 0x80;
			fseek(f, 1, SEEK_CUR);
			s->relativeNote = (int8_t)fgetc(f);
			s->volume = (uint8_t)fgetc(f);
			s->panning = (uint8_t)fgetc(f);
			fread(&s->length, 4, 1, f);
			fread(&s->loopStart, 4, 1, f);
			uint32_t loopEnd;
			fread(&loopEnd, 4, 1, f);
			s->loopLength = loopEnd - s->loopStart;

			uint16_t flags;
			fread(&flags, 2, 1, f);
			if (flags &  1) s->flags |= SAMPLE_16BIT;
			if (flags & 16) s->flags |= LOOP_FWD;
			if (flags & 32) s->flags |= LOOP_BIDI;

			char *smpName = readString(f);
			if (smpName == NULL)
				return false;
			
			uint32_t smpNameLen = (uint32_t)strlen(smpName);
			if (smpNameLen > 22)
				smpNameLen = 22;

			memcpy(s->name, smpName, smpNameLen);
			free(smpName);
		}
	}

	// load tracks

	uint16_t rowsInPattern[256];
	
	fread(rowsInPattern, 2, h.numpat, f);
	fread(trackList, 2, h.numpat * h.numchn, f);
	
	for (int32_t i = 0; i < h.numtrk; i++)
	{
		uint16_t trackBytesInFile;
		fread(&trackBytesInFile, 2, 1, f);
		if (trackBytesInFile == 0)
		{
			loaderMsgBox("Error loading BEM: This module is corrupt!");
trackError:
			for (int32_t j = 0; j < i; j++)
				free(decodedTrack[j]);

			return false;
		}

		decodedTrack[i] = (note_t *)calloc(rowsInPattern[i], sizeof (note_t));
		if (decodedTrack[i] == NULL)
		{
			loaderMsgBox("Not enough memory!");
			goto trackError;
		}

		note_t *out = decodedTrack[i];

		// decode track

		uint32_t trackPosInFile = (uint32_t)ftell(f);
		while ((uint32_t)ftell(f) < trackPosInFile+trackBytesInFile)
		{
			uint8_t byte = (uint8_t)fgetc(f);
			if (byte == 0)
				break; // end of track

			uint8_t repeat = byte >> 5;
			uint8_t opcodeBytes = (byte & 0x1F) - 1;

			uint32_t opcodeStart = (uint32_t)ftell(f);
			uint32_t opcodeEnd = opcodeStart + opcodeBytes;

			for (int32_t j = 0; j <= repeat; j++, out++)
			{
				fseek(f, opcodeStart, SEEK_SET);
				while ((uint32_t)ftell(f) < opcodeEnd)
				{
					uint8_t opcode = (uint8_t)fgetc(f);

					if (opcode == 0)
						break;

					if (opcode == UNI_NOTE)
					{
						out->note = 1 + (uint8_t)fgetc(f);
					}
					else if (opcode == UNI_INSTRUMENT)
					{
						out->instr = 1 + (uint8_t)fgetc(f);
					}
					else if (opcode >= UNI_PTEFFECT0 && opcode <= UNI_PTEFFECTF) // PT effects
					{
						out->efx = opcode - UNI_PTEFFECT0;
						out->efxData = (uint8_t)fgetc(f);
					}
					else if (opcode >= UNI_XMEFFECTA && opcode <= UNI_XMEFFECTP) // XM effects
					{
						out->efx = xmEfxTab[opcode-UNI_XMEFFECTA];
						out->efxData = (uint8_t)fgetc(f);
					}
					else
					{
						if (opcode >= UNI_LAST) // illegal opcode
						{
							loaderMsgBox("Error loading BEM: illegal pattern opcode!");
							goto trackError;
						}

						// unsupported opcode, skip it
						if (opcode > 0)
							fseek(f, 1, SEEK_CUR);
					}
				}
			}
		}
	}

	// create patterns from tracks
	for (int32_t i = 0; i < h.numpat; i++)
	{
		uint16_t numRows = rowsInPattern[i];
		if (numRows == 0 || numRows > 256)
			continue;

		if (!allocateTmpPatt(i, numRows))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		note_t *dst = patternTmp[i];
		for (int32_t j = 0; j < h.numchn; j++)
		{
			note_t *src = (note_t *)decodedTrack[trackList[(i * h.numchn) + j]];
			if (src != NULL)
			{
				for (int32_t k = 0; k < numRows; k++)
					dst[(k * MAX_CHANNELS) + j] = src[k];
			}
		}
	}

	// load sample data
	for (int32_t i = 0; i < h.numins; i++)
	{
		instr_t *ins = instrTmp[1 + i];
		if (ins == NULL)
			continue;

		for (int32_t j = 0; j < ins->numSamples; j++)
		{
			sample_t *s = &ins->smp[j];

			bool sampleIs16Bit = !!(s->flags & SAMPLE_16BIT);
			if (!allocateSmpData(s, s->length, sampleIs16Bit))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			fread(s->dataPtr, 1 + sampleIs16Bit, s->length, f);
			delta2Samp(s->dataPtr, s->length, s->flags);
		}
	}

	return true;
}
