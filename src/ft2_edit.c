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
#include "ft2_tables.h"
#include "ft2_structs.h"

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
static note_t clearNote;

static note_t blkCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t ptnCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t trackCopyBuff[MAX_PATT_LEN];

static const int8_t tickArr[16] = { 16, 8, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1 };

void recordNote(uint8_t note, int8_t vol);

// when the cursor is at the note slot
static bool testNoteKeys(SDL_Scancode scancode)
{
	const int8_t noteNum = scancodeKeyToNote(scancode);
	if (noteNum == NOTE_OFF)
	{
		// inserts "note off" if editing song
		if (playMode == PLAYMODE_EDIT || playMode == PLAYMODE_RECPATT || playMode == PLAYMODE_RECSONG)
		{
			if (!allocatePattern(editor.editPattern))
				return true; // key pressed

			pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch].note = NOTE_OFF;

			const uint16_t numRows = patternNumRows[editor.editPattern];
			if (playMode == PLAYMODE_EDIT && numRows >= 1)
				setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);

			ui.updatePatternEditor = true;
			setSongModifiedFlag();
		}

		return true; // key pressed
	}

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
	const int8_t noteNum = scancodeKeyToNote(scancode); // convert key scancode to note number
	if (noteNum > 0 && noteNum <= 96)
		recordNote(noteNum, 0); // release note
}

static bool testEditKeys(SDL_Scancode scancode, SDL_Keycode keycode)
{
	int8_t i;

	if (cursor.object == CURSOR_NOTE)
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

	if (cursor.object == CURSOR_VOL1)
	{
		// volume column effect type (mixed keys)

		for (i = 0; i < KEY2VOL_ENTRIES; i++)
		{
			if (keycode == key2VolTab[i])
				break;
		}

		if (i == KEY2VOL_ENTRIES)
		{
			// volume column key not found, let's try a hack for '-' and '+' keys first
			if (scancode == SDL_SCANCODE_MINUS)
				i = 5;
			else if (scancode == SDL_SCANCODE_EQUALS)
				i = 6;
			else
				i = -1; // invalid key for slot
		}
	}
	else if (cursor.object == CURSOR_EFX0)
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

	note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
	switch (cursor.object)
	{
		case CURSOR_INST1:
		{
			uint8_t oldVal = p->instr;

			p->instr = (p->instr & 0x0F) | (i << 4);
			if (p->instr > MAX_INST)
				p->instr = MAX_INST;

			if (p->instr != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_INST2:
		{
			uint8_t oldVal = p->instr;
			p->instr = (p->instr & 0xF0) | i;

			if (p->instr != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_VOL1:
		{
			uint8_t oldVal = p->vol;

			p->vol = (p->vol & 0x0F) | ((i + 1) << 4);
			if (p->vol >= 0x51 && p->vol <= 0x5F)
				p->vol = 0x50;

			if (p->vol != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_VOL2:
		{
			uint8_t oldVal = p->vol;

			if (p->vol < 0x10)
				p->vol = 0x10 + i;
			else
				p->vol = (p->vol & 0xF0) | i;

			if (p->vol >= 0x51 && p->vol <= 0x5F)
				p->vol = 0x50;

			if (p->vol != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX0:
		{
			uint8_t oldVal = p->efx;

			p->efx = i;
			if (p->efx != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX1:
		{
			uint8_t oldVal = p->efxData;

			p->efxData = (p->efxData & 0x0F) | (i << 4);
			if (p->efxData != oldVal)
				setSongModifiedFlag();
		}
		break;

		case CURSOR_EFX2:
		{
			uint8_t oldVal = p->efxData;

			p->efxData = (p->efxData & 0xF0) | i;
			if (p->efxData != oldVal)
				setSongModifiedFlag();
		}
		break;

		default: break;
	}

	// increase row (only in edit mode)

	const int16_t numRows = patternNumRows[editor.editPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);

	if (i == 0) // if we inserted a zero, check if pattern is empty, for killing
		killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	return true;
}

static void evaluateTimeStamp(int16_t *songPos, int16_t *pattNum, int16_t *row, int16_t *tick)
{
	int16_t outSongPos = editor.songPos;
	int16_t outPattern = editor.editPattern;
	int16_t outRow = editor.row;
	int16_t outTick = editor.speed - editor.tick;

	outTick = CLAMP(outTick, 0, editor.speed - 1);

	// this is needed, but also breaks quantization on speed>15
	if (outTick > 15)
		outTick = 15;

	const int16_t numRows = patternNumRows[outPattern];

	if (config.recQuant > 0)
	{
		if (config.recQuantRes >= 16)
		{
			outTick += (editor.speed >> 1) + 1;
		}
		else
		{
			int16_t r = tickArr[config.recQuantRes-1];
			int16_t p = outRow & (r - 1);

			if (p < (r >> 1))
				outRow -= p;
			else
				outRow = (outRow + r) - p;

			outTick = 0;
		}
	}

	if (outTick > editor.speed)
	{
		outTick -= editor.speed;
		outRow++;
	}

	if (outRow >= numRows)
	{
		outRow = 0;

		if (playMode == PLAYMODE_RECSONG)
			outSongPos++;

		if (outSongPos >= song.songLength)
			outSongPos = song.songLoopStart;

		outPattern = song.orders[outSongPos];
	}

	*songPos = outSongPos;
	*pattNum = outPattern;
	*row = outRow;
	*tick = outTick;
}

void recordNote(uint8_t noteNum, int8_t vol) // directly ported from the original FT2 code - what a mess, but it works...
{
	int8_t i;
	int16_t pattNum, songPos, row, tick;
	int32_t time;
	note_t *p;

	const int16_t oldRow = editor.row;

	if (songPlaying)
	{
		// row quantization
		evaluateTimeStamp(&songPos, &pattNum, &row, &tick);
	}
	else
	{
		songPos = editor.songPos;
		pattNum = editor.editPattern;
		row = editor.row;
		tick = 0;
	}

	bool editmode = (playMode == PLAYMODE_EDIT);
	bool recmode = (playMode == PLAYMODE_RECSONG) || (playMode == PLAYMODE_RECPATT);

	if (noteNum == NOTE_OFF)
		vol = 0;

	int8_t c = -1;
	int8_t k = -1;

	if (editmode || recmode)
	{
		// find out what channel is the most suitable in edit/record mode

		if ((config.multiEdit && editmode) || (config.multiRec && recmode))
		{
			time = 0x7FFFFFFF;
			for (i = 0; i < song.numChannels; i++)
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
			c = cursor.ch;
		}

		for (i = 0; i < song.numChannels; i++)
		{
			if (noteNum == editor.keyOnTab[i] && config.multiRecChn[i])
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
				for (i = 0; i < song.numChannels; i++)
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
				for (i = 0; i < song.numChannels; i++)
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
			c = cursor.ch;
		}

		for (i = 0; i < song.numChannels; i++)
		{
			if (noteNum == editor.keyOnTab[i])
				k = i;
		}
	}

	if (vol != 0)
	{
		if (c < 0 || (k >= 0 && (config.multiEdit || (recmode || !editmode))))
			return;

		// play note

		editor.keyOnTab[c] = noteNum;

		if (row >= oldRow) // non-FT2 fix: only do this if we didn't quantize to next row
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, noteNum, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, noteNum, vol, 0, 0);
#endif
		}

		if (editmode || recmode)
		{
			if (allocatePattern(pattNum))
			{
				const int16_t numRows = patternNumRows[pattNum];
				p = &pattern[pattNum][(row * MAX_CHANNELS) + c];

				// insert data
				p->note = noteNum;
				if (editor.curInstr > 0)
					p->instr = editor.curInstr;

				if (vol >= 0)
					p->vol = 0x10 + vol;

				if (!recmode)
				{
					// increase row (only in edit mode)
					if (numRows >= 1)
						setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);
				}
				else
				{
					// apply tick delay for note if quantization is disabled
					if (!config.recQuant && tick > 0)
					{
						p->efx = 0x0E;
						p->efxData = 0xD0 + (tick & 0x0F);
					}
				}

				ui.updatePatternEditor = true;
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

		editor.keyOffNr++;

		editor.keyOnTab[c] = 0;
		editor.keyOffTime[c] = editor.keyOffNr;

		if (row >= oldRow) // non-FT2 fix: only do this if we didn't quantize to next row
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, NOTE_OFF, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, NOTE_OFF, vol, 0, 0);
#endif
		}

		if (config.recRelease && recmode)
		{
			if (allocatePattern(pattNum))
			{
				// insert data

				int16_t numRows = patternNumRows[pattNum];
				p = &pattern[pattNum][(row * MAX_CHANNELS) + c];

				if (p->note != 0)
					row++;

				if (row >= numRows)
				{
					row = 0;

					if (songPlaying)
					{
						songPos++;
						if (songPos >= song.songLength)
							songPos = song.songLoopStart;

						pattNum = song.orders[songPos];
						numRows = patternNumRows[pattNum];
					}
				}

				p = &pattern[pattNum][(row * MAX_CHANNELS) + c];
				p->note = NOTE_OFF;

				if (!recmode)
				{
					// increase row (only in edit mode)
					if (numRows >= 1)
						setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);
				}
				else
				{
					// apply tick delay for note if quantization is disabled
					if (!config.recQuant && tick > 0)
					{
						p->efx = 0x0E;
						p->efxData = 0xD0 + (tick & 0x0F);
					}
				}

				ui.updatePatternEditor = true;
				setSongModifiedFlag();
			}
		}
	}
}

bool handleEditKeys(SDL_Keycode keycode, SDL_Scancode scancode)
{
	// special case for delete - manipulate note data
	if (keycode == SDLK_DELETE)
	{
		if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
			return false; // we're not editing, test other keys

		if (pattern[editor.editPattern] == NULL)
			return true;

		note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];

		if (keyb.leftShiftPressed)
		{
			// delete all
			p->note = p->instr = p->vol = p->efx = p->efxData = 0;
		}
		else if (keyb.leftCtrlPressed)
		{
			// delete volume column + effect
			p->vol = 0;
			p->efx = 0;
			p->efxData = 0;
		}
		else if (keyb.leftAltPressed)
		{
			// delete effect
			p->efx = 0;
			p->efxData = 0;
		}
		else
		{
			if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
			{
				// delete volume column
				p->vol = 0;
			}
			else
			{
				// delete note + instrument
				p->note = 0;
				p->instr = 0;
			}
		}

		killPatternIfUnused(editor.editPattern);

		// increase row (only in edit mode)
		const int16_t numRows = patternNumRows[editor.editPattern];
		if (playMode == PLAYMODE_EDIT && numRows >= 1)
			setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);

		ui.updatePatternEditor = true;
		setSongModifiedFlag();

		return true;
	}

	// a kludge for french keyb. layouts to allow writing numbers in the pattern data with left SHIFT
	const bool frKeybHack = keyb.leftShiftPressed && !keyb.leftAltPressed && !keyb.leftCtrlPressed &&
	               (scancode >= SDL_SCANCODE_1) && (scancode <= SDL_SCANCODE_0);

	if (frKeybHack || !keyb.keyModifierDown)
		return (testEditKeys(scancode, keycode));

	return false;
}

void writeToMacroSlot(uint8_t slot)
{
	uint16_t writeVol = 0;
	uint16_t writeEfx = 0;

	if (pattern[editor.editPattern] != NULL)
	{
		note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
		writeVol = p->vol;
		writeEfx = (p->efx << 8) | p->efxData;
	}

	if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
		config.volMacro[slot] = writeVol;
	else
		config.comMacro[slot] = writeEfx;
}

void writeFromMacroSlot(uint8_t slot)
{
	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return;

	if (!allocatePattern(editor.editPattern))
		return;
	
	note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
	if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
	{
		p->vol = (uint8_t)config.volMacro[slot];
	}
	else
	{
		uint8_t efx = (uint8_t)(config.comMacro[slot] >> 8);
		if (efx > 35)
		{
			// illegal effect
			p->efx = 0;
			p->efxData = 0;
		}
		else
		{
			p->efx = efx;
			p->efxData = config.comMacro[slot] & 0xFF;
		}
	}

	const int16_t numRows = patternNumRows[editor.editPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setPos(-1, (editor.row + editor.editRowSkip) % numRows, true);

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternNote(void)
{
	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	const int16_t row = editor.row;
	const int16_t numRows = patternNumRows[editor.editPattern];

	if (numRows > 1)
	{
		for (int32_t i = numRows-2; i >= row; i--)
			p[((i+1) * MAX_CHANNELS) + cursor.ch] = p[(i * MAX_CHANNELS) + cursor.ch];
	}

	memset(&p[(row * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternLine(void)
{
	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	setPatternLen(editor.editPattern, patternNumRows[editor.editPattern] + config.recTrueInsert); // config.recTrueInsert is 0 or 1

	note_t *p = pattern[editor.editPattern];
	if (p != NULL)
	{
		const int16_t row = editor.row;
		const int16_t numRows = patternNumRows[editor.editPattern];

		if (numRows > 1)
		{
			for (int32_t i = numRows-2; i >= row; i--)
			{
				for (int32_t j = 0; j < MAX_CHANNELS; j++)
					p[((i+1) * MAX_CHANNELS) + j] = p[(i * MAX_CHANNELS) + j];
			}
		}

		memset(&p[row * MAX_CHANNELS], 0, TRACK_WIDTH);

		killPatternIfUnused(editor.editPattern);
	}

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternNote(void)
{
	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	int16_t row = editor.row;
	const int16_t numRows = patternNumRows[editor.editPattern];

	note_t *p = pattern[editor.editPattern];
	if (p != NULL)
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;

			for (int32_t i = row; i < numRows-1; i++)
				p[(i * MAX_CHANNELS) + cursor.ch] = p[((i+1) * MAX_CHANNELS) + cursor.ch];

			memset(&p[((numRows-1) * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));
		}
	}
	else
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;
		}
	}

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternLine(void)
{
	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	int16_t row = editor.row;
	const int16_t numRows = patternNumRows[editor.editPattern];

	note_t *p = pattern[editor.editPattern];
	if (p != NULL)
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;

			for (int32_t i = row; i < numRows-1; i++)
			{
				for (int32_t j = 0; j < MAX_CHANNELS; j++)
					p[(i * MAX_CHANNELS) + j] = p[((i+1) * MAX_CHANNELS) + j];
			}

			memset(&p[(numRows-1) * MAX_CHANNELS], 0, TRACK_WIDTH);
		}
	}
	else
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;
		}
	}

	if (config.recTrueInsert && numRows > 1)
		setPatternLen(editor.editPattern, numRows-1);

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

// ----- TRANSPOSE FUNCTIONS -----

static void countOverflowingNotes(uint8_t currInsOnly, uint8_t transpMode, int8_t addVal)
{
	transpDelNotes = 0;
	switch (transpMode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			p += cursor.ch;

			const int32_t numRows = patternNumRows[editor.editPattern];
			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				if ((p->note >= 1 && p->note <= 96) && (!currInsOnly || p->instr == editor.curInstr))
				{
					if ((int8_t)p->note+addVal > 96 || (int8_t)p->note+addVal <= 0)
						transpDelNotes++;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			const int32_t numRows = patternNumRows[editor.editPattern];
			const int32_t pitch = MAX_CHANNELS-song.numChannels;

			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (!currInsOnly || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addVal > 96 || (int8_t)p->note+addVal <= 0)
							transpDelNotes++;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS-song.numChannels;
			for (int32_t i = 0; i < MAX_PATTERNS; i++)
			{
				note_t *p = pattern[i];
				if (p == NULL)
					continue; // empty pattern

				const int32_t numRows = patternNumRows[i];
				for (int32_t row = 0; row < numRows; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						if ((p->note >= 1 && p->note <= 96) && (!currInsOnly || p->instr == editor.curInstr))
						{
							if ((int8_t)p->note+addVal > 96 || (int8_t)p->note+addVal <= 0)
								transpDelNotes++;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (pattMark.markY1 == pattMark.markY2)
				return; // no pattern marking

			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			p += (pattMark.markY1 * MAX_CHANNELS) + pattMark.markX1;

			const int32_t pitch = MAX_CHANNELS - ((pattMark.markX2 + 1) - pattMark.markX1);
			for (int32_t row = pattMark.markY1; row < pattMark.markY2; row++, p += pitch)
			{
				for (int32_t ch = pattMark.markX1; ch <= pattMark.markX2; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (!currInsOnly || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addVal > 96 || (int8_t)p->note+addVal <= 0)
							transpDelNotes++;
					}
				}
			}
		}
		break;

		default: break;
	}
}

void doTranspose(void)
{
	char text[48];

	countOverflowingNotes(lastInsMode, lastTranspMode, lastTranspVal);
	if (transpDelNotes > 0)
	{
		sprintf(text, "%d note(s) will be erased! Proceed?", (int32_t)transpDelNotes);
		if (okBox(2, "System request", text) != 1)
			return;
	}

	// lastTranspVal is never <-12 or >12, so unsigned testing for >96 is safe
	switch (lastTranspMode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			p += cursor.ch;

			const int32_t numRows = patternNumRows[editor.editPattern];
			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				uint8_t note = p->note;
				if ((note >= 1 && note <= 96) && (!lastInsMode || p->instr == editor.curInstr))
				{
					note += lastTranspVal;
					if (note > 96)
						note = 0; // also handles underflow

					p->note = note;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			const int32_t numRows = patternNumRows[editor.editPattern];
			const int32_t pitch = MAX_CHANNELS - song.numChannels;

			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (!lastInsMode || p->instr == editor.curInstr))
					{
						note += lastTranspVal;
						if (note > 96)
							note = 0; // also handles underflow

						p->note = note;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS - song.numChannels;
			for (int32_t i = 0; i < MAX_PATTERNS; i++)
			{
				note_t *p = pattern[i];
				if (p == NULL)
					continue; // empty pattern

				const int32_t numRows = patternNumRows[i];
				for (int32_t row = 0; row < numRows; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						uint8_t note = p->note;
						if ((note >= 1 && note <= 96) && (!lastInsMode || p->instr == editor.curInstr))
						{
							note += lastTranspVal;
							if (note > 96)
								note = 0; // also handles underflow

							p->note = note;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (pattMark.markY1 == pattMark.markY2)
				return; // no pattern marking

			note_t *p = pattern[editor.editPattern];
			if (p == NULL)
				return; // empty pattern

			p += (pattMark.markY1 * MAX_CHANNELS) + pattMark.markX1;

			const int32_t pitch = MAX_CHANNELS - ((pattMark.markX2 + 1) - pattMark.markX1);
			for (int32_t row = pattMark.markY1; row < pattMark.markY2; row++, p += pitch)
			{
				for (int32_t ch = pattMark.markX1; ch <= pattMark.markX2; ch++, p++)
				{
					uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (!lastInsMode || p->instr == editor.curInstr))
					{
						note += lastTranspVal;
						if (note > 96)
							note = 0; // also handles underflow

						p->note = note;
					}
				}
			}
		}
		break;

		default: break;
	}

	ui.updatePatternEditor = true;
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

void copyNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0]) dst->note = src->note;
		if (editor.copyMask[1]) dst->instr = src->instr;
		if (editor.copyMask[2]) dst->vol = src->vol;
		if (editor.copyMask[3]) dst->efx = src->efx;
		if (editor.copyMask[4]) dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

void pasteNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0] && (src->note    != 0 || !editor.transpMask[0])) dst->note = src->note;
		if (editor.copyMask[1] && (src->instr   != 0 || !editor.transpMask[1])) dst->instr = src->instr;
		if (editor.copyMask[2] && (src->vol     != 0 || !editor.transpMask[2])) dst->vol = src->vol;
		if (editor.copyMask[3] && (src->efx     != 0 || !editor.transpMask[3])) dst->efx = src->efx;
		if (editor.copyMask[4] && (src->efxData != 0 || !editor.transpMask[4])) dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

void cutTrack(void)
{
	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[editor.editPattern];

	if (config.ptnCutToBuffer)
	{
		memset(trackCopyBuff, 0, MAX_PATT_LEN * sizeof (note_t));
		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

		trkBufLen = numRows;
	}

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		pasteNote(&clearNote, &p[(i * MAX_CHANNELS) + cursor.ch]);
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyTrack(void)
{
	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[editor.editPattern];

	memset(trackCopyBuff, 0, MAX_PATT_LEN * sizeof (note_t));
	for (int16_t i = 0; i < numRows; i++)
		copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

	trkBufLen = numRows;
}

void pasteTrack(void)
{
	if (trkBufLen == 0 || !allocatePattern(editor.editPattern))
		return;

	note_t *p = pattern[editor.editPattern];
	const int16_t numRows = patternNumRows[editor.editPattern];

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		pasteNote(&trackCopyBuff[i], &p[(i * MAX_CHANNELS) + cursor.ch]);
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutPattern(void)
{
	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[editor.editPattern];

	if (config.ptnCutToBuffer)
	{
		memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_CHANNELS) * sizeof (note_t));
		for (int16_t x = 0; x < song.numChannels; x++)
		{
			for (int16_t i = 0; i < numRows; i++)
				copyNote(&p[(i * MAX_CHANNELS) + x], &ptnCopyBuff[(i * MAX_CHANNELS) + x]);
		}

		ptnBufLen = numRows;
	}

	pauseMusic();
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			pasteNote(&clearNote, &p[(i * MAX_CHANNELS) + x]);
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyPattern(void)
{
	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[editor.editPattern];

	memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_CHANNELS) * sizeof (note_t));
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + x], &ptnCopyBuff[(i * MAX_CHANNELS) + x]);
	}

	ptnBufLen = numRows;

	ui.updatePatternEditor = true;
}

void pastePattern(void)
{
	if (ptnBufLen == 0)
		return;

	if (patternNumRows[editor.editPattern] != ptnBufLen)
	{
		if (okBox(1, "System request", "Change pattern length to copybuffer's length?") == 1)
			setPatternLen(editor.editPattern, ptnBufLen);
	}

	if (!allocatePattern(editor.editPattern))
		return;

	note_t *p = pattern[editor.editPattern];
	const int16_t numRows = patternNumRows[editor.editPattern];

	pauseMusic();
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			pasteNote(&ptnCopyBuff[(i * MAX_CHANNELS) + x], &p[(i * MAX_CHANNELS) + x]);
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutBlock(void)
{
	if (pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	if (config.ptnCutToBuffer)
	{
		for (int16_t x = pattMark.markX1; x <= pattMark.markX2; x++)
		{
			for (int16_t y = pattMark.markY1; y < pattMark.markY2; y++)
			{
				assert(x < song.numChannels && y < patternNumRows[editor.editPattern]);
				copyNote(&p[(y * MAX_CHANNELS) + x], &blkCopyBuff[((y - pattMark.markY1) * MAX_CHANNELS) + (x - pattMark.markX1)]);
			}
		}
	}

	pauseMusic();
	for (int16_t x = pattMark.markX1; x <= pattMark.markX2; x++)
	{
		for (int16_t y = pattMark.markY1; y < pattMark.markY2; y++)
			pasteNote(&clearNote, &p[(y * MAX_CHANNELS) + x]);
	}
	resumeMusic();

	markXSize = pattMark.markX2 - pattMark.markX1;
	markYSize = pattMark.markY2 - pattMark.markY1;
	blockCopied = true;

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyBlock(void)
{
	if (pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return;

	for (int16_t x = pattMark.markX1; x <= pattMark.markX2; x++)
	{
		for (int16_t y = pattMark.markY1; y < pattMark.markY2; y++)
		{
			assert(x < song.numChannels && y < patternNumRows[editor.editPattern]);
			copyNote(&p[(y * MAX_CHANNELS) + x], &blkCopyBuff[((y - pattMark.markY1) * MAX_CHANNELS) + (x - pattMark.markX1)]);
		}
	}

	markXSize = pattMark.markX2 - pattMark.markX1;
	markYSize = pattMark.markY2 - pattMark.markY1;
	blockCopied = true;
}

void pasteBlock(void)
{
	if (!blockCopied || !allocatePattern(editor.editPattern))
		return;

	const int16_t numRows = patternNumRows[editor.editPattern];

	const int32_t xpos = cursor.ch;
	const int32_t ypos = editor.row;

	int32_t j = markXSize;
	if (j+xpos >= song.numChannels)
		j = song.numChannels - xpos - 1;

	int32_t k = markYSize;
	if (k+ypos >= numRows)
		k = numRows-ypos;

	note_t *p = pattern[editor.editPattern];

	pauseMusic();
	for (int32_t x = xpos; x <= xpos+j; x++)
	{
		for (int32_t y = ypos; y < ypos+k; y++)
		{
			assert(x < song.numChannels && y < numRows);
			pasteNote(&blkCopyBuff[((y - ypos) * MAX_CHANNELS) + (x - xpos)], &p[(y * MAX_CHANNELS) + x]);
		}
	}
	resumeMusic();

	killPatternIfUnused(editor.editPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

static void remapInstrXY(uint16_t pattNum, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t src, uint8_t dst)
{
	// this routine is only used sanely, so no need to check input

	note_t *pattPtr = pattern[pattNum];
	if (pattPtr == NULL)
		return;

	note_t *p = &pattPtr[(y1 * MAX_CHANNELS) + x1];

	const int32_t pitch = MAX_CHANNELS - ((x2 + 1) - x1);
	for (uint16_t y = y1; y <= y2; y++, p += pitch)
	{
		for (uint16_t x = x1; x <= x2; x++, p++)
		{
			if (p->instr == src)
				p->instr = dst;
		}
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

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapTrack(void)
{
	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(editor.editPattern,
	             cursor.ch, 0,
	             cursor.ch, patternNumRows[editor.editPattern] - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapPattern(void)
{
	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(editor.editPattern,
	             0, 0,
	             (uint16_t)(song.numChannels - 1), patternNumRows[editor.editPattern] - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapSong(void)
{
	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		const uint8_t pattNum = (uint8_t)i;

		remapInstrXY(pattNum,
		             0, 0,
		             (uint16_t)(song.numChannels - 1), patternNumRows[pattNum] - 1,
		             editor.srcInstr, editor.curInstr);
	}
	resumeMusic();

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

// "scale-fade volume" routines

static int8_t getNoteVolume(note_t *p)
{
	int8_t nv, vv, ev;

	if (p->vol >= 0x10 && p->vol <= 0x50)
		vv = p->vol - 0x10;
	else
		vv = -1;

	if (p->efx == 0xC)
		ev = MIN(p->efxData, 64);
	else
		ev = -1;

	if (p->instr != 0 && instr[p->instr] != NULL)
		nv = (int8_t)instr[p->instr]->smp[0].volume;
	else
		nv = -1;

	int8_t finalv = -1;
	if (nv >= 0) finalv = nv;
	if (vv >= 0) finalv = vv;
	if (ev >= 0) finalv = ev;

	return finalv;
}

static void setNoteVolume(note_t *p, int8_t newVol)
{
	if (newVol < 0)
		return;

	const int8_t oldv = getNoteVolume(p);
	if (p->vol == oldv)
		return; // volume is the same

	if (p->efx == 0x0C)
		p->efxData = newVol; // Cxx effect
	else
		p->vol = 0x10 + newVol; // volume column
}

static void scaleNote(uint16_t pattNum, int8_t ch, int16_t row, double dScale)
{
	if (pattern[pattNum] == NULL)
		return;

	const int16_t numRows = patternNumRows[pattNum];
	if (row < 0 || row >= numRows || ch < 0 || ch >= song.numChannels)
		return;

	note_t *p = &pattern[pattNum][(row * MAX_CHANNELS) + ch];

	int32_t vol = getNoteVolume(p);
	if (vol >= 0)
	{
		vol = (int32_t)((vol * dScale) + 0.5); // rounded
		vol = MIN(MAX(0, vol), 64);
		setNoteVolume(p, (int8_t)vol);
	}
}

static bool askForScaleFade(char *msg)
{
	char volstr[32+1];

	sprintf(volstr, "%0.2f,%0.2f", dVolScaleFK1, dVolScaleFK2);
	if (inputBox(1, msg, volstr, sizeof (volstr) - 1) != 1)
		return false;

	bool err = false;

	char *val1 = volstr;
	if (strlen(val1) < 3)
		err = true;

	char *val2 = strchr(volstr, ',');
	if (val2 == NULL || strlen(val2) < 3)
		err = true;

	if (err)
	{
		okBox(0, "System message", "Invalid constant expressions.");
		return false;
	}

	dVolScaleFK1 = atof(val1+0);
	dVolScaleFK2 = atof(val2+1);

	return true;
}

void scaleFadeVolumeTrack(void)
{
	if (!askForScaleFade("Volume scale-fade track (start-, end scale)"))
		return;

	if (pattern[editor.editPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[editor.editPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int16_t row = 0; row < numRows; row++)
	{
		scaleNote(editor.editPattern, cursor.ch, row, dVol);
		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumePattern(void)
{
	if (!askForScaleFade("Volume scale-fade pattern (start-, end scale)"))
		return;

	if (pattern[editor.editPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[editor.editPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int16_t row = 0; row < numRows; row++)
	{
		for (int8_t ch = 0; ch < song.numChannels; ch++)
			scaleNote(editor.editPattern, ch, row, dVol);

		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumeBlock(void)
{
	if (!askForScaleFade("Volume scale-fade block (start-, end scale)"))
		return;

	if (pattern[editor.editPattern] == NULL || pattMark.markY1 == pattMark.markY2 || pattMark.markY1 > pattMark.markY2)
		return;

	const int32_t numRows = pattMark.markY2 - pattMark.markY1;

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int16_t row = pattMark.markY1; row < pattMark.markY2; row++)
	{
		for (int16_t ch = pattMark.markX1; ch <= pattMark.markX2; ch++)
			scaleNote(editor.editPattern, (uint8_t)ch, row, dVol);

		dVol += dVolDelta;
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
