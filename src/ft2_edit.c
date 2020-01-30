// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_keyboard.h"
#include "ft2_audio.h"
#include "ft2_midi.h"
#include "ft2_pattern_ed.h"
#include "ft2_sysreqs.h"
#include "ft2_textboxes.h"

enum
{
	KEYTYPE_NUM = 0,
	KEYTYPE_ALPHA = 1
};

static double dVolScaleFK1 = 1.0, dVolScaleFK2 = 1.0;

// for block cut/copy/paste
static bool blockCopied;
static int16_t markXSize, markYSize;
static uint16_t ptnBufLen, trkBufLen;

// for transposing - these are set and tested accordingly
static int8_t lastTranspVal;
static uint8_t lastInsMode, lastTranspMode;
static uint32_t transpDelNotes; // count of under-/overflowing notes for warning message
static tonTyp clearNote;

static tonTyp blkCopyBuff[MAX_PATT_LEN * MAX_VOICES];
static tonTyp ptnCopyBuff[MAX_PATT_LEN * MAX_VOICES];
static tonTyp trackCopyBuff[MAX_PATT_LEN];

static const int8_t tickArr[16] = { 16, 8, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1 };

static const SDL_Keycode key2VolTab[] = 
{
	SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_MINUS, SDLK_PLUS, SDLK_d,
	SDLK_u, SDLK_s, SDLK_v, SDLK_p, SDLK_l, SDLK_r, SDLK_m
};
#define KEY2VOL_ENTRIES (signed)(sizeof (key2VolTab) / sizeof (SDL_Keycode))

static const SDL_Keycode key2EfxTab[] = 
{
	SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7,
	SDLK_8, SDLK_9, SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f,
	SDLK_g, SDLK_h, SDLK_i, SDLK_j, SDLK_k, SDLK_l, SDLK_m, SDLK_n,
	SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, SDLK_u, SDLK_v,
	SDLK_w, SDLK_x, SDLK_y, SDLK_z
};
#define KEY2EFX_ENTRIES (signed)(sizeof (key2EfxTab) / sizeof (SDL_Keycode))

static const SDL_Keycode key2HexTab[] = 
{
	SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7,
	SDLK_8, SDLK_9, SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f
};
#define KEY2HEX_ENTRIES (signed)(sizeof (key2HexTab) / sizeof (SDL_Keycode))

void recordNote(uint8_t note, int8_t vol);

// when the cursor is at the note slot
static bool testNoteKeys(SDL_Scancode scancode)
{
	int8_t noteNum = scancodeKeyToNote(scancode);
	if (noteNum > 0 && noteNum <= 96)
	{
		recordNote(noteNum, -1);
		return true; // note key pressed (and note triggered)
	}

	return false; // no note key pressed
}

// when the cursor is at the note slot
void testNoteKeysRelease(SDL_Scancode scancode)
{
	int8_t noteNum = scancodeKeyToNote(scancode); // convert key scancode to note number
	if (noteNum > 0 && noteNum <= 96)
		recordNote(noteNum, 0); // release note
}

static bool testEditKeys(SDL_Scancode scancode, SDL_Keycode keycode)
{
	int8_t i;
	uint8_t oldVal;
	uint16_t pattLen;
	tonTyp *ton;

	if (editor.cursor.object == CURSOR_NOTE)
	{
		// the edit cursor is at the note slot

		if (testNoteKeys(scancode))
		{
			keyb.keyRepeat = (playMode == PLAYMODE_EDIT); // repeat keys only if in edit mode
			return true; // we jammed an instrument
		}

		return false; // no note key pressed, test other keys
	}

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return false; // we're not editing, test other keys

	// convert key to slot data

	if (editor.cursor.object == CURSOR_VOL1)
	{
		// volume column effect type (mixed keys)

		for (i = 0; i < KEY2VOL_ENTRIES; i++)
		{
			if (keycode == key2VolTab[i])
				break;
		}

		if (i == KEY2VOL_ENTRIES)
			i = -1; // invalid key for slot
	}
	else if (editor.cursor.object == CURSOR_EFX0)
	{
		// effect type (mixed keys)

		for (i = 0; i < KEY2EFX_ENTRIES; i++)
		{
			if (keycode == key2EfxTab[i])
				break;
		}

		if (i == KEY2EFX_ENTRIES)
			i = -1; // invalid key for slot
	}
	else
	{
		// all other slots (hex keys)

		for (i = 0; i < KEY2HEX_ENTRIES; i++)
		{
			if (keycode == key2HexTab[i])
				break;
		}

		if (i == KEY2HEX_ENTRIES)
			i = -1; // invalid key for slot
	}

	if (i == -1 || !allocatePattern(editor.editPattern))
		return false; // no edit to be done

	// insert slot data

	ton = &patt[editor.editPattern][(editor.pattPos * MAX_VOICES) + editor.cursor.ch];
	switch (editor.cursor.object)
	{
		case CURSOR_INST1:
		{
			oldVal = ton->instr;

			ton->instr = (ton->instr & 0x0F) | (i << 4);
			if (ton->instr > MAX_INST)
				ton->instr = MAX_INST;

			if (ton->instr != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_INST2:
		{
			oldVal = ton->instr;
			ton->instr = (ton->instr & 0xF0) | i;

			if (ton->instr != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_VOL1:
		{
			oldVal = ton->vol;

			ton->vol = (ton->vol & 0x0F) | ((i + 1) << 4);
			if (ton->vol >= 0x51 && ton->vol <= 0x5F)
				ton->vol = 0x50;

			if (ton->vol != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_VOL2:
		{
			oldVal = ton->vol;

			if (ton->vol < 0x10)
				ton->vol = 0x10 + i;
			else
				ton->vol = (ton->vol & 0xF0) | i;

			if (ton->vol >= 0x51 && ton->vol <= 0x5F)
				ton->vol = 0x50;

			if (ton->vol != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX0:
		{
			oldVal = ton->effTyp;

			ton->effTyp = i;

			if (ton->effTyp != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX1:
		{
			oldVal = ton->eff;

			ton->eff = (ton->eff & 0x0F) | (i << 4);

			if (ton->eff != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX2:
		{
			oldVal = ton->eff;

			ton->eff = (ton->eff & 0xF0) | i;

			if (ton->eff != oldVal)
				setSongModifiedFlag();
		}
		break;

		default: break;
	}

	// increase row (only in edit mode)

	pattLen = pattLens[editor.editPattern];
	if (playMode == PLAYMODE_EDIT && pattLen >= 1)
		setPos(-1, (editor.pattPos + editor.ID_Add) % pattLen, true);

	if (i == 0) // if we inserted a zero, check if pattern is empty, for killing
		killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	return true;
}

// directly ported from the original FT2 code (fun fact: named EvulateTimeStamp() in the FT2 code)
static void evaluateTimeStamp(int16_t *songPos, int16_t *pattNr, int16_t *pattPos, int16_t *tick)
{
	int16_t nr, t, p, r, sp, row;
	uint16_t pattLen;

	sp = editor.songPos;
	nr = editor.editPattern;
	row = editor.pattPos;
	t = editor.tempo - editor.timer;

	t = CLAMP(t, 0, editor.tempo - 1);

	// this is needed, but also breaks quantization on speed>15
	if (t > 15)
		t = 15;

	pattLen = pattLens[nr];

	if (config.recQuant > 0)
	{
		if (config.recQuantRes >= 16)
		{
			t += (editor.tempo >> 1) + 1;
		}
		else
		{
			r = tickArr[config.recQuantRes-1];

			p = row & (r - 1);
			if (p < (r >> 1))
				row -= p;
			else
				row = (row + r) - p;

			t = 0;
		}
	}

	if (t > editor.tempo)
	{
		t -= editor.tempo;
		row++;
	}

	if (row >= pattLen)
	{
		if (playMode == PLAYMODE_RECSONG)
			sp++;

		row = 0;
		if (sp >= song.len)
			sp = song.repS;

		nr = song.songTab[sp];
	}

	*songPos = sp;
	*pattNr = nr;
	*pattPos = row;
	*tick = t;
}

// directly ported from the original FT2 code - what a mess, but it works...
void recordNote(uint8_t note, int8_t vol)
{
	int8_t i, k, c, editmode, recmode;
	int16_t nr, sp, oldpattpos, pattpos, tick;
	uint16_t pattLen;
	int32_t time;
	tonTyp *noteData;

	oldpattpos = editor.pattPos;

	if (songPlaying)
	{
		// row quantization
		evaluateTimeStamp(&sp, &nr, &pattpos, &tick);
	}
	else
	{
		sp = editor.songPos;
		nr = editor.editPattern;
		pattpos = editor.pattPos;
		tick = 0;
	}

	editmode = (playMode == PLAYMODE_EDIT);
	recmode = (playMode == PLAYMODE_RECSONG) || (playMode == PLAYMODE_RECPATT);

	if (note == 97)
		vol = 0;

	c = -1;
	k = -1;

	if (editmode || recmode)
	{
		// find out what channel is the most suitable in edit/record mode

		if ((config.multiEdit && editmode) || (config.multiRec && recmode))
		{
			time = 0x7FFFFFFF;
			for (i = 0; i < song.antChn; i++)
			{
				if (editor.chnMode[i] && config.multiRecChn[i] && editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0)
				{
					c = i;
					time = editor.keyOffTime[i];
				}
			}
		}
		else
		{
			c = editor.cursor.ch;
		}

		for (i = 0; i < song.antChn; i++)
		{
			if (note == editor.keyOnTab[i] && config.multiRecChn[i])
				k = i;
		}
	}
	else
	{
		// find out what channel is the most suitable in idle/play mode (jamming)
		if (config.multiKeyJazz)
		{
			time = 0x7FFFFFFF;
			c = 0;

			if (songPlaying)
			{
				for (i = 0; i < song.antChn; i++)
				{
					if (editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0 && config.multiRecChn[i])
					{
						c = i;
						time = editor.keyOffTime[i];
					}
				}
			}

			if (time == 0x7FFFFFFF)
			{
				for (i = 0; i < song.antChn; i++)
				{
					if (editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0)
					{
						c = i;
						time = editor.keyOffTime[i];
					}
				}
			}
		}
		else
		{
			c = editor.cursor.ch;
		}

		for (i = 0; i < song.antChn; i++)
		{
			if (note == editor.keyOnTab[i])
				k = i;
		}
	}

	if (vol != 0)
	{
		if (c < 0 || (k >= 0 && (config.multiEdit || (recmode || !editmode))))
			return;

		// play note

		editor.keyOnTab[c] = note;

		if (pattpos >= oldpattpos) // non-FT2 fix: only do this if we didn't quantize to next row
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, note, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, note, vol, 0, 0);
#endif
		}

		if (editmode || recmode)
		{
			if (allocatePattern(nr))
			{
				pattLen  = pattLens[nr];
				noteData = &patt[nr][(pattpos * MAX_VOICES) + c];

				// insert data
				noteData->ton = note;
				if (editor.curInstr > 0)
					noteData->instr = editor.curInstr;

				if (vol >= 0)
					noteData->vol = 0x10 + vol;

				if (!recmode)
				{
					// increase row (only in edit mode)
					if (pattLen >= 1)
						setPos(-1, (editor.pattPos + editor.ID_Add) % pattLen, true);
				}
				else
				{
					// apply tick delay for note if quantization is disabled
					if (!config.recQuant && tick > 0)
					{
						noteData->effTyp = 0x0E;
						noteData->eff = 0xD0 + (tick & 0x0F);
					}
				}

				editor.ui.updatePatternEditor = true;
				setSongModifiedFlag();
			}
		}
	}
	else
	{
		// note off

		if (k != -1)
			c = k;

		if (c < 0)
			return;

		editor.keyOnTab[c]   = 0;
		editor.keyOffTime[c] = ++editor.keyOffNr;

		if (pattpos >= oldpattpos) // non-FT2 fix: only do this if we didn't quantize to next row
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, 97, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, 97, vol, 0, 0);
#endif
		}

		if (config.recRelease && recmode)
		{
			if (allocatePattern(nr))
			{
				// insert data

				pattLen = pattLens[nr];
				noteData = &patt[nr][(pattpos * MAX_VOICES) + c];

				if (noteData->ton != 0)
					pattpos++;

				if (pattpos >= pattLen)
				{
					if (songPlaying)
						sp++;

					if (sp >= song.len)
						sp  = song.repS;

					nr = song.songTab[sp];
					pattpos = 0;
					pattLen = pattLens[nr];
				}

				noteData = &patt[nr][(pattpos * MAX_VOICES) + c];
				noteData->ton = 97;

				if (!recmode)
				{
					// increase row (only in edit mode)
					if (pattLen >= 1)
						setPos(-1, (editor.pattPos + editor.ID_Add) % pattLen, true);
				}
				else
				{
					// apply tick delay for note if quantization is disabled
					if (!config.recQuant && tick > 0)
					{
						noteData->effTyp = 0x0E;
						noteData->eff = 0xD0 + (tick & 0x0F);
					}
				}

				editor.ui.updatePatternEditor = true;
				setSongModifiedFlag();
			}
		}
	}
}

bool handleEditKeys(SDL_Keycode keycode, SDL_Scancode scancode)
{
	bool frKeybHack;
	uint16_t pattLen;
	tonTyp *note;

	// special case for delete - manipulate note data
	if (keycode == SDLK_DELETE)
	{
		if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
			return false; // we're not editing, test other keys

		if (patt[editor.editPattern] == NULL)
			return true;

		note = &patt[editor.editPattern][(editor.pattPos * MAX_VOICES) + editor.cursor.ch];

		if (keyb.leftShiftPressed)
		{
			// delete all
			memset(note, 0, sizeof (tonTyp));
		}
		else if (keyb.leftCtrlPressed)
		{
			// delete volume column + effect
			note->vol = 0;
			note->effTyp = 0;
			note->eff = 0;
		}
		else if (keyb.leftAltPressed)
		{
			// delete effect
			note->effTyp = 0;
			note->eff = 0;
		}
		else
		{
			if (editor.cursor.object == CURSOR_VOL1 || editor.cursor.object == CURSOR_VOL2)
			{
				// delete volume column
				note->vol = 0;
			}
			else
			{
				// delete note + instrument
				note->ton = 0;
				note->instr = 0;
			}
		}

		killPatternIfUnused(editor.editPattern);

		// increase row (only in edit mode)
		pattLen = pattLens[editor.editPattern];
		if (playMode == PLAYMODE_EDIT && pattLen >= 1)
			setPos(-1, (editor.pattPos + editor.ID_Add) % pattLen, true);

		editor.ui.updatePatternEditor = true;
		setSongModifiedFlag();

		return true;
	}

	// a kludge for french keyb. layouts to allow writing numbers in the pattern data with left SHIFT
	frKeybHack = keyb.leftShiftPressed && !keyb.leftAltPressed && !keyb.leftCtrlPressed &&
	               (scancode >= SDL_SCANCODE_1) && (scancode <= SDL_SCANCODE_0);

	if (frKeybHack || !keyb.keyModifierDown)
		return (testEditKeys(scancode, keycode));

	return false;
}

void writeToMacroSlot(uint8_t slot)
{
	uint16_t writeVol, writeEff;
	tonTyp *note;

	writeVol = 0;
	writeEff = 0;

	if (patt[editor.editPattern] != NULL)
	{
		note = &patt[editor.editPattern][(editor.pattPos * MAX_VOICES) + editor.cursor.ch];
		writeVol = note->vol;
		writeEff = (note->effTyp << 8) | note->eff;
	}

	if (editor.cursor.object == CURSOR_VOL1 || editor.cursor.object == CURSOR_VOL2)
		config.volMacro[slot] = writeVol;
	else
		config.comMacro[slot] = writeEff;
}

void writeFromMacroSlot(uint8_t slot)
{
	uint8_t effTyp;
	uint16_t pattLen;
	tonTyp *note;

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return;

	if (!allocatePattern(editor.editPattern))
		return;

	pattLen = pattLens[editor.editPattern];
	note = &patt[editor.editPattern][(editor.pattPos * MAX_VOICES) + editor.cursor.ch];

	if (editor.cursor.object == CURSOR_VOL1 || editor.cursor.object == CURSOR_VOL2)
	{
		note->vol = (uint8_t)config.volMacro[slot];
	}
	else
	{
		effTyp = (uint8_t)(config.comMacro[slot] >> 8);
		if (effTyp > 35)
		{
			// illegal effect
			note->effTyp = 0;
			note->eff = 0;
		}
		else
		{
			note->effTyp = effTyp;
			note->eff = config.comMacro[slot] & 0xFF;
		}
	}

	if (playMode == PLAYMODE_EDIT && pattLen >= 1)
		setPos(-1, (editor.pattPos + editor.ID_Add) % pattLen, true);

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternNote(void)
{
	int16_t nr, pattPos;
	uint16_t pattLen;
	tonTyp *pattPtr;

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	nr = editor.editPattern;

	pattPtr = patt[nr];
	if (pattPtr == NULL)
		return;

	pattPos = editor.pattPos;
	pattLen = pattLens[nr];

	if (pattLen > 1)
	{
		for (int32_t i = pattLen-2; i >= pattPos; i--)
			pattPtr[((i + 1) * MAX_VOICES) + editor.cursor.ch] = pattPtr[(i * MAX_VOICES) + editor.cursor.ch];
	}

	memset(&pattPtr[(pattPos * MAX_VOICES) + editor.cursor.ch], 0, sizeof (tonTyp));

	killPatternIfUnused(nr);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternLine(void)
{
	int16_t nr, pattLen, pattPos;
	tonTyp *pattPtr;

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	nr = editor.editPattern;

	setPatternLen(nr, pattLens[nr] + config.recTrueInsert); // config.recTrueInsert is 0 or 1

	pattPtr = patt[nr];
	if (pattPtr != NULL)
	{
		pattPos = editor.pattPos;
		pattLen = pattLens[nr];

		if (pattLen > 1)
		{
			for (int32_t i = pattLen-2; i >= pattPos; i--)
			{
				for (int32_t j = 0; j < MAX_VOICES; j++)
					pattPtr[((i + 1) * MAX_VOICES) + j] = pattPtr[(i * MAX_VOICES) + j];
			}
		}

		memset(&pattPtr[pattPos * MAX_VOICES], 0, TRACK_WIDTH);

		killPatternIfUnused(nr);
	}

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternNote(void)
{
	int16_t nr, pattPos;
	uint16_t pattLen;
	tonTyp *pattPtr;

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	nr = editor.editPattern;
	pattPos = editor.pattPos;
	pattLen = pattLens[nr];

	pattPtr = patt[editor.editPattern];
	if (pattPtr != NULL)
	{
		if (pattPos > 0)
		{
			pattPos--;
			editor.pattPos = song.pattPos = pattPos;

			for (int32_t i = pattPos; i < pattLen-1; i++)
				pattPtr[(i * MAX_VOICES) + editor.cursor.ch] = pattPtr[((i + 1) * MAX_VOICES) + editor.cursor.ch];

			memset(&pattPtr[((pattLen - 1) * MAX_VOICES) + editor.cursor.ch], 0, sizeof (tonTyp));
		}
	}
	else
	{
		if (pattPos > 0)
		{
			pattPos--;
			editor.pattPos = song.pattPos = pattPos;
		}
	}

	killPatternIfUnused(nr);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternLine(void)
{
	int16_t nr, pattPos;
	uint16_t pattLen;
	tonTyp *pattPtr;

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	nr = editor.editPattern;
	pattPos = editor.pattPos;
	pattLen = pattLens[nr];

	pattPtr = patt[editor.editPattern];
	if (pattPtr != NULL)
	{
		if (pattPos > 0)
		{
			pattPos--;
			editor.pattPos = song.pattPos = pattPos;

			for (int32_t i = pattPos; i < pattLen-1; i++)
			{
				for (int32_t j = 0; j < MAX_VOICES; j++)
					pattPtr[(i * MAX_VOICES) + j] = pattPtr[((i + 1) * MAX_VOICES) + j];
			}

			memset(&pattPtr[(pattLen - 1) * MAX_VOICES], 0, TRACK_WIDTH);
		}
	}
	else
	{
		if (pattPos > 0)
		{
			pattPos--;
			editor.pattPos = song.pattPos = pattPos;
		}
	}

	if (config.recTrueInsert && pattLen > 1)
		setPatternLen(nr, pattLen - 1);

	killPatternIfUnused(nr);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

// ----- TRANSPOSE FUNCTIONS -----

static void countOverflowingNotes(uint8_t currInsOnly, uint8_t transpMode, int8_t addVal)
{
	uint8_t ton;
	uint16_t p, pattLen, ch, row;
	tonTyp *pattPtr;

	transpDelNotes = 0;
	switch (transpMode)
	{
		case TRANSP_TRACK:
		{
			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattPtr += editor.cursor.ch;

			pattLen = pattLens[editor.editPattern];
			for (row = 0; row < pattLen; row++)
			{
				ton = pattPtr->ton;
				if ((ton >= 1 && ton <= 96) && (!currInsOnly || pattPtr->instr == editor.curInstr))
				{
					if ((int8_t)ton+addVal > 96 || (int8_t)ton+addVal <= 0)
						transpDelNotes++;
				}

				pattPtr += MAX_VOICES;
			}
		}
		break;

		case TRANSP_PATT:
		{
			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattLen = pattLens[editor.editPattern];
			for (row = 0; row < pattLen; row++)
			{
				for (ch = 0; ch < song.antChn; ch++)
				{
					ton = pattPtr->ton;
					if ((ton >= 1 && ton <= 96) && (!currInsOnly || pattPtr->instr == editor.curInstr))
					{
						if ((int8_t)ton+addVal > 96 || (int8_t)ton+addVal <= 0)
							transpDelNotes++;
					}

					pattPtr++;
				}

				pattPtr += MAX_VOICES - song.antChn;
			}
		}
		break;

		case TRANSP_SONG:
		{
			for (p = 0; p < MAX_PATTERNS; p++)
			{
				pattPtr = patt[p];
				if (pattPtr == NULL)
					continue; // empty pattern

				pattLen = pattLens[p];
				for (row = 0; row < pattLen; row++)
				{
					for (ch = 0; ch < song.antChn; ch++)
					{
						ton = pattPtr->ton;
						if ((ton >= 1 && ton <= 96) && (!currInsOnly || pattPtr->instr == editor.curInstr))
						{
							if ((int8_t)ton+addVal > 96 || (int8_t)ton+addVal <= 0)
								transpDelNotes++;
						}

						pattPtr++;
					}

					pattPtr += MAX_VOICES - song.antChn;
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (pattMark.markY1 == pattMark.markY2)
				return; // no pattern marking

			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattPtr += (pattMark.markY1 * MAX_VOICES) + pattMark.markX1;

			pattLen = pattLens[editor.editPattern];
			for (row = pattMark.markY1; row < pattMark.markY2; row++)
			{
				for (ch = pattMark.markX1; ch <= pattMark.markX2; ch++)
				{
					ton = pattPtr->ton;
					if ((ton >= 1 && ton <= 96) && (!currInsOnly || pattPtr->instr == editor.curInstr))
					{
						if ((int8_t)ton+addVal > 96 || (int8_t)ton+addVal <= 0)
							transpDelNotes++;
					}

					pattPtr++;
				}

				pattPtr += MAX_VOICES - ((pattMark.markX2 + 1) - pattMark.markX1);
			}
		}
		break;

		default: break;
	}
}

void doTranspose(void)
{
	char text[48];
	uint8_t ton;
	uint16_t p, pattLen, ch, row;
	tonTyp *pattPtr;

	countOverflowingNotes(lastInsMode, lastTranspMode, lastTranspVal);
	if (transpDelNotes > 0)
	{
		sprintf(text, "%d note(s) will be erased! Proceed?", transpDelNotes);
		if (okBox(2, "System request", text) != 1)
			return;
	}

	// lastTranspVal is never <-12 or >12, so unsigned testing for >96 is safe
	switch (lastTranspMode)
	{
		case TRANSP_TRACK:
		{
			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattPtr += editor.cursor.ch;

			pattLen = pattLens[editor.editPattern];
			for (row = 0; row < pattLen; row++)
			{
				ton = pattPtr->ton;
				if ((ton >= 1 && ton <= 96) && (!lastInsMode || pattPtr->instr == editor.curInstr))
				{
					ton += lastTranspVal;
					if (ton > 96)
						ton = 0; // also handles underflow

					pattPtr->ton = ton;
				}

				pattPtr += MAX_VOICES;
			}
		}
		break;

		case TRANSP_PATT:
		{
			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattLen = pattLens[editor.editPattern];
			for (row = 0; row < pattLen; row++)
			{
				for (ch = 0; ch < song.antChn; ch++)
				{
					ton = pattPtr->ton;
					if ((ton >= 1 && ton <= 96) && (!lastInsMode || pattPtr->instr == editor.curInstr))
					{
						ton += lastTranspVal;
						if (ton > 96)
							ton = 0; // also handles underflow

						pattPtr->ton = ton;
					}

					pattPtr++;
				}

				pattPtr += MAX_VOICES - song.antChn;
			}
		}
		break;

		case TRANSP_SONG:
		{
			for (p = 0; p < MAX_PATTERNS; p++)
			{
				pattPtr = patt[p];
				if (pattPtr == NULL)
					continue; // empty pattern

				pattLen  = pattLens[p];
				for (row = 0; row < pattLen; row++)
				{
					for (ch = 0; ch < song.antChn; ch++)
					{
						ton = pattPtr->ton;
						if ((ton >= 1 && ton <= 96) && (!lastInsMode || pattPtr->instr == editor.curInstr))
						{
							ton += lastTranspVal;
							if (ton > 96)
								ton = 0; // also handles underflow

							pattPtr->ton = ton;
						}

						pattPtr++;
					}

					pattPtr += MAX_VOICES - song.antChn;
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (pattMark.markY1 == pattMark.markY2)
				return; // no pattern marking

			pattPtr = patt[editor.editPattern];
			if (pattPtr == NULL)
				return; // empty pattern

			pattPtr += (pattMark.markY1 * MAX_VOICES) + pattMark.markX1;

			pattLen = pattLens[editor.editPattern];
			for (row = pattMark.markY1; row < pattMark.markY2; row++)
			{
				for (ch = pattMark.markX1; ch <= pattMark.markX2; ch++)
				{
					ton = pattPtr->ton;
					if ((ton >= 1 && ton <= 96) && (!lastInsMode || pattPtr->instr == editor.curInstr))
					{
						ton += lastTranspVal;
						if (ton > 96)
							ton = 0; // also handles underflow

						pattPtr->ton = ton;
					}

					pattPtr++;
				}

				pattPtr += MAX_VOICES - ((pattMark.markX2 + 1) - pattMark.markX1);
			}
		}
		break;

		default: break;
	}

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void trackTranspCurInsUp(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = 1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void trackTranspCurInsDn(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = -1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void trackTranspCurIns12Up(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = 12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void trackTranspCurIns12Dn(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = -12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void trackTranspAllInsUp(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = 1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void trackTranspAllInsDn(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = -1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void trackTranspAllIns12Up(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = 12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void trackTranspAllIns12Dn(void)
{
	lastTranspMode = TRANSP_TRACK;
	lastTranspVal = -12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void pattTranspCurInsUp(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = 1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void pattTranspCurInsDn(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = -1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void pattTranspCurIns12Up(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = 12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void pattTranspCurIns12Dn(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = -12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void pattTranspAllInsUp(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = 1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void pattTranspAllInsDn(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = -1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void pattTranspAllIns12Up(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = 12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void pattTranspAllIns12Dn(void)
{
	lastTranspMode = TRANSP_PATT;
	lastTranspVal = -12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void songTranspCurInsUp(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = 1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void songTranspCurInsDn(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = -1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void songTranspCurIns12Up(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = 12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void songTranspCurIns12Dn(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = -12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void songTranspAllInsUp(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = 1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void songTranspAllInsDn(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = -1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void songTranspAllIns12Up(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = 12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void songTranspAllIns12Dn(void)
{
	lastTranspMode = TRANSP_SONG;
	lastTranspVal = -12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void blockTranspCurInsUp(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = 1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void blockTranspCurInsDn(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = -1;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void blockTranspCurIns12Up(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = 12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void blockTranspCurIns12Dn(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = -12;
	lastInsMode = TRANSP_CUR_INST;
	doTranspose();
}

void blockTranspAllInsUp(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = 1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void blockTranspAllInsDn(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = -1;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void blockTranspAllIns12Up(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = 12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void blockTranspAllIns12Dn(void)
{
	lastTranspMode = TRANSP_BLOCK;
	lastTranspVal = -12;
	lastInsMode = TRANSP_ALL_INST;
	doTranspose();
}

void copyNote(tonTyp *src, tonTyp *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0]) dst->ton = src->ton;
		if (editor.copyMask[1]) dst->instr = src->instr;
		if (editor.copyMask[2]) dst->vol = src->vol;
		if (editor.copyMask[3]) dst->effTyp = src->effTyp;
		if (editor.copyMask[4]) dst->eff = src->eff;
	}
	else
	{
		*dst = *src;
	}
}

void pasteNote(tonTyp *src, tonTyp *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0] && (src->ton    != 0 || !editor.transpMask[0])) dst->ton = src->ton;
		if (editor.copyMask[1] && (src->instr  != 0 || !editor.transpMask[1])) dst->instr = src->instr;
		if (editor.copyMask[2] && (src->vol    != 0 || !editor.transpMask[2])) dst->vol = src->vol;
		if (editor.copyMask[3] && (src->effTyp != 0 || !editor.transpMask[3])) dst->effTyp = src->effTyp;
		if (editor.copyMask[4] && (src->eff    != 0 || !editor.transpMask[4])) dst->eff = src->eff;
	}
	else
	{
		*dst = *src;
	}
}

void cutTrack(void)
{
	uint16_t i, pattLen;
	tonTyp *pattPtr;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	if (config.ptnCutToBuffer)
	{
		memset(trackCopyBuff, 0, MAX_PATT_LEN * sizeof (tonTyp));
		for (i = 0; i < pattLen; i++)
			copyNote(&pattPtr[(i * MAX_VOICES) + editor.cursor.ch], &trackCopyBuff[i]);

		trkBufLen = pattLen;
	}

	pauseMusic();
	for (i = 0; i < pattLen; i++)
		pasteNote(&clearNote, &pattPtr[(i * MAX_VOICES) + editor.cursor.ch]);
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyTrack(void)
{
	uint16_t i, pattLen;
	tonTyp *pattPtr;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	memset(trackCopyBuff, 0, MAX_PATT_LEN * sizeof (tonTyp));
	for (i = 0; i < pattLen; i++)
		copyNote(&pattPtr[(i * MAX_VOICES) + editor.cursor.ch], &trackCopyBuff[i]);

	trkBufLen = pattLen;
}

void pasteTrack(void)
{
	uint16_t i, pattLen;
	tonTyp *pattPtr;

	if (trkBufLen == 0 || !allocatePattern(editor.editPattern))
		return;

	pattPtr = patt[editor.editPattern];
	pattLen = pattLens[editor.editPattern];

	pauseMusic();
	for (i = 0; i < pattLen; i++)
		pasteNote(&trackCopyBuff[i], &pattPtr[(i * MAX_VOICES) + editor.cursor.ch]);
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutPattern(void)
{
	uint16_t i, x, pattLen;
	tonTyp *pattPtr;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	if (config.ptnCutToBuffer)
	{
		memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_VOICES) * sizeof (tonTyp));
		for (x = 0; x < song.antChn; x++)
		{
			for (i = 0; i < pattLen; i++)
				copyNote(&pattPtr[(i * MAX_VOICES) + x], &ptnCopyBuff[(i * MAX_VOICES) + x]);
		}

		ptnBufLen = pattLen;
	}

	pauseMusic();
	for (x = 0; x < song.antChn; x++)
	{
		for (i = 0; i < pattLen; i++)
			pasteNote(&clearNote, &pattPtr[(i * MAX_VOICES) + x]);
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyPattern(void)
{
	uint16_t i, x, pattLen;
	tonTyp *pattPtr;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_VOICES) * sizeof (tonTyp));
	for (x = 0; x < song.antChn; x++)
	{
		for (i = 0; i < pattLen; i++)
			copyNote(&pattPtr[(i * MAX_VOICES) + x], &ptnCopyBuff[(i * MAX_VOICES) + x]);
	}

	ptnBufLen = pattLen;

	editor.ui.updatePatternEditor = true;
}

void pastePattern(void)
{
	uint16_t i, x, pattLen;
	tonTyp *pattPtr;
	
	if (ptnBufLen == 0)
		return;

	if (pattLens[editor.editPattern] != ptnBufLen)
	{
		if (okBox(1, "System request", "Change pattern length to copybuffer's length?") == 1)
			setPatternLen(editor.editPattern, ptnBufLen);
	}

	if (!allocatePattern(editor.editPattern))
		return;

	pattPtr = patt[editor.editPattern];
	pattLen = pattLens[editor.editPattern];

	pauseMusic();
	for (x = 0; x < song.antChn; x++)
	{
		for (i = 0; i < pattLen; i++)
			pasteNote(&ptnCopyBuff[(i * MAX_VOICES) + x], &pattPtr[(i * MAX_VOICES) + x]);
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutBlock(void)
{
	uint16_t x, y;
	tonTyp *pattPtr;

	if (pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	if (config.ptnCutToBuffer)
	{
		for (x = pattMark.markX1; x <= pattMark.markX2; x++)
		{
			for (y = pattMark.markY1; y < pattMark.markY2; y++)
			{
				assert(x < song.antChn && y < pattLens[editor.editPattern]);
				copyNote(&pattPtr[(y * MAX_VOICES) + x],
				         &blkCopyBuff[((y - pattMark.markY1) * MAX_VOICES) + (x - pattMark.markX1)]);
			}
		}
	}

	pauseMusic();
	for (x = pattMark.markX1; x <= pattMark.markX2; x++)
	{
		for (y = pattMark.markY1; y < pattMark.markY2; y++)
			pasteNote(&clearNote, &pattPtr[(y * MAX_VOICES) + x]);
	}
	resumeMusic();

	markXSize = pattMark.markX2 - pattMark.markX1;
	markYSize = pattMark.markY2 - pattMark.markY1;
	blockCopied = true;

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyBlock(void)
{
	uint16_t x, y;
	tonTyp *pattPtr;

	if (pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	pattPtr = patt[editor.editPattern];
	if (pattPtr == NULL)
		return;

	for (x = pattMark.markX1; x <= pattMark.markX2; x++)
	{
		for (y = pattMark.markY1; y < pattMark.markY2; y++)
		{
			assert(x < song.antChn && y < pattLens[editor.editPattern]);
			copyNote(&pattPtr[(y * MAX_VOICES) + x],
			         &blkCopyBuff[((y - pattMark.markY1) * MAX_VOICES) + (x - pattMark.markX1)]);
		}
	}

	markXSize = pattMark.markX2 - pattMark.markX1;
	markYSize = pattMark.markY2 - pattMark.markY1;
	blockCopied = true;
}

void pasteBlock(void)
{
	uint16_t xpos, ypos, j, k, pattLen;
	tonTyp *pattPtr;

	if (!blockCopied || !allocatePattern(editor.editPattern))
		return;

	pattLen = pattLens[editor.editPattern];

	xpos = editor.cursor.ch;
	ypos = editor.pattPos;

	j = markXSize;
	if (j+xpos >= song.antChn)
		j = song.antChn - xpos - 1;

	k = markYSize;
	if (k+ypos >= pattLen)
		k = pattLen - ypos;

	pattPtr = patt[editor.editPattern];

	pauseMusic();
	for (uint16_t x = xpos; x <= xpos+j; x++)
	{
		for (uint16_t y = ypos; y < ypos+k; y++)
		{
			assert(x < song.antChn && y < pattLen);
			pasteNote(&blkCopyBuff[((y - ypos) * MAX_VOICES) + (x - xpos)], &pattPtr[(y * MAX_VOICES) + x]);
		}
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

static void remapInstrXY(uint16_t nr, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t src, uint8_t dst)
{
	int32_t noteSkipLen;
	tonTyp *pattPtr, *note;

	// this routine is only used sanely, so no need to check input

	pattPtr = patt[nr];
	if (pattPtr == NULL)
		return;

	noteSkipLen = MAX_VOICES - ((x2 + 1) - x1);
	note = &pattPtr[(y1 * MAX_VOICES) + x1];

	for (uint16_t y = y1; y <= y2; y++)
	{
		for (uint16_t x = x1; x <= x2; x++)
		{
			assert(x < song.antChn && y < pattLens[nr]);
			if (note->instr == src)
				note->instr = dst;

			note++;
		}

		note += noteSkipLen;
	}
}

void remapBlock(void)
{
	if (editor.srcInstr == editor.curInstr || pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	pauseMusic();
	remapInstrXY(editor.editPattern,
	             pattMark.markX1, pattMark.markY1,
	             pattMark.markX2, pattMark.markY2 - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapTrack(void)
{
	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(editor.editPattern,
	             editor.cursor.ch, 0,
	             editor.cursor.ch, pattLens[editor.editPattern] - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapPattern(void)
{
	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(editor.editPattern,
	             0, 0,
	             song.antChn - 1, pattLens[editor.editPattern] - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapSong(void)
{
	uint8_t pattNr;

	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
	{
		pattNr = (uint8_t)i;

		remapInstrXY(pattNr,
		             0, 0,
		             song.antChn - 1, pattLens[pattNr] - 1,
		             editor.srcInstr, editor.curInstr);
	}
	resumeMusic();

	editor.ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

static int8_t getNoteVolume(tonTyp *note)
{
	int8_t nv, vv, ev, finalv;

	if (note->vol >= 0x10 && note->vol <= 0x50)
		vv = note->vol - 0x10;
	else
		vv = -1;

	if (note->effTyp == 0xC)
		ev = MIN(note->eff, 64);
	else
		ev = -1;

	if (note->instr != 0 && instr[note->instr] != NULL)
		nv = (int8_t)instr[note->instr]->samp[0].vol;
	else
		nv = -1;

	finalv = -1;
	if (nv >= 0) finalv = nv;
	if (vv >= 0) finalv = vv;
	if (ev >= 0) finalv = ev;

	return finalv;
}

static void setNoteVolume(tonTyp *note, int8_t newVol)
{
	int8_t oldv;

	if (newVol < 0)
		return;

	oldv = getNoteVolume(note);
	if (note->vol == oldv)
		return; // volume is the same

	if (note->effTyp == 0x0C)
		note->eff = newVol; // Cxx effect
	else
		note->vol = 0x10 + newVol; // volume column
}

static void scaleNote(uint16_t ptn, int8_t ch, int16_t row, double dScale)
{
	int32_t vol;
	uint16_t pattLen;
	tonTyp *note;

	if (patt[ptn] == NULL)
		return;

	pattLen = pattLens[ptn];
	if (row < 0 || row >= pattLen || ch < 0 || ch >= song.antChn)
		return;

	note = &patt[ptn][(row * MAX_VOICES) + ch];

	vol = getNoteVolume(note);
	if (vol >= 0)
	{
		vol = (int32_t)round(vol * dScale);
		vol = MIN(MAX(0, vol), 64);
		setNoteVolume(note, (int8_t)vol);
	}
}

static bool askForScaleFade(char *msg)
{
	char *val1, *val2, volstr[32 + 1];
	uint8_t err;

	sprintf(volstr, "%0.2f,%0.2f", dVolScaleFK1, dVolScaleFK2);
	if (inputBox(1, msg, volstr, sizeof (volstr) - 1) != 1)
		return false;

	err = false;

	val1 = volstr;
	if (strlen(val1) < 3)
		err = true;

	val2 = strchr(volstr, ',');
	if (val2 == NULL || strlen(val2) < 3)
		err = true;

	if (err)
	{
		okBox(0, "System message", "Invalid constant expressions.");
		return false;
	}

	dVolScaleFK1 = atof(val1);
	dVolScaleFK2 = atof(val2 + 1);

	return true;
}

void scaleFadeVolumeTrack(void)
{
	uint16_t pattLen;
	double dIPy, dVol;

	if (!askForScaleFade("Volume scale-fade track (start-, end scale)"))
		return;

	if (patt[editor.editPattern] == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	dIPy = 0.0;
	if (pattLen > 0)
		dIPy = (dVolScaleFK2 - dVolScaleFK1) / pattLen;

	dVol = dVolScaleFK1;

	pauseMusic();
	for (uint16_t row = 0; row < pattLen; row++)
	{
		scaleNote(editor.editPattern, editor.cursor.ch, row, dVol);
		dVol += dIPy;
	}
	resumeMusic();
}

void scaleFadeVolumePattern(void)
{
	uint16_t pattLen;
	double dIPy, dVol;

	if (!askForScaleFade("Volume scale-fade pattern (start-, end scale)"))
		return;

	if (patt[editor.editPattern] == NULL)
		return;

	pattLen = pattLens[editor.editPattern];

	dIPy = 0.0;
	if (pattLen > 0)
		dIPy = (dVolScaleFK2 - dVolScaleFK1) / pattLen;

	dVol = dVolScaleFK1;

	pauseMusic();
	for (uint16_t row = 0; row < pattLen; row++)
	{
		for (uint8_t ch = 0; ch < song.antChn; ch++)
			scaleNote(editor.editPattern, ch, row, dVol);

		dVol += dIPy;
	}
	resumeMusic();
}

void scaleFadeVolumeBlock(void)
{
	uint16_t dy;
	double dIPy, dVol;

	if (!askForScaleFade("Volume scale-fade block (start-, end scale)"))
		return;

	if (patt[editor.editPattern] == NULL || pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	dy = pattMark.markY2 - pattMark.markY1;

	dIPy = 0.0;
	if (dy > 0)
		dIPy = (dVolScaleFK2 - dVolScaleFK1) / dy;

	dVol = dVolScaleFK1;

	pauseMusic();
	for (uint16_t row = pattMark.markY1; row < pattMark.markY2; row++)
	{
		for (uint16_t ch = pattMark.markX1; ch <= pattMark.markX2; ch++)
			scaleNote(editor.editPattern, (uint8_t)ch, row, dVol);

		dVol += dIPy;
	}
	resumeMusic();
}

void toggleCopyMaskEnable(void) { editor.copyMaskEnable ^= 1; }
void toggleCopyMask0(void) { editor.copyMask[0] ^= 1; };
void toggleCopyMask1(void) { editor.copyMask[1] ^= 1; };
void toggleCopyMask2(void) { editor.copyMask[2] ^= 1; };
void toggleCopyMask3(void) { editor.copyMask[3] ^= 1; };
void toggleCopyMask4(void) { editor.copyMask[4] ^= 1; };
void togglePasteMask0(void) { editor.pasteMask[0] ^= 1; };
void togglePasteMask1(void) { editor.pasteMask[1] ^= 1; };
void togglePasteMask2(void) { editor.pasteMask[2] ^= 1; };
void togglePasteMask3(void) { editor.pasteMask[3] ^= 1; };
void togglePasteMask4(void) { editor.pasteMask[4] ^= 1; };
void toggleTranspMask0(void) { editor.transpMask[0] ^= 1; };
void toggleTranspMask1(void) { editor.transpMask[1] ^= 1; };
void toggleTranspMask2(void) { editor.transpMask[2] ^= 1; };
void toggleTranspMask3(void) { editor.transpMask[3] ^= 1; };
void toggleTranspMask4(void) { editor.transpMask[4] ^= 1; };
