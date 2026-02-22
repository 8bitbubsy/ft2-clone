/* (Lossy) Impulse Tracker module loader.
**
** It makes little sense to convert this format to XM, as it results
** in severe conversion losses. The reason I wrote this loader anyway,
** is so that you can import IT files to extract samples, pattern data
** and so on.
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

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct itHdr_t
{
	char ID[4], songName[26];
	uint16_t rowHighlight, ordNum, insNum, smpNum, patNum, cwtv, cmwt, flags, special;
	uint8_t globalVol, mixingVol, speed, BPM, panSep, pitchWheelDepth;
	uint16_t msgLen;
	uint32_t msgOffs, reserved;
	uint8_t initialPans[64], initialVols[64];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itHdr_t;

typedef struct envNode_t
{
	int8_t magnitude;
	uint16_t tick;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
envNode_t;

typedef struct env_t
{
	uint8_t flags, num, loopBegin, loopEnd, sustainLoopBegin, sustainLoopEnd;
	envNode_t nodePoints[25];
	uint8_t reserved;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
env_t;

typedef struct itInsHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t NNA, DCT, DCA;
	uint16_t fadeOut;
	uint8_t pitchPanSep, pitchPanCenter, globVol, defPan, randVol, randPan;
	uint16_t trackerVer;
	uint8_t numSamples, res1;
	char instrumentName[26];
	uint8_t filterCutoff, filterResonance, midiChn, midiProg;
	uint16_t midiBank;
	uint16_t smpNoteTable[120];
	env_t volEnv, panEnv, pitchEnv;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itInsHdr_t;

typedef struct itOldInsHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t volEnvFlags, volEnvLoopBegin, volEnvLoopEnd, volEnvSusLoopBegin, volEnvSusLoopEnd;
	uint16_t res1, fadeOut;
	uint8_t NNA, DNC;
	uint16_t trackerVer;
	uint8_t numSamples, res2;
	char instrumentName[26];
	uint8_t res3[6];
	uint16_t smpNoteTable[120];
	uint8_t volEnv[200];
	uint16_t volEnvPoints[25];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itOldInsHdr_t;

typedef struct itSmpHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t globVol, flags, vol;
	char sampleName[26];
	uint8_t cvt, defPan;
	uint32_t length, loopBegin, loopEnd, c5Speed, sustainLoopBegin, sustainLoopEnd, offsetInFile;
	uint8_t autoVibratoSpeed, autoVibratoDepth, autoVibratoRate, autoVibratoWaveform;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itSmpHdr_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint8_t volPortaConv[9] = { 1, 4, 8, 16, 32, 64, 96, 128, 255 };
static uint32_t insOffs[256], smpOffs[256], patOffs[256];
static itSmpHdr_t *srcSmp, smpHdrs[256];

static bool loadCompressed16BitSample(FILE *f, sample_t *s, bool deltaEncoded);
static bool loadCompressed8BitSample(FILE *f, sample_t *s, bool deltaEncoded);
static void setAutoVibrato(instr_t *ins, itSmpHdr_t *is);
static bool loadSample(FILE *f, sample_t *s, itSmpHdr_t *is);

bool loadIT(FILE *f, uint32_t filesize)
{
	itHdr_t header;

	if (filesize < sizeof (header))
	{
		loaderMsgBox("This IT module is not supported or is corrupt!");
		goto error;
	}

	fread(&header, sizeof (header), 1, f);

	if (header.ordNum > 257 || header.insNum > 256 || header.smpNum > 256 || header.patNum > 256)
	{
		loaderMsgBox("This IT module is not supported or is corrupt!");
		goto error;
	}

	tmpLinearPeriodsFlag = !!(header.flags & 8);

	songTmp.pattNum = header.patNum;
	songTmp.speed = header.speed;
	songTmp.BPM = header.BPM;

	memcpy(songTmp.name, header.songName, 20);
	songTmp.name[20] = '\0';

	bool oldFormat = (header.cmwt < 0x200);
	bool songUsesInstruments = !!(header.flags & 4);
	bool oldEffects = !!(header.flags & 16);
	bool compatGxx = !!(header.flags & 32);

	// read order list
	for (int32_t i = 0; i < MAX_ORDERS; i++)
	{
		const uint8_t patt = (uint8_t)fgetc(f);
		if (patt == 254) // separator ("+++"), skip it
			continue;

		if (patt == 255) // end of pattern list
			break;

		songTmp.orders[songTmp.songLength] = patt;

		songTmp.songLength++;
		if (songTmp.songLength == MAX_ORDERS-1)
			break;
	}

	// read file pointers
	fseek(f, sizeof (header) + header.ordNum, SEEK_SET);
	fread(insOffs, 4, header.insNum, f);
	fread(smpOffs, 4, header.smpNum, f);
	fread(patOffs, 4, header.patNum, f);

	for (int32_t i = 0; i < header.smpNum; i++)
	{
		fseek(f, smpOffs[i], SEEK_SET);
		fread(&smpHdrs[i], sizeof (itSmpHdr_t), 1, f);
	}

	if (!songUsesInstruments) // read samples (as instruments)
	{
		int32_t numIns = MIN(header.smpNum, MAX_INST);

		srcSmp = smpHdrs;
		for (int16_t i = 0; i < numIns; i++, srcSmp++)
		{
			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];
			sample_t *s = &ins->smp[0];

			memcpy(songTmp.instrName[1+i], srcSmp->sampleName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->numSamples = (srcSmp->length > 0) ? 1 : 0;
			if (ins->numSamples > 0)
			{
				setAutoVibrato(ins, srcSmp);

				if (!loadSample(f, s, srcSmp))
				{
					loaderMsgBox("Not enough memory!");
					goto error;
				}
			}
		}
	}
	else if (oldFormat) // read instruments (old format version)
	{
		itOldInsHdr_t itIns;

		int32_t numIns = MIN(header.insNum, MAX_INST);
		for (int16_t i = 0; i < numIns; i++)
		{
			fseek(f, insOffs[i], SEEK_SET);
			fread(&itIns, sizeof (itIns), 1, f);

			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];

			memcpy(songTmp.instrName[1+i], itIns.instrumentName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->fadeout = itIns.fadeOut * 64; // 0..64 -> 0..4096
			if (ins->fadeout > 4095)
				ins->fadeout = 4095;

			// find out what samples to load into this XM instrument header

			int16_t numSamples = 0;
			uint8_t sampleList[MAX_SMP_PER_INST];

			bool sampleAdded[256];
			memset(sampleList, 0, sizeof (sampleList));
			memset(sampleAdded, 0, sizeof (sampleAdded));

			for (int32_t j = 0; j < 96; j++)
			{
				uint8_t sample = itIns.smpNoteTable[12+j] >> 8;
				if (sample > 0 && !sampleAdded[sample-1] && numSamples < MAX_SMP_PER_INST)
				{
					sampleAdded[sample-1] = true;
					sampleList[numSamples] = sample-1;
					numSamples++;
				}
			}

			/* If instrument only has one sample, copy over the sample's
			** auto-vibrato parameters to this instrument.
			*/
			bool singleSample = true;
			if (numSamples > 1)
			{
				uint8_t firstSample = sampleList[0];
				for (int32_t j = 1; j < numSamples; j++)
				{
					if (sampleList[j] != firstSample)
					{
						singleSample = false;
						break;
					}
				}
			}

			if (singleSample)
				setAutoVibrato(ins, &smpHdrs[sampleList[0]]);

			// create new note-to-sample table
			for (int32_t j = 0; j < 8*12; j++)
			{
				uint8_t inSmp = itIns.smpNoteTable[(1 * 12) + j] >> 8;

				uint8_t outSmp = 0;
				if (inSmp > 0)
				{
					inSmp--;
					for (; outSmp < numSamples; outSmp++)
					{
						if (inSmp == sampleList[outSmp])
							break;
					}

					if (outSmp >= numSamples)
						outSmp = 0;
				}

				ins->note2SampleLUT[j] = outSmp;
			}

			// load volume envelope
			if (itIns.volEnvFlags & 1)
			{
				bool volEnvLoopOn = !!(itIns.volEnvFlags & 2);
				bool volEnvSusOn = !!(itIns.volEnvFlags & 4);

				ins->volEnvFlags |= ENV_ENABLED;
				if (volEnvLoopOn) ins->volEnvFlags |= ENV_LOOP;
				if (volEnvSusOn) ins->volEnvFlags |= ENV_SUSTAIN;
				
				ins->volEnvLoopStart = MIN(itIns.volEnvLoopBegin, 11);
				ins->volEnvLoopEnd = MIN(itIns.volEnvLoopEnd, 11);
				ins->volEnvSustain = MIN(itIns.volEnvSusLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!volEnvLoopOn && volEnvSusOn)
				{
					ins->volEnvLoopStart = MIN(itIns.volEnvSusLoopBegin, 11);
					ins->volEnvLoopEnd = MIN(itIns.volEnvSusLoopEnd, 11);
					ins->volEnvSustain = MIN(itIns.volEnvSusLoopEnd, 11);
					ins->volEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				int32_t j = 0;
				for (; j < 12; j++)
				{
					if (itIns.volEnvPoints[j] >> 8 == 0xFF)
						break; // end of volume envelope

					ins->volEnvPoints[j][0] = itIns.volEnvPoints[j] & 0xFF;
					ins->volEnvPoints[j][1] = itIns.volEnvPoints[j] >> 8;
				}
				ins->volEnvLength = (uint8_t)j;

				// increase loop end point tick by one to better match IT style env looping
				if (ins->volEnvFlags & ENV_LOOP)
					ins->volEnvPoints[ins->volEnvLoopEnd][0]++;
			}

			ins->numSamples = numSamples;
			if (ins->numSamples > 0)
			{
				sample_t *s = ins->smp;
				for (int32_t j = 0; j < ins->numSamples; j++, s++)
				{
					if (!loadSample(f, s, &smpHdrs[sampleList[j]]))
					{
						loaderMsgBox("Not enough memory!");
						goto error;
					}
				}
			}
		}
	}
	else // read instruments (later format version)
	{
		itInsHdr_t itIns;

		int32_t numIns = MIN(header.insNum, MAX_INST);
		for (int16_t i = 0; i < numIns; i++)
		{
			fseek(f, insOffs[i], SEEK_SET);
			fread(&itIns, sizeof (itIns), 1, f);

			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];

			memcpy(songTmp.instrName[1+i], itIns.instrumentName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->fadeout = itIns.fadeOut * 32; // 0..128 -> 0..4096
			if (ins->fadeout > 4095)
				ins->fadeout = 4095;

			// find out what samples to load into this XM instrument header

			int16_t numSamples = 0;
			uint8_t sampleList[MAX_SMP_PER_INST];

			bool sampleAdded[256];
			memset(sampleList, 0, sizeof (sampleList));
			memset(sampleAdded, 0, sizeof (sampleAdded));

			for (int32_t j = 0; j < 96; j++)
			{
				uint8_t sample = itIns.smpNoteTable[12+j] >> 8;
				if (sample > 0 && !sampleAdded[sample-1] && numSamples < MAX_SMP_PER_INST)
				{
					sampleAdded[sample-1] = true;
					sampleList[numSamples] = sample-1;
					numSamples++;
				}
			}

			/* If instrument only has one sample, copy over the sample's
			** auto-vibrato parameters to this instrument.
			*/
			bool singleSample = true;
			if (numSamples > 1)
			{
				uint8_t firstSample = sampleList[0];
				for (int32_t j = 1; j < numSamples; j++)
				{
					if (sampleList[j] != firstSample)
					{
						singleSample = false;
						break;
					}
				}
			}

			if (singleSample)
				setAutoVibrato(ins, &smpHdrs[sampleList[0]]);

			// create new note-to-sample table
			for (int32_t j = 0; j < 8*12; j++)
			{
				uint8_t inSmp = itIns.smpNoteTable[(1 * 12) + j] >> 8;

				uint8_t outSmp = 0;
				if (inSmp > 0)
				{
					inSmp--;
					for (; outSmp < numSamples; outSmp++)
					{
						if (inSmp == sampleList[outSmp])
							break;
					}

					if (outSmp >= numSamples)
						outSmp = 0;
				}

				ins->note2SampleLUT[j] = outSmp;
			}

			// load volume envelope
			env_t *volEnv = &itIns.volEnv;
			bool volEnvEnabled = !!(volEnv->flags & 1);
			if (volEnvEnabled && volEnv->num > 0)
			{
				bool volEnvLoopOn = !!(volEnv->flags & 2);
				bool volEnvSusOn = !!(volEnv->flags & 4);

				ins->volEnvFlags |= ENV_ENABLED;
				if (volEnvLoopOn) ins->volEnvFlags |= ENV_LOOP;
				if (volEnvSusOn) ins->volEnvFlags |= ENV_SUSTAIN;
				
				ins->volEnvLength = MIN(volEnv->num, 12);
				ins->volEnvLoopStart = MIN(volEnv->loopBegin, 11);
				ins->volEnvLoopEnd = MIN(volEnv->loopEnd, 11);
				ins->volEnvSustain = MIN(volEnv->sustainLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!volEnvLoopOn && volEnvSusOn)
				{
					ins->volEnvLoopStart = MIN(volEnv->sustainLoopBegin, 11);
					ins->volEnvLoopEnd = MIN(volEnv->sustainLoopEnd, 11);
					ins->volEnvSustain = MIN(volEnv->sustainLoopEnd, 11);
					ins->volEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				for (int32_t j = 0; j < ins->volEnvLength; j++)
				{
					ins->volEnvPoints[j][0] = volEnv->nodePoints[j].tick;
					ins->volEnvPoints[j][1] = volEnv->nodePoints[j].magnitude;
				}

				// increase loop end point tick by one to better match IT style env looping
				if (ins->volEnvFlags & ENV_LOOP)
					ins->volEnvPoints[ins->volEnvLoopEnd][0]++;
			}

			// load pan envelope
			env_t *panEnv = &itIns.panEnv;
			bool panEnvEnabled = !!(panEnv->flags & 1);
			if (panEnvEnabled && panEnv->num > 0)
			{
				bool panEnvLoopOn = !!(panEnv->flags & 2);
				bool panEnvSusOn = !!(panEnv->flags & 4);

				ins->panEnvFlags |= ENV_ENABLED;
				if (panEnvLoopOn) ins->panEnvFlags |= ENV_LOOP;
				if (panEnvSusOn) ins->panEnvFlags |= ENV_SUSTAIN;
				
				ins->panEnvLength = MIN(panEnv->num, 12);
				ins->panEnvLoopStart = MIN(panEnv->loopBegin, 11);
				ins->panEnvLoopEnd = MIN(panEnv->loopEnd, 11);
				ins->panEnvSustain = MIN(panEnv->sustainLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!panEnvLoopOn && panEnvSusOn)
				{
					ins->panEnvLoopStart = MIN(panEnv->sustainLoopBegin, 11);
					ins->panEnvLoopEnd = MIN(panEnv->sustainLoopEnd, 11);
					ins->panEnvSustain = MIN(panEnv->sustainLoopEnd, 11);
					ins->panEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				for (int32_t j = 0; j < ins->panEnvLength; j++)
				{
					ins->panEnvPoints[j][0] = panEnv->nodePoints[j].tick;
					ins->panEnvPoints[j][1] = panEnv->nodePoints[j].magnitude + 32;
				}

				// increase loop end point tick by one to better match IT style env looping
				if (ins->panEnvFlags & ENV_LOOP)
					ins->panEnvPoints[ins->panEnvLoopEnd][0] = panEnv->nodePoints[ins->panEnvLoopEnd].tick + 1;
			}

			ins->numSamples = numSamples;
			if (ins->numSamples > 0)
			{
				sample_t *s = ins->smp;
				for (int32_t j = 0; j < ins->numSamples; j++, s++)
				{
					if (!loadSample(f, s, &smpHdrs[sampleList[j]]))
					{
						loaderMsgBox("Not enough memory!");
						goto error;
					}
				}
			}
		}
	}

	// patterns

	uint32_t numChannels = 0;
	for (int32_t i = 0; i < songTmp.pattNum; i++)
	{
		if (patOffs[i] == 0)
			continue;
	
		fseek(f, patOffs[i], SEEK_SET);

		uint16_t length, numRows;
		fread(&length, 2, 1, f);
		fread(&numRows, 2, 1, f);
		fseek(f, 4, SEEK_CUR);

		numRows = MIN(numRows, MAX_PATT_LEN);
		if (numRows == 0)
			continue;

		if (!allocateTmpPatt(i, numRows))
		{
			loaderMsgBox("Not enough memory!");
			goto error;
		}
		note_t *patt = patternTmp[i];

		uint8_t lastMask[64];
		memset(lastMask, 0, sizeof (lastMask));

		note_t lastNote[64];
		memset(lastNote, 0, sizeof (lastNote));

		fread(tmpBuffer, 1, length, f);
		uint8_t *pattPtr = tmpBuffer;

		int32_t bytesRead = 0;
		int32_t row = 0;
		while (bytesRead < length && row < numRows)
		{
			uint8_t byte = *pattPtr++;
			bytesRead++;

			if (byte == 0)
			{
				row++;
				continue;
			}

			const uint8_t ch = (byte - 1) & 63;
			if (ch > numChannels)
				numChannels = ch;

			note_t emptyNote;
			note_t *p = (ch >= MAX_CHANNELS) ? &emptyNote : &patt[(row * MAX_CHANNELS) + ch];

			if (byte & 128)
			{
				lastMask[ch] = *pattPtr++;
				bytesRead++;
			}

			if (lastMask[ch] & 16)
				p->note = lastNote[ch].note;

			if (lastMask[ch] & 32)
				p->instr = lastNote[ch].instr;

			if (lastMask[ch] & 64)
				p->vol = lastNote[ch].vol;

			if (lastMask[ch] & 128)
			{
				p->efx = lastNote[ch].efx;
				p->efxData = lastNote[ch].efxData;
			}

			if (lastMask[ch] & 1)
			{
				uint8_t note = *pattPtr++;
				bytesRead++;

				if (note < 120)
				{
					note++;
					if (note < 12 || note >= 96+12)
						note = 0;
					else
						note -= 12;
				}
				else if (note != 254)
				{
					note = NOTE_OFF;
				}

				if (note > NOTE_OFF && note != 254)
					note = 0; // remove note

				// 254 (note cut) is handled later!

				p->note = lastNote[ch].note = note;
			}

			if (lastMask[ch] & 2)
			{
				uint8_t ins = *pattPtr++;
				bytesRead++;

				if (ins > MAX_INST)
					ins = 0;

				p->instr = lastNote[ch].instr = ins;
			}

			if (lastMask[ch] & 4)
			{
				p->vol = lastNote[ch].vol = 1 + *pattPtr++;
				bytesRead++;
			}

			if (lastMask[ch] & 8)
			{
				p->efx = lastNote[ch].efx = *pattPtr++;
				bytesRead++;;

				p->efxData = lastNote[ch].efxData = *pattPtr++;
				bytesRead++;
			}
		}
	}
	numChannels++;

	songTmp.numChannels = MIN((numChannels + 1) & ~1, MAX_CHANNELS);

	// convert pattern data

	uint8_t lastInstr[MAX_CHANNELS], lastGInstr[MAX_CHANNELS];
	uint8_t lastDxy[MAX_CHANNELS], lastExy[MAX_CHANNELS], lastFxy[MAX_CHANNELS];
	uint8_t lastJxy[MAX_CHANNELS], lastKxy[MAX_CHANNELS], lastLxy[MAX_CHANNELS];
	uint8_t lastOxx[MAX_CHANNELS];

	memset(lastInstr, 0, sizeof (lastInstr));
	memset(lastGInstr, 0, sizeof (lastGInstr));
	memset(lastDxy, 0, sizeof (lastDxy));
	memset(lastExy, 0, sizeof (lastExy));
	memset(lastFxy, 0, sizeof (lastFxy));
	memset(lastJxy, 0, sizeof (lastJxy));
	memset(lastKxy, 0, sizeof (lastKxy));
	memset(lastLxy, 0, sizeof (lastLxy));
	memset(lastOxx, 0, sizeof (lastOxx));

	for (int32_t i = 0; i < songTmp.pattNum; i++)
	{
		note_t *p = patternTmp[i];
		if (p == NULL)
			continue;

		for (int32_t j = 0; j < patternNumRowsTmp[i]; j++)
		{
			for (int32_t ch = 0; ch < songTmp.numChannels; ch++, p++)
			{
				if (p->instr > 0)
					lastInstr[ch] = p->instr;

				// effect
				if (p->efx != 0)
				{
					const uint8_t itEfx = 'A' + (p->efx - 1);
					switch (itEfx)
					{
						case 'A': // set speed
						{
							if (p->efxData == 0) // A00 is ignored in IT
							{
								p->efx = p->efxData = 0;
							}
							else
							{
								p->efx = 0xF;
								if (p->efxData > 31)
									p->efxData = 31;
							}
						}
						break;

						case 'B': p->efx = 0xB; break; // position jump
						case 'C': p->efx = 0xD; break; // pattern break

						case 'D': // volume slide
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastDxy[ch] & 0x0F) == 0x0F || (lastDxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastDxy[ch];
							}
							else
							{
								lastDxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0xA;
							}
						}
						break;

						case 'E': // portamento down
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastExy[ch] & 0x0F) == 0x0F || (lastExy[ch] >> 4) == 0x0F;
								bool lastWasExtraFineSlide = (lastExy[ch] & 0x0F) == 0x0E || (lastExy[ch] >> 4) == 0x0E;

								if (lastWasFineSlide || lastWasExtraFineSlide)
									p->efxData = lastExy[ch];
							}
							else
							{
								lastExy[ch] = p->efxData;
							}

							if (p->efxData < 224)
							{
								p->efx = 0x2;
							}
							else if ((p->efxData >> 4) == 0x0E)
							{
								p->efx = 16 + ('X' - 'G');
								p->efxData = 0x20 + (p->efxData & 0x0F);
							}
							else if ((p->efxData >> 4) == 0x0F)
							{
								p->efx = 0xE;
								p->efxData = 0x20 + (p->efxData & 0x0F);
							}
						}
						break;

						case 'F': // portamento up
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastFxy[ch] & 0x0F) == 0x0F || (lastFxy[ch] >> 4) == 0x0F;
								bool lastWasExtraFineSlide = (lastFxy[ch] & 0x0F) == 0x0E || (lastFxy[ch] >> 4) == 0x0E;

								if (lastWasFineSlide || lastWasExtraFineSlide)
									p->efxData = lastFxy[ch];
							}
							else
							{
								lastFxy[ch] = p->efxData;
							}

							if (p->efxData < 224)
							{
								p->efx = 0x1;
							}
							else if ((p->efxData >> 4) == 0x0E)
							{
								p->efx = 16 + ('X' - 'G');
								p->efxData = 0x10 + (p->efxData & 0x0F);
							}
							else if ((p->efxData >> 4) == 0x0F)
							{
								p->efx = 0xE;
								p->efxData = 0x10 + (p->efxData & 0x0F);
							}
						}
						break;

						case 'G': // tone portamento
						{
							p->efx = 3;

							// remove illegal slides (this is not quite right, but good enough)
							if (!compatGxx && p->instr != 0 && p->instr != lastGInstr[ch])
								p->efx = p->efxData = 0;
						}
						break;

						case 'H': // vibrato
						{
							p->efx = 4;
							if (!oldEffects && p->efxData > 0)
								p->efxData = (p->efxData & 0xF0) | ((p->efxData & 0x0F) >> 1);
						}
						break;

						case 'I': // tremor
						{
							p->efx = 16 + ('T' - 'G');

							int8_t onTime = p->efxData >> 4;
							if (onTime > 0) // closer to IT2 (but still off)
								onTime--;

							int8_t offTime = p->efxData & 0x0F;
							if (offTime > 0) // ---
								offTime--;

							p->efxData = (onTime << 4) | offTime;
						}
						break;

						case 'J': // arpeggio
						{
							p->efx = 0;

							if (p->efxData != 0)
								p->efxData = lastJxy[ch] = (p->efxData >> 4) | (p->efxData << 4); // swap order (FT2 = reversed)
							else
								p->efxData = lastJxy[ch];
						}
						break;

						case 'K': // volume slide + vibrato
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastKxy[ch] & 0x0F) == 0x0F || (lastKxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastKxy[ch];
							}
							else
							{
								lastKxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+203; // IT2 vibrato of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+203; // IT2 vibrato of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0x6;
							}
						}
						break;

						case 'L': // volume slide + tone portamento
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastLxy[ch] & 0x0F) == 0x0F || (lastLxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastLxy[ch];
							}
							else
							{
								lastLxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+193; // IT2 tone portamento of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+193; // IT2 tone portamento of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0x5;
							}
						}
						break;

						case 'O': // set sample offset
						{
							p->efx = 0x9;

							if (p->efxData > 0)
								lastOxx[ch] = p->efxData;

							// handle cases where the sample offset is after the end of the sample
							if (lastInstr[ch] > 0 && lastOxx[ch] > 0 && p->note > 0 && p->note <= 96)
							{
								instr_t *ins = instrTmp[lastInstr[ch]];
								if (ins != NULL)
								{
									const uint8_t sample = ins->note2SampleLUT[p->note-1];
									if (sample < MAX_SMP_PER_INST)
									{
										sample_t *s = &ins->smp[sample];
										if (s->length > 0)
										{
											const bool loopEnabled = (GET_LOOPTYPE(s->flags) != LOOP_DISABLED);
											const uint32_t sampleEnd = loopEnabled ? s->loopStart+s->loopLength : s->length;

											if (lastOxx[ch]*256UL >= sampleEnd)
											{
												if (oldEffects)
												{
													if (loopEnabled)
														p->efxData = (uint8_t)(sampleEnd >> 8);
												}
												else
												{
													p->efx = p->efxData = 0;
												}
											}
										}
									}
								}
							}
						}
						break;

						case 'P': // panning slide
						{
							p->efx = 16 + ('P' - 'G');

							if ((p->efxData >> 4) == 0)
							{
								uint8_t param = (((p->efxData & 0x0F) * 255) + 32) / 64;
								if (param > 15)
									param = 15;

								p->efxData = param << 4;
							}
							else if ((p->efxData & 0x0F) == 0)
							{
								uint8_t param = (((p->efxData >> 4) * 255) + 32) / 64;
								if (param > 15)
									param = 15;

								p->efxData = param;
							}
						}
						break;

						case 'Q': // note retrigger
						{
							p->efx = 16 + ('R' - 'G');

							if ((p->efxData & 0xF0) == 0x00)
								p->efxData |= 0x80;
						}
						break;

						case 'R': // tremolo
						{
							p->efx = 7;
							p->efxData = (p->efxData & 0xF0) | ((p->efxData & 0x0F) >> 1);
						}
						break;

						case 'S': // special effects
						{
							switch (p->efxData >> 4)
							{
								case 0x1: p->efx = 0xE3; break; // set glissando control

								case 0x3: // set vibrato waveform
								{
									if ((p->efxData & 0x0F) > 2)
										p->efx = p->efxData = 0;
									else
										p->efx = 0xE4;
								}
								break;

								case 0x4: // set tremolo waveform
								{
									if ((p->efxData & 0x0F) > 2)
										p->efx = p->efxData = 0;
									else
										p->efx = 0xE7;
								}
								break;

								case 0x8:
									p->efx = 0x08;
									p->efxData = (p->efxData << 4) | (p->efxData & 0x0F);
								break;

								case 0xB: p->efx = 0xE6; break; // pattern loop
								case 0xC: p->efx = 0xEC; break; // note cut
								case 0xD: p->efx = 0xED; break; // note delay
								case 0xE: p->efx = 0xEE; break; // pattern delay

								default:
									p->efx = p->efxData = 0;
								break;
							}
						}
						break;

						case 'T': // set tempo (BPM)
						{
							p->efx = 0xF;
							if (p->efxData < 32)
								p->efx = p->efxData = 0; // tempo slide is not supported
						}
						break;

						case 'V': // set global volume
						{
							p->efx = 16 + ('G' - 'G');
							p->efxData >>= 1; // IT2 g.vol. ranges 0..128, FT2 g.vol. ranges 0..64

							if (p->efxData > 64)
								p->efxData = 64;
						}
						break;

						case 'W': // global volume slide
						{
							p->efx = 16 + ('H' - 'G');

							// IT2 g.vol. ranges 0..128, FT2 g.vol. ranges 0..64
							if (p->efxData >> 4 == 0)
							{
								uint8_t param = p->efxData & 0x0F;
								if (param > 1)
									p->efxData = param >> 1;
							}
							else if ((p->efxData & 0x0F) == 0)
							{
								uint8_t param = p->efxData >> 4;
								if (param > 1)
									p->efxData = (param >> 1) << 4;
							}
						}
						break;

						case 'X': p->efx = 8; break; // set 8-bit panning

						default:
							p->efx = p->efxData = 0;
						break;
					}
				}
				else
				{
					p->efxData = 0;
				}

				if (p->instr != 0 && p->efx != 0x3)
					lastGInstr[ch] = p->instr;

				// volume column
				if (p->vol > 0)
				{
					p->vol--;
					if (p->vol <= 64) // set volume
					{
						p->vol += 0x10;
					}
					else if (p->vol <= 74) // fine volume slide up
					{
						p->vol = 0x90 + (p->vol - 65);
					}
					else if (p->vol <= 84) // fine volume slide down
					{
						p->vol = 0x80 + (p->vol - 75);
					}
					else if (p->vol <= 94) // volume slide up
					{
						p->vol = 0x70 + (p->vol - 85);
					}
					else if (p->vol <= 104) // volume slide down
					{
						p->vol = 0x60 + (p->vol - 95);
					}
					else if (p->vol <= 114) // pitch slide down
					{
						uint8_t param = p->vol - 105;
						p->vol = 0;

						if (p->efx == 0 && p->efxData == 0)
						{
							p->efx = 2;
							p->efxData = param * 4;
						}
					}
					else if (p->vol <= 124) // pitch slide up
					{
						uint8_t param = p->vol - 115;
						p->vol = 0;

						if (p->efx == 0 && p->efxData == 0)
						{
							p->efx = 1;
							p->efxData = param * 4;
						}
					}
					else if (p->vol <= 192) // set panning
					{
						p->vol = 0xC0 + (((p->vol - 128) * 15) / 64);
					}
					else if (p->vol >= 193 && p->vol <= 202) // portamento
					{
						uint8_t param = p->vol - 193;
					
						if (p->efx == 0 && p->efxData == 0)
						{
							p->vol = 0;

							p->efx = 3;
							p->efxData = (param == 0) ? 0 : volPortaConv[param-1];
						}
						else
						{
							p->vol = 0xF0 + param;
						}
					}
					else if (p->vol <= 212) // vibrato
					{
						p->vol = 0xB0 + (p->vol - 203);
					}
				}

				// note
				if (p->note == 254) // note cut
				{
					p->note = 0;
					if (p->efx == 0 && p->efxData == 0)
					{
						// EC0 (instant note cut)
						p->efx = 0xE;
						p->efxData = 0xC0;
					}
					else if (p->vol == 0)
					{
						// volume command vol 0
						p->vol = 0x10;
					}
				}
			}

			p += MAX_CHANNELS - songTmp.numChannels;
		}
	}

	// removing this message is considered a criminal act!!!
	loaderMsgBox("This format is very incompatible with FT2. Please don't use this program as a .IT player!");

	return true;

error:
	return false;
}

static void decompress16BitData(int16_t *dst, const uint8_t *src, uint32_t blockLength)
{
	uint8_t byte8, bitDepth, bitDepthInv, bitsRead;
	uint16_t bytes16, lastVal;
	uint32_t bytes32;

	lastVal = 0;
	bitDepth = 17;
	bitDepthInv = bitsRead = 0;

	blockLength >>= 1;
	while (blockLength != 0)
	{
		bytes32 = (*(uint32_t *)src) >> bitsRead;

		bitsRead += bitDepth;
		src += bitsRead >> 3;
		bitsRead &= 7;

		if (bitDepth <= 6)
		{
			bytes32 <<= bitDepthInv & 0x1F;

			bytes16 = (uint16_t)bytes32;
			if (bytes16 != 0x8000)
			{
				lastVal += (int16_t)bytes16 >> (bitDepthInv & 0x1F); // arithmetic shift
				*dst++ = lastVal;
				blockLength--;
			}
			else
			{
				byte8 = ((bytes32 >> 16) & 0xF) + 1;
				if (byte8 >= bitDepth)
					byte8++;
				bitDepth = byte8;

				bitDepthInv = 16;
				if (bitDepthInv < bitDepth)
					bitDepthInv++;
				bitDepthInv -= bitDepth;

				bitsRead += 4;
			}

			continue;
		}

		bytes16 = (uint16_t)bytes32;

		if (bitDepth <= 16)
		{
			uint16_t tmp16 = 0xFFFF >> (bitDepthInv & 0x1F);
			bytes16 &= tmp16;
			tmp16 = (tmp16 >> 1) - 8;

			if (bytes16 > tmp16+16 || bytes16 <= tmp16)
			{
				bytes16 <<= bitDepthInv & 0x1F;
				bytes16 = (int16_t)bytes16 >> (bitDepthInv & 0x1F); // arithmetic shift
				lastVal += bytes16;
				*dst++ = lastVal;
				blockLength--;
				continue;
			}

			byte8 = (uint8_t)(bytes16 - tmp16);
			if (byte8 >= bitDepth)
				byte8++;
			bitDepth = byte8;

			bitDepthInv = 16;
			if (bitDepthInv < bitDepth)
				bitDepthInv++;
			bitDepthInv -= bitDepth;
			continue;
		}

		if (bytes32 & 0x10000)
		{
			bitDepth = (uint8_t)(bytes16 + 1);
			bitDepthInv = 16 - bitDepth;
		}
		else
		{
			lastVal += bytes16;
			*dst++ = lastVal;
			blockLength--;
		}
	}
}

static void decompress8BitData(int8_t *dst, const uint8_t *src, uint32_t blockLength)
{
	uint8_t lastVal, byte8, bitDepth, bitDepthInv, bitsRead;
	uint16_t bytes16;

	lastVal = 0;
	bitDepth = 9;
	bitDepthInv = bitsRead = 0;

	while (blockLength != 0)
	{
		bytes16 = (*(uint16_t *)src) >> bitsRead;

		bitsRead += bitDepth;
		src += (bitsRead >> 3);
		bitsRead &= 7;

		byte8 = bytes16 & 0xFF;

		if (bitDepth <= 6)
		{
			bytes16 <<= (bitDepthInv & 0x1F);
			byte8 = bytes16 & 0xFF;

			if (byte8 != 0x80)
			{
				lastVal += (int8_t)byte8 >> (bitDepthInv & 0x1F); // arithmetic shift
				*dst++ = lastVal;
				blockLength--;
				continue;
			}

			byte8 = (bytes16 >> 8) & 7;
			bitsRead += 3;
			src += (bitsRead >> 3);
			bitsRead &= 7;
		}
		else
		{
			if (bitDepth == 8)
			{
				if (byte8 < 0x7C || byte8 > 0x83)
				{
					lastVal += byte8;
					*dst++ = lastVal;
					blockLength--;
					continue;
				}
				byte8 -= 0x7C;
			}
			else if (bitDepth < 8)
			{
				byte8 <<= 1;
				if (byte8 < 0x78 || byte8 > 0x86)
				{
					lastVal += (int8_t)byte8 >> (bitDepthInv & 0x1F); // arithmetic shift
					*dst++ = lastVal;
					blockLength--;
					continue;
				}
				byte8 = (byte8 >> 1) - 0x3C;
			}
			else
			{
				bytes16 &= 0x1FF;
				if ((bytes16 & 0x100) == 0)
				{
					lastVal += byte8;
					*dst++ = lastVal;
					blockLength--;
					continue;
				}
			}
		}

		byte8++;
		if (byte8 >= bitDepth)
			byte8++;
		bitDepth = byte8;

		bitDepthInv = 8;
		if (bitDepthInv < bitDepth)
			bitDepthInv++;
		bitDepthInv -= bitDepth;
	}
}

static bool loadCompressed16BitSample(FILE *f, sample_t *s, bool deltaEncoded)
{
	int8_t *dstPtr = (int8_t *)s->dataPtr;

	uint32_t i = s->length * 2;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		uint16_t packedLen;
		fread(&packedLen, sizeof (uint16_t), 1, f);
		fread(tmpBuffer, 1, packedLen, f);

		decompress16BitData((int16_t *)dstPtr, tmpBuffer, bytesToUnpack);

		if (deltaEncoded) // convert from delta values to PCM
		{
			int16_t *ptr16 = (int16_t *)dstPtr;
			int16_t lastSmp16 = 0; // yes, reset this every block!

			const uint32_t length = bytesToUnpack >> 1;
			for (uint32_t j = 0; j < length; j++)
			{
				lastSmp16 += ptr16[j];
				ptr16[j] = lastSmp16;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}

	return true;
}

static bool loadCompressed8BitSample(FILE *f, sample_t *s, bool deltaEncoded)
{
	int8_t *dstPtr = (int8_t *)s->dataPtr;

	uint32_t i = s->length;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		uint16_t packedLen;
		fread(&packedLen, sizeof (uint16_t), 1, f);
		fread(tmpBuffer, 1, packedLen, f);

		decompress8BitData(dstPtr, tmpBuffer, bytesToUnpack);

		if (deltaEncoded) // convert from delta values to PCM
		{
			int8_t lastSmp8 = 0; // yes, reset this every block!
			for (uint32_t j = 0; j < bytesToUnpack; j++)
			{
				lastSmp8 += dstPtr[j];
				dstPtr[j] = lastSmp8;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}

	return true;
}

static void setAutoVibrato(instr_t *ins, itSmpHdr_t *is)
{
	ins->autoVibType = is->autoVibratoWaveform;
	if (ins->autoVibType > 3 || is->autoVibratoRate == 0)
	{
		// turn off auto-vibrato
		ins->autoVibDepth = ins->autoVibRate = ins->autoVibSweep = ins->autoVibType = 0;
		return;
	}

	ins->autoVibRate = is->autoVibratoSpeed;
	if (ins->autoVibRate > 63)
		ins->autoVibRate = 63;

	int32_t autoVibSweep = ((is->autoVibratoDepth * 256) + 128) / is->autoVibratoRate;
	if (autoVibSweep > 255)
		autoVibSweep = 255;
	ins->autoVibSweep = (uint8_t)autoVibSweep;

	ins->autoVibDepth = is->autoVibratoDepth;
	if (ins->autoVibDepth > 15)
		ins->autoVibDepth = 15;
}

static bool loadSample(FILE *f, sample_t *s, itSmpHdr_t *is)
{
	bool sampleIs16Bit = !!(is->flags & 2);
	bool compressed = !!(is->flags & 8);
	bool hasLoop = !!(is->flags & 16);
	bool bidiLoop = !!(is->flags & 64);
	bool signedSamples = !!(is->cvt & 1);
	bool deltaEncoded = !!(is->cvt & 4);

	if (sampleIs16Bit)
		s->flags |= SAMPLE_16BIT;

	if (hasLoop)
		s->flags |= bidiLoop ? LOOP_BIDI : LOOP_FWD;

	s->length = is->length;
	s->loopStart = is->loopBegin;
	s->loopLength = is->loopEnd - is->loopBegin;
	s->volume = is->vol;

	s->panning = 128;
	if (is->defPan & 128) // use panning?
	{
		int32_t pan = (is->defPan & 127) * 4; // 0..64 -> 0..256
		if (pan > 255)
			pan = 255;

		s->panning = (uint8_t)pan;
	}

	memcpy(s->name, is->sampleName, 22);
	s->name[22] = '\0';

	setSampleC4Hz(s, is->c5Speed);

	if (s->length <= 0 || is->offsetInFile == 0)
		return true; // empty sample, skip data loading

	if (!allocateSmpData(s, s->length, sampleIs16Bit))
		return false;

	// begin sample loading

	fseek(f, is->offsetInFile, SEEK_SET);

	if (compressed)
	{
		if (sampleIs16Bit)
			loadCompressed16BitSample(f, s, deltaEncoded);
		else
			loadCompressed8BitSample(f, s, deltaEncoded);
	}
	else
	{
		if (sampleIs16Bit)
			fread(s->dataPtr, 2, s->length, f);
		else
			fread(s->dataPtr, 1, s->length, f);

		if (!signedSamples)
		{
			if (sampleIs16Bit)
			{
				int16_t *ptr16 = (int16_t *)s->dataPtr;
				for (int32_t i = 0; i < s->length; i++)
					ptr16[i] ^= 0x8000;
			}
			else
			{
				int8_t *ptr8 = (int8_t *)s->dataPtr;
				for (int32_t i = 0; i < s->length; i++)
					ptr8[i] ^= 0x80;
			}
		}
	}

	return true;
}
