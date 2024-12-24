/* Scream Tracker 3 (or compatible) S3M loader
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

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct s3mSmpHdr_t
{
	uint8_t type, junk1[12], offsetInFileH;
	uint16_t offsetInFile;
	int32_t length, loopStart, loopEnd;
	uint8_t volume, junk2, packFlag, flags;
	int32_t midCFreq, junk3;
	uint16_t junk4;
	uint8_t junk5[6];
	char name[28], ID[4];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
s3mSmpHdr_t;

typedef struct s3mHdr_t
{
	char name[28];
	uint8_t junk1, type;
	uint16_t junk2;
	int16_t numOrders, numSamples, numPatterns;
	uint16_t flags, junk3, version;
	char ID[4];
	uint8_t junk4, speed, BPM, junk5, junk6[12], chnSettings[32];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
s3mHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint8_t pattBuff[12288];

static int8_t countS3MChannels(uint16_t antPtn);

bool loadS3M(FILE *f, uint32_t filesize)
{
	uint8_t alastnfo[32], alastefx[32], alastvibnfo[32], s3mLastGInstr[32];
	int16_t ii, kk, tmp;
	int32_t patternOffsets[256], sampleOffsets[256], j, k;
	note_t tmpNote;
	sample_t *s;
	s3mHdr_t hdr;
	s3mSmpHdr_t smpHdr;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&hdr, 0, sizeof (hdr));
	if (fread(&hdr, 1, sizeof (hdr), f) != sizeof (hdr))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (hdr.numSamples > MAX_INST || hdr.numOrders > MAX_ORDERS || hdr.numPatterns > MAX_PATTERNS ||
		hdr.type != 16 || hdr.version < 1 || hdr.version > 2)
	{
		loaderMsgBox("Error loading .s3m: Incompatible module!");
		return false;
	}

	memset(songTmp.orders, 255, 256); // pad by 255
	if (fread(songTmp.orders, hdr.numOrders, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	songTmp.songLength = hdr.numOrders;

	// remove pattern separators (254)
	k = 0;
	j = 0;
	for (int32_t i = 0; i < songTmp.songLength; i++)
	{
		if (songTmp.orders[i] != 254)
			songTmp.orders[j++] = songTmp.orders[i];
		else
			k++;
	}

	if (k <= songTmp.songLength)
		songTmp.songLength -= (uint16_t)k;
	else
		songTmp.songLength = 1;

	for (int32_t i = 1; i < songTmp.songLength; i++)
	{
		if (songTmp.orders[i] == 255)
		{
			songTmp.songLength = (uint16_t)i;
			break;
		}
	}
	
	// clear unused song table entries
	if (songTmp.songLength < 255)
		memset(&songTmp.orders[songTmp.songLength], 0, 255-songTmp.songLength);

	memcpy(songTmp.name, hdr.name, 20);

	songTmp.BPM = hdr.BPM;
	songTmp.speed = hdr.speed;

	// load sample offsets
	for (int32_t i = 0; i < hdr.numSamples; i++)
	{
		uint16_t offset;
		if (fread(&offset, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		sampleOffsets[i] = offset << 4;
	}

	// load pattern offsets
	for (int32_t i = 0; i < hdr.numPatterns; i++)
	{
		uint16_t offset;
		if (fread(&offset, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		patternOffsets[i] = offset << 4;
	}

	// *** PATTERNS ***

	k = 0;
	for (int32_t i = 0; i < hdr.numPatterns; i++)
	{
		if (patternOffsets[i]  == 0)
			continue; // empty pattern

		memset(alastnfo, 0, sizeof (alastnfo));
		memset(alastefx, 0, sizeof (alastefx));
		memset(alastvibnfo, 0, sizeof (alastvibnfo));
		memset(s3mLastGInstr, 0, sizeof (s3mLastGInstr));

		fseek(f, patternOffsets[i], SEEK_SET);
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
				uint8_t bits = pattBuff[k++];

				if (bits == 0)
				{
					kk++;
				}
				else
				{
					ii = bits & 31;

					memset(&tmpNote, 0, sizeof (tmpNote));

					// note and sample
					if (bits & 32)
					{
						tmpNote.note = pattBuff[k++];
						tmpNote.instr = pattBuff[k++];

						if (tmpNote.instr > MAX_INST)
							tmpNote.instr = 0;

						     if (tmpNote.note == 254) tmpNote.note = NOTE_OFF;
						else if (tmpNote.note == 255) tmpNote.note = 0;
						else
						{
							tmpNote.note = 1 + (tmpNote.note & 0xF) + (tmpNote.note >> 4) * 12;
							if (tmpNote.note > 96)
								tmpNote.note = 0;
						}
					}

					// volume column
					if (bits & 64)
					{
						tmpNote.vol = pattBuff[k++];

						if (tmpNote.vol <= 64)
							tmpNote.vol += 0x10;
						else
							tmpNote.vol = 0;
					}

					// effect
					if (bits & 128)
					{
						tmpNote.efx = pattBuff[k++];
						tmpNote.efxData = pattBuff[k++];

						if (tmpNote.efxData > 0)
						{
							alastnfo[ii] = tmpNote.efxData;
							if (tmpNote.efx == 8 || tmpNote.efx == 21)
								alastvibnfo[ii] = tmpNote.efxData; // H/U
						}

						// in ST3, a lot of effects directly share the same memory!
						if (tmpNote.efxData == 0 && tmpNote.efx != 7) // G
						{
							uint8_t efx = tmpNote.efx;
							if (efx == 8 || efx == 21) // H/U
								tmpNote.efxData = alastvibnfo[ii];
							else if ((efx >= 4 && efx <= 12) || (efx >= 17 && efx <= 19)) // D/E/F/I/J/K/L/Q/R/S
								tmpNote.efxData = alastnfo[ii];

							/* If effect data is zero and effect type was the same as last one, clear out
							** data if it's not J or S (those have no memory in the equivalent XM effects).
							** Also goes for extra fine pitch slides and fine volume slides,
							** since they get converted to other effects.
							*/
							if (efx == alastefx[ii] && tmpNote.efx != 10 && tmpNote.efx != 19) // J/S
							{
								uint8_t nfo = tmpNote.efxData;
								bool extraFinePitchSlides = (efx == 5 || efx == 6) && ((nfo & 0xF0) == 0xE0);
								bool fineVolSlides = (efx == 4 || efx == 11) &&
								     ((nfo > 0xF0) || (((nfo & 0xF) == 0xF) && ((nfo & 0xF0) > 0)));

								if (!extraFinePitchSlides && !fineVolSlides)
									tmpNote.efxData = 0;
							}
						}

						if (tmpNote.efx > 0)
							alastefx[ii] = tmpNote.efx;

						switch (tmpNote.efx)
						{
							case 1: // A
							{
								tmpNote.efx = 0xF;
								if (tmpNote.efxData == 0) // A00 does nothing in ST3
								{
									tmpNote.efx = tmpNote.efxData = 0;
								}
								else if (tmpNote.efxData > 0x1F)
								{
									tmpNote.efxData = 0x1F;
								}
							}
							break;

							case 2: tmpNote.efx = 0xB; break; // B
							case 3: tmpNote.efx = 0xD; break; // C
							case 4: // D
							{
								if (tmpNote.efxData > 0xF0) // fine slide up
								{
									tmpNote.efx = 0xE;
									tmpNote.efxData = 0xB0 | (tmpNote.efxData & 0xF);
								}
								else if ((tmpNote.efxData & 0x0F) == 0x0F && (tmpNote.efxData & 0xF0) > 0) // fine slide down
								{
									tmpNote.efx = 0xE;
									tmpNote.efxData = 0xA0 | (tmpNote.efxData >> 4);
								}
								else
								{
									tmpNote.efx = 0xA;
									if (tmpNote.efxData & 0x0F) // on D/K, last nybble has first priority in ST3
										tmpNote.efxData &= 0x0F;
								}
							}
							break;

							case 5: // E
							case 6: // F
							{
								if ((tmpNote.efxData & 0xF0) >= 0xE0)
								{
									// convert to fine slide
									if ((tmpNote.efxData & 0xF0) == 0xE0)
										tmp = 0x21;
									else
										tmp = 0xE;

									tmpNote.efxData &= 0x0F;

									if (tmpNote.efx == 0x05)
										tmpNote.efxData |= 0x20;
									else
										tmpNote.efxData |= 0x10;

									tmpNote.efx = (uint8_t)tmp;

									if (tmpNote.efx == 0x21 && tmpNote.efxData == 0)
									{
										tmpNote.efx = 0;
									}
								}
								else
								{
									// convert to normal 1xx/2xx slide
									tmpNote.efx = 7 - tmpNote.efx;
								}
							}
							break;

							case 7: // G
							{
								tmpNote.efx = 0x03;

								// fix illegal slides (to new instruments)
								if (tmpNote.instr != 0 && tmpNote.instr != s3mLastGInstr[ii])
									tmpNote.instr = s3mLastGInstr[ii];
							}
							break;

							case 11: // K
							{
								if (tmpNote.efxData > 0xF0) // fine slide up
								{
									tmpNote.efx = 0xE;
									tmpNote.efxData = 0xB0 | (tmpNote.efxData & 0xF);

									// if volume column is unoccupied, set to vibrato
									if (tmpNote.vol == 0)
										tmpNote.vol = 0xB0;
								}
								else if ((tmpNote.efxData & 0x0F) == 0x0F && (tmpNote.efxData & 0xF0) > 0) // fine slide down
								{
									tmpNote.efx = 0xE;
									tmpNote.efxData = 0xA0 | (tmpNote.efxData >> 4);

									// if volume column is unoccupied, set to vibrato
									if (tmpNote.vol == 0)
										tmpNote.vol = 0xB0;
								}
								else
								{
									tmpNote.efx = 0x6;
									if (tmpNote.efxData & 0x0F) // on D/K, last nybble has first priority in ST3
										tmpNote.efxData &= 0x0F;
								}
							}
							break;

							case 8:  tmpNote.efx = 0x04; break; // H
							case 9:  tmpNote.efx = 0x1D; break; // I
							case 10: tmpNote.efx = 0x00; break; // J
							case 12: tmpNote.efx = 0x05; break; // L
							case 15: tmpNote.efx = 0x09; break; // O
							case 17: tmpNote.efx = 0x1B; break; // Q
							case 18: tmpNote.efx = 0x07; break; // R

							case 19: // S
							{
								tmpNote.efx = 0xE;
								tmp = tmpNote.efxData >> 4;
								tmpNote.efxData &= 0x0F;

								     if (tmp == 0x1) tmpNote.efxData |= 0x30;
								else if (tmp == 0x2) tmpNote.efxData |= 0x50;
								else if (tmp == 0x3) tmpNote.efxData |= 0x40;
								else if (tmp == 0x4) tmpNote.efxData |= 0x70;
								else if (tmp == 0x8)
								{
									tmpNote.efx = 8;
									tmpNote.efxData = ((tmpNote.efxData & 0x0F) << 4) | (tmpNote.efxData & 0x0F);
								}
								else if (tmp == 0xB) tmpNote.efxData |= 0x60;
								else if (tmp == 0xC) // Note Cut
								{
									tmpNote.efxData |= 0xC0;
									if (tmpNote.efxData == 0xC0)
									{
										// EC0 does nothing in ST3 but cuts voice in FT2, remove effect
										tmpNote.efx = tmpNote.efxData = 0;
									}
								}
								else if (tmp == 0xD) // Note Delay
								{
									tmpNote.efxData |= 0xD0;
									if (tmpNote.note == 0 || tmpNote.note == NOTE_OFF)
									{
										// EDx without a note does nothing in ST3 but retrigs in FT2, remove effect
										tmpNote.efx = tmpNote.efxData = 0;
									}
									else if (tmpNote.efxData == 0xD0)
									{
										// ED0 prevents note/smp/vol from updating in ST3, remove everything
										tmpNote.note = 0;
										tmpNote.instr = 0;
										tmpNote.vol = 0;
										tmpNote.efx = 0;
										tmpNote.efxData = 0;
									}
								}
								else if (tmp == 0xE) tmpNote.efxData |= 0xE0;
								else if (tmp == 0xF) tmpNote.efxData |= 0xF0;
								else
								{
									tmpNote.efx = tmpNote.efxData = 0;
								}
							}
							break;

							case 20: // T
							{
								tmpNote.efx = 0x0F;
								if (tmpNote.efxData < 0x21) // Txx with a value lower than 33 (0x21) does nothing in ST3, remove effect
								{
									tmpNote.efx = 0;
									tmpNote.efxData = 0;
								}
							}
							break;

							case 22: // V
							{
								if (tmpNote.efxData > 0x40) // Vxx > 0x40 does nothing in ST3
									tmpNote.efx = tmpNote.efxData = 0;
								else
									tmpNote.efx = 0x10;
							}
							break;

							case 24: // X (set 7-bit panning + surround)
							{
								if (tmpNote.efxData > 0x80)
								{
									tmpNote.efx = tmpNote.efxData = 0;
								}
								else
								{
									tmpNote.efx = 8;

									int32_t pan = tmpNote.efxData * 2;
									if (pan > 255)
										pan = 255;

									tmpNote.efxData = (uint8_t)pan;
								}
							}
							break;

							default:
								tmpNote.efx = tmpNote.efxData = 0;
							break;
						}
					}

					if (tmpNote.instr != 0 && tmpNote.efx != 0x3)
						s3mLastGInstr[ii] = tmpNote.instr;

					patternTmp[i][(kk * MAX_CHANNELS) + ii] = tmpNote;
				}
			}

			if (tmpPatternEmpty((uint16_t)i))
			{
				if (patternTmp[i] != NULL)
				{
					free(patternTmp[i]);
					patternTmp[i] = NULL;
				}
			}
		}
	}

	// *** SAMPLES ***

	bool adlibInsWarn = false;
	for (int32_t i = 0; i < hdr.numSamples; i++)
	{
		if (sampleOffsets[i] == 0)
			continue;

		fseek(f, sampleOffsets[i], SEEK_SET);

		if (fread(&smpHdr, 1, sizeof (smpHdr), f) != sizeof (smpHdr))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		
		memcpy(songTmp.instrName[1+i], smpHdr.name, 22);

		if (smpHdr.type == 2)
		{
			adlibInsWarn = true;
		}
		else if (smpHdr.type == 1)
		{
			int32_t offsetInFile = ((smpHdr.offsetInFileH << 16) | smpHdr.offsetInFile) << 4;
			if ((smpHdr.flags & (255-1-2-4)) != 0 || smpHdr.packFlag != 0)
			{
				loaderMsgBox("Error loading .s3m: Incompatible module!");
				return false;
			}
			else if (offsetInFile > 0 && smpHdr.length > 0)
			{
				if (!allocateTmpInstr((int16_t)(1 + i)))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				setNoEnvelope(instrTmp[1 + i]);
				s = &instrTmp[1+i]->smp[0];

				if (smpHdr.midCFreq > 65535) // ST3 (and OpenMPT) does this
					smpHdr.midCFreq = 65535;

				memcpy(s->name, smpHdr.name, 22);

				// non-FT2: fixes "miracle man.s3m" and other broken S3Ms
				if (offsetInFile+smpHdr.length > (int32_t)filesize)
					smpHdr.length = filesize - offsetInFile;

				bool hasLoop = !!(smpHdr.flags & 1);
				bool stereoSample = !!(smpHdr.flags & 2);
				bool sample16Bit = !!(smpHdr.flags & 4);

				if (stereoSample)
					smpHdr.length <<= 1;

				int32_t lengthInFile = smpHdr.length;

				s->length = smpHdr.length;
				s->volume = smpHdr.volume;
				s->loopStart = smpHdr.loopStart;
				s->loopLength = smpHdr.loopEnd - smpHdr.loopStart;

				setSampleC4Hz(s, smpHdr.midCFreq);

				if (sample16Bit)
				{
					s->flags |= SAMPLE_16BIT;
					lengthInFile <<= 1;
				}

				if (!allocateSmpData(s, s->length, sample16Bit))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				if (s->loopLength <= 1 || s->loopStart+s->loopLength > s->length || s->loopLength == 0)
				{
					s->loopStart = 0;
					s->loopLength = 0;
					hasLoop = false;
				}

				if (hasLoop)
					s->flags |= LOOP_FWD;

				fseek(f, offsetInFile, SEEK_SET);

				if (hdr.version == 1)
				{
					fseek(f, lengthInFile, SEEK_CUR); // sample not supported
				}
				else
				{
					if (fread(s->dataPtr, SAMPLE_LENGTH_BYTES(s), 1, f) != 1)
					{
						loaderMsgBox("General I/O error during loading! Is the file in use?");
						return false;
					}

					if (sample16Bit)
						conv16BitSample(s->dataPtr, s->length, stereoSample);
					else
						conv8BitSample(s->dataPtr, s->length, stereoSample);

					// if stereo sample: reduce memory footprint after sample was downmixed to mono
					if (stereoSample)
					{
						s->length >>= 1;
						reallocateSmpData(s, s->length, sample16Bit);
					}
				}
			}
		}
	}

	songTmp.numChannels = countS3MChannels(hdr.numPatterns);

	if (adlibInsWarn)
		loaderMsgBox("Warning: The module contains unsupported AdLib instruments!");

	if (!(config.dontShowAgainFlags & DONT_SHOW_IMPORT_WARNING_FLAG))
		loaderSysReq(0, "System message", "Loading of this format is not fully supported and can have issues.", configToggleImportWarning);

	return true;
}

static int8_t countS3MChannels(uint16_t antPtn)
{
	int32_t channels = 0;
	for (int32_t i = 0; i < antPtn; i++)
	{
		if (patternTmp[i] == NULL)
			continue;

		note_t *p = patternTmp[i];
		for (int32_t j = 0; j < 64; j++)
		{
			for (int32_t k = 0; k < MAX_CHANNELS; k++, p++)
			{
				if (p->note == 0 && p->instr == 0 && p->vol == 0 && p->efx == 0 && p->efxData == 0)
					continue;

				if (k > channels)
					channels = k;
			}
		}
	}
	channels++;

	return (int8_t)channels;
}
