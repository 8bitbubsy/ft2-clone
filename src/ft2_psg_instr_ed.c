// PSG instrument editor for ft2-clone Atari mode
// See ft2_psg_instr_ed.h for documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_psg_instr_ed.h"
#include "ft2_atari_mode.h"
#include "ft2_atari_replayer.h"
#include "ft2_structs.h"
#include "ft2_gui.h"
#include "ft2_palette.h"
#include "ft2_header.h"

// Editor screen origin (positioned in the top area)
#define PSG_ED_X   4
#define PSG_ED_Y   4
#define PSG_ED_W   560
#define PSG_ED_H   173

// Cursor: which field is selected (0=volEnv, 1=arp, 2=pitch, 3=flags, 4=hwenv, 5=noise)
static int8_t  psgEdCursor = 0;  // field index
static int8_t  psgEdSubCursor = 0; // position within the field

// -----------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------

#define PSGI_MAGIC   "PSGI"
#define PSGI_VERSION 1

bool savePsgInstr(const char *path, const psgInstrument_t *ins)
{
	if (path == NULL || ins == NULL)
		return false;

	FILE *f = fopen(path, "wb");
	if (f == NULL)
		return false;

	// Magic + version
	fwrite(PSGI_MAGIC, 1, 4, f);
	const uint8_t version = PSGI_VERSION;
	fwrite(&version, 1, 1, f);

	// Name: 22 bytes, null-padded
	char nameBuf[22];
	memset(nameBuf, 0, sizeof (nameBuf));
	strncpy(nameBuf, ins->name, 22);
	fwrite(nameBuf, 1, 22, f);

	// Volume envelope (64 bytes)
	fwrite(ins->volEnvelope, 1, MAX_PSG_ENVELOPE_LEN, f);
	fwrite(&ins->volEnvLength,    1, 1, f);
	fwrite(&ins->volEnvLoopStart, 1, 1, f);

	// Arpeggio table (64 bytes, signed)
	fwrite(ins->arpTable, 1, MAX_PSG_ARPEGGIO_LEN, f);
	fwrite(&ins->arpLength,    1, 1, f);
	fwrite(&ins->arpLoopStart, 1, 1, f);

	// Pitch envelope (64 × int16, LE = 128 bytes)
	for (int i = 0; i < MAX_PSG_PITCH_LEN; i++)
	{
		uint8_t lo = (uint8_t)(ins->pitchEnvelope[i] & 0xFF);
		uint8_t hi = (uint8_t)((ins->pitchEnvelope[i] >> 8) & 0xFF);
		fwrite(&lo, 1, 1, f);
		fwrite(&hi, 1, 1, f);
	}
	fwrite(&ins->pitchEnvLength,    1, 1, f);
	fwrite(&ins->pitchEnvLoopStart, 1, 1, f);

	// Flags bitfield
	uint8_t flags = 0;
	if (ins->toneEnabled)    flags |= 0x01;
	if (ins->noiseEnabled)   flags |= 0x02;
	if (ins->useHwEnvelope)  flags |= 0x04;
	fwrite(&flags, 1, 1, f);

	// Hardware envelope
	fwrite(&ins->hwEnvShape, 1, 1, f);
	uint8_t hwlo = (uint8_t)(ins->hwEnvPeriod & 0xFF);
	uint8_t hwhi = (uint8_t)((ins->hwEnvPeriod >> 8) & 0xFF);
	fwrite(&hwlo, 1, 1, f);
	fwrite(&hwhi, 1, 1, f);

	// Noise period
	fwrite(&ins->noisePeriod, 1, 1, f);

	fclose(f);
	return true;
}

bool loadPsgInstr(const char *path, psgInstrument_t *ins)
{
	if (path == NULL || ins == NULL)
		return false;

	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return false;

	// Magic
	char magic[4];
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, PSGI_MAGIC, 4) != 0)
	{
		fclose(f);
		return false;
	}

	// Version
	uint8_t version;
	if (fread(&version, 1, 1, f) != 1 || version != PSGI_VERSION)
	{
		fclose(f);
		return false;
	}

	memset(ins, 0, sizeof (*ins));

	// Name
	char nameBuf[22];
	if (fread(nameBuf, 1, 22, f) != 22)
	{
		fclose(f);
		return false;
	}
	memcpy(ins->name, nameBuf, 22);
	ins->name[22] = '\0';

	// Volume envelope
	if (fread(ins->volEnvelope,    1, MAX_PSG_ENVELOPE_LEN, f) != MAX_PSG_ENVELOPE_LEN) goto loadErr;
	if (fread(&ins->volEnvLength,    1, 1, f) != 1) goto loadErr;
	if (fread(&ins->volEnvLoopStart, 1, 1, f) != 1) goto loadErr;

	// Arpeggio table
	if (fread(ins->arpTable,    1, MAX_PSG_ARPEGGIO_LEN, f) != MAX_PSG_ARPEGGIO_LEN) goto loadErr;
	if (fread(&ins->arpLength,    1, 1, f) != 1) goto loadErr;
	if (fread(&ins->arpLoopStart, 1, 1, f) != 1) goto loadErr;

	// Pitch envelope
	for (int i = 0; i < MAX_PSG_PITCH_LEN; i++)
	{
		uint8_t lo, hi;
		if (fread(&lo, 1, 1, f) != 1 || fread(&hi, 1, 1, f) != 1) goto loadErr;
		ins->pitchEnvelope[i] = (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
	}
	if (fread(&ins->pitchEnvLength,    1, 1, f) != 1) goto loadErr;
	if (fread(&ins->pitchEnvLoopStart, 1, 1, f) != 1) goto loadErr;

	// Flags
	uint8_t flags;
	if (fread(&flags, 1, 1, f) != 1) goto loadErr;
	ins->toneEnabled   = (flags & 0x01) != 0;
	ins->noiseEnabled  = (flags & 0x02) != 0;
	ins->useHwEnvelope = (flags & 0x04) != 0;

	// Hardware envelope
	if (fread(&ins->hwEnvShape, 1, 1, f) != 1) goto loadErr;
	{
		uint8_t lo, hi;
		if (fread(&lo, 1, 1, f) != 1 || fread(&hi, 1, 1, f) != 1) goto loadErr;
		ins->hwEnvPeriod = (uint16_t)lo | ((uint16_t)hi << 8);
	}

	// Noise period
	if (fread(&ins->noisePeriod, 1, 1, f) != 1) goto loadErr;

	fclose(f);
	return true;

loadErr:
	fclose(f);
	return false;
}

// -----------------------------------------------------------------------
// UI drawing helpers
// -----------------------------------------------------------------------

static void drawPsgLabel(uint16_t x, uint16_t y, const char *text)
{
	textOutShadow(x, y, PAL_FORGRND, PAL_DSKTOP2, text);
}

// Draw a row of 8 hex values (used for vol/arp envelopes)
static void drawByteRow(uint16_t x, uint16_t y, const uint8_t *data, uint8_t len,
                        uint8_t maxLen, int selectedIdx, bool isSigned)
{
	char buf[8];
	for (int i = 0; i < maxLen; i++)
	{
		uint8_t bgColor = (i == selectedIdx) ? PAL_BLCKMRK : PAL_DESKTOP;
		if (i < (int)len)
		{
			if (isSigned)
				snprintf(buf, sizeof (buf), "%4d", (int)(int8_t)data[i]);
			else
				snprintf(buf, sizeof (buf), "%3u", (unsigned)data[i]);
		}
		else
		{
			snprintf(buf, sizeof (buf), " -- ");
		}
		// Highlight selected
		if (i == selectedIdx)
			textOutShadow((uint16_t)(x + i * 28), y, PAL_BLCKTXT, PAL_BLCKMRK, buf);
		else
			textOut((uint16_t)(x + i * 28), y, PAL_FORGRND, buf);
		(void)bgColor;
	}
}

// -----------------------------------------------------------------------
// Public UI API
// -----------------------------------------------------------------------

psgInstrument_t *getCurPsgInstr(void)
{
	atariReplayer_t *ar = atariMode_getReplayer();
	if (ar == NULL)
		return NULL;

	int idx = (int)editor.curInstr - 1; // editor.curInstr is 1-based
	if (idx < 0 || idx >= MAX_PSG_INSTRUMENTS)
		return NULL;

	return atariReplayer_getInstrument(ar, idx);
}

void showPsgInstrEditor(void)
{
	ui.atariExportShown = true;
	drawPsgInstrEditor();
}

void hidePsgInstrEditor(void)
{
	ui.atariExportShown = false;
}

void drawPsgInstrEditor(void)
{
	if (!ui.atariExportShown)
		return;

	drawFramework(PSG_ED_X, PSG_ED_Y, PSG_ED_W, PSG_ED_H, FRAMEWORK_TYPE1);

	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 4, "PSG INSTRUMENT EDITOR");

	const psgInstrument_t *ins = getCurPsgInstr();
	if (ins == NULL)
	{
		drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 20, "(no instrument)");
		return;
	}

	// Name
	char nameLine[64];
	snprintf(nameLine, sizeof (nameLine), "Name: %.22s", ins->name);
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 18, nameLine);

	// Volume envelope
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 34, "VolEnv:");
	drawByteRow(PSG_ED_X + 60, PSG_ED_Y + 34, ins->volEnvelope,
	            ins->volEnvLength, 8,
	            (psgEdCursor == 0) ? psgEdSubCursor : -1, false);

	// Arpeggio table (signed)
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 50, "ArpTbl :");
	drawByteRow(PSG_ED_X + 60, PSG_ED_Y + 50, (const uint8_t *)ins->arpTable,
	            ins->arpLength, 8,
	            (psgEdCursor == 1) ? psgEdSubCursor : -1, true);

	// Mixer flags
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 66,
	             ins->toneEnabled  ? "[X] TONE" : "[ ] TONE");
	drawPsgLabel(PSG_ED_X + 68, PSG_ED_Y + 66,
	             ins->noiseEnabled ? "[X] NOISE" : "[ ] NOISE");

	// Hardware envelope
	char hwLine[64];
	snprintf(hwLine, sizeof (hwLine),
	         "HW ENV: %s  Shape:%u  Period:%u",
	         ins->useHwEnvelope ? "ON " : "OFF",
	         (unsigned)ins->hwEnvShape,
	         (unsigned)ins->hwEnvPeriod);
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 82, hwLine);

	// Noise period
	char noiseLine[32];
	snprintf(noiseLine, sizeof (noiseLine), "Noise period: %u", (unsigned)ins->noisePeriod);
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 98, noiseLine);

	// Loop points
	char loopLine[80];
	snprintf(loopLine, sizeof (loopLine),
	         "VolLoop:%02X  ArpLoop:%02X  PitchLoop:%02X",
	         (unsigned)ins->volEnvLoopStart,
	         (unsigned)ins->arpLoopStart,
	         (unsigned)ins->pitchEnvLoopStart);
	drawPsgLabel(PSG_ED_X + 4, PSG_ED_Y + 114, loopLine);
}

void updatePsgInstrEditor(void)
{
	if (ui.atariExportShown)
		drawPsgInstrEditor();
}

bool testPsgInstrEditorMouseDown(void)
{
	// Minimal implementation: no drag/click handling yet
	return false;
}

void psgInstrEditorKeyDown(SDL_Keycode key)
{
	psgInstrument_t *ins = getCurPsgInstr();
	if (ins == NULL)
		return;

	switch (key)
	{
		case SDLK_LEFT:
			if (psgEdSubCursor > 0)
				psgEdSubCursor--;
			break;
		case SDLK_RIGHT:
			if (psgEdSubCursor < 7)
				psgEdSubCursor++;
			break;
		case SDLK_UP:
			if (psgEdCursor > 0)
				psgEdCursor--;
			break;
		case SDLK_DOWN:
			if (psgEdCursor < 5)
				psgEdCursor++;
			break;
		case SDLK_t:
			// Toggle tone
			ins->toneEnabled = !ins->toneEnabled;
			break;
		case SDLK_n:
			// Toggle noise
			ins->noiseEnabled = !ins->noiseEnabled;
			break;
		case SDLK_h:
			// Toggle hardware envelope
			ins->useHwEnvelope = !ins->useHwEnvelope;
			break;
		default:
			break;
	}

	updatePsgInstrEditor();
}
