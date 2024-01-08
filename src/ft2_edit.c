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
static uint16_t ptnBufLen, trkBufLen;
static int32_t markXSize, markYSize;
static note_t blkCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t ptnCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t trackCopyBuff[MAX_PATT_LEN];

// for recordNote()
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
			pauseMusic();
			const volatile uint16_t curPattern = editor.editPattern;
			int16_t row = editor.row;
			resumeMusic();

			if (!allocatePattern(curPattern))
				return true; // key pressed

			pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch].note = NOTE_OFF;

			const uint16_t numRows = patternNumRows[curPattern];
			if (playMode == PLAYMODE_EDIT && numRows >= 1)
				setPos(-1, (row + editor.editRowSkip) % numRows, true);

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

	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (i == -1 || !allocatePattern(curPattern))
		return false; // no edit to be done

	// insert slot data

	note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
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

	const int16_t numRows = patternNumRows[curPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setPos(-1, (row + editor.editRowSkip) % numRows, true);

	if (i == 0) // if we inserted a zero, check if pattern is empty
		killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	return true;
}

static void evaluateTimeStamp(int16_t *songPos, int16_t *pattNum, int16_t *row, int16_t *tick)
{
	int16_t outSongPos = editor.songPos;
	int16_t outPattern = editor.editPattern;
	int16_t outRow = editor.row;
	int16_t outTick = editor.speed - editor.tick;

	outTick = CLAMP(outTick, 0, editor.speed-1);

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
			time = INT32_MAX;
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
			time = INT32_MAX;
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

			if (time == INT32_MAX)
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

		pauseMusic();
		const volatile uint16_t curPattern = editor.editPattern;
		int16_t row = editor.row;
		resumeMusic();

		if (pattern[curPattern] == NULL)
			return true;

		note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];

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

		killPatternIfUnused(curPattern);

		// increase row (only in edit mode)
		const int16_t numRows = patternNumRows[curPattern];
		if (playMode == PLAYMODE_EDIT && numRows >= 1)
			setPos(-1, (row + editor.editRowSkip) % numRows, true);

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
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	uint16_t writeVol = 0;
	uint16_t writeEfx = 0;

	if (pattern[curPattern] != NULL)
	{
		note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
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
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return;

	if (!allocatePattern(curPattern))
		return;
	
	note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
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

	const int16_t numRows = patternNumRows[curPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setPos(-1, (row + editor.editRowSkip) % numRows, true);

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternNote(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[curPattern];

	if (numRows > 1)
	{
		for (int32_t i = numRows-2; i >= row; i--)
			p[((i+1) * MAX_CHANNELS) + cursor.ch] = p[(i * MAX_CHANNELS) + cursor.ch];
	}

	memset(&p[(row * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void insertPatternLine(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	setPatternLen(curPattern, patternNumRows[curPattern] + config.recTrueInsert); // config.recTrueInsert is 0 or 1

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		const int16_t numRows = patternNumRows[curPattern];

		if (numRows > 1)
		{
			for (int32_t i = numRows-2; i >= row; i--)
			{
				for (int32_t j = 0; j < MAX_CHANNELS; j++)
					p[((i+1) * MAX_CHANNELS) + j] = p[(i * MAX_CHANNELS) + j];
			}
		}

		memset(&p[row * MAX_CHANNELS], 0, TRACK_WIDTH);

		killPatternIfUnused(curPattern);
	}

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternNote(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	const int16_t numRows = patternNumRows[curPattern];

	note_t *p = pattern[curPattern];
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

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void deletePatternLine(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (playMode != PLAYMODE_EDIT && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	const int16_t numRows = patternNumRows[curPattern];
	note_t *p = pattern[curPattern];
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
		setPatternLen(curPattern, numRows-1);

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

// ----- TRANSPOSE FUNCTIONS -----

static uint32_t countOverflowingNotes(uint8_t mode, int8_t addValue, bool allInstrumentsFlag,
	uint16_t curPattern, int32_t numRows, int32_t markX1, int32_t markX2, int32_t markY1, int32_t markY2)
{
	uint32_t notesToDelete = 0;

	// "addValue" is never <-12 or >12, so unsigned 8-bit testing for >96 is safe
	switch (mode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0; // empty pattern

			p += cursor.ch;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
				{
					if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
						notesToDelete++;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0; // empty pattern

			const int32_t pitch = MAX_CHANNELS-song.numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
							notesToDelete++;
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
					return 0; // empty pattern

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
						{
							if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
								notesToDelete++;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (markY1 == markY2 || markY1 > markY2)
				return 0;

			if (markX1 > song.numChannels-1)
				markX1 = song.numChannels-1;

			if (markX2 > song.numChannels-1)
				markX2 = song.numChannels-1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows-1;

			if (markY2 > numRows)
				markY2 = numRows-markY1;

			note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return 0;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
							notesToDelete++;
					}
				}
			}
		}
		break;

		default: break;
	}

	return notesToDelete;
}

static void doTranspose(uint8_t mode, int8_t addValue, bool allInstrumentsFlag)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	const int32_t numRows = patternNumRows[curPattern];
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();
	
	uint32_t overflowingNotes = countOverflowingNotes(mode, addValue, allInstrumentsFlag,
		curPattern, numRows, markX1, markX2, markY1, markY2);
	if (overflowingNotes > 0)
	{
		char text[48];
		sprintf(text, "%u note(s) will be erased! Proceed?", overflowingNotes);
		if (okBox(2, "System request", text, NULL) != 1)
			return;
	}

	// "addValue" is never <-12 or >12, so unsigned 8-bit testing for >96 is safe
	switch (mode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			p += cursor.ch;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				volatile uint8_t note = p->note;
				if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
				{
					note += addValue;
					if (note > 96)
						note = 0; // also handles underflow

					p->note = note;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			const int32_t pitch = MAX_CHANNELS - song.numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					volatile uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						note += addValue;
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

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						volatile uint8_t note = p->note;
						if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
						{
							note += addValue;
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
			if (markY1 == markY2 || markY1 > markY2)
				return;

			if (markX1 > song.numChannels-1)
				markX1 = song.numChannels-1;

			if (markX2 > song.numChannels-1)
				markX2 = song.numChannels-1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows-1;

			if (markY2 > numRows)
				markY2 = numRows-markY1;

			note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					volatile uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						note += addValue;
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
	doTranspose(TRANSP_TRACK, 1, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurInsDn(void)
{
	doTranspose(TRANSP_TRACK, -1, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurIns12Up(void)
{
	doTranspose(TRANSP_TRACK, 12, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_TRACK, -12, TRANSP_CUR_INSTRUMENT);
}

void trackTranspAllInsUp(void)
{
	doTranspose(TRANSP_TRACK, 1, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllInsDn(void)
{
	doTranspose(TRANSP_TRACK, -1, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllIns12Up(void)
{
	doTranspose(TRANSP_TRACK, 12, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_TRACK, -12, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspCurInsUp(void)
{
	doTranspose(TRANSP_PATT, 1, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurInsDn(void)
{
	doTranspose(TRANSP_PATT, -1, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurIns12Up(void)
{
	doTranspose(TRANSP_PATT, 12, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_PATT, -12, TRANSP_CUR_INSTRUMENT);
}

void pattTranspAllInsUp(void)
{
	doTranspose(TRANSP_PATT, 1, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllInsDn(void)
{
	doTranspose(TRANSP_PATT, -1, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllIns12Up(void)
{
	doTranspose(TRANSP_PATT, 12, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_PATT, -12, TRANSP_ALL_INSTRUMENTS);
}

void songTranspCurInsUp(void)
{
	doTranspose(TRANSP_SONG, 1, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurInsDn(void)
{
	doTranspose(TRANSP_SONG, -1, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurIns12Up(void)
{
	doTranspose(TRANSP_SONG, 12, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_SONG, -12, TRANSP_CUR_INSTRUMENT);
}

void songTranspAllInsUp(void)
{
	doTranspose(TRANSP_SONG, 1, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllInsDn(void)
{
	doTranspose(TRANSP_SONG, -1, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllIns12Up(void)
{
	doTranspose(TRANSP_SONG, 12, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_SONG, -12, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspCurInsUp(void)
{
	doTranspose(TRANSP_BLOCK, 1, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurInsDn(void)
{
	doTranspose(TRANSP_BLOCK, -1, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurIns12Up(void)
{
	doTranspose(TRANSP_BLOCK, 12, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_BLOCK, -12, TRANSP_CUR_INSTRUMENT);
}

void blockTranspAllInsUp(void)
{
	doTranspose(TRANSP_BLOCK, 1, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllInsDn(void)
{
	doTranspose(TRANSP_BLOCK, -1, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllIns12Up(void)
{
	doTranspose(TRANSP_BLOCK, 12, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_BLOCK, -12, TRANSP_ALL_INSTRUMENTS);
}

static void copyNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0])
			dst->note = src->note;

		if (editor.copyMask[1])
			dst->instr = src->instr;

		if (editor.copyMask[2])
			dst->vol = src->vol;

		if (editor.copyMask[3])
			dst->efx = src->efx;

		if (editor.copyMask[4])
			dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

static void pasteNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0] && (src->note != 0 || !editor.transpMask[0]))
			dst->note = src->note;

		if (editor.copyMask[1] && (src->instr != 0 || !editor.transpMask[1]))
			dst->instr = src->instr;

		if (editor.copyMask[2] && (src->vol != 0 || !editor.transpMask[2]))
			dst->vol = src->vol;

		if (editor.copyMask[3] && (src->efx != 0 || !editor.transpMask[3]))
			dst->efx = src->efx;

		if (editor.copyMask[4] && (src->efxData != 0 || !editor.transpMask[4]))
			dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

void cutTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[curPattern];

	if (config.ptnCutToBuffer)
	{
		memset(trackCopyBuff, 0, sizeof (trackCopyBuff));

		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

		trkBufLen = numRows;
	}

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		memset(&p[(i * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		memset(trackCopyBuff, 0, sizeof (trackCopyBuff));

		const int16_t numRows = patternNumRows[curPattern];
		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

		trkBufLen = numRows;
	}
}

void pasteTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (trkBufLen == 0 || !allocatePattern(curPattern))
		return;

	note_t *p = pattern[curPattern];
	const int16_t numRows = patternNumRows[curPattern];

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		pasteNote(&trackCopyBuff[i], &p[(i * MAX_CHANNELS) + cursor.ch]);
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;

	const int16_t numRows = patternNumRows[curPattern];

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
			memset(&p[(i * MAX_CHANNELS) + x], 0, sizeof (note_t));
	}
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void copyPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_CHANNELS) * sizeof (note_t));

		const int16_t numRows = patternNumRows[curPattern];
		for (int16_t x = 0; x < song.numChannels; x++)
		{
			for (int16_t i = 0; i < numRows; i++)
				copyNote(&p[(i * MAX_CHANNELS) + x], &ptnCopyBuff[(i * MAX_CHANNELS) + x]);
		}

		ptnBufLen = numRows;
	}
}

void pastePattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (ptnBufLen == 0)
		return;

	if (patternNumRows[curPattern] != ptnBufLen)
	{
		if (okBox(2, "System request", "Adjust pattern length to match copied pattern length?", NULL) == 1)
			setPatternLen(curPattern, ptnBufLen);
	}

	if (!allocatePattern(curPattern))
		return;

	note_t *p = pattern[curPattern];
	const int16_t numRows = patternNumRows[curPattern];
	
	pauseMusic();
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			pasteNote(&ptnCopyBuff[(i * MAX_CHANNELS) + x], &p[(i * MAX_CHANNELS) + x]);
	}
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void cutBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	const int16_t numRows = patternNumRows[curPattern];

	if (markY1 == markY2 || markY1 > markY2)
		return;

	if (markX1 > song.numChannels-1)
		markX1 = song.numChannels-1;

	if (markX2 > song.numChannels-1)
		markX2 = song.numChannels-1;

	if (markX2 < markX1)
		markX2 = markX1;

	if (markY1 >= numRows)
		markY1 = numRows-1;

	if (markY2 > numRows)
		markY2 = numRows-markY1;

	note_t *p = pattern[curPattern];
	if (p != NULL && markY1 >= 0 && markX1 >= 0 && markX2 >= 0 && markY2 >= 0)
	{
		pauseMusic();
		for (int32_t x = markX1; x <= markX2; x++)
		{
			for (int32_t y = markY1; y < markY2; y++)
			{
				note_t *n = &p[(y * MAX_CHANNELS) + x];

				if (config.ptnCutToBuffer)
					copyNote(n, &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);

				memset(n, 0, sizeof (note_t));
			}
		}
		resumeMusic();

		killPatternIfUnused(curPattern);

		if (config.ptnCutToBuffer)
		{
			markXSize = markX2 - markX1;
			markYSize = markY2 - markY1;
			blockCopied = true;
		}

		ui.updatePatternEditor = true;
		setSongModifiedFlag();
	}
}

void copyBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	const int16_t numRows = patternNumRows[curPattern];

	if (markY1 == markY2 || markY1 > markY2)
		return;

	if (markX1 > song.numChannels-1)
		markX1 = song.numChannels-1;

	if (markX2 > song.numChannels-1)
		markX2 = song.numChannels-1;

	if (markX2 < markX1)
		markX2 = markX1;

	if (markY1 >= numRows)
		markY1 = numRows-1;

	if (markY2 > numRows)
		markY2 = numRows-markY1;

	note_t *p = pattern[curPattern];
	if (p != NULL && markY1 >= 0 && markX1 >= 0 && markX2 >= 0 && markY2 >= 0)
	{
		for (int32_t x = markX1; x <= markX2; x++)
		{
			for (int32_t y = markY1; y < markY2; y++)
				copyNote(&p[(y * MAX_CHANNELS) + x], &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);
		}

		markXSize = markX2 - markX1;
		markYSize = markY2 - markY1;
		blockCopied = true;
	}
}

void pasteBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	const volatile uint16_t curRow = editor.row;
	resumeMusic();

	if (!blockCopied || !allocatePattern(curPattern))
		return;

	int32_t chStart = cursor.ch;
	int32_t rowStart = curRow;
	const int16_t numRows = patternNumRows[curPattern];

	if (chStart >= song.numChannels)
		chStart = song.numChannels-1;

	if (rowStart >= numRows)
		rowStart = numRows-1;

	int32_t markedChannels = markXSize + 1;
	if (chStart+markedChannels > song.numChannels)
		markedChannels = song.numChannels - chStart;

	int32_t markedRows = markYSize;
	if (rowStart+markedRows > numRows)
		markedRows = numRows - rowStart;

	if (markedChannels > 0 && markedRows > 0)
	{
		note_t *p = pattern[curPattern];

		pauseMusic();
		for (int32_t x = chStart; x < chStart+markedChannels; x++)
		{
			for (int32_t y = rowStart; y < rowStart+markedRows; y++)
				pasteNote(&blkCopyBuff[((y - rowStart) * MAX_CHANNELS) + (x - chStart)], &p[(y * MAX_CHANNELS) + x]);
		}
		resumeMusic();
	}

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

static void remapInstrXY(int32_t pattNum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t src, uint8_t dst)
{
	note_t *pattPtr = pattern[pattNum];
	if (pattPtr == NULL)
		return;

	if (x1 > song.numChannels-1)
		x1 = song.numChannels-1;

	if (x2 > song.numChannels-1)
		x2 = song.numChannels-1;

	if (x2 < x1)
		x2 = x1;

	const int16_t numRows = patternNumRows[pattNum];
	if (y1 >= numRows)
		y1 = numRows-1;

	if (y2 > numRows)
		y2 = numRows-y1;

	note_t *p = &pattPtr[(y1 * MAX_CHANNELS) + x1];
	const int32_t pitch = MAX_CHANNELS - ((x2 + 1) - x1);

	for (int32_t y = y1; y <= y2; y++, p += pitch)
	{
		for (int32_t x = x1; x <= x2; x++, p++)
		{
			if (p->instr == src)
				p->instr = dst;
		}
	}
}

void remapBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	if (editor.srcInstr == editor.curInstr || markY1 == markY2 || markY1 > markY2)
		return;

	remapInstrXY(curPattern,
	             markX1, markY1,
	             markX2, markY2 - 1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(curPattern,
	             cursor.ch, 0,
	             cursor.ch, patternNumRows[curPattern]-1,
	             editor.srcInstr, editor.curInstr);
	resumeMusic();

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void remapPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (editor.srcInstr == editor.curInstr)
		return;

	pauseMusic();
	remapInstrXY(curPattern,
	             0, 0,
	             song.numChannels-1, patternNumRows[curPattern]-1,
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
		// remapInstrXY() also checks if pattern is not allocated!
		remapInstrXY(i,
		             0, 0,
		             song.numChannels-1, patternNumRows[i]-1,
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

	if (newVol > 64)
		newVol = 64;

	const int8_t oldv = getNoteVolume(p);
	if (p->vol == oldv)
		return; // volume is the same

	if (p->efx == 0x0C)
		p->efxData = newVol; // Cxx effect
	else
		p->vol = 0x10 + newVol; // volume column
}

static void scaleNote(int32_t pattNum, int32_t ch, int32_t row, double dScale)
{
	if (pattern[pattNum] == NULL)
		return;

	const int32_t numRows = patternNumRows[pattNum];
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
	if (inputBox(1, msg, volstr, sizeof (volstr)-1) != 1)
		return false;

	bool error = false;

	char *val1 = volstr;
	if (strlen(val1) < 3)
		error = true;

	char *val2 = strchr(volstr, ',');
	if (val2 == NULL || strlen(val2) < 3)
		error = true;

	if (error)
	{
		okBox(0, "System message", "Invalid constant expressions.", NULL);
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

	const volatile uint16_t curPattern = editor.editPattern;

	if (pattern[curPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[curPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = 0; row < numRows; row++)
	{
		scaleNote(curPattern, cursor.ch, row, dVol);
		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumePattern(void)
{
	if (!askForScaleFade("Volume scale-fade pattern (start-, end scale)"))
		return;

	const volatile uint16_t curPattern = editor.editPattern;

	if (pattern[curPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[curPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t ch = 0; ch < song.numChannels; ch++)
			scaleNote(curPattern, ch, row, dVol);

		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumeBlock(void)
{
	if (!askForScaleFade("Volume scale-fade block (start-, end scale)"))
		return;

	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	if (pattern[curPattern] == NULL || markY1 == markY2 || markY1 > markY2)
		return;

	const int32_t numRows = markY2 - markY1;

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = markY1; row < markY2; row++)
	{
		for (int32_t ch = markX1; ch <= markX2; ch++)
			scaleNote(curPattern, ch, row, dVol);

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
