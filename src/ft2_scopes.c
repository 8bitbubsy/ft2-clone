// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <math.h> // modf()
#ifndef _WIN32
#include <unistd.h> // usleep()
#endif
#include "ft2_header.h"
#include "ft2_events.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_bmp.h"
#include "ft2_scopes.h"
#include "ft2_mouse.h"
#include "ft2_video.h"
#include "ft2_scopedraw.h"
#include "ft2_tables.h"
#include "ft2_structs.h"

static volatile bool scopesUpdatingFlag, scopesDisplayingFlag;
static uint32_t scopeTimeLen, scopeTimeLenFrac;
static uint64_t timeNext64, timeNext64Frac;
static volatile scope_t scope[MAX_VOICES];
static SDL_Thread *scopeThread;

lastChInstr_t lastChInstr[MAX_VOICES]; // global

void resetCachedScopeVars(void)
{
	volatile scope_t *sc = scope;
	for (int32_t i = 0; i < MAX_VOICES; i++, sc++)
	{
		sc->dOldHz = -1.0;
		sc->oldDelta = 0;
		sc->oldDrawDelta = 0;
	}
}

int32_t getSamplePosition(uint8_t ch)
{
	if (ch >= song.antChn)
		return -1;

	volatile scope_t *sc = &scope[ch];

	// cache some stuff
	volatile bool active = sc->active;
	volatile int32_t pos = sc->pos;
	volatile int32_t end = sc->end;
	volatile bool sampleIs16Bit = sc->sampleIs16Bit;

	if (!active || end == 0)
		return -1;

	if (pos >= 0 && pos < end)
	{
		if (sampleIs16Bit)
			pos <<= 1;

		return pos;
	}

	return -1; // not active or overflown
}

void stopAllScopes(void)
{
	// wait for scopes to finish updating
	while (scopesUpdatingFlag);
	
	volatile scope_t *sc = scope;
	for (int32_t i = 0; i < MAX_VOICES; i++, sc++)
		sc->active = false;

	// wait for scope displaying to be done (safety)
	while (scopesDisplayingFlag);
}

// toggle mute
static void setChannel(int32_t nr, bool on)
{
	stmTyp *ch = &stm[nr];

	ch->stOff = !on;
	if (ch->stOff)
	{
		ch->effTyp = 0;
		ch->eff = 0;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->dFinalVol = 0.0;
		ch->outPan = 128;
		ch->oldPan = 128;
		ch->finalPan = 128;
		ch->status = IS_Vol;

		ch->envSustainActive = false; // non-FT2 bug fix for stuck piano keys
	}

	scope[nr].wasCleared = false;
}

static void drawScopeNumber(uint16_t scopeXOffs, uint16_t scopeYOffs, uint8_t channel, bool outline)
{
	scopeXOffs++;
	scopeYOffs++;
	channel++;

	if (outline)
	{
		if (channel < 10) // one digit?
		{
			charOutOutlined(scopeXOffs, scopeYOffs, PAL_MOUSEPT, '0' + channel);
		}
		else
		{
			charOutOutlined(scopeXOffs, scopeYOffs, PAL_MOUSEPT, chDecTab1[channel]);
			charOutOutlined(scopeXOffs + 7, scopeYOffs, PAL_MOUSEPT, chDecTab2[channel]);
		}
	}
	else
	{
		if (channel < 10) // one digit?
		{
			charOut(scopeXOffs, scopeYOffs, PAL_MOUSEPT, '0' + channel);
		}
		else
		{
			charOut(scopeXOffs, scopeYOffs, PAL_MOUSEPT, chDecTab1[channel]);
			charOut(scopeXOffs + 7, scopeYOffs, PAL_MOUSEPT, chDecTab2[channel]);
		}
	}
}

static void redrawScope(int32_t ch)
{
	int32_t i;

	int32_t chansPerRow = (uint32_t)song.antChn >> 1;
	int32_t chanLookup = chansPerRow - 1;
	const uint16_t *scopeLens = scopeLenTab[chanLookup];

	// get x,y,len for scope according to channel (we must do it this way since 'len' can differ!)

	uint16_t x = 2;
	uint16_t y = 94;

	uint16_t scopeLen = 0; // prevent compiler warning
	for (i = 0; i < song.antChn; i++)
	{
		scopeLen = scopeLens[i];

		if (i == chansPerRow) // did we reach end of row?
		{
			// yes, go one row down
			x  = 2;
			y += 39;
		}

		if (i == ch)
			break;

		// adjust position to next channel
		x += scopeLen + 3;
	}

	drawFramework(x, y, scopeLen + 2, 38, FRAMEWORK_TYPE2);

	// draw mute graphics if channel is muted
	if (!editor.chnMode[i])
	{
		const uint16_t muteGfxLen = scopeMuteBMP_Widths[chanLookup];
		const uint16_t muteGfxX = x + ((scopeLen - muteGfxLen) >> 1);

		blitFastClipX(muteGfxX, y + 6, bmp.scopeMute+scopeMuteBMP_Offs[chanLookup], 162, scopeMuteBMP_Heights[chanLookup], muteGfxLen);

		if (config.ptnChnNumbers)
			drawScopeNumber(x + 1, y + 1, (uint8_t)i, true);
	}

	scope[ch].wasCleared = false;
}

void refreshScopes(void)
{
	for (int32_t i = 0; i < MAX_VOICES; i++)
		scope[i].wasCleared = false;
}

static void channelMode(int32_t chn)
{
	int32_t i;
	
	assert(chn < song.antChn);

	bool m = mouse.leftButtonPressed && !mouse.rightButtonPressed;
	bool m2 = mouse.rightButtonPressed && mouse.leftButtonPressed;

	if (m2)
	{
		bool test = false;
		for (i = 0; i < song.antChn; i++)
		{
			if (i != chn && !editor.chnMode[i])
				test = true;
		}

		if (test)
		{
			for (i = 0; i < song.antChn; i++)
				editor.chnMode[i] = true;
		}
		else
		{
			for (i = 0; i < song.antChn; i++)
				editor.chnMode[i] = (i == chn);
		}
	}
	else if (m)
	{
		editor.chnMode[chn] ^= 1;
	}
	else
	{
		if (editor.chnMode[chn])
		{
			config.multiRecChn[chn] ^= 1;
		}
		else
		{
			config.multiRecChn[chn] = true;
			editor.chnMode[chn] = true;
			m = true;
		}
	}

	for (i = 0; i < song.antChn; i++)
		setChannel(i, editor.chnMode[i]);

	if (m2)
	{
		for (i = 0; i < song.antChn; i++)
			redrawScope(i);
	}
	else
	{
		redrawScope(chn);
	}
}

bool testScopesMouseDown(void)
{
	int32_t i;

	if (!ui.scopesShown)
		return false;

	if (mouse.y >= 95 && mouse.y <= 169 && mouse.x >= 3 && mouse.x <= 288)
	{
		if (mouse.y > 130 && mouse.y < 134)
			return true;

		int32_t chansPerRow = (uint32_t)song.antChn >> 1;
		const uint16_t *scopeLens = scopeLenTab[chansPerRow-1];

		// find out if we clicked inside a scope
		uint16_t x = 3;
		for (i = 0; i < chansPerRow; i++)
		{
			if (mouse.x >= x && mouse.x < x+scopeLens[i])
				break;

			x += scopeLens[i]+3;
		}

		if (i == chansPerRow)
			return true; // scope framework was clicked instead

		int32_t chanToToggle = i;
		if (mouse.y >= 134) // second row of scopes?
			chanToToggle += chansPerRow; // yes, increase lookup offset

		channelMode(chanToToggle);
		return true;
	}

	return false;
}

static void scopeTrigger(int32_t ch, const sampleTyp *s, int32_t playOffset)
{
	scope_t tempState;

	volatile scope_t *sc = &scope[ch];

	int32_t length = s->len;
	int32_t loopStart = s->repS;
	int32_t loopLength = s->repL;
	int32_t loopEnd = s->repS + s->repL;
	uint8_t loopType = s->typ & 3;
	bool sampleIs16Bit = (s->typ >> 4) & 1;

	if (sampleIs16Bit)
	{
		assert(!(length & 1));
		assert(!(loopStart & 1));
		assert(!(loopLength & 1));
		assert(!(loopEnd & 1));

		length >>= 1;
		loopStart >>= 1;
		loopLength >>= 1;
		loopEnd >>= 1;
	}

	if (s->pek == NULL || length < 1)
	{
		sc->active = false; // shut down scope (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sampleIs16Bit)
		tempState.base16 = (const int16_t *)s->pek;
	else
		tempState.base8 = s->pek;

	tempState.sampleIs16Bit = sampleIs16Bit;
	tempState.loopType = loopType;

	tempState.direction = 1; // forwards
	tempState.end = (loopType > 0) ? loopEnd : length;
	tempState.loopStart = loopStart;
	tempState.loopLength = loopLength;
	tempState.pos = playOffset;
	tempState.posFrac = 0;
	
	// if position overflows (f.ex. through 9xx command), shut down scopes
	if (tempState.pos >= tempState.end)
	{
		sc->active = false;
		return;
	}

	// these has to be copied so that they are not lost
	tempState.wasCleared = sc->wasCleared;
	tempState.delta = sc->delta;
	tempState.oldDelta = sc->oldDelta;
	tempState.drawDelta = sc->drawDelta;
	tempState.oldDrawDelta = sc->oldDrawDelta;
	tempState.dOldHz = sc->dOldHz;
	tempState.vol = sc->vol;

	tempState.active = true;

	/* Update live scope now.
	** In theory it -can- be written to in the middle of a cached read,
	** then the read thread writes its own non-updated cached copy back and
	** the trigger never happens. So far I have never seen it happen,
	** so it's probably very rare. Yes, this is not good coding...
	*/

	*sc = tempState;
}

static void updateScopes(void)
{
	int32_t loopOverflowVal;

	scopesUpdatingFlag = true;

	volatile scope_t *sc = scope;
	for (int32_t i = 0; i < song.antChn; i++, sc++)
	{
		scope_t s = *sc; // cache it
		if (!s.active)
			continue; // scope is not active, no need

		// scope position update

		s.posFrac += s.delta;
		const int32_t wholeSamples = s.posFrac >> 32;
		s.posFrac &= 0xFFFFFFFF;

		if (s.direction == 1)
			s.pos += wholeSamples; // forwards
		else
			s.pos -= wholeSamples; // backwards

		// handle loop wrapping or sample end
		if (s.direction == -1 && s.pos < s.loopStart) // sampling backwards (definitely pingpong loop)
		{
			s.direction = 1; // change direction to forwards

			if (s.loopLength >= 2)
				s.pos = s.loopStart + ((s.loopStart - s.pos - 1) % s.loopLength);
			else
				s.pos = s.loopStart;

			assert(s.pos >= s.loopStart && s.pos < s.end);
		}
		else if (s.pos >= s.end)
		{
			if (s.loopLength >= 2)
				loopOverflowVal = (s.pos - s.end) % s.loopLength;
			else
				loopOverflowVal = 0;

			if (s.loopType == LOOP_DISABLED)
			{
				s.active = false;
			}
			else if (s.loopType == LOOP_FORWARD)
			{
				s.pos = s.loopStart + loopOverflowVal;
				assert(s.pos >= s.loopStart && s.pos < s.end);
			}
			else // pingpong loop
			{
				s.direction = -1; // change direction to backwards
				s.pos = (s.end - 1) - loopOverflowVal;
				assert(s.pos >= s.loopStart && s.pos < s.end);
			}
		}
		assert(s.pos >= 0);

		*sc = s; // update scope state
	}
	scopesUpdatingFlag = false;
}

void drawScopes(void)
{
	scopesDisplayingFlag = true;
	int32_t chansPerRow = (uint32_t)song.antChn >> 1;

	const uint16_t *scopeLens = scopeLenTab[chansPerRow-1];
	uint16_t scopeXOffs = 3;
	uint16_t scopeYOffs = 95;
	int16_t scopeLineY = 112;

	for (int32_t i = 0; i < song.antChn; i++)
	{
		// if we reached the last scope on the row, go to first scope on the next row
		if (i == chansPerRow)
		{
			scopeXOffs = 3;
			scopeYOffs = 134;
			scopeLineY = 151;
		}

		const uint16_t scopeDrawLen = scopeLens[i];
		if (!editor.chnMode[i]) // scope muted (mute graphics blit()'ed elsewhere)
		{
			scopeXOffs += scopeDrawLen+3; // align x to next scope
			continue;
		}

		const scope_t s = scope[i]; // cache scope to lower thread race condition issues
		if (s.active && s.vol > 0 && !audio.locked)
		{
			// scope is active
			scope[i].wasCleared = false;

			// clear scope background
			clearRect(scopeXOffs, scopeYOffs, scopeDrawLen, SCOPE_HEIGHT);

			// draw scope
			bool linedScopesFlag = !!(config.specialFlags & LINED_SCOPES);
			scopeDrawRoutineTable[(linedScopesFlag * 6) + (s.sampleIs16Bit * 3) + s.loopType](&s, scopeXOffs, scopeLineY, scopeDrawLen);
		}
		else
		{
			// scope is inactive
			volatile scope_t *sc = &scope[i];
			if (!sc->wasCleared)
			{
				// clear scope background
				clearRect(scopeXOffs, scopeYOffs, scopeDrawLen, SCOPE_HEIGHT);

				// draw empty line
				hLine(scopeXOffs, scopeLineY, scopeDrawLen, PAL_PATTEXT);

				sc->wasCleared = true;
			}
		}

		// draw channel numbering (if enabled)
		if (config.ptnChnNumbers)
			drawScopeNumber(scopeXOffs, scopeYOffs, (uint8_t)i, false);

		// draw rec. symbol (if enabled)
		if (config.multiRecChn[i])
			blit(scopeXOffs + 1, scopeYOffs + 31, bmp.scopeRec, 13, 4);

		scopeXOffs += scopeDrawLen+3; // align x to next scope
	}

	scopesDisplayingFlag = false;
}

void drawScopeFramework(void)
{
	drawFramework(0, 92, 291, 81, FRAMEWORK_TYPE1);
	for (int32_t i = 0; i < song.antChn; i++)
		redrawScope(i);
}

void handleScopesFromChQueue(chSyncData_t *chSyncData, uint8_t *scopeUpdateStatus)
{
	volatile scope_t *sc = scope;
	syncedChannel_t *ch = chSyncData->channels;
	for (int32_t i = 0; i < song.antChn; i++, sc++, ch++)
	{
		const uint8_t status = scopeUpdateStatus[i];

		if (status & IS_Vol)
			sc->vol = ((ch->vol * SCOPE_HEIGHT) + 128) >> 8; // rounded

		if (status & IS_Period)
		{
			// use cached values when possible
			if (ch->dHz != sc->dOldHz)
			{
				sc->dOldHz = ch->dHz;

				const double dHz2ScopeDeltaMul = SCOPE_FRAC_SCALE / (double)SCOPE_HZ;
				sc->oldDelta = (int64_t)((ch->dHz * dHz2ScopeDeltaMul) + 0.5); // Hz -> 32.32fp delta (rounded)

				const double dRelativeHz = ch->dHz * (1.0 / (8363.0 / 2.0));
				sc->oldDrawDelta = (int32_t)((dRelativeHz * SCOPE_DRAW_FRAC_SCALE) + 0.5); // Hz -> 13.19fp draw delta (rounded)
			}

			sc->delta = sc->oldDelta;
			sc->drawDelta = sc->oldDrawDelta;
		}

		if (status & IS_NyTon)
		{
			if (instr[ch->instrNr] != NULL)
			{
				scopeTrigger(i, &instr[ch->instrNr]->samp[ch->sampleNr], ch->smpStartPos);

				// set some stuff used by Smp. Ed. for sampling position line

				if (ch->instrNr == 130 || (ch->instrNr == editor.curInstr && ch->sampleNr == editor.curSmp))
					editor.curSmpChannel = (uint8_t)i;

				lastChInstr[i].instrNr = ch->instrNr;
				lastChInstr[i].sampleNr = ch->sampleNr;
			}
			else
			{
				// empty instrument, shut down scope
				scope[i].active = false;
				lastChInstr[i].instrNr = 255;
				lastChInstr[i].sampleNr = 255;
			}
		}
	}
}

static int32_t SDLCALL scopeThreadFunc(void *ptr)
{
	// this is needed for scope stability (confirmed)
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

	// set next frame time
	timeNext64 = SDL_GetPerformanceCounter() + scopeTimeLen;
	timeNext64Frac = scopeTimeLenFrac;

	while (editor.programRunning)
	{
		editor.scopeThreadMutex = true;
		updateScopes();
		editor.scopeThreadMutex = false;

		uint64_t time64 = SDL_GetPerformanceCounter();
		if (time64 < timeNext64)
		{
			time64 = timeNext64 - time64;
			if (time64 > INT32_MAX)
				time64 = INT32_MAX;

			const int32_t diff32 = (int32_t)time64;

			// convert and round to microseconds
			const int32_t time32 = (int32_t)((diff32 * editor.dPerfFreqMulMicro) + 0.5);

			// delay until we have reached the next frame
			if (time32 > 0)
				usleep(time32);
		}

		// update next tick time
		timeNext64 += scopeTimeLen;
		timeNext64Frac += scopeTimeLenFrac;
		if (timeNext64Frac > UINT32_MAX)
		{
			timeNext64Frac &= UINT32_MAX;
			timeNext64++;
		}
	}

	(void)ptr;
	return true;
}

bool initScopes(void)
{
	double dInt;

	// calculate scope time for performance counters and split into int/frac
	double dFrac = modf(editor.dPerfFreq / SCOPE_HZ, &dInt);

	// integer part
	scopeTimeLen = (int32_t)dInt;

	// fractional part (scaled to 0..2^32-1)
	dFrac *= UINT32_MAX+1.0;
	scopeTimeLenFrac = (uint32_t)dFrac;

	scopeThread = SDL_CreateThread(scopeThreadFunc, NULL, NULL);
	if (scopeThread == NULL)
	{
		showErrorMsgBox("Couldn't create channel scope thread!");
		return false;
	}

	SDL_DetachThread(scopeThread);
	return true;
}
