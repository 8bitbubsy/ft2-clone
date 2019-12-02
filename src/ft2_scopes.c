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
#include "ft2_gfxdata.h"
#include "ft2_scopes.h"
#include "ft2_mouse.h"
#include "ft2_video.h"

enum
{
	LOOP_NONE = 0,
	LOOP_FORWARD = 1,
	LOOP_PINGPONG = 2
};

#define SCOPE_HEIGHT 36

// data to be read from main update thread during sample trigger
typedef struct scopeState_t
{
	int8_t *pek;
	uint8_t typ;
	int32_t len, repS, repL, playOffset;
} scopeState_t;

// actual scope data
typedef struct scope_t
{
	volatile bool active;
	const int8_t *sampleData8;
	const int16_t *sampleData16;
	int8_t SVol;
	bool wasCleared, sample16Bit;
	uint8_t loopType;
	int32_t SRepS, SRepL, SLen, SPos;
	uint32_t SFrq, SPosDec, posXOR;
} scope_t;

static volatile bool scopesUpdatingFlag, scopesDisplayingFlag;
static uint32_t oldVoiceDelta, oldSFrq, scopeTimeLen, scopeTimeLenFrac;
static uint64_t timeNext64, timeNext64Frac;
static volatile scope_t scope[MAX_VOICES];
static SDL_Thread *scopeThread;

lastChInstr_t lastChInstr[MAX_VOICES]; // global

static const uint8_t scopeMuteBMPWidths[16] =
{
	162,111, 76, 56, 42, 35, 28, 24,
	 21, 21, 17, 17, 12, 12,  9,  9
};

static const uint8_t scopeMuteBMPHeights[16] =
{
	27, 27, 26, 25, 25, 25, 24, 24,
	24, 24, 24, 24, 24, 24, 24, 24
};

static const uint8_t *scopeMuteBMPPointers[16] =
{
	scopeMuteBMP1, scopeMuteBMP2, scopeMuteBMP3, scopeMuteBMP4,
	scopeMuteBMP5, scopeMuteBMP6, scopeMuteBMP7, scopeMuteBMP8,
	scopeMuteBMP9, scopeMuteBMP9, scopeMuteBMP10,scopeMuteBMP10,
	scopeMuteBMP11,scopeMuteBMP11,scopeMuteBMP12,scopeMuteBMP12
};

static const uint16_t scopeLenTab[16][32] =
{
	/*  2 ch */ {285,285},
	/*  4 ch */ {141,141,141,141},
	/*  6 ch */ {93,93,93,93,93,93},
	/*  8 ch */ {69,69,69,69,69,69,69,69},
	/* 10 ch */ {55,55,55,54,54,55,55,55,54,54},
	/* 12 ch */ {45,45,45,45,45,45,45,45,45,45,45,45},
	/* 14 ch */ {39,38,38,38,38,38,38,39,38,38,38,38,38,38},
	/* 16 ch */ {33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33},
	/* 18 ch */ {29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29},
	/* 20 ch */ {26,26,26,26,26,26,26,26,25,25,26,26,26,26,26,26,26,26,25,25},
	/* 22 ch */ {24,24,23,23,23,23,23,23,23,23,23,24,24,23,23,23,23,23,23,23,23,23},
	/* 24 ch */ {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21},
	/* 26 ch */ {20,20,19,19,19,19,19,19,19,19,19,19,19,20,20,19,19,19,19,19,19,19,19,19,19,19},
	/* 28 ch */ {18,18,18,18,18,18,18,18,17,17,17,17,17,17,18,18,18,18,18,18,18,18,17,17,17,17,17,17},
	/* 30 ch */ {17,17,17,16,16,16,16,16,16,16,16,16,16,16,16,17,17,17,16,16,16,16,16,16,16,16,16,16,16,16},
	/* 32 ch */ {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15}
};

void resetOldScopeRates(void)
{
	oldVoiceDelta = 0;
	oldSFrq = 0;
}

int32_t getSamplePosition(uint8_t ch)
{
	volatile bool active, sample16Bit;
	volatile int32_t pos, len;
	volatile scope_t *sc;

	if (ch >= song.antChn)
		return -1;

	sc = &scope[ch];

	// cache some stuff
	active = sc->active;
	pos = sc->SPos;
	len = sc->SLen;
	sample16Bit = sc->sample16Bit;

	if (!active || len == 0)
		return -1;

	if (pos >= 0 && pos < len)
	{
		if (sample16Bit)
			pos <<= 1;

		return pos;
	}

	return -1; // not active or overflown
}

void stopAllScopes(void)
{
	// wait for scopes to finish updating
	while (scopesUpdatingFlag);

	for (uint8_t i = 0; i < MAX_VOICES; i++)
		scope[i].active = false;

	// wait for scope displaying to be done (safety)
	while (scopesDisplayingFlag);
}

// toggle mute
static void setChannel(int16_t nr, bool on)
{
	stmTyp *ch;

	ch = &stm[nr];

	ch->stOff = !on;
	if (ch->stOff)
	{
		ch->effTyp = 0;
		ch->eff = 0;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->finalVol = 0;
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
			charOutOutlined(scopeXOffs, scopeYOffs, PAL_MOUSEPT, '0' + (channel / 10));
			charOutOutlined(scopeXOffs + 7, scopeYOffs, PAL_MOUSEPT, '0' + (channel % 10));
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
			charOut(scopeXOffs, scopeYOffs, PAL_MOUSEPT, '0' + (channel / 10));
			charOut(scopeXOffs + 7, scopeYOffs, PAL_MOUSEPT, '0' + (channel % 10));
		}
	}
}

static void redrawScope(int16_t ch)
{
	uint8_t chansPerRow;
	const uint16_t *scopeLens;
	uint16_t x, y, i, chanLookup, scopeLen, muteGfxLen, muteGfxX;

	chansPerRow = song.antChn / 2;
	chanLookup = chansPerRow - 1;
	scopeLens = scopeLenTab[chanLookup];

	x = 2;
	y = 94;

	scopeLen = 0; // prevent compiler warning
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
		muteGfxLen = scopeMuteBMPWidths[chanLookup];
		muteGfxX = x + ((scopeLen - muteGfxLen) / 2);

		blitFast(muteGfxX, y + 6, scopeMuteBMPPointers[chanLookup], muteGfxLen, scopeMuteBMPHeights[chanLookup]);

		if (config.ptnChnNumbers)
			drawScopeNumber(x + 1, y + 1, (uint8_t)i, true);
	}

	scope[ch].wasCleared = false;
}

void refreshScopes(void)
{
	for (int16_t i = 0; i < MAX_VOICES; i++)
		scope[i].wasCleared = false;
}

static void channelMode(int16_t chn)
{
	bool m, m2, test;
	int16_t i;
	
	assert(chn < song.antChn);

	m = mouse.leftButtonPressed && !mouse.rightButtonPressed;
	m2 = mouse.rightButtonPressed && mouse.leftButtonPressed;

	if (m2)
	{
		test = false;
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
	int8_t chanToToggle;
	uint8_t i, chansPerRow;
	uint16_t x;
	const uint16_t *scopeLens;

	if (!editor.ui.scopesShown)
		return false;

	if (mouse.y >= 95 && mouse.y <= 169 && mouse.x >= 3 && mouse.x <= 288)
	{
		if (mouse.y > 130 && mouse.y < 134)
			return true;

		chansPerRow = song.antChn / 2;
		scopeLens = scopeLenTab[chansPerRow-1];

		// find out if we clicked inside a scope
		x = 3;
		for (i = 0; i < chansPerRow; i++)
		{
			if (mouse.x >= x && mouse.x < x+scopeLens[i])
				break;

			x += scopeLens[i] + 3;
		}

		if (i == chansPerRow)
			return true; // scope framework was clicked instead

		chanToToggle = i;
		if (mouse.y >= 134) // second row of scopes?
			chanToToggle += chansPerRow; // yes, increase lookup offset

		channelMode(chanToToggle);
		return true;
	}

	return false;
}

static void scopeTrigger(uint8_t ch, sampleTyp *s, int32_t playOffset)
{
	bool sampleIs16Bit;
	uint8_t loopType;
	int32_t length, loopBegin, loopLength;
	volatile scope_t *sc;
	scope_t tempState;

	sc = &scope[ch];

	length = s->len;
	loopBegin = s->repS;
	loopLength = s->repL;
	loopType = s->typ & 3;
	sampleIs16Bit = (s->typ >> 4) & 1;

	if (sampleIs16Bit)
	{
		assert(!(length & 1));
		assert(!(loopBegin & 1));
		assert(!(loopLength & 1));

		length >>= 1;
		loopBegin >>= 1;
		loopLength >>= 1;
	}

	if (s->pek == NULL || length < 1)
	{
		sc->active = false; // shut down scope (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sampleIs16Bit)
		tempState.sampleData16 = (const int16_t *)s->pek;
	else
		tempState.sampleData8 = (const int8_t *)s->pek;

	tempState.sample16Bit = sampleIs16Bit;
	tempState.loopType = loopType;

	tempState.posXOR = 0; // forwards
	tempState.SLen = (loopType > 0) ? (loopBegin + loopLength) : length;
	tempState.SRepS = loopBegin;
	tempState.SRepL = loopLength;
	tempState.SPos = playOffset;
	tempState.SPosDec = 0; // position fraction
	
	// if 9xx position overflows, shut down scopes
	if (tempState.SPos >= tempState.SLen)
	{
		sc->active = false;
		return;
	}

	// these has to be read
	tempState.active = true;
	tempState.wasCleared = sc->wasCleared;
	tempState.SFrq = sc->SFrq;
	tempState.SVol = sc->SVol;

	/* Update live scope now.
	** In theory it -can- be written to in the middle of a cached read,
    ** then the read thread writes its own non-updated cached copy back and
	** the trigger never happens. So far I have never seen it happen,
	** so it's probably very rare. Yes, this is not good coding... */
	*sc = tempState;
}

static void updateScopes(void)
{
	int32_t loopOverflowVal;
	volatile scope_t *sc;
	scope_t tempState;

	scopesUpdatingFlag = true;
	for (uint32_t i = 0; i < song.antChn; i++)
	{
		sc = &scope[i];
		tempState = *sc; // cache it

		if (!tempState.active)
			continue; // scope is not active, no need

		// scope position update

		tempState.SPosDec += tempState.SFrq;
		tempState.SPos += ((tempState.SPosDec >> 16) ^ tempState.posXOR);
		tempState.SPosDec &= 0xFFFF;

		// handle loop wrapping or sample end

		if (tempState.posXOR == 0xFFFFFFFF && tempState.SPos < tempState.SRepS) // sampling backwards (definitely pingpong loop)
		{
			tempState.posXOR = 0; // change direction to forwards

			if (tempState.SRepL < 2)
				tempState.SPos = tempState.SRepS;
			else
				tempState.SPos = tempState.SRepS + ((tempState.SRepS - tempState.SPos - 1) % tempState.SRepL);

			assert(tempState.SPos >= tempState.SRepS && tempState.SPos < tempState.SLen);
		}
		else if (tempState.SPos >= tempState.SLen)
		{
			if (tempState.SRepL < 2)
				loopOverflowVal = 0;
			else
				loopOverflowVal = (tempState.SPos - tempState.SLen) % tempState.SRepL;

			if (tempState.loopType == LOOP_NONE)
			{
				tempState.active = false;
			}
			else if (tempState.loopType == LOOP_FORWARD)
			{
				tempState.SPos = tempState.SRepS + loopOverflowVal;
				assert(tempState.SPos >= tempState.SRepS && tempState.SPos < tempState.SLen);
			}
			else // pingpong loop
			{
				tempState.posXOR = 0xFFFFFFFF; // change direction to backwards
				tempState.SPos = (tempState.SLen - 1) - loopOverflowVal;
				assert(tempState.SPos >= tempState.SRepS && tempState.SPos < tempState.SLen);
			}
		}
		assert(tempState.SPos >= 0);

		*sc = tempState; // update scope state
	}
	scopesUpdatingFlag = false;
}

static void scopeLine(int16_t x1, int16_t y1, int16_t y2)
{
	int16_t d, sy, dy;
	uint16_t ay;
	int32_t pitch;
	uint32_t pixVal, *dst32;

	dy = y2 - y1;
	ay = ABS(dy);
	sy = SGN(dy);

	pixVal = video.palette[PAL_PATTEXT];
	pitch = sy * SCREEN_W;

	dst32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];
	*dst32 = pixVal;

	if (ay <= 1)
	{
		if (ay != 0)
			dst32 += pitch;

		*++dst32 = pixVal;
		return;
	}

	d = 2 - ay;

	ay *= 2;
	while (y1 != y2)
	{
		if (d >= 0)
		{
			d -= ay;
			dst32++;
		}

		y1 += sy;
		d  += 2;

		 dst32 += pitch;
		*dst32  = pixVal;
	}
}

static inline int8_t getScaledScopeSample8(scope_t *sc, int32_t drawPos)
{
	if (!sc->active)
		return 0;

	assert(drawPos >= 0 && drawPos < sc->SLen);
	return (sc->sampleData8[drawPos] * sc->SVol) >> 8;
}

static inline int8_t getScaledScopeSample16(scope_t *sc, int32_t drawPos)
{
	if (!sc->active)
		return 0;

	assert(drawPos >= 0 && drawPos < sc->SLen);
	return (int8_t)((sc->sampleData16[drawPos] * sc->SVol) >> 16);
}

#define SCOPE_UPDATE_DRAWPOS \
	scopeDrawFrac += s.SFrq >> 6; \
	scopeDrawPos += ((scopeDrawFrac >> 16) ^ drawPosXOR); \
	scopeDrawFrac &= 0xFFFF; \
	\
	if (drawPosXOR == 0xFFFFFFFF && scopeDrawPos < s.SRepS) /* sampling backwards (definitely pingpong loop) */ \
	{ \
		drawPosXOR = 0; /* change direction to forwards */ \
		\
		if (s.SRepL < 2) \
			scopeDrawPos = s.SRepS; \
		else \
			scopeDrawPos = s.SRepS + ((s.SRepS - scopeDrawPos - 1) % s.SRepL); \
		\
		assert(scopeDrawPos >= s.SRepS && scopeDrawPos < s.SLen); \
	} \
	else if (scopeDrawPos >= s.SLen) \
	{ \
		if (s.SRepL < 2) \
			loopOverflowVal = 0; \
		else \
			loopOverflowVal = (scopeDrawPos - s.SLen) % s.SRepL; \
		\
		if (s.loopType == LOOP_NONE) \
		{ \
			s.active = false; \
		} \
		else if (s.loopType == LOOP_FORWARD) \
		{ \
			scopeDrawPos = s.SRepS + loopOverflowVal; \
			assert(scopeDrawPos >= s.SRepS && scopeDrawPos < s.SLen); \
		} \
		else /* pingpong loop */ \
		{ \
			drawPosXOR = 0xFFFFFFFF; /* change direction to backwards */ \
			scopeDrawPos = (s.SLen - 1) - loopOverflowVal; \
			assert(scopeDrawPos >= s.SRepS && scopeDrawPos < s.SLen); \
		} \
		\
	} \
	assert(scopeDrawPos >= 0); \

void drawScopes(void)
{
	int16_t y1, y2, sample, scopeLineY;
	const uint16_t *scopeLens;
	uint16_t chansPerRow, x16, scopeXOffs, scopeYOffs, scopeDrawLen;
	int32_t scopeDrawPos, loopOverflowVal;
	uint32_t x, len, drawPosXOR, scopeDrawFrac, scopePixelColor;
	volatile scope_t *sc;
	scope_t s;

	scopesDisplayingFlag = true;
	chansPerRow = song.antChn / 2;

	scopeLens = scopeLenTab[chansPerRow-1];
	scopeXOffs = 3;
	scopeYOffs = 95;
	scopeLineY = 112;

	for (int16_t i = 0; i < song.antChn; i++)
	{
		// if we reached the last scope on the row, go to first scope on the next row
		if (i == chansPerRow)
		{
			scopeXOffs = 3;
			scopeYOffs = 134;
			scopeLineY = 151;
		}

		scopeDrawLen = scopeLens[i];

		if (!editor.chnMode[i]) // scope muted (mute graphics blit()'ed elsewhere)
		{
			scopeXOffs += scopeDrawLen + 3; // align x to next scope
			continue;
		}

		s = scope[i]; // cache scope to lower thread race condition issues
		if (s.active && s.SVol > 0 && !audio.locked)
		{
			// scope is active

			scope[i].wasCleared = false;

			// clear scope background
			clearRect(scopeXOffs, scopeYOffs, scopeDrawLen, SCOPE_HEIGHT);

			scopeDrawPos = s.SPos;
			scopeDrawFrac = 0;
			drawPosXOR = s.posXOR;

			// draw current scope
			if (config.specialFlags & LINED_SCOPES)
			{
				// LINE SCOPE

				if (s.sample16Bit)
				{
					y1 = scopeLineY - getScaledScopeSample16(&s, scopeDrawPos);
					SCOPE_UPDATE_DRAWPOS

					x16 = scopeXOffs;
					len = scopeXOffs + (scopeDrawLen - 1);

					for (; x16 < len; x16++)
					{
						y2 = scopeLineY - getScaledScopeSample16(&s, scopeDrawPos);
						scopeLine(x16, y1, y2);
						y1 = y2;

						SCOPE_UPDATE_DRAWPOS
					}
				}
				else
				{
					y1 = scopeLineY - getScaledScopeSample8(&s, scopeDrawPos);
					SCOPE_UPDATE_DRAWPOS

					x16 = scopeXOffs;
					len = scopeXOffs + (scopeDrawLen - 1);

					for (; x16 < len; x16++)
					{
						y2 = scopeLineY - getScaledScopeSample8(&s, scopeDrawPos);
						scopeLine(x16, y1, y2);
						y1 = y2;

						SCOPE_UPDATE_DRAWPOS
					}
				}
			}
			else
			{
				// PIXEL SCOPE

				scopePixelColor = video.palette[PAL_PATTEXT];

				x = scopeXOffs;
				len = scopeXOffs + scopeDrawLen;

				if (s.sample16Bit)
				{
					for (; x < len; x++)
					{
						sample = getScaledScopeSample16(&s, scopeDrawPos);
						video.frameBuffer[((scopeLineY - sample) * SCREEN_W) + x] = scopePixelColor;

						SCOPE_UPDATE_DRAWPOS
					}
				}
				else
				{
					for (; x < len; x++)
					{
						sample = getScaledScopeSample8(&s, scopeDrawPos);
						video.frameBuffer[((scopeLineY - sample) * SCREEN_W) + x] = scopePixelColor;

						SCOPE_UPDATE_DRAWPOS
					}
				}
			}
		}
		else
		{
			// scope is inactive

			sc = &scope[i];
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
			blit(scopeXOffs, scopeYOffs + 31, scopeRecBMP, 13, 4);

		scopeXOffs += scopeDrawLen + 3; // align x to next scope
	}

	scopesDisplayingFlag = false;
}

void drawScopeFramework(void)
{
	drawFramework(0, 92, 291, 81, FRAMEWORK_TYPE1);
	for (uint8_t i = 0; i < song.antChn; i++)
		redrawScope(i);
}

void handleScopesFromChQueue(chSyncData_t *chSyncData, uint8_t *scopeUpdateStatus)
{
	uint8_t status;
	syncedChannel_t *ch;
	volatile scope_t *sc;
	sampleTyp *smpPtr;

	for (int32_t i = 0; i < song.antChn; i++)
	{
		sc = &scope[i];
		ch = &chSyncData->channels[i];
		status = scopeUpdateStatus[i];

		// set scope volume
		if (status & IS_Vol)
			sc->SVol = (int8_t)(((ch->finalVol * SCOPE_HEIGHT) + (1 << 10)) >> 11); // rounded

		// set scope frequency
		if (status & IS_Period)
		{
			if (ch->voiceDelta != oldVoiceDelta)
			{
				oldVoiceDelta = ch->voiceDelta;
				oldSFrq = (uint32_t)((oldVoiceDelta * audio.dScopeFreqMul) + 0.5); // rounded
			}

			sc->SFrq = oldSFrq;
		}

		// start scope sample
		if (status & IS_NyTon)
		{
			if (instr[ch->instrNr] != NULL)
			{
				smpPtr = &instr[ch->instrNr]->samp[ch->sampleNr];
				scopeTrigger((uint8_t)i, smpPtr, ch->smpStartPos);

				// set stuff used by Smp. Ed. for sampling position line

				if (ch->instrNr == 130 || (ch->instrNr == editor.curInstr && ch->sampleNr == editor.curSmp))
					editor.curSmpChannel = (uint8_t)i;

				lastChInstr[i].instrNr = ch->instrNr;
				lastChInstr[i].sampleNr = ch->sampleNr;
			}
		}
	}
}

static int32_t SDLCALL scopeThreadFunc(void *ptr)
{
	int32_t time32;
	uint32_t diff32;
	uint64_t time64;

	(void)ptr;

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

		time64 = SDL_GetPerformanceCounter();
		if (time64 < timeNext64)
		{
			assert(timeNext64-time64 <= 0xFFFFFFFFULL);
			diff32 = (uint32_t)(timeNext64 - time64);

			// convert to microseconds and round to integer
			time32 = (int32_t)((diff32 * editor.dPerfFreqMulMicro) + 0.5);

			// delay until we have reached next tick
			if (time32 > 0)
				usleep(time32);
		}

		// update next tick time
		timeNext64 += scopeTimeLen;

		timeNext64Frac += scopeTimeLenFrac;
		if (timeNext64Frac >= (1ULL << 32))
		{
			timeNext64++;
			timeNext64Frac &= 0xFFFFFFFF;
		}
	}

	return true;
}

bool initScopes(void)
{
	double dInt, dFrac;

	// calculate scope time for performance counters and split into int/frac
	dFrac = modf(editor.dPerfFreq / SCOPE_HZ, &dInt);

	// integer part
	scopeTimeLen = (uint32_t)dInt;

	// fractional part scaled to 0..2^32-1
	dFrac *= UINT32_MAX + 1.0;
	if (dFrac > (double)UINT32_MAX)
		dFrac = (double)UINT32_MAX;
	scopeTimeLenFrac = (uint32_t)round(dFrac);

	scopeThread = SDL_CreateThread(scopeThreadFunc, NULL, NULL);
	if (scopeThread == NULL)
	{
		showErrorMsgBox("Couldn't create channel scope thread!");
		return false;
	}

	SDL_DetachThread(scopeThread);
	return true;
}
