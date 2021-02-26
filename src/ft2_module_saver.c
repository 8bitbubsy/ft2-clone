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

/* These savers are directly ported, so they should act identical to FT2
** except for some very minor changes.
*/

static SDL_Thread *thread;

static uint8_t packedPattData[65536];
static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows);

static const char modSig[32][5] =
{
	"1CHN", "2CHN", "3CHN", "4CHN", "5CHN", "6CHN", "7CHN", "8CHN",
	"9CHN", "10CH", "11CH", "12CH", "13CH", "14CH", "15CH", "16CH",
	"17CH", "18CH", "19CH", "20CH", "21CH", "22CH", "23CH", "24CH",
	"25CH", "26CH", "27CH", "28CH", "29CH", "30CH", "31CH", "32CH"
};

bool saveXM(UNICHAR *filenameU)
{
	int16_t i, j, k, a;
	size_t result;
	songHeaderTyp h;
	patternHeaderTyp ph;
	instrTyp *ins;
	instrHeaderTyp ih;
	sampleTyp *s;
	sampleHeaderTyp *dst;

	FILE *f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "Error opening file for saving, is it in use?");
		return false;
	}

	memcpy(h.sig, "Extended Module: ", 17);
	memset(h.name, ' ', 20);
	h.name[20] = 0x1A;
	memcpy(h.name, song.name, strlen(song.name));
	memcpy(h.progName, PROG_NAME_STR, 20);
	h.ver = 0x0104;
	h.headerSize = 20 + 256;
	h.len = song.len;
	h.repS = song.repS;
	h.antChn = (uint16_t)song.antChn;
	h.defTempo = song.tempo;
	h.defSpeed = song.speed;

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
	h.antPtn = ap;

	// count number of instruments
	int16_t ai = 128;
	while (ai > 0 && getUsedSamples(ai) == 0 && song.instrName[ai][0] == '\0')
		ai--;
	h.antInstrs = ai;

	h.flags = audio.linearPeriodsFlag;
	memcpy(h.songTab, song.songTab, sizeof (song.songTab));

	if (fwrite(&h, sizeof (h), 1, f) != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
		return false;
	}

	for (i = 0; i < ap; i++)
	{
		if (patternEmpty(i))
		{
			if (patt[i] != NULL)
			{
				free(patt[i]);
				patt[i] = NULL;
			}

			pattLens[i] = 64;
		}

		ph.patternHeaderSize = sizeof (patternHeaderTyp);
		ph.pattLen = pattLens[i];
		ph.typ = 0;

		if (patt[i] == NULL)
		{
			ph.dataLen = 0;
			if (fwrite(&ph, ph.patternHeaderSize, 1, f) != 1)
			{
				fclose(f);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
				return false;
			}
		}
		else
		{
			ph.dataLen = packPatt(packedPattData, (uint8_t *)patt[i], pattLens[i]);

			result = fwrite(&ph, ph.patternHeaderSize, 1, f);
			result += fwrite(packedPattData, ph.dataLen, 1, f);

			if (result != 2) // write was not OK
			{
				fclose(f);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
				return false;
			}
		}
	}

	memset(&ih, 0, sizeof (ih)); // important, clears reserved stuff

	for (i = 1; i <= ai; i++)
	{
		if (instr[i] == NULL)
			j = 0;
		else
			j = i;

		a = getUsedSamples(i);

		memset(ih.name, 0, 22);
		memcpy(ih.name, song.instrName[i], strlen(song.instrName[i]));

		ih.typ = 0;
		ih.antSamp = a;
		ih.sampleSize = sizeof (sampleHeaderTyp);

		if (a > 0)
		{
			ins = instr[j];

			memcpy(ih.ta, ins->ta, 96);
			memcpy(ih.envVP, ins->envVP, 12*2*sizeof(int16_t));
			memcpy(ih.envPP, ins->envPP, 12*2*sizeof(int16_t));
			ih.envVPAnt = ins->envVPAnt;
			ih.envPPAnt = ins->envPPAnt;
			ih.envVSust = ins->envVSust;
			ih.envVRepS = ins->envVRepS;
			ih.envVRepE = ins->envVRepE;
			ih.envPSust = ins->envPSust;
			ih.envPRepS = ins->envPRepS;
			ih.envPRepE = ins->envPRepE;
			ih.envVTyp = ins->envVTyp;
			ih.envPTyp = ins->envPTyp;
			ih.vibTyp = ins->vibTyp;
			ih.vibSweep = ins->vibSweep;
			ih.vibDepth = ins->vibDepth;
			ih.vibRate = ins->vibRate;
			ih.fadeOut = ins->fadeOut;
			ih.midiOn = ins->midiOn ? 1 : 0;
			ih.midiChannel = ins->midiChannel;
			ih.midiProgram = ins->midiProgram;
			ih.midiBend = ins->midiBend;
			ih.mute = ins->mute ? 1 : 0;
			ih.instrSize = INSTR_HEADER_SIZE;
			
			for (k = 0; k < a; k++)
			{
				s = &instr[j]->samp[k];
				dst = &ih.samp[k];

				dst->len = s->len;
				dst->repS = s->repS;
				dst->repL = s->repL;
				dst->vol = s->vol;
				dst->fine = s->fine;
				dst->typ = s->typ;
				dst->pan = s->pan;
				dst->relTon = s->relTon;

				uint8_t nameLen = (uint8_t)strlen(s->name);

				dst->nameLen = nameLen;
				memset(dst->name, ' ', 22);
				memcpy(dst->name, s->name, nameLen);

				if (s->pek == NULL)
					dst->len = 0;
			}
		}
		else
		{
			ih.instrSize = 22 + 11;
		}

		if (fwrite(&ih, ih.instrSize + (a * sizeof (sampleHeaderTyp)), 1, f) != 1)
		{
			fclose(f);
			okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
			return false;
		}

		for (k = 1; k <= a; k++)
		{
			s = &instr[j]->samp[k-1];
			if (s->pek != NULL)
			{
				restoreSample(s);
				samp2Delta(s->pek, s->len, s->typ);

				result = fwrite(s->pek, 1, s->len, f);

				delta2Samp(s->pek, s->len, s->typ);
				fixSample(s);

				if (result != (size_t)s->len) // write not OK
				{
					fclose(f);
					okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
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
	bool test, tooManyInstr, incompatEfx, noteUnderflow;
	int8_t *srcPtr, *dstPtr;
	uint8_t ton, inst, pattBuff[64*4*32];
	int16_t a, i, ap;
	int32_t j, k, l1, l2, l3, writeLen, bytesToWrite, bytesWritten;
	FILE *f;
	instrTyp *ins;
	sampleTyp *smp;
	tonTyp *t;
	songMOD31HeaderTyp hm;

	tooManyInstr = false;
	incompatEfx = false;
	noteUnderflow = false;

	if (audio.linearPeriodsFlag)
		okBoxThreadSafe(0, "System message", "Linear frequency table used!");

	// sanity checking

	test = false;
	if (song.len > 128)
		test = true;

	for (i = 100; i < 256; i++)
	{
		if (patt[i] != NULL)
		{
			test = true;
			break;
		}
	}
	if (test) okBoxThreadSafe(0, "System message", "Too many patterns!");

	for (i = 32; i <= 128; i++)
	{
		if (getRealUsedSamples(i) > 0)
		{
			okBoxThreadSafe(0, "System message", "Too many instruments!");
			break;
		}
	}

	test = false;
	for (i = 1; i <= 31; i++)
	{
		ins = instr[i];
		if (ins == NULL)
			continue;

		smp = &ins->samp[0];

		j = getRealUsedSamples(i);
		if (j > 1)
		{
			test = true;
			break;
		}

		if (j == 1)
		{
			if (smp->len > 65534 || ins->fadeOut != 0 || ins->envVTyp != 0 || ins->envPTyp != 0 ||
				(smp->typ & 3) == 2 || smp->relTon != 0 || ins->midiOn)
			{
				test = true;
				break;
			}
		}
	}
	if (test) okBoxThreadSafe(0, "System message", "Incompatible instruments!");

	for (i = 0; i < 99; i++)
	{
		if (patt[i] != NULL)
		{
			if (pattLens[i] != 64)
			{
				okBoxThreadSafe(0, "System message", "Unable to convert module. (Illegal pattern length)");
				return false;
			}

			for (j = 0; j < 64; j++)
			{
				for (k = 0; k < song.antChn; k++)
				{
					t = &patt[i][(j * MAX_VOICES) + k];

					if (t->instr > 31)
						tooManyInstr = true;

					if (t->effTyp > 15 || t->vol != 0)
						incompatEfx = true;

					// added security that wasn't present in FT2
					if (t->ton > 0 && t->ton < 10)
						noteUnderflow = true;
				}
			}
		}
	}
	if (tooManyInstr)  okBoxThreadSafe(0, "System message", "Instrument(s) above 31 was found in pattern data!");
	if (incompatEfx)   okBoxThreadSafe(0, "System message", "Incompatible effect(s) was found in pattern data!");
	if (noteUnderflow) okBoxThreadSafe(0, "System message", "Note(s) below A-0 were found in pattern data!");

	// setup header buffer
	memset(&hm, 0, sizeof (hm));
	memcpy(hm.name, song.name, sizeof (hm.name));
	hm.len = (uint8_t)song.len;
	if (hm.len > 128) hm.len = 128;
	hm.repS = (uint8_t)song.repS;
	if (hm.repS > 127) hm.repS = 0;
	memcpy(hm.songTab, song.songTab, song.len);

	// calculate number of patterns
	ap = 0;
	for (i = 0; i < song.len; i++)
	{
		if (song.songTab[i] > ap)
			ap = song.songTab[i];
	}

	if (song.antChn == 4)
		memcpy(hm.sig, (ap > 64) ? "M!K!" : "M.K.", 4);
	else
		memcpy(hm.sig, modSig[song.antChn-1], 4);

	// read sample information into header buffer
	for (i = 1; i <= 31; i++)
	{
		songMODInstrHeaderTyp *modIns = &hm.instr[i-1];

		memcpy(modIns->name, song.instrName[i], sizeof (modIns->name));
		if (instr[i] != NULL && getRealUsedSamples(i) != 0)
		{
			smp = &instr[i]->samp[0];

			l1 = smp->len >> 1;
			l2 = smp->repS >> 1;
			l3 = smp->repL >> 1;

			if (smp->typ & 16)
			{
				l1 >>= 1;
				l2 >>= 1;
				l3 >>= 1;
			}

			if (l1 > 32767)
				l1 = 32767;

			if (l2 > l1)
				l2 = l1;

			if (l2+l3 > l1)
				l3 = l1 - l2;

			// FT2 bug-fix
			if (l3 < 1)
			{
				l2 = 0;
				l3 = 1;
			}

			modIns->len = (uint16_t)(SWAP16(l1));
			modIns->fine = ((smp->fine + 128) >> 4) ^ 8;
			modIns->vol = smp->vol;

			if ((smp->typ & 3) == 0)
			{
				modIns->repS = 0;
				modIns->repL = SWAP16(1);
			}
			else
			{
				modIns->repS = (uint16_t)(SWAP16(l2));
				modIns->repL = (uint16_t)(SWAP16(l3));
			}
		}

		// FT2 bugfix: never allow replen being below 2 (1)
		if (SWAP16(modIns->repL) < 1)
		{
			modIns->repS = SWAP16(0);
			modIns->repL = SWAP16(1);
		}
	}

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "Error opening file for saving, is it in use?");
		return false;
	}

	// write header
	if (fwrite(&hm, 1, sizeof (hm), f) != sizeof (hm))
	{
		okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
		goto modSaveError;
	}

	// write pattern data
	for (i = 0; i <= ap; i++)
	{
		if (patt[i] == NULL)
		{
			// empty pattern
			memset(pattBuff, 0, song.antChn * (64 * 4));
		}
		else
		{
			a = 0;
			for (j = 0; j < 64; j++)
			{
				for (k = 0; k < song.antChn; k++)
				{
					t = &patt[i][(j * MAX_VOICES) + k];

					inst = t->instr;
					ton = t->ton;

					// FT2 bugfix: prevent overflow
					if (inst > 31)
						inst = 0;

					// FT2 bugfix: convert note-off into no note
					if (ton == 97)
						ton = 0;

					// FT2 bugfix: clamp notes below 10 (A-0) to prevent 12-bit period overflow
					if (ton > 0 && ton < 10)
						ton = 10;

					if (ton == 0)
					{
						pattBuff[a+0] = inst & 0xF0;
						pattBuff[a+1] = 0;
					}
					else
					{
						pattBuff[a+0] = (inst & 0xF0) | ((amigaPeriod[ton-1] >> 8) & 0x0F);
						pattBuff[a+1] = amigaPeriod[ton-1] & 0xFF;
					}

					// FT2 bugfix: if effect is overflowing (0xF in .MOD), set effect and param to 0
					if (t->effTyp > 0x0F)
					{
						pattBuff[a+2] = (inst & 0x0F) << 4;
						pattBuff[a+3] = 0;
					}
					else
					{
						pattBuff[a+2] = ((inst & 0x0F) << 4) | (t->effTyp & 0x0F);
						pattBuff[a+3] = t->eff;
					}

					a += 4;
				}
			}
		}

		if (fwrite(pattBuff, 1, song.antChn * (64 * 4), f) != (size_t)(song.antChn * (64 * 4)))
		{
			okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
			goto modSaveError;
		}
	}

	// write sample data
	for (i = 0; i < 31; i++)
	{
		if (instr[1+i] == NULL || getRealUsedSamples(1+i) == 0)
			continue;

		smp = &instr[1+i]->samp[0];
		if (smp->pek == NULL || smp->len <= 0)
			continue;

		restoreSample(smp);

		l1 = smp->len >> 1;
		if (smp->typ & 16) // 16-bit sample (convert to 8-bit)
		{
			if (l1 > 65534)
				l1 = 65534;

			// let's borrow "pattBuff" here
			dstPtr = (int8_t *)pattBuff;

			writeLen = l1;
			bytesWritten = 0;
			while (bytesWritten < writeLen) // write in 8K blocks
			{
	 			bytesToWrite = sizeof (pattBuff);
				if (bytesWritten+bytesToWrite > writeLen)
					bytesToWrite = writeLen - bytesWritten;

				srcPtr = &smp->pek[(bytesWritten << 1) + 1]; // +1 to align to high byte
				for (j = 0; j < bytesToWrite; j++)
					dstPtr[j] = srcPtr[j << 1];

				if (fwrite(dstPtr, 1, bytesToWrite, f) != (size_t)bytesToWrite)
				{
					fixSample(smp);
					okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
					goto modSaveError;
				}

				bytesWritten += bytesToWrite;
			}
		}
		else
		{
			// 8-bit sample

			if (l1 > 32767)
				l1 = 32767;

			l1 <<= 1;

			if (fwrite(smp->pek, 1, l1, f) != (size_t)l1)
			{
				fixSample(smp);
				okBoxThreadSafe(0, "System message", "Error saving module: general I/O error!");
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
		okBoxThreadSafe(0, "System message", "Couldn't create thread!");
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

	const int32_t pitch = sizeof (tonTyp) * (MAX_VOICES - song.antChn);
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < song.antChn; chn++)
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
