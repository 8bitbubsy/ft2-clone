// Scream Tracker 3 (or compatible) S3M loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct songS3MinstrHeaderTyp_t
{
	uint8_t typ;
	char dosName[12];
	uint8_t memSegH;
	uint16_t memSeg;
	int32_t len, repS, repE;
	uint8_t vol, dsk, pack, flags;
	int32_t c2Spd, res1;
	uint16_t gusPos;
	uint8_t res2[6];
	char name[28], id[4];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songS3MinstrHeaderTyp;

typedef struct songS3MHeaderTyp_t
{
	char name[28];
	uint8_t id1a, typ;
	uint16_t res1;
	int16_t songTabLen, antInstr, antPatt;
	uint16_t flags, trackerID, ver;
	char id[4];
	uint8_t globalVol, defSpeed, defTempo, masterVol, res2[12], chanType[32];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songS3MHeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint8_t pattBuff[12288];

static int8_t countS3MChannels(uint16_t antPtn);

bool loadS3M(FILE *f, uint32_t filesize)
{
	uint8_t ha[2048];
	uint8_t alastnfo[32], alastefx[32], alastvibnfo[32], s3mLastGInstr[32];
	uint8_t typ;
	int16_t ai, ap, ver, ii, kk, tmp;
	uint16_t ptnOfs[256];
	int32_t i, j, k, len;
	tonTyp ton;
	sampleTyp *s;
	songS3MHeaderTyp h_S3M;
	songS3MinstrHeaderTyp h_S3MInstr;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (h_S3M))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&h_S3M, 0, sizeof (h_S3M));
	if (fread(&h_S3M, 1, sizeof (h_S3M), f) != sizeof (h_S3M))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h_S3M.antInstr > MAX_INST || h_S3M.songTabLen > 256 || h_S3M.antPatt > 256 ||
		h_S3M.typ != 16 || h_S3M.ver < 1 || h_S3M.ver > 2)
	{
		loaderMsgBox("Error loading .s3m: Incompatible module!");
		return false;
	}

	memset(songTmp.songTab, 255, sizeof (songTmp.songTab));
	if (fread(songTmp.songTab, h_S3M.songTabLen, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	songTmp.len = h_S3M.songTabLen;

	// remove pattern separators (254)
	k = 0;
	j = 0;
	for (i = 0; i < songTmp.len; i++)
	{
		if (songTmp.songTab[i] != 254)
			songTmp.songTab[j++] = songTmp.songTab[i];
		else
			k++;
	}

	if (k <= songTmp.len)
		songTmp.len -= (uint16_t)k;
	else
		songTmp.len = 1;

	for (i = 1; i < songTmp.len; i++)
	{
		if (songTmp.songTab[i] == 255)
		{
			songTmp.len = (uint16_t)i;
			break;
		}
	}
	
	// clear unused song table entries
	if (songTmp.len < 255)
		memset(&songTmp.songTab[songTmp.len], 0, 255-songTmp.len);

	songTmp.speed = h_S3M.defTempo;
	if (songTmp.speed < 32)
		songTmp.speed = 32;

	songTmp.tempo = h_S3M.defSpeed;
	if (songTmp.tempo == 0)
		songTmp.tempo = 6;

	if (songTmp.tempo > 31)
		songTmp.tempo = 31;

	songTmp.initialTempo = songTmp.tempo;
	memcpy(songTmp.name, h_S3M.name, 20);

	ap = h_S3M.antPatt;
	ai = h_S3M.antInstr;
	ver = h_S3M.ver;

	k = 31;
	while (k >= 0 && h_S3M.chanType[k] >= 16) k--;
	songTmp.antChn = (k + 2) & 254;

	if (fread(ha, ai*2, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	if (fread(ptnOfs, ap*2, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	// *** PATTERNS ***

	k = 0;
	for (i = 0; i < ap; i++)
	{
		if (ptnOfs[i]  == 0)
			continue; // empty pattern

		memset(alastnfo, 0, sizeof (alastnfo));
		memset(alastefx, 0, sizeof (alastefx));
		memset(alastvibnfo, 0, sizeof (alastvibnfo));
		memset(s3mLastGInstr, 0, sizeof (s3mLastGInstr));

		fseek(f, ptnOfs[i] << 4, SEEK_SET);
		if (feof(f))
			continue;

		if (fread(&j, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		if (j > 0 && j <= 12288)
		{
			if (!allocateTmpPatt(i, 64))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			fread(pattBuff, j, 1, f);

			k = 0;
			kk = 0;

			while (k < j && kk < 64)
			{
				typ = pattBuff[k++];

				if (typ == 0)
				{
					kk++;
				}
				else
				{
					ii = typ & 31;

					memset(&ton, 0, sizeof (ton));

					// note and sample
					if (typ & 32)
					{
						ton.ton = pattBuff[k++];
						ton.instr = pattBuff[k++];

						if (ton.instr > MAX_INST)
							ton.instr = 0;

						     if (ton.ton == 254) ton.ton = 97;
						else if (ton.ton == 255) ton.ton = 0;
						else
						{
							ton.ton = 1 + (ton.ton & 0xF) + (ton.ton >> 4) * 12;
							if (ton.ton > 96)
								ton.ton = 0;
						}
					}

					// volume column
					if (typ & 64)
					{
						ton.vol = pattBuff[k++];

						if (ton.vol <= 64)
							ton.vol += 0x10;
						else
							ton.vol = 0;
					}

					// effect
					if (typ & 128)
					{
						ton.effTyp = pattBuff[k++];
						ton.eff = pattBuff[k++];

						if (ton.eff > 0)
						{
							alastnfo[ii] = ton.eff;
							if (ton.effTyp == 8 || ton.effTyp == 21)
								alastvibnfo[ii] = ton.eff; // H/U
						}

						// in ST3, a lot of effects directly share the same memory!
						if (ton.eff == 0 && ton.effTyp != 7) // G
						{
							uint8_t efx = ton.effTyp;
							if (efx == 8 || efx == 21) // H/U
								ton.eff = alastvibnfo[ii];
							else if ((efx >= 4 && efx <= 12) || (efx >= 17 && efx <= 19)) // D/E/F/I/J/K/L/Q/R/S
								ton.eff = alastnfo[ii];

							/* If effect data is zero and effect type was the same as last one, clear out
							** data if it's not J or S (those have no memory in the equivalent XM effects).
							** Also goes for extra fine pitch slides and fine volume slides,
							** since they get converted to other effects.
							*/
							if (efx == alastefx[ii] && ton.effTyp != 10 && ton.effTyp != 19) // J/S
							{
								uint8_t nfo = ton.eff;
								bool extraFinePitchSlides = (efx == 5 || efx == 6) && ((nfo & 0xF0) == 0xE0);
								bool fineVolSlides = (efx == 4 || efx == 11) &&
								     ((nfo > 0xF0) || (((nfo & 0xF) == 0xF) && ((nfo & 0xF0) > 0)));

								if (!extraFinePitchSlides && !fineVolSlides)
									ton.eff = 0;
							}
						}

						if (ton.effTyp > 0)
							alastefx[ii] = ton.effTyp;

						switch (ton.effTyp)
						{
							case 1: // A
							{
								ton.effTyp = 0xF;
								if (ton.eff == 0 || ton.eff > 0x1F)
								{
									ton.effTyp = 0;
									ton.eff = 0;
								}
							}
							break;

							case 2: ton.effTyp = 0xB; break; // B
							case 3: ton.effTyp = 0xD; break; // C
							case 4: // D
							{
								if (ton.eff > 0xF0) // fine slide up
								{
									ton.effTyp = 0xE;
									ton.eff = 0xB0 | (ton.eff & 0xF);
								}
								else if ((ton.eff & 0x0F) == 0x0F && (ton.eff & 0xF0) > 0) // fine slide down
								{
									ton.effTyp = 0xE;
									ton.eff = 0xA0 | (ton.eff >> 4);
								}
								else
								{
									ton.effTyp = 0xA;
									if (ton.eff & 0x0F) // on D/K, last nybble has first priority in ST3
										ton.eff &= 0x0F;
								}
							}
							break;

							case 5: // E
							case 6: // F
							{
								if ((ton.eff & 0xF0) >= 0xE0)
								{
									// convert to fine slide
									if ((ton.eff & 0xF0) == 0xE0)
										tmp = 0x21;
									else
										tmp = 0xE;

									ton.eff &= 0x0F;

									if (ton.effTyp == 0x05)
										ton.eff |= 0x20;
									else
										ton.eff |= 0x10;

									ton.effTyp = (uint8_t)tmp;

									if (ton.effTyp == 0x21 && ton.eff == 0)
									{
										ton.effTyp = 0;
									}
								}
								else
								{
									// convert to normal 1xx/2xx slide
									ton.effTyp = 7 - ton.effTyp;
								}
							}
							break;

							case 7: // G
							{
								ton.effTyp = 0x03;

								// fix illegal slides (to new instruments)
								if (ton.instr != 0 && ton.instr != s3mLastGInstr[ii])
									ton.instr = s3mLastGInstr[ii];
							}
							break;

							case 11: // K
							{
								if (ton.eff > 0xF0) // fine slide up
								{
									ton.effTyp = 0xE;
									ton.eff = 0xB0 | (ton.eff & 0xF);

									// if volume column is unoccupied, set to vibrato
									if (ton.vol == 0)
										ton.vol = 0xB0;
								}
								else if ((ton.eff & 0x0F) == 0x0F && (ton.eff & 0xF0) > 0) // fine slide down
								{
									ton.effTyp = 0xE;
									ton.eff = 0xA0 | (ton.eff >> 4);

									// if volume column is unoccupied, set to vibrato
									if (ton.vol == 0)
										ton.vol = 0xB0;
								}
								else
								{
									ton.effTyp = 0x6;
									if (ton.eff & 0x0F) // on D/K, last nybble has first priority in ST3
										ton.eff &= 0x0F;
								}
							}
							break;

							case 8: ton.effTyp = 0x04; break; // H
							case 9: ton.effTyp = 0x1D; break; // I
							case 10: ton.effTyp = 0x00; break; // J
							case 12: ton.effTyp = 0x05; break; // L
							case 15: ton.effTyp = 0x09; break; // O
							case 17: ton.effTyp = 0x1B; break; // Q
							case 18: ton.effTyp = 0x07; break; // R

							case 19: // S
							{
								ton.effTyp = 0xE;
								tmp = ton.eff >> 4;
								ton.eff &= 0x0F;

								     if (tmp == 0x1) ton.eff |= 0x30;
								else if (tmp == 0x2) ton.eff |= 0x50;
								else if (tmp == 0x3) ton.eff |= 0x40;
								else if (tmp == 0x4) ton.eff |= 0x70;
								// ignore S8x becuase it's not compatible with FT2 panning
								else if (tmp == 0xB) ton.eff |= 0x60;
								else if (tmp == 0xC) // Note Cut
								{
									ton.eff |= 0xC0;
									if (ton.eff == 0xC0)
									{
										// EC0 does nothing in ST3 but cuts voice in FT2, remove effect
										ton.effTyp = 0;
										ton.eff = 0;
									}
								}
								else if (tmp == 0xD) // Note Delay
								{
									ton.eff |= 0xD0;
									if (ton.ton == 0 || ton.ton == 97)
									{
										// EDx without a note does nothing in ST3 but retrigs in FT2, remove effect
										ton.effTyp = 0;
										ton.eff = 0;
									}
									else if (ton.eff == 0xD0)
									{
										// ED0 prevents note/smp/vol from updating in ST3, remove everything
										ton.ton = 0;
										ton.instr = 0;
										ton.vol = 0;
										ton.effTyp = 0;
										ton.eff = 0;
									}
								}
								else if (tmp == 0xE) ton.eff |= 0xE0;
								else if (tmp == 0xF) ton.eff |= 0xF0;
								else
								{
									ton.effTyp = 0;
									ton.eff = 0;
								}
							}
							break;

							case 20: // T
							{
								ton.effTyp = 0x0F;
								if (ton.eff < 0x21) // Txx with a value lower than 33 (0x21) does nothing in ST3, remove effect
								{
									ton.effTyp = 0;
									ton.eff = 0;
								}
							}
							break;

							case 22: // V
							{
								ton.effTyp = 0x10;
								if (ton.eff > 0x40)
								{
									// Vxx > 0x40 does nothing in ST3
									ton.effTyp = 0;
									ton.eff = 0;
								}
							}
							break;

							default:
							{
								ton.effTyp = 0;
								ton.eff = 0;
							}
							break;
						}
					}

					if (ton.instr != 0 && ton.effTyp != 0x3)
						s3mLastGInstr[ii] = ton.instr;

					pattTmp[i][(kk * MAX_VOICES) + ii] = ton;
				}
			}

			if (tmpPatternEmpty((uint16_t)i))
			{
				if (pattTmp[i] != NULL)
				{
					free(pattTmp[i]);
					pattTmp[i] = NULL;
				}
			}
		}
	}

	// *** SAMPLES ***

	bool adlibInsWarn = false;

	memcpy(ptnOfs, ha, 512);
	for (i = 0; i < ai; i++)
	{
		fseek(f, ptnOfs[i] << 4, SEEK_SET);

		if (fread(&h_S3MInstr, 1, sizeof (h_S3MInstr), f) != sizeof (h_S3MInstr))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		
		memcpy(songTmp.instrName[1+i], h_S3MInstr.name, 22);

		if (h_S3MInstr.typ == 2)
		{
			adlibInsWarn = true;
		}
		else if (h_S3MInstr.typ == 1)
		{
			if ((h_S3MInstr.flags & (255-1-2-4)) != 0 || h_S3MInstr.pack != 0)
			{
				loaderMsgBox("Error loading .s3m: Incompatible module!");
				return false;
			}
			else if (h_S3MInstr.memSeg > 0 && h_S3MInstr.len > 0)
			{
				if (!allocateTmpInstr((int16_t)(1 + i)))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				setNoEnvelope(instrTmp[1 + i]);
				s = &instrTmp[1+i]->samp[0];

				// non-FT2: fixes "miracle man.s3m" and other broken S3Ms
				if ((h_S3MInstr.memSeg<<4)+h_S3MInstr.len > (int32_t)filesize)
					h_S3MInstr.len = filesize - (h_S3MInstr.memSeg << 4);

				len = h_S3MInstr.len;

				bool hasLoop = h_S3MInstr.flags & 1;
				bool stereoSample = (h_S3MInstr.flags >> 1) & 1;
				bool is16Bit = (h_S3MInstr.flags >> 2) & 1;
			
				if (is16Bit) // 16-bit
					len <<= 1;

				if (stereoSample) // stereo
					len <<= 1;

				if (!allocateTmpSmpData(s, len))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				memcpy(s->name, h_S3MInstr.name, 21);

				if (h_S3MInstr.c2Spd > 65535) // ST3 (and OpenMPT) does this
					h_S3MInstr.c2Spd = 65535;

				s->len = h_S3MInstr.len;
				s->vol = h_S3MInstr.vol;
				s->repS = h_S3MInstr.repS;
				s->repL = h_S3MInstr.repE - h_S3MInstr.repS;

				tuneSample(s, h_S3MInstr.c2Spd, tmpLinearPeriodsFlag);

				if (s->vol > 64)
					s->vol = 64;

				if (s->repL <= 2 || s->repS+s->repL > s->len || s->repL == 0)
				{
					s->repS = 0;
					s->repL = 0;
					hasLoop = false;
				}

				s->typ = hasLoop + (is16Bit << 4);

				fseek(f, h_S3MInstr.memSeg << 4, SEEK_SET);

				if (ver == 1)
				{
					fseek(f, len, SEEK_CUR); // sample not supported
				}
				else
				{
					if (fread(s->pek, len, 1, f) != 1)
					{
						loaderMsgBox("General I/O error during loading! Is the file in use?");
						return false;
					}

					if (is16Bit)
					{
						conv16BitSample(s->pek, len, stereoSample);

						s->len <<= 1;
						s->repS <<= 1;
						s->repL <<= 1;
					}
					else
					{
						conv8BitSample(s->pek, len, stereoSample);
					}

					// if stereo sample: reduce memory footprint after sample was downmixed to mono
					if (stereoSample)
						reallocateTmpSmpData(s, s->len);
				}
			}
		}
	}

	songTmp.antChn = countS3MChannels(ap);

	if (adlibInsWarn)
		loaderMsgBox("Warning: The module contains unsupported AdLib instruments!");

	if (!(config.dontShowAgainFlags & DONT_SHOW_IMPORT_WARNING_FLAG))
		loaderSysReq(6, "System message", "Loading of this format is not fully supported and can have issues.");

	return true;
}

static int8_t countS3MChannels(uint16_t antPtn)
{
	int32_t channels = 0;
	for (int32_t i = 0; i < antPtn; i++)
	{
		if (pattTmp[i] == NULL)
			continue;

		tonTyp *ton = pattTmp[i];
		for (int32_t j = 0; j < 64; j++)
		{
			for (int32_t k = 0; k < MAX_VOICES; k++, ton++)
			{
				if (ton->eff == 0 && ton->effTyp == 0 && ton->instr == 0 && ton->ton == 0 && ton->vol == 0)
					continue;

				if (k > channels)
					channels = k;
			}
		}
	}
	channels++;

	return (int8_t)channels;
}
