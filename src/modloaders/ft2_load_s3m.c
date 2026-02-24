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
	uint8_t _magic_pinit, type;
	uint8_t _reserved1[2];
	int16_t numOrders, numSamples, numPatterns;
	uint16_t flags, junk3, ffi;
	char ID[4];
	uint8_t globalVol, speed, BPM, mastermul, ultraclick, defaultpan252, reserved[10], chnSettings[32];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
s3mHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

bool loadS3M(FILE *f, uint32_t filesize)
{
	uint8_t alastnfo[32], alastefx[32], alastvibnfo[32], alastGxxInstr[32];

	uint16_t tmpU16;
	int32_t patternOffsets[256], sampleOffsets[256];
	note_t tmpNote;
	sample_t *s;
	s3mHdr_t header;
	s3mSmpHdr_t smpHdr;

	tmpLinearPeriodsFlag = false; // use Amiga periods

	if (filesize < sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	memset(&header, 0, sizeof (header));
	if (fread(&header, 1, sizeof (header), f) != sizeof (header))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (header.numSamples > MAX_INST || header.numOrders > MAX_ORDERS || header.numPatterns > MAX_PATTERNS ||
		header.type != 16 || header.ffi < 1 || header.ffi > 2)
	{
		loaderMsgBox("Error loading .s3m: Incompatible module!");
		return false;
	}

	memset(songTmp.orders, 255, 256); // pad by 255
	if (fread(songTmp.orders, header.numOrders, 1, f) != 1)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	songTmp.songLength = header.numOrders;

	// remove pattern separators (254)
	int32_t removedOrders = 0;
	int32_t offset = 0;
	for (int32_t i = 0; i < songTmp.songLength; i++)
	{
		if (songTmp.orders[i] != 254)
			songTmp.orders[offset++] = songTmp.orders[i];
		else
			removedOrders++;
	}

	if (removedOrders <= songTmp.songLength)
		songTmp.songLength -= (uint16_t)removedOrders;
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

	memcpy(songTmp.name, header.name, 20);

	songTmp.BPM = header.BPM;
	songTmp.speed = header.speed;

	// load sample offsets
	for (int32_t i = 0; i < header.numSamples; i++)
	{
		if (fread(&tmpU16, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		sampleOffsets[i] = tmpU16 << 4;
	}

	// load pattern offsets
	for (int32_t i = 0; i < header.numPatterns; i++)
	{
		if (fread(&tmpU16, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		patternOffsets[i] = tmpU16 << 4;
	}

	// *** PATTERNS ***

	int32_t highestChannel = 0;
	for (int32_t i = 0; i < header.numPatterns; i++)
	{
		if (patternOffsets[i]  == 0)
			continue; // empty pattern

		memset(alastnfo, 0, sizeof (alastnfo));
		memset(alastefx, 0, sizeof (alastefx));
		memset(alastvibnfo, 0, sizeof (alastvibnfo));
		memset(alastGxxInstr, 0, sizeof (alastGxxInstr));

		fseek(f, patternOffsets[i], SEEK_SET);
		if (feof(f))
			continue;

		uint16_t packedPattLen;
		if (fread(&packedPattLen, 2, 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading! Is the file in use?");
			return false;
		}

		if (packedPattLen > 0 && packedPattLen <= 12288)
		{
			if (!allocateTmpPatt(i, 64))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}
			note_t *p = patternTmp[i];

			fread(tmpBuffer, packedPattLen, 1, f);

			uint16_t index = 0, chn = 0, row = 0;
			while (index < packedPattLen)
			{
				const uint8_t bits = tmpBuffer[index++];

				if (bits == 0)
				{
					if (++row >= 64)
						break;
				}
				else
				{
					chn = bits & 31;
					if (chn > highestChannel)
						highestChannel = chn;

					tmpNote.note = tmpNote.instr = tmpNote.vol = tmpNote.efx = tmpNote.efxData = 0;

					// note and sample
					if (bits & 32)
					{
						tmpNote.note = tmpBuffer[index++];
						tmpNote.instr = tmpBuffer[index++];

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
						tmpNote.vol = tmpBuffer[index++];

						if (tmpNote.vol <= 64)
							tmpNote.vol += 0x10;
						else
							tmpNote.vol = 0;
					}

					// effect
					if (bits & 128)
					{
						tmpNote.efx = tmpBuffer[index++];
						tmpNote.efxData = tmpBuffer[index++];

						if (tmpNote.efxData > 0)
						{
							alastnfo[chn] = tmpNote.efxData;
							if (tmpNote.efx == 8 || tmpNote.efx == 21)
								alastvibnfo[chn] = tmpNote.efxData; // H/U
						}

						// in ST3, a lot of effects directly share the same memory!
						if (tmpNote.efxData == 0 && tmpNote.efx != 7) // G
						{
							uint8_t efx = tmpNote.efx;
							if (efx == 8 || efx == 21) // H/U
								tmpNote.efxData = alastvibnfo[chn];
							else if ((efx >= 4 && efx <= 12) || (efx >= 17 && efx <= 19)) // D/E/F/I/J/K/L/Q/R/S
								tmpNote.efxData = alastnfo[chn];

							/* If effect data is zero and effect type was the same as last one, clear out
							** data if it's not J or S (those have no memory in the equivalent XM effects).
							** Also goes for fine/extra fine pitch slides and fine volume slides,
							** since they get converted to other effects.
							*/
							if (efx == alastefx[chn] && tmpNote.efx != 10 && tmpNote.efx != 19) // J/S
							{
								uint8_t nfo = tmpNote.efxData;
								bool finePitchSlides = (efx == 5 || efx == 6) && ((nfo & 0xF0) == 0xF0);
								bool extraFinePitchSlides = (efx == 5 || efx == 6) && ((nfo & 0xF0) == 0xE0);
								bool fineVolSlides = (efx == 4 || efx == 11) &&
								     ((nfo > 0xF0) || (((nfo & 0xF) == 0xF) && ((nfo & 0xF0) > 0)));

								if (!finePitchSlides && !extraFinePitchSlides && !fineVolSlides)
									tmpNote.efxData = 0;
							}
						}

						if (tmpNote.efx > 0)
							alastefx[chn] = tmpNote.efx;

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
									uint8_t tmp;

									// convert to fine slide
									if ((tmpNote.efxData & 0xF0) == 0xE0)
										tmp = 0x21;
									else
										tmp = 0xE;

									tmpNote.efxData &= 0x0F;

									if (tmpNote.efx == 5)
										tmpNote.efxData |= 0x20;
									else
										tmpNote.efxData |= 0x10;

									tmpNote.efx = (uint8_t)tmp;
									if (tmpNote.efx == 0x21 && tmpNote.efxData == 0)
										tmpNote.efx = 0;
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
								tmpNote.efx = 3;

								// fix illegal slides (to new instruments)
								if (tmpNote.instr != 0 && tmpNote.instr != alastGxxInstr[chn])
									tmpNote.instr = alastGxxInstr[chn];
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
									tmpNote.efx = 6;
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
								uint8_t tmp = tmpNote.efxData >> 4;
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
								tmpNote.efx = 0xF;
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

					if (tmpNote.instr != 0 && tmpNote.efx != 3)
						alastGxxInstr[chn] = tmpNote.instr;

					p[(row * MAX_CHANNELS) + chn] = tmpNote;
				}
			}
		}
	}

	songTmp.numChannels = highestChannel + 1;

	// samples

	bool adlibInsWarn = false;
	for (int32_t i = 0; i < header.numSamples; i++)
	{
		if (sampleOffsets[i] == 0)
			continue;

		fseek(f, sampleOffsets[i], SEEK_SET);

		if (fread(&smpHdr, 1, sizeof (smpHdr), f) != sizeof (smpHdr))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
		s3mSmpHdr_t *srcSmp = &smpHdr;

		memcpy(songTmp.instrName[1+i], srcSmp->name, 22);

		if (srcSmp->type == 2)
		{
			adlibInsWarn = true;
		}
		else if (srcSmp->type == 1)
		{
			uint32_t offsetInFile = ((srcSmp->offsetInFileH << 16) | srcSmp->offsetInFile) << 4;
			if (offsetInFile >= filesize)
				continue;

			if ((srcSmp->flags & (255-1-2-4)) != 0 || srcSmp->packFlag != 0)
			{
				loaderMsgBox("Error loading .s3m: Incompatible module!");
				return false;
			}
			else if (offsetInFile > 0 && srcSmp->length > 0)
			{
				if (!allocateTmpInstr((int16_t)(1 + i)))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}
				setNoEnvelope(instrTmp[1 + i]);
				s = &instrTmp[1+i]->smp[0];

				memcpy(s->name, srcSmp->name, 22);

				if (srcSmp->midCFreq > 65535) // ST3 (and OpenMPT) does this
					srcSmp->midCFreq = 65535;

				// non-FT2: fixes "miracle man.s3m" and other broken S3Ms
				if (offsetInFile+srcSmp->length > filesize)
					srcSmp->length = filesize - offsetInFile;

				bool hasLoop = !!(srcSmp->flags & 1);
				bool stereoSample = !!(srcSmp->flags & 2);
				bool sample16Bit = !!(srcSmp->flags & 4);

				if (stereoSample)
					srcSmp->length <<= 1;

				s->length = srcSmp->length;
				s->volume = srcSmp->volume;
				s->loopStart = srcSmp->loopStart;
				s->loopLength = srcSmp->loopEnd - srcSmp->loopStart;
				setSampleC4Hz(s, srcSmp->midCFreq);

				if (sample16Bit)
					s->flags |= SAMPLE_16BIT;

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
					s->flags |= LOOP_FORWARD;

				fseek(f, offsetInFile, SEEK_SET);

				if (sample16Bit)
					fread(s->dataPtr, 2, s->length, f);
				else
					fread(s->dataPtr, 1, s->length, f);

				if (header.ffi == 2) // unsigned samples, convert to signed
				{
					if (sample16Bit)
						conv16BitSample(s->dataPtr, s->length, stereoSample);
					else
						conv8BitSample(s->dataPtr, s->length, stereoSample);
				}

				// if stereo sample: reduce memory footprint after sample was downmixed to mono
				if (stereoSample)
				{
					s->length >>= 1;
					reallocateSmpData(s, s->length, sample16Bit);
				}
			}
		}
	}

	if (adlibInsWarn)
		loaderMsgBox("Warning: The module contains unsupported AdLib instruments!");

	if (!(config.dontShowAgainFlags & DONT_SHOW_IMPORT_WARNING_FLAG))
		loaderSysReq(0, "System message", "Loading of this format is not fully supported and can have issues.", configToggleImportWarning);

	return true;
}
