// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_pattern_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_midi.h"
#include "ft2_scopes.h"
#include "ft2_mouse.h"
#include "ft2_sample_loader.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "mixer/ft2_windowed_sinc.h"

/* This is a mess, directly ported from the original FT2 code (with some modifications).
** You will experience a lot of headaches if you dig into it...
** If something looks to be off, it probably isn't!
*/

static double dLogTab[768], dExp2MulTab[32];
static bool bxxOverflow;
static tonTyp nilPatternLine[MAX_VOICES];

typedef void (*volKolEfxRoutine)(stmTyp *ch);
typedef void (*volKolEfxRoutine2)(stmTyp *ch, uint8_t *volKol);
typedef void (*efxRoutine)(stmTyp *ch, uint8_t param);

// globally accessed
int8_t playMode = 0;
bool songPlaying = false, audioPaused = false, musicPaused = false;
volatile bool replayerBusy = false;
const uint16_t *note2Period = NULL;
int16_t pattLens[MAX_PATTERNS];
stmTyp stm[MAX_VOICES];
songTyp song;
instrTyp *instr[132];
tonTyp *patt[MAX_PATTERNS];
// ----------------------------------

void fixString(char *str, int32_t lastChrPos) // removes leading spaces and 0x1A chars
{
	for (int32_t i = lastChrPos; i >= 0; i--)
	{
		if (str[i] == ' ' || str[i] == 0x1A)
			str[i] = '\0';
		else if (str[i] != '\0')
			break;
	}
	str[lastChrPos+1] = '\0';
}

void fixSongName(void)
{
	fixString(song.name, 19);
}

void fixInstrAndSampleNames(int16_t nr)
{
	fixString(song.instrName[nr], 21);
	if (instr[nr] != NULL)
	{
		sampleTyp *s = instr[nr]->samp;
		for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++)
			fixString(s->name, 21);
	}
}

void resetChannels(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	memset(stm, 0, sizeof (stm));

	stmTyp *ch = stm;
	for (int32_t i = 0; i < MAX_VOICES; i++, ch++)
	{
		ch->instrPtr = instr[0];
		ch->status = IS_Vol;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;

		ch->stOff = !editor.chnMode[i]; // set channel mute flag from global mute flag
	}

	if (audioWasntLocked)
		unlockAudio();
}

void setSongModifiedFlag(void)
{
	song.isModified = true;
	editor.updateWindowTitle = true;
}

void removeSongModifiedFlag(void)
{
	song.isModified = false;
	editor.updateWindowTitle = true;
}

// used on external sample load and during sample loading in some module formats
void tuneSample(sampleTyp *s, const int32_t midCFreq, bool linearPeriodsFlag)
{
	#define NOTE_C4 (4*12)
	#define MIN_PERIOD (0)
	#define MAX_PERIOD (((10*12*16)-1)-1) /* -1 (because of bugged amigaPeriods table values) */

	double (*dGetHzFromPeriod)(int32_t) = linearPeriodsFlag ? dLinearPeriod2Hz : dAmigaPeriod2Hz;
	const uint16_t *periodTab = linearPeriodsFlag ? linearPeriods : amigaPeriods;

	if (midCFreq <= 0 || periodTab == NULL)
	{
		s->fine = s->relTon = 0;
		return;
	}

	// handle frequency boundaries first...

	if (midCFreq <= (int32_t)dGetHzFromPeriod(periodTab[MIN_PERIOD]))
	{
		s->fine = -128;
		s->relTon = -48;
		return;
	}

	if (midCFreq >= (int32_t)dGetHzFromPeriod(periodTab[MAX_PERIOD]))
	{
		s->fine = 127;
		s->relTon = 71;
		return;
	}

	// check if midCFreq is matching any of the non-finetuned note frequencies (C-0..B-9)

	for (int8_t i = 0; i < 10*12; i++)
	{
		if (midCFreq == (int32_t)dGetHzFromPeriod(periodTab[16 + (i<<4)]))
		{
			s->fine = 0;
			s->relTon = i - NOTE_C4;
			return;
		}
	}

	// find closest frequency in period table

	int32_t period = MAX_PERIOD;
	for (; period >= MIN_PERIOD; period--)
	{
		const int32_t curr = (int32_t)dGetHzFromPeriod(periodTab[period]);
		if (midCFreq == curr)
			break;

		if (midCFreq > curr)
		{
			const int32_t next = (int32_t)dGetHzFromPeriod(periodTab[period+1]);
			const int32_t errorCurr = ABS(curr-midCFreq);
			const int32_t errorNext = ABS(next-midCFreq);

			if (errorCurr <= errorNext)
				break; // current is the closest

			period++;
			break; // current+1 is the closest
		}
	}

	s->fine = ((period & 31) - 16) << 3;
	s->relTon = (int8_t)(((period & ~31) >> 4) - NOTE_C4);
}

void setPatternLen(uint16_t nr, int16_t len)
{
	assert(nr < MAX_PATTERNS);
	if ((len < 1 || len > MAX_PATT_LEN) || len == pattLens[nr])
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	pattLens[nr] = len;

	if (patt[nr] != NULL)
		killPatternIfUnused(nr);

	song.pattLen = pattLens[nr];
	if (song.pattPos >= song.pattLen)
	{
		song.pattPos = song.pattLen - 1;
		editor.pattPos = song.pattPos;
	}

	checkMarkLimits();

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;
}

int16_t getUsedSamples(int16_t nr)
{
	if (instr[nr] == NULL)
		return 0;

	instrTyp *ins = instr[nr];

	int16_t i = 16 - 1;
	while (i >= 0 && ins->samp[i].pek == NULL && ins->samp[i].name[0] == '\0')
		i--;

	/* Yes, 'i' can be -1 here, and will be set to at least 0
	** because of ins->ta values. Possibly an FT2 bug...
	*/
	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->ta[j] > i)
			i = ins->ta[j];
	}

	return i+1;
}

int16_t getRealUsedSamples(int16_t nr)
{
	if (instr[nr] == NULL)
		return 0;

	int8_t i = 16 - 1;
	while (i >= 0 && instr[nr]->samp[i].pek == NULL)
		i--;

	return i+1;
}

double dLinearPeriod2Hz(int32_t period)
{
	if (period == 0)
		return 0.0; // in FT2, a period of 0 results in 0Hz

	const uint16_t invPeriod = (12 * 192 * 4) - (uint16_t)period; // intentionally underflows (uint16_t) to be FT2-accurate
	const int32_t quotient = invPeriod / 768;
	const int32_t remainder = invPeriod % 768;

	return dLogTab[remainder] * dExp2MulTab[(14-quotient) & 31]; // x = y / 2^((14-quotient) & 31)
}

double dAmigaPeriod2Hz(int32_t period)
{
	if (period == 0)
		return 0.0; // in FT2, a period of 0 results in 0Hz

	return (double)(8363 * 1712) / period;
}

double dPeriod2Hz(int32_t period)
{
	return audio.linearPeriodsFlag ? dLinearPeriod2Hz(period) : dAmigaPeriod2Hz(period);
}

// returns *exact* FT2 C-4 voice rate (depending on finetune, relative note and linear/Amiga period mode)
double getSampleC4Rate(sampleTyp *s)
{
	int32_t note = (96/2) + s->relTon;
	if (note >= (10*12)-1)
		return -1; // B-9 (from relTon) = illegal! (won't play in replayer)

	const int32_t C4Period = (note << 4) + (((int8_t)s->fine >> 3) + 16);

	const uint16_t period = audio.linearPeriodsFlag ? linearPeriods[C4Period] : amigaPeriods[C4Period];
	return dPeriod2Hz(period);
}

void setFrqTab(bool linear)
{
	pauseAudio();

	audio.linearPeriodsFlag = linear;

	if (audio.linearPeriodsFlag)
		note2Period = linearPeriods;
	else
		note2Period = amigaPeriods;

	resumeAudio();

	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
	{
		// update "frequency table" radiobutton, if it's shown
		setConfigIORadioButtonStates(); 

		// update mid-C freq. in instr. editor (it can slightly differ between Amiga/linear)
		if (ui.instEditorShown)
			drawC4Rate();
	}
}

static void retrigVolume(stmTyp *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= IS_Vol + IS_Pan + IS_QuickVol;
}

static void retrigEnvelopeVibrato(stmTyp *ch)
{
	if (!(ch->waveCtrl & 0x04)) ch->vibPos = 0;
	if (!(ch->waveCtrl & 0x40)) ch->tremPos = 0;

	ch->retrigCnt = 0;
	ch->tremorPos = 0;

	ch->envSustainActive = true;

	instrTyp *ins = ch->instrPtr;
	assert(ins != NULL);

	if (ins->envVTyp & 1)
	{
		ch->envVCnt = 65535;
		ch->envVPos = 0;
	}

	if (ins->envPTyp & 1)
	{
		ch->envPCnt = 65535;
		ch->envPPos = 0;
	}

	ch->fadeOutSpeed = ins->fadeOut; // FT2 doesn't check if fadeout is more than 4095
	ch->fadeOutAmp = 32768;

	if (ins->vibDepth > 0)
	{
		ch->eVibPos = 0;

		if (ins->vibSweep > 0)
		{
			ch->eVibAmp = 0;
			ch->eVibSweep = (ins->vibDepth << 8) / ins->vibSweep;
		}
		else
		{
			ch->eVibAmp = ins->vibDepth << 8;
			ch->eVibSweep = 0;
		}
	}
}

void keyOff(stmTyp *ch)
{
	ch->envSustainActive = false;

	instrTyp *ins = ch->instrPtr;
	assert(ins != NULL);

	if (!(ins->envPTyp & 1)) // yes, FT2 does this (!). Most likely a bug?
	{
		if (ch->envPCnt >= (uint16_t)ins->envPP[ch->envPPos][0])
			ch->envPCnt = ins->envPP[ch->envPPos][0] - 1;
	}

	if (ins->envVTyp & 1)
	{
		if (ch->envVCnt >= (uint16_t)ins->envVP[ch->envVPos][0])
			ch->envVCnt = ins->envVP[ch->envVPos][0] - 1;
	}
	else
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

void calcReplayerLogTab(void)
{
	for (int32_t i = 0; i < 32; i++)
		dExp2MulTab[i] = 1.0 / exp2(i); // 1/2^i

	for (int32_t i = 0; i < 768; i++)
		dLogTab[i] = 8363.0 * 256.0 * exp2(i * (1.0 / 768.0));
}

void calcReplayerVars(int32_t audioFreq)
{
	assert(audioFreq > 0);
	if (audioFreq <= 0)
		return;

	audio.dHz2MixDeltaMul = (double)MIXER_FRAC_SCALE / audioFreq;
	audio.quickVolRampSamples = (int32_t)((audioFreq / 200.0) + 0.5); // rounded
	audio.dRampQuickVolMul = 1.0 / audio.quickVolRampSamples;

	audio.dSamplesPerTickTab[0] = 0.0;
	audio.tickTimeTab[0] = UINT64_MAX;
	audio.dRampTickMulTab[0] = 0.0;

	for (int32_t i = MIN_BPM; i <= MAX_BPM; i++)
	{
		const double dBpmHz = i * (1.0 / 2.5); // i / 2.5
		const double dSamplesPerTick = audioFreq / dBpmHz;
		audio.dSamplesPerTickTab[i] = dSamplesPerTick;

		// BPM Hz -> tick length for performance counter (syncing visuals to audio)
		double dTimeInt;
		double dTimeFrac = modf(editor.dPerfFreq / dBpmHz, &dTimeInt);
		const int32_t timeInt = (int32_t)dTimeInt;

		dTimeFrac = floor((UINT32_MAX+1.0) * dTimeFrac); // fractional part (scaled to 0..2^32-1)

		audio.tickTimeTab[i] = ((uint64_t)timeInt << 32) | (uint32_t)dTimeFrac;

		// for calculating volume ramp length for tick-lenghted ramps
		const int32_t samplesPerTick = (int32_t)(dSamplesPerTick + 0.5); // this has to be rounded first
		audio.dRampTickMulTab[i] = 1.0 / samplesPerTick;
	}
}

int32_t getPianoKey(uint16_t period, int8_t finetune, int8_t relativeNote) // for piano in Instr. Ed.
{
	assert(note2Period != NULL);

	if (period > note2Period[0])
		return -1; // out of lower range on piano

	if (audio.linearPeriodsFlag)
	{
		int32_t note = ((10*12*16*4+64) - (period + 16*2)) >> 2;
		note -= ((int8_t)finetune >> 3) + 16;
		note = ((note >> 4) + 1) - relativeNote;

		return note;
	}

	/* Amiga periods require a slower method...
	** This is not 100% accurate for all periods, but should be faster
	** than using log2() and floating-point arithmetics.
	*/
	finetune = ((int8_t)finetune >> 3) + 16;
	int32_t hiPeriod = (10*12*16)+16;
	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 7; i++)
	{
		const int32_t tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + finetune;

		int32_t lookUp = tmpPeriod - 8;
		if (lookUp < 0)
			lookUp = 0;

		if (period >= note2Period[lookUp])
			hiPeriod = (tmpPeriod - finetune) & ~15;
		else
			loPeriod = (tmpPeriod - finetune) & ~15;
	}

	int32_t note = loPeriod;
	note >>= 4;
	note -= relativeNote;

	return note;
}

static void startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch)
{
	if (ton == 97)
	{
		keyOff(ch);
		return;
	}

	// if we came from Rxy (retrig), we didn't check note (Ton) yet
	if (ton == 0)
	{
		ton = ch->tonNr;
		if (ton == 0)
			return; // if still no note, exit from routine
	}

	ch->tonNr = ton;

	assert(ch->instrNr <= 130);
	instrTyp *ins = instr[ch->instrNr];
	if (ins == NULL)
		ins = instr[0]; // empty instruments use this placeholder instrument

	ch->instrPtr = ins;
	ch->mute = ins->mute;

	if (ton > 96) // non-FT2 security (should never happen because I clamp in the patt. loader now)
		ton = 96;

	const uint8_t smp = ins->ta[ton-1] & 0xF;
	ch->sampleNr = smp;

	sampleTyp *s = &ins->samp[smp];
	ch->smpPtr = s;
	ch->relTonNr = s->relTon;

	ton += ch->relTonNr;
	if (ton >= 10*12)
		return;

	ch->oldVol = s->vol;
	ch->oldPan = s->pan;

	if (effTyp == 0x0E && (eff & 0xF0) == 0x50)
		ch->fineTune = ((eff & 0x0F) << 4) - 128; // result is now -128..127
	else
		ch->fineTune = s->fine;

	if (ton != 0)
	{
		const uint16_t tmpTon = ((ton-1) << 4) + (((int8_t)ch->fineTune >> 3) + 16); // 0..1935
		if (tmpTon < MAX_NOTES) // tmpTon is *always* below MAX_NOTES here, so this check is not really needed
		{
			assert(note2Period != NULL);
			ch->outPeriod = ch->realPeriod = note2Period[tmpTon];
		}
	}

	ch->status |= IS_Period + IS_Vol + IS_Pan + IS_NyTon + IS_QuickVol;

	if (effTyp == 9)
	{
		if (eff)
			ch->smpOffset = ch->eff;

		ch->smpStartPos = ch->smpOffset << 8;
	}
	else
	{
		ch->smpStartPos = 0;
	}
}

static void volume(stmTyp *ch, uint8_t param); // volume slide
static void vibrato2(stmTyp *ch);
static void tonePorta(stmTyp *ch, uint8_t param);

static void dummy(stmTyp *ch, uint8_t param)
{
	(void)ch;
	(void)param;
	return;
}

static void finePortaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaUpSpeed;

	ch->fPortaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void finePortaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaDownSpeed;

	ch->fPortaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod > MAX_FRQ-1) // FT2 bug, should've been unsigned comparison
		ch->realPeriod = MAX_FRQ-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void setGlissCtrl(stmTyp *ch, uint8_t param)
{
	ch->glissFunk = param;
}

static void setVibratoCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (ch->waveCtrl & 0xF0) | param;
}

static void jumpLoop(stmTyp *ch, uint8_t param)
{
	if (param == 0)
	{
		ch->pattPos = song.pattPos & 0xFF;
	}
	else if (ch->loopCnt == 0)
	{
		ch->loopCnt = param;

		song.pBreakPos = ch->pattPos;
		song.pBreakFlag = true;
	}
	else if (--ch->loopCnt > 0)
	{
		song.pBreakPos = ch->pattPos;
		song.pBreakFlag = true;
	}
}

static void setTremoloCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (param << 4) | (ch->waveCtrl & 0x0F);
}

static void volFineUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideUpSpeed;

	ch->fVolSlideUpSpeed = param;

	ch->realVol += param;
	if (ch->realVol > 64)
		ch->realVol = 64;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

static void volFineDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideDownSpeed;

	ch->fVolSlideDownSpeed = param;

	ch->realVol -= param;
	if ((int8_t)ch->realVol < 0)
		ch->realVol = 0;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

static void noteCut0(stmTyp *ch, uint8_t param)
{
	if (param == 0) // only a parameter of zero is handled here
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void pattDelay(stmTyp *ch, uint8_t param)
{
	if (song.pattDelTime2 == 0)
		song.pattDelTime = param + 1;

	(void)ch;
}

static const efxRoutine EJumpTab_TickZero[16] =
{
	dummy, // 0
	finePortaUp, // 1
	finePortaDown, // 2
	setGlissCtrl, // 3
	setVibratoCtrl, // 4
	dummy, // 5
	jumpLoop, // 6
	setTremoloCtrl, // 7
	dummy, // 8
	dummy, // 9
	volFineUp, // A
	volFineDown, // B
	noteCut0, // C
	dummy, // D
	pattDelay, // E
	dummy // F
};

static void E_Effects_TickZero(stmTyp *ch, uint8_t param)
{
	const uint8_t efx = param >> 4;
	param &= 0x0F;

	if (ch->stOff) // channel is muted, only handle some E effects
	{
		     if (efx == 0x6) jumpLoop(ch, param);
		else if (efx == 0xE) pattDelay(ch, param);

		return;
	}

	EJumpTab_TickZero[efx](ch, param);
}

static void posJump(stmTyp *ch, uint8_t param)
{
	if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
	{
		const int16_t pos = (int16_t)param - 1;
		if (pos < 0 || pos >= song.len)
			bxxOverflow = true; // non-FT2 security fix...
		else
			song.songPos = pos;
	}

	song.pBreakPos = 0;
	song.posJumpFlag = true;

	(void)ch;
}

static void pattBreak(stmTyp *ch, uint8_t param)
{
	song.posJumpFlag = true;

	param = ((param >> 4) * 10) + (param & 0x0F);
	if (param <= 63)
		song.pBreakPos = param;
	else
		song.pBreakPos = 0;

	(void)ch;
}

static void setSpeed(stmTyp *ch, uint8_t param)
{
	if (param >= 32)
	{
		song.speed = param;
		P_SetSpeed(song.speed);
	}
	else
	{
		song.timer = song.tempo = param;
	}

	(void)ch;
}

static void setGlobaVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	song.globVol = param;

	stmTyp *c = stm;
	for (int32_t i = 0; i < song.antChn; i++, c++) // update all voice volumes
		c->status |= IS_Vol;

	(void)ch;
}

static void setEnvelopePos(stmTyp *ch, uint8_t param)
{
	bool envUpdate;
	int8_t point;
	int16_t tick;

	instrTyp *ins = ch->instrPtr;
	assert(ins != NULL);

	// *** VOLUME ENVELOPE ***
	if (ins->envVTyp & 1)
	{
		ch->envVCnt = param-1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->envVPAnt > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->envVPAnt-1; i++)
			{
				if (tick < ins->envVP[point][0])
				{
					point--;

					tick -= ins->envVP[point][0];
					if (tick == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->envVP[point+1][0] <= ins->envVP[point][0])
					{
						envUpdate = true;
						break;
					}

					ch->envVIPValue = ((ins->envVP[point+1][1] - ins->envVP[point][1]) << 16) / (ins->envVP[point+1][0] - ins->envVP[point][0]);
					ch->envVAmp = (ch->envVIPValue * (tick-1)) + (ins->envVP[point][1] << 16);

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate)
		{
			ch->envVIPValue = 0;
			ch->envVAmp = ins->envVP[point][1];
		}

		if (point >= ins->envVPAnt)
		{
			point = ins->envVPAnt-1;
			if (point < 0)
				point = 0;
		}

		ch->envVPos = point;
	}

	// *** PANNING ENVELOPE ***
	if (ins->envVTyp & 2) // probably an FT2 bug
	{
		ch->envPCnt = param-1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->envPPAnt > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->envPPAnt-1; i++)
			{
				if (tick < ins->envPP[point][0])
				{
					point--;

					tick -= ins->envPP[point][0];
					if (tick == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->envPP[point+1][0] <= ins->envPP[point][0])
					{
						envUpdate = true;
						break;
					}

					ch->envPIPValue = ((ins->envPP[point+1][1] - ins->envPP[point][1]) << 16) / (ins->envPP[point+1][0] - ins->envPP[point][0]);
					ch->envPAmp = (ch->envPIPValue * (tick-1)) + (ins->envPP[point][1] << 16);

					point++;

					envUpdate = false;
					break;
				}

				point++;
			}

			if (envUpdate)
				point--;
		}

		if (envUpdate)
		{
			ch->envPIPValue = 0;
			ch->envPAmp = ins->envPP[point][1];
		}

		if (point >= ins->envPPAnt)
		{
			point = ins->envPPAnt-1;
			if (point < 0)
				point = 0;
		}

		ch->envPPos = point;
	}
}

static const efxRoutine JumpTab_TickZero[36] =
{
	dummy, // 0
	dummy, // 1
	dummy, // 2
	dummy, // 3
	dummy, // 4
	dummy, // 5
	dummy, // 6
	dummy, // 7
	dummy, // 8
	dummy, // 9
	dummy, // A
	posJump, // B
	dummy, // C
	pattBreak, // D
	E_Effects_TickZero, // E
	setSpeed, // F
	setGlobaVol, // G
	dummy, // H
	dummy, // I
	dummy, // J
	dummy, // K
	setEnvelopePos, // L
	dummy, // M
	dummy, // N
	dummy, // O
	dummy, // P
	dummy, // Q
	dummy, // R
	dummy, // S
	dummy, // T
	dummy, // U
	dummy, // V
	dummy, // W
	dummy, // X
	dummy, // Y
	dummy  // Z
};

static void handleMoreEffects_TickZero(stmTyp *ch) // called even if channel is muted
{
	if (ch->effTyp > 35)
		return;

	JumpTab_TickZero[ch->effTyp](ch, ch->eff);
}

/* -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy command (multiretrig)
*/

static void v_SetVibSpeed(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) << 2;
	if (*volKol != 0)
		ch->vibSpeed = *volKol;
}

static void v_Volume(stmTyp *ch, uint8_t *volKol)
{
	*volKol -= 16;
	if (*volKol > 64) // no idea why FT2 has this check...
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void v_FineSlideDown(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)*volKol < 0)
		*volKol = 0;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

static void v_FineSlideUp(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (*volKol > 64)
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

static void v_SetPan(stmTyp *ch, uint8_t *volKol)
{
	*volKol <<= 4;

	ch->outPan = *volKol;
	ch->status |= IS_Pan;
}

// -- non-tick-zero volume column effects --

static void v_SlideDown(stmTyp *ch)
{
	uint8_t newVol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)newVol < 0)
		newVol = 0;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_SlideUp(stmTyp *ch)
{
	uint8_t newVol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (newVol > 64)
		newVol = 64;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_Vibrato(stmTyp *ch)
{
	const uint8_t param = ch->volKolVol & 0xF;
	if (param > 0)
		ch->vibDepth = param;

	vibrato2(ch);
}

static void v_PanSlideLeft(stmTyp *ch)
{
	uint16_t tmp16 = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->outPan;
	if (tmp16 < 256) // includes an FT2 bug: pan-slide-left of 0 = set pan to 0
		tmp16 = 0;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_PanSlideRight(stmTyp *ch)
{
	uint16_t tmp16 = (ch->volKolVol & 0x0F) + ch->outPan;
	if (tmp16 > 255)
		tmp16 = 255;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_TonePorta(stmTyp *ch)
{
	tonePorta(ch, 0); // the last parameter is actually not used in tonePorta()
}

static void v_dummy(stmTyp *ch)
{
	(void)ch;
	return;
}

static void v_dummy2(stmTyp *ch, uint8_t *volKol)
{
	(void)ch;
	(void)volKol;
	return;
}

static const volKolEfxRoutine VJumpTab_TickNonZero[16] =
{
	v_dummy,        v_dummy,         v_dummy,  v_dummy,
	v_dummy,        v_dummy,     v_SlideDown, v_SlideUp,
	v_dummy,        v_dummy,         v_dummy, v_Vibrato,
	v_dummy, v_PanSlideLeft, v_PanSlideRight, v_TonePorta
};

static const volKolEfxRoutine2 VJumpTab_TickZero[16] =
{
	       v_dummy2,      v_Volume,      v_Volume, v_Volume,
	       v_Volume,      v_Volume,      v_dummy2, v_dummy2,
	v_FineSlideDown, v_FineSlideUp, v_SetVibSpeed, v_dummy2,
	       v_SetPan,      v_dummy2,      v_dummy2, v_dummy2
};

static void setPan(stmTyp *ch, uint8_t param)
{
	ch->outPan = param;
	ch->status |= IS_Pan;
}

static void setVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	ch->outVol = ch->realVol = param;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void xFinePorta(stmTyp *ch, uint8_t param)
{
	const uint8_t type = param >> 4;
	param &= 0x0F;

	if (type == 0x1) // extra fine porta up
	{
		if (param == 0)
			param = ch->ePortaUpSpeed;

		ch->ePortaUpSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod -= param;
		if ((int16_t)newPeriod < 1)
			newPeriod = 1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	}
	else if (type == 0x2) // extra fine porta down
	{
		if (param == 0)
			param = ch->ePortaDownSpeed;

		ch->ePortaDownSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod += param;
		if ((int16_t)newPeriod > MAX_FRQ-1) // FT2 bug, should've been unsigned comparison
			newPeriod = MAX_FRQ-1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	}
}

static void doMultiRetrig(stmTyp *ch, uint8_t param) // "param" is never used (needed for efx jumptable structure)
{
	uint8_t cnt = ch->retrigCnt + 1;
	if (cnt < ch->retrigSpeed)
	{
		ch->retrigCnt = cnt;
		return;
	}

	ch->retrigCnt = 0;

	int16_t vol = ch->realVol;
	switch (ch->retrigVol)
	{
		case 0x1: vol -= 1; break;
		case 0x2: vol -= 2; break;
		case 0x3: vol -= 4; break;
		case 0x4: vol -= 8; break;
		case 0x5: vol -= 16; break;
		case 0x6: vol = (vol >> 1) + (vol >> 3) + (vol >> 4); break;
		case 0x7: vol >>= 1; break;
		case 0x8: break; // does not change the volume
		case 0x9: vol += 1; break;
		case 0xA: vol += 2; break;
		case 0xB: vol += 4; break;
		case 0xC: vol += 8; break;
		case 0xD: vol += 16; break;
		case 0xE: vol = (vol >> 1) + vol; break;
		case 0xF: vol += vol; break;
		default: break;
	}
	vol = CLAMP(vol, 0, 64);

	ch->realVol = (uint8_t)vol;
	ch->outVol = ch->realVol;

	if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50)
	{
		ch->outVol = ch->volKolVol - 0x10;
		ch->realVol = ch->outVol;
	}
	else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF)
	{
		ch->outPan = (ch->volKolVol & 0x0F) << 4;
	}

	startTone(0, 0, 0, ch);

	(void)param;
}

static void multiRetrig(stmTyp *ch, uint8_t param, uint8_t volumeColumnData)
{
	uint8_t tmpParam;

	tmpParam = param & 0x0F;
	if (tmpParam == 0)
		tmpParam = ch->retrigSpeed;

	ch->retrigSpeed = tmpParam;

	tmpParam = param >> 4;
	if (tmpParam == 0)
		tmpParam = ch->retrigVol;

	ch->retrigVol = tmpParam;

	if (volumeColumnData == 0)
		doMultiRetrig(ch, 0); // the second parameter is never used (needed for efx jumptable structure)
}

static void handleEffects_TickZero(stmTyp *ch)
{
	// volume column effects
	uint8_t newVolKol = ch->volKolVol; // manipulated by vol. column effects, then used for multiretrig check (FT2 quirk)
	VJumpTab_TickZero[ch->volKolVol >> 4](ch, &newVolKol);

	// normal effects
	const uint8_t param = ch->eff;
	if (ch->effTyp == 0 && param == 0)
		return; // no effect

	     if (ch->effTyp ==  8) setPan(ch, param);
	else if (ch->effTyp == 12) setVol(ch, param);
	else if (ch->effTyp == 27) multiRetrig(ch, param, newVolKol);
	else if (ch->effTyp == 33) xFinePorta(ch, param);

	handleMoreEffects_TickZero(ch);
}

static void fixTonePorta(stmTyp *ch, const tonTyp *p, uint8_t inst)
{
	if (p->ton > 0)
	{
		if (p->ton == 97)
		{
			keyOff(ch);
		}
		else
		{
			const uint16_t note = (((p->ton-1) + ch->relTonNr) << 4) + (((int8_t)ch->fineTune >> 3) + 16);
			if (note < MAX_NOTES)
			{
				assert(note2Period != NULL);
				ch->wantPeriod = note2Period[note];

				if (ch->wantPeriod == ch->realPeriod)
					ch->portaDir = 0;
				else if (ch->wantPeriod > ch->realPeriod)
					ch->portaDir = 1;
				else
					ch->portaDir = 2;
			}
		}
	}

	if (inst > 0)
	{
		retrigVolume(ch);
		if (p->ton != 97)
			retrigEnvelopeVibrato(ch);
	}
}

static void getNewNote(stmTyp *ch, const tonTyp *p)
{
	ch->volKolVol = p->vol;

	if (ch->effTyp == 0)
	{
		if (ch->eff > 0) // we have an arpeggio running, set period back
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}
	else
	{
		// if we have a vibrato on previous row (ch) that ends at current row (p), set period back
		if ((ch->effTyp == 4 || ch->effTyp == 6) && (p->effTyp != 4 && p->effTyp != 6))
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}

	ch->effTyp = p->effTyp;
	ch->eff = p->eff;
	ch->tonTyp = (p->instr << 8) | p->ton;

	if (ch->stOff) // channel is muted, only handle some effects
	{
		handleMoreEffects_TickZero(ch);
		return;
	}

	// 'inst' var is used for later if checks...
	uint8_t inst = p->instr;
	if (inst > 0)
	{
		if (inst <= MAX_INST)
			ch->instrNr = inst;
		else
			inst = 0;
	}

	bool checkEfx = true;
	if (p->effTyp == 0x0E)
	{
		if (p->eff >= 0xD1 && p->eff <= 0xDF)
			return; // we have a note delay (ED1..EDF)
		else if (p->eff == 0x90)
			checkEfx = false;
	}

	if (checkEfx)
	{
		if ((ch->volKolVol & 0xF0) == 0xF0) // gxx
		{
			const uint8_t volKolParam = ch->volKolVol & 0x0F;
			if (volKolParam > 0)
				ch->portaSpeed = volKolParam << 6;

			fixTonePorta(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->effTyp == 3 || p->effTyp == 5) // 3xx or 5xx
		{
			if (p->effTyp != 5 && p->eff != 0)
				ch->portaSpeed = p->eff << 2;

			fixTonePorta(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->effTyp == 0x14 && p->eff == 0) // K00 (KeyOff - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				retrigVolume(ch);

			handleEffects_TickZero(ch);
			return;
		}

		if (p->ton == 0)
		{
			if (inst > 0)
			{
				retrigVolume(ch);
				retrigEnvelopeVibrato(ch);
			}

			handleEffects_TickZero(ch);
			return;
		}
	}

	if (p->ton == 97)
		keyOff(ch);
	else
		startTone(p->ton, p->effTyp, p->eff, ch);

	if (inst > 0)
	{
		retrigVolume(ch);
		if (p->ton != 97)
			retrigEnvelopeVibrato(ch);
	}

	handleEffects_TickZero(ch);
}

static void fixaEnvelopeVibrato(stmTyp *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	int16_t autoVibVal;
	uint16_t autoVibAmp;
	int32_t envVal;
	double dVol;

	instrTyp *ins = ch->instrPtr;
	assert(ins != NULL);

	// *** FADEOUT ***
	if (!ch->envSustainActive)
	{
		ch->status |= IS_Vol;

		// unsigned clamp + reset
		if (ch->fadeOutAmp >= ch->fadeOutSpeed)
		{
			ch->fadeOutAmp -= ch->fadeOutSpeed;
		}
		else
		{
			ch->fadeOutAmp = 0;
			ch->fadeOutSpeed = 0;
		}
	}

	if (!ch->mute)
	{
		// *** VOLUME ENVELOPE ***
		envVal = 0;
		if (ins->envVTyp & 1)
		{
			envDidInterpolate = false;
			envPos = ch->envVPos;

			if (++ch->envVCnt == ins->envVP[envPos][0])
			{
				ch->envVAmp = ins->envVP[envPos][1] << 16;

				envPos++;
				if (ins->envVTyp & 4)
				{
					envPos--;

					if (envPos == ins->envVRepE)
					{
						if (!(ins->envVTyp & 2) || envPos != ins->envVSust || ch->envSustainActive)
						{
							envPos = ins->envVRepS;
							ch->envVCnt = ins->envVP[envPos][0];
							ch->envVAmp = ins->envVP[envPos][1] << 16;
						}
					}

					envPos++;
				}

				if (envPos < ins->envVPAnt)
				{
					envInterpolateFlag = true;
					if ((ins->envVTyp & 2) && ch->envSustainActive)
					{
						if (envPos-1 == ins->envVSust)
						{
							envPos--;
							ch->envVIPValue = 0;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag)
					{
						ch->envVPos = envPos;

						ch->envVIPValue = 0;
						if (ins->envVP[envPos][0] > ins->envVP[envPos-1][0])
						{
							ch->envVIPValue = ((ins->envVP[envPos][1] - ins->envVP[envPos-1][1]) << 16) / (ins->envVP[envPos][0] - ins->envVP[envPos-1][0]);

							envVal = ch->envVAmp;
							envDidInterpolate = true;
						}
					}
				}
				else
				{
					ch->envVIPValue = 0;
				}
			}

			if (!envDidInterpolate)
			{
				ch->envVAmp += ch->envVIPValue;

				envVal = ch->envVAmp;
				if (envVal > 64<<16)
				{
					if (envVal > 128<<16)
						envVal = 64<<16;
					else
						envVal = 0;

					ch->envVIPValue = 0;
				}
			}

			const int32_t vol = song.globVol * ch->outVol * ch->fadeOutAmp;

			dVol = vol * (1.0 / (64.0 * 64.0 * 32768.0));
			dVol *= (int32_t)envVal * (1.0 / (64.0 * (1 << 16)));

			ch->status |= IS_Vol; // update vol every tick because vol envelope is enabled
		}
		else
		{
			const int32_t vol = song.globVol * ch->outVol * ch->fadeOutAmp;
			dVol = vol * (1.0 / (64.0 * 64.0 * 32768.0));
		}

		if (dVol > 1.0) // shouldn't happen, but just in case...
			dVol = 1.0;

		ch->dFinalVol = dVol;
		ch->finalVol = (uint8_t)(int32_t)((ch->dFinalVol * 255) + 0.5); // 0.0 .. 1.0 -> 0..255 rounded, used for visuals
	}
	else
	{
		ch->dFinalVol = 0.0;
		ch->finalVol = 0;
	}

	// *** PANNING ENVELOPE ***

	envVal = 0;
	if (ins->envPTyp & 1)
	{
		envDidInterpolate = false;
		envPos = ch->envPPos;

		if (++ch->envPCnt == ins->envPP[envPos][0])
		{
			ch->envPAmp = ins->envPP[envPos][1] << 16;

			envPos++;
			if (ins->envPTyp & 4)
			{
				envPos--;

				if (envPos == ins->envPRepE)
				{
					if (!(ins->envPTyp & 2) || envPos != ins->envPSust || ch->envSustainActive)
					{
						envPos = ins->envPRepS;

						ch->envPCnt = ins->envPP[envPos][0];
						ch->envPAmp = ins->envPP[envPos][1] << 16;
					}
				}

				envPos++;
			}

			if (envPos < ins->envPPAnt)
			{
				envInterpolateFlag = true;
				if ((ins->envPTyp & 2) && ch->envSustainActive)
				{
					if (envPos-1 == ins->envPSust)
					{
						envPos--;
						ch->envPIPValue = 0;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag)
				{
					ch->envPPos = envPos;

					ch->envPIPValue = 0;
					if (ins->envPP[envPos][0] > ins->envPP[envPos-1][0])
					{
						ch->envPIPValue = ((ins->envPP[envPos][1] - ins->envPP[envPos-1][1]) << 16) / (ins->envPP[envPos][0] - ins->envPP[envPos-1][0]);

						envVal = ch->envPAmp;
						envDidInterpolate = true;
					}
				}
			}
			else
			{
				ch->envPIPValue = 0;
			}
		}

		if (!envDidInterpolate)
		{
			ch->envPAmp += ch->envPIPValue;

			envVal = ch->envPAmp;
			if (envVal > 64<<16)
			{
				if (envVal > 128<<16)
					envVal = 64<<16;
				else
					envVal = 0;

				ch->envPIPValue = 0;
			}
		}

		envVal -= 32<<16; // center panning envelope value

		const int32_t pan = 128 - ABS(ch->outPan-128);
		const int32_t panAdd = (pan * envVal) >> (16+5);
		ch->finalPan = (uint8_t)CLAMP(ch->outPan + panAdd, 0, 255);

		ch->status |= IS_Pan; // update pan every tick because pan envelope is enabled
	}
	else
	{
		ch->finalPan = ch->outPan;
	}

	// *** AUTO VIBRATO ***
#ifdef HAS_MIDI
	if (ch->midiVibDepth > 0 || ins->vibDepth > 0)
#else
	if (ins->vibDepth > 0)
#endif
	{
		if (ch->eVibSweep > 0)
		{
			autoVibAmp = ch->eVibSweep;
			if (ch->envSustainActive)
			{
				autoVibAmp += ch->eVibAmp;
				if ((autoVibAmp >> 8) > ins->vibDepth)
				{
					autoVibAmp = ins->vibDepth << 8;
					ch->eVibSweep = 0;
				}

				ch->eVibAmp = autoVibAmp;
			}
		}
		else
		{
			autoVibAmp = ch->eVibAmp;
		}

#ifdef HAS_MIDI
		// non-FT2 hack to make modulation wheel work when auto vibrato rate is zero
		if (ch->midiVibDepth > 0 && ins->vibRate == 0)
			ins->vibRate = 0x20;

		autoVibAmp += ch->midiVibDepth;
#endif
		ch->eVibPos += ins->vibRate;

		     if (ins->vibTyp == 1) autoVibVal = (ch->eVibPos > 127) ? 64 : -64; // square
		else if (ins->vibTyp == 2) autoVibVal = (((ch->eVibPos >> 1) + 64) & 127) - 64; // ramp up
		else if (ins->vibTyp == 3) autoVibVal = ((-(ch->eVibPos >> 1) + 64) & 127) - 64; // ramp down
		else autoVibVal = vibSineTab[ch->eVibPos]; // sine

		autoVibVal <<= 2;
		uint16_t tmpPeriod = (autoVibVal * (int16_t)autoVibAmp) >> 16;

		tmpPeriod += ch->outPeriod;
		if (tmpPeriod > MAX_FRQ-1)
			tmpPeriod = 0; // yes, FT2 does this (!)

#ifdef HAS_MIDI
		if (midi.enable)
			tmpPeriod -= ch->midiPitch;
#endif

		ch->finalPeriod = tmpPeriod;
		ch->status |= IS_Period;
	}
	else
	{
		ch->finalPeriod = ch->outPeriod;

#ifdef HAS_MIDI
		if (midi.enable)
		{
			ch->finalPeriod -= ch->midiPitch;
			ch->status |= IS_Period;
		}
#endif
	}
}

// for arpeggio and portamento (semitone-slide mode)
static uint16_t relocateTon(uint16_t period, uint8_t arpNote, stmTyp *ch)
{
	int32_t tmpPeriod;

	const int32_t fineTune = ((int8_t)ch->fineTune >> 3) + 16;

	/* FT2 bug, should've been 10*12*16. Notes above B-7 (95) will have issues.
	** You can only achieve such high notes by having a high relative note setting.
	*/
	int32_t hiPeriod = 8*12*16;

	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 8; i++)
	{
		tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + fineTune;

		int32_t lookUp = tmpPeriod - 8;
		if (lookUp < 0)
			lookUp = 0; // safety fix (C-0 w/ finetune <= -65). This buggy read seems to return 0 in FT2 (TODO: Verify...)

		if (period >= note2Period[lookUp])
			hiPeriod = (tmpPeriod - fineTune) & ~15;
		else
			loPeriod = (tmpPeriod - fineTune) & ~15;
	}

	tmpPeriod = loPeriod + fineTune + (arpNote << 4);
	if (tmpPeriod >= (8*12*16+15)-1) // FT2 bug, should've been 10*12*16+16 (also notice the +2 difference)
		tmpPeriod = (8*12*16+16)-1;

	return note2Period[tmpPeriod];
}

static void vibrato2(stmTyp *ch)
{
	uint8_t tmpVib = (ch->vibPos >> 2) & 0x1F;

	switch (ch->waveCtrl & 3)
	{
		// 0: sine
		case 0: tmpVib = vibTab[tmpVib]; break;

		// 1: ramp
		case 1:
		{
			tmpVib <<= 3;
			if ((int8_t)ch->vibPos < 0)
				tmpVib = ~tmpVib;
		}
		break;

		// 2/3: square
		default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibDepth) >> 5; // logical shift (unsigned calc.), not arithmetic shift

	if ((int8_t)ch->vibPos < 0)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= IS_Period;
	ch->vibPos += ch->vibSpeed;
}

static void arp(stmTyp *ch, uint8_t param)
{
	uint8_t note;

	const uint8_t tick = arpTab[song.timer & 0xFF]; // non-FT2 protection (we have 248 extra overflow bytes in LUT, but not more!)
	if (tick == 0)
	{
		ch->outPeriod = ch->realPeriod;
	}
	else
	{
		if (tick == 1)
			note = param >> 4;
		else
			note = param & 0x0F; // tick 2

		ch->outPeriod = relocateTon(ch->realPeriod, note, ch);
	}

	ch->status |= IS_Period;
}

static void portaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaUpSpeed;

	ch->portaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void portaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaDownSpeed;

	ch->portaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod > MAX_FRQ-1) // FT2 bug, should've been unsigned comparison
		ch->realPeriod = MAX_FRQ-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void tonePorta(stmTyp *ch, uint8_t param)
{
	if (ch->portaDir == 0)
		return;

	if (ch->portaDir > 1)
	{
		ch->realPeriod -= ch->portaSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->wantPeriod)
		{
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}
	else
	{
		ch->realPeriod += ch->portaSpeed;
		if (ch->realPeriod >= ch->wantPeriod)
		{
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}

	if (ch->glissFunk) // semitone-slide flag
		ch->outPeriod = relocateTon(ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= IS_Period;

	(void)param;
}

static void vibrato(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;

	if (ch->eff > 0)
	{
		tmp8 = param & 0x0F;
		if (tmp8 > 0)
			ch->vibDepth = tmp8;

		tmp8 = (param & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->vibSpeed = tmp8;
	}

	vibrato2(ch);
}

static void tonePlusVol(stmTyp *ch, uint8_t param)
{
	tonePorta(ch, 0); // the last parameter is actually not used in tonePorta()
	volume(ch, param);

	(void)param;
}

static void vibratoPlusVol(stmTyp *ch, uint8_t param)
{
	vibrato2(ch);
	volume(ch, param);

	(void)param;
}

static void tremolo(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;
	int16_t tremVol;

	const uint8_t tmpEff = param;
	if (tmpEff > 0)
	{
		tmp8 = tmpEff & 0x0F;
		if (tmp8 > 0)
			ch->tremDepth = tmp8;

		tmp8 = (tmpEff & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->tremSpeed = tmp8;
	}

	uint8_t tmpTrem = (ch->tremPos >> 2) & 0x1F;
	switch ((ch->waveCtrl >> 4) & 3)
	{
		// 0: sine
		case 0: tmpTrem = vibTab[tmpTrem]; break;

		// 1: ramp
		case 1:
		{
			tmpTrem <<= 3;
			if ((int8_t)ch->vibPos < 0) // FT2 bug, should've been ch->tremPos
				tmpTrem = ~tmpTrem;
		}
		break;

		// 2/3: square
		default: tmpTrem = 255; break;
	}
	tmpTrem = (tmpTrem * ch->tremDepth) >> 6; // logical shift (unsigned calc.), not arithmetic shift

	if ((int8_t)ch->tremPos < 0)
	{
		tremVol = ch->realVol - tmpTrem;
		if (tremVol < 0)
			tremVol = 0;
	}
	else
	{
		tremVol = ch->realVol + tmpTrem;
		if (tremVol > 64)
			tremVol = 64;
	}

	ch->outVol = (uint8_t)tremVol;
	ch->status |= IS_Vol;
	ch->tremPos += ch->tremSpeed;
}

static void volume(stmTyp *ch, uint8_t param) // volume slide
{
	if (param == 0)
		param = ch->volSlideSpeed;

	ch->volSlideSpeed = param;

	uint8_t newVol = ch->realVol;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void globalVolSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->globVolSlideSpeed;

	ch->globVolSlideSpeed = param;

	uint8_t newVol = (uint8_t)song.globVol;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	song.globVol = newVol;

	stmTyp *c = stm;
	for (int32_t i = 0; i < song.antChn; i++, c++) // update all voice volumes
		c->status |= IS_Vol;
}

static void keyOffCmd(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == (param & 31))
		keyOff(ch);
}

static void panningSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->panningSlideSpeed;

	ch->panningSlideSpeed = param;

	int16_t newPan = (int16_t)ch->outPan;
	if ((param & 0xF0) == 0)
	{
		newPan -= param;
		if (newPan < 0)
			newPan = 0;
	}
	else
	{
		param >>= 4;

		newPan += param;
		if (newPan > 255)
			newPan = 255;
	}

	ch->outPan = (uint8_t)newPan;
	ch->status |= IS_Pan;
}

static void tremor(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->tremorSave;

	ch->tremorSave = param;

	uint8_t tremorSign = ch->tremorPos & 0x80;
	uint8_t tremorData = ch->tremorPos & 0x7F;

	tremorData--;
	if ((int8_t)tremorData < 0)
	{
		if (tremorSign == 0x80)
		{
			tremorSign = 0x00;
			tremorData = param & 0x0F;
		}
		else
		{
			tremorSign = 0x80;
			tremorData = param >> 4;
		}
	}

	ch->tremorPos = tremorSign | tremorData;
	ch->outVol = (tremorSign == 0x80) ? ch->realVol : 0;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void retrigNote(stmTyp *ch, uint8_t param)
{
	if (param == 0) // E9x with a param of zero is handled in getNewNote()
		return;

	if ((song.tempo-song.timer) % param == 0)
	{
		startTone(0, 0, 0, ch);
		retrigEnvelopeVibrato(ch);
	}
}

static void noteCut(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == param)
	{
		ch->outVol = ch->realVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void noteDelay(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == param)
	{
		startTone(ch->tonTyp & 0xFF, 0, 0, ch);

		if ((ch->tonTyp & 0xFF00) > 0)
			retrigVolume(ch);

		retrigEnvelopeVibrato(ch);

		if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50)
		{
			ch->outVol = ch->volKolVol - 16;
			ch->realVol = ch->outVol;
		}
		else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF)
		{
			ch->outPan = (ch->volKolVol & 0x0F) << 4;
		}
	}
}

static const efxRoutine EJumpTab_TickNonZero[16] =
{
	dummy, // 0
	dummy, // 1
	dummy, // 2
	dummy, // 3
	dummy, // 4
	dummy, // 5
	dummy, // 6
	dummy, // 7
	dummy, // 8
	retrigNote, // 9
	dummy, // A
	dummy, // B
	noteCut, // C
	noteDelay, // D
	dummy, // E
	dummy // F
};

static void E_Effects_TickNonZero(stmTyp *ch, uint8_t param)
{
	EJumpTab_TickNonZero[param >> 4](ch, param & 0xF);
}

static const efxRoutine JumpTab_TickNonZero[36] =
{
	arp, // 0
	portaUp, // 1
	portaDown, // 2
	tonePorta, // 3
	vibrato, // 4
	tonePlusVol, // 5
	vibratoPlusVol, // 6
	tremolo, // 7
	dummy, // 8
	dummy, // 9
	volume, // A
	dummy, // B
	dummy, // C
	dummy, // D
	E_Effects_TickNonZero, // E
	dummy, // F
	dummy, // G
	globalVolSlide, // H
	dummy, // I
	dummy, // J
	keyOffCmd, // K
	dummy, // L
	dummy, // M
	dummy, // N
	dummy, // O
	panningSlide, // P
	dummy, // Q
	doMultiRetrig, // R
	dummy, // S
	tremor, // T
	dummy, // U
	dummy, // V
	dummy, // W
	dummy, // X
	dummy, // Y
	dummy  // Z
};

static void handleEffects_TickNonZero(stmTyp *ch)
{
	if (ch->stOff)
		return; // muted

	// volume column effects
	VJumpTab_TickNonZero[ch->volKolVol >> 4](ch);

	// normal effects
	if ((ch->eff == 0 && ch->effTyp == 0) || ch->effTyp > 35)
		return; // no effect

	JumpTab_TickNonZero[ch->effTyp](ch, ch->eff);
}

static void getNextPos(void)
{
	if (song.timer != 1)
		return;

	song.pattPos++;

	if (song.pattDelTime > 0)
	{
		song.pattDelTime2 = song.pattDelTime;
		song.pattDelTime = 0;
	}

	if (song.pattDelTime2 > 0)
	{
		song.pattDelTime2--;
		if (song.pattDelTime2 > 0)
			song.pattPos--;
	}

	if (song.pBreakFlag)
	{
		song.pBreakFlag = false;
		song.pattPos = song.pBreakPos;
	}

	if (song.pattPos >= song.pattLen || song.posJumpFlag)
	{
		song.pattPos = song.pBreakPos;
		song.pBreakPos = 0;
		song.posJumpFlag = false;

		if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
		{
			if (bxxOverflow)
			{
				song.songPos = 0;
				bxxOverflow = false;
			}
			else if (++song.songPos >= song.len)
			{
				editor.wavReachedEndFlag = true;
				song.songPos = song.repS;
			}

			assert(song.songPos <= 255);
			song.pattNr = song.songTab[song.songPos & 0xFF];
			song.pattLen = pattLens[song.pattNr & 0xFF];
		}
	}
}

void pauseMusic(void) // stops reading pattern data
{
	musicPaused = true;
	while (replayerBusy);
}

void resumeMusic(void) // starts reading pattern data
{
	musicPaused = false;
}


void tickReplayer(void) // periodically called from audio callback
{
	int32_t i;
	stmTyp *c;

	if (musicPaused || !songPlaying)
	{
		c = stm;
		for (i = 0; i < song.antChn; i++, c++)
			fixaEnvelopeVibrato(c);

		return;
	}

	// for song playback counter (hh:mm:ss)
	if (song.speed >= MIN_BPM && song.speed <= MAX_BPM)
		song.musicTime64 += musicTimeTab64[song.speed];

	bool tickZero = false;
	if (--song.timer == 0)
	{
		song.timer = song.tempo;
		tickZero = true;
	}

	song.curReplayerTimer = (uint8_t)song.timer; // for audio/video syncing (and recording)

	const bool readNewNote = tickZero && (song.pattDelTime2 == 0);
	if (readNewNote)
	{
		// set audio/video syncing variables
		song.curReplayerPattPos = (uint8_t)song.pattPos;
		song.curReplayerPattNr = (uint8_t)song.pattNr;
		song.curReplayerSongPos = (uint8_t)song.songPos;
		// ----------------------------------------------

		const tonTyp *pattPtr = nilPatternLine;
		if (patt[song.pattNr] != NULL)
		{
			assert(song.pattNr  >= 0 && song.pattNr  < MAX_PATTERNS &&
			       song.pattPos >= 0 && song.pattPos < MAX_PATT_LEN);

			pattPtr = &patt[song.pattNr][song.pattPos * MAX_VOICES];
		}

		c = stm;
		for (i = 0; i < song.antChn; i++, c++, pattPtr++)
		{
			getNewNote(c, pattPtr);
			fixaEnvelopeVibrato(c);
		}
	}
	else
	{
		c = stm;
		for (i = 0; i < song.antChn; i++, c++)
		{
			handleEffects_TickNonZero(c);
			fixaEnvelopeVibrato(c);
		}
	}

	getNextPos();
}

void resetMusic(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.timer = 1;
	stopVoices();

	if (audioWasntLocked)
		unlockAudio();

	setPos(0, 0, false);

	if (!songPlaying)
	{
		setScrollBarEnd(SB_POS_ED, (song.len - 1) + 5);
		setScrollBarPos(SB_POS_ED, 0, false);
	}
}

void setPos(int16_t songPos, int16_t pattPos, bool resetTimer)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPos > -1)
	{
		song.songPos = songPos;
		if (song.len > 0 && song.songPos >= song.len)
			song.songPos = song.len - 1;

		song.pattNr = song.songTab[song.songPos];
		assert(song.pattNr < MAX_PATTERNS);
		song.pattLen = pattLens[song.pattNr];

		checkMarkLimits(); // non-FT2 safety
	}

	if (pattPos > -1)
	{
		song.pattPos = pattPos;
		if (song.pattPos >= song.pattLen)
			song.pattPos = song.pattLen-1;
	}

	// if not playing, update local position variables
	if (!songPlaying)
	{
		if (pattPos > -1)
		{
			editor.pattPos = (uint8_t)pattPos;
			ui.updatePatternEditor = true;
		}

		if (songPos > -1)
		{
			editor.editPattern = (uint8_t)song.pattNr;
			editor.songPos = song.songPos;
			ui.updatePosSections = true;
		}
	}

	if (resetTimer)
		song.timer = 1;

	if (audioWasntLocked)
		unlockAudio();
}

void delta2Samp(int8_t *p, int32_t len, uint8_t typ)
{
	if (typ & 16) len >>= 1; // 16-bit
	if (typ & 32) len >>= 1; // stereo

	if (typ & 32)
	{
		if (typ & 16)
		{
			int16_t *p16 = (int16_t *)p;

			int16_t olds16L = 0;
			int16_t olds16R = 0;

			for (int32_t i = 0; i < len; i++)
			{
				const int16_t news16L = p16[i] + olds16L;
				p16[i] = news16L;
				olds16L = news16L;

				const int16_t news16R = p16[len+i] + olds16R;
				p16[len+i] = news16R;
				olds16R = news16R;

				const int32_t tmp32 = olds16L + olds16R;
				p16[i] = (int16_t)(tmp32 >> 1);
			}
		}
		else
		{
			int8_t *p8 = (int8_t *)p;

			int8_t olds8L = 0;
			int8_t olds8R = 0;

			for (int32_t i = 0; i < len; i++)
			{
				const int8_t news8L = p8[i] + olds8L;
				p8[i] = news8L;
				olds8L = news8L;

				const int8_t news8R = p8[len+i] + olds8R;
				p8[len+i] = news8R;
				olds8R = news8R;

				const int16_t tmp16 = olds8L + olds8R;
				p8[i] = (int8_t)(tmp16 >> 1);
			}
		}
	}
	else
	{
		if (typ & 16)
		{
			int16_t *p16 = (int16_t *)p;

			int16_t olds16L = 0;
			for (int32_t i = 0; i < len; i++)
			{
				const int16_t news16 = p16[i] + olds16L;
				p16[i] = news16;
				olds16L = news16;
			}
		}
		else
		{
			int8_t *p8 = (int8_t *)p;

			int8_t olds8L = 0;
			for (int32_t i = 0; i < len; i++)
			{
				const int8_t news8 = p8[i] + olds8L;
				p8[i] = news8;
				olds8L = news8;
			}
		}
	}
}

void samp2Delta(int8_t *p, int32_t len, uint8_t typ)
{
	if (typ & 16)
		len >>= 1; // 16-bit

	if (typ & 16)
	{
		int16_t *p16 = (int16_t *)p;

		int16_t news16 = 0;
		for (int32_t i = 0; i < len; i++)
		{
			const int16_t olds16 = p16[i];
			p16[i] -= news16;
			news16 = olds16;
		}
	}
	else
	{
		int8_t *p8 = (int8_t *)p;

		int8_t news8 = 0;
		for (int32_t i = 0; i < len; i++)
		{
			const int8_t olds8 = p8[i];
			p8[i] -= news8;
			news8 = olds8;
		}
	}
}

bool allocateInstr(int16_t nr)
{
	if (instr[nr] != NULL)
		return false; // already allocated

	instrTyp *p = (instrTyp *)malloc(sizeof (instrTyp));
	if (p == NULL)
		return false;

	memset(p, 0, sizeof (instrTyp));
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		p->samp[i].pan = 128;
		p->samp[i].vol = 64;
	}

	setStdEnvelope(p, 0, 3);

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	instr[nr] = p;

	if (audioWasntLocked)
		unlockAudio();

	return true;
}

void freeInstr(int32_t nr)
{
	if (instr[nr] == NULL)
		return; // not allocated

	pauseAudio(); // channel instrument pointers are now cleared

	sampleTyp *s = instr[nr]->samp;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++) // free sample data
	{
		if (s->origPek != NULL)
			free(s->origPek);
	}

	free(instr[nr]);
	instr[nr] = NULL;
	
	resumeAudio();
}

void freeAllInstr(void)
{
	pauseAudio(); // channel instrument pointers are now cleared
	for (int32_t i = 1; i <= MAX_INST; i++)
	{
		if (instr[i] != NULL)
		{
			sampleTyp *s = instr[i]->samp;
			for (int32_t j = 0; j < MAX_SMP_PER_INST; j++, s++) // free sample data
			{
				if (s->origPek != NULL)
					free(s->origPek);
			}

			free(instr[i]);
			instr[i] = NULL;
		}
	}
	resumeAudio();
}

void freeSample(int16_t nr, int16_t nr2)
{
	if (instr[nr] == NULL)
		return; // instrument not allocated

	pauseAudio(); // voice sample pointers are now cleared

	sampleTyp *s = &instr[nr]->samp[nr2];
	if (s->origPek != NULL)
		free(s->origPek);

	memset(s, 0, sizeof (sampleTyp));

	s->pan = 128;
	s->vol = 64;

	resumeAudio();
}

void freeAllPatterns(void)
{
	pauseAudio();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (patt[i] != NULL)
		{
			free(patt[i]);
			patt[i] = NULL;
		}
	}
	resumeAudio();
}

void setStdEnvelope(instrTyp *ins, int16_t i, uint8_t typ)
{
	if (ins == NULL)
		return;

	pauseMusic();

	if (typ & 1)
	{
		memcpy(ins->envVP, config.stdEnvP[i][0], 2*2*12);
		ins->envVPAnt = (uint8_t)config.stdVolEnvAnt[i];
		ins->envVSust = (uint8_t)config.stdVolEnvSust[i];
		ins->envVRepS = (uint8_t)config.stdVolEnvRepS[i];
		ins->envVRepE = (uint8_t)config.stdVolEnvRepE[i];
		ins->fadeOut = config.stdFadeOut[i];
		ins->vibRate = (uint8_t)config.stdVibRate[i];
		ins->vibDepth = (uint8_t)config.stdVibDepth[i];
		ins->vibSweep = (uint8_t)config.stdVibSweep[i];
		ins->vibTyp = (uint8_t)config.stdVibTyp[i];
		ins->envVTyp = (uint8_t)config.stdVolEnvTyp[i];
	}

	if (typ & 2)
	{
		memcpy(ins->envPP, config.stdEnvP[i][1], 2*2*12);
		ins->envPPAnt = (uint8_t)config.stdPanEnvAnt[0];
		ins->envPSust = (uint8_t)config.stdPanEnvSust[0];
		ins->envPRepS = (uint8_t)config.stdPanEnvRepS[0];
		ins->envPRepE = (uint8_t)config.stdPanEnvRepE[0];
		ins->envPTyp  = (uint8_t)config.stdPanEnvTyp[0];
	}

	resumeMusic();
}

void setNoEnvelope(instrTyp *ins)
{
	if (ins == NULL)
		return;

	pauseMusic();

	memcpy(ins->envVP, config.stdEnvP[0][0], 2*2*12);
	ins->envVPAnt = (uint8_t)config.stdVolEnvAnt[0];
	ins->envVSust = (uint8_t)config.stdVolEnvSust[0];
	ins->envVRepS = (uint8_t)config.stdVolEnvRepS[0];
	ins->envVRepE = (uint8_t)config.stdVolEnvRepE[0];
	ins->envVTyp = 0;

	memcpy(ins->envPP, config.stdEnvP[0][1], 2*2*12);
	ins->envPPAnt = (uint8_t)config.stdPanEnvAnt[0];
	ins->envPSust = (uint8_t)config.stdPanEnvSust[0];
	ins->envPRepS = (uint8_t)config.stdPanEnvRepS[0];
	ins->envPRepE = (uint8_t)config.stdPanEnvRepE[0];
	ins->envPTyp = 0;

	ins->fadeOut = 0;
	ins->vibRate = 0;
	ins->vibDepth = 0;
	ins->vibSweep = 0;
	ins->vibTyp = 0;

	resumeMusic();
}

bool patternEmpty(uint16_t nr)
{
	if (patt[nr] == NULL)
		return true;

	const uint8_t *scanPtr = (const uint8_t *)patt[nr];
	const uint32_t scanLen = pattLens[nr] * TRACK_WIDTH;

	for (uint32_t i = 0; i < scanLen; i++)
	{
		if (scanPtr[i] != 0)
			return false;
	}

	return true;
}

void updateChanNums(void)
{
	assert(!(song.antChn & 1));

	const int32_t maxChannelsShown = getMaxVisibleChannels();

	int32_t channelsShown = song.antChn;
	if (channelsShown > maxChannelsShown)
		channelsShown = maxChannelsShown;

	ui.numChannelsShown = (uint8_t)channelsShown;
	ui.pattChanScrollShown = (song.antChn > maxChannelsShown);

	if (ui.patternEditorShown)
	{
		if (ui.channelOffset > song.antChn-ui.numChannelsShown)
			setScrollBarPos(SB_CHAN_SCROLL, song.antChn - ui.numChannelsShown, true);
	}

	if (ui.pattChanScrollShown)
	{
		if (ui.patternEditorShown)
		{
			showScrollBar(SB_CHAN_SCROLL);
			showPushButton(PB_CHAN_SCROLL_LEFT);
			showPushButton(PB_CHAN_SCROLL_RIGHT);
		}

		setScrollBarEnd(SB_CHAN_SCROLL, song.antChn);
		setScrollBarPageLength(SB_CHAN_SCROLL, ui.numChannelsShown);
	}
	else
	{
		hideScrollBar(SB_CHAN_SCROLL);
		hidePushButton(PB_CHAN_SCROLL_LEFT);
		hidePushButton(PB_CHAN_SCROLL_RIGHT);

		setScrollBarPos(SB_CHAN_SCROLL, 0, false);

		ui.channelOffset = 0;
	}

	if (cursor.ch >= ui.channelOffset+ui.numChannelsShown)
		cursor.ch = ui.channelOffset+ui.numChannelsShown - 1;
}

void conv8BitSample(int8_t *p, int32_t len, bool stereo)
{
	if (stereo)
	{
		len /= 2;

		int8_t *p2 = &p[len];
		for (int32_t i = 0; i < len; i++)
		{
			const int8_t l = p[i] ^ 0x80;
			const int8_t r = p2[i] ^ 0x80;

			int16_t tmp16 = l + r;
			p[i] = (int8_t)(tmp16 >> 1);
		}
	}
	else
	{
		for (int32_t i = 0; i < len; i++)
			p[i] ^= 0x80;
	}
}

void conv16BitSample(int8_t *p, int32_t len, bool stereo)
{
	int16_t *p16_1 = (int16_t *)p;
	len /= 2;

	if (stereo)
	{
		len /= 2;

		int16_t *p16_2 = (int16_t *)&p[len * 2];
		for (int32_t i = 0; i < len; i++)
		{
			const int16_t l = p16_1[i] ^ 0x8000;
			const int16_t r = p16_2[i] ^ 0x8000;

			int32_t tmp32 = l + r;
			p16_1[i] = (int16_t)(tmp32 >> 1);
		}
	}
	else
	{
		for (int32_t i = 0; i < len; i++)
			p16_1[i] ^= 0x8000;
	}
}

void closeReplayer(void)
{
	freeAllInstr();
	freeAllPatterns();

	// free reserved instruments

	if (instr[0] != NULL)
	{
		free(instr[0]);
		instr[0] = NULL;
	}

	if (instr[130] != NULL)
	{
		free(instr[130]);
		instr[130] = NULL;
	}

	if (instr[131] != NULL)
	{
		free(instr[131]);
		instr[131] = NULL;
	}

	freeWindowedSincTables();
}

bool setupReplayer(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		pattLens[i] = 64;

	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	// unmute all channels (must be done before resetChannels() call)
	for (int32_t i = 0; i < MAX_VOICES; i++)
		editor.chnMode[i] = 1;

	resetChannels();

	song.len = 1;
	song.antChn = 8;
	editor.speed = song.speed = 125;
	editor.tempo = song.tempo = 6;
	editor.globalVol = song.globVol = 64;
	song.initialTempo = song.tempo;
	audio.linearPeriodsFlag = true;
	note2Period = linearPeriods;

	if (!calcWindowedSincTables())
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	calcPanningTable();

	setPos(0, 0, true);

	if (!allocateInstr(0))
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}
	instr[0]->samp[0].vol = 0;

	if (!allocateInstr(130))
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}
	memset(instr[130], 0, sizeof (instrTyp));

	if (!allocateInstr(131)) // Instr. Ed. display instrument for unallocated/empty instruments
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	memset(instr[131], 0, sizeof (instrTyp));
	for (int32_t i = 0; i < 16; i++)
		instr[131]->samp[i].pan = 128;

	editor.tmpPattern = 65535; // pattern editor update/redraw kludge
	return true;
}

void startPlaying(int8_t mode, int16_t row)
{
	lockMixerCallback();

	assert(mode != PLAYMODE_IDLE && mode != PLAYMODE_EDIT);
	if (mode == PLAYMODE_PATT || mode == PLAYMODE_RECPATT)
		setPos(-1, row, true);
	else
		setPos(editor.songPos, row, true);

	playMode = mode;
	songPlaying = true;

	resetReplayerState();
	resetPlaybackTime();

	// non-FT2 fix: If song speed was 0, set it back to initial speed on play
	if (song.tempo == 0)
		song.tempo = song.initialTempo;

	audio.dTickSampleCounter = 0.0; // zero tick sample counter so that it will instantly initiate a tick

	unlockMixerCallback();

	ui.updatePosSections = true;
	ui.updatePatternEditor = true;
}

void stopPlaying(void)
{
	bool songWasPlaying = songPlaying;
	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	if (config.killNotesOnStopPlay)
	{
		// safely kills all voices
		lockMixerCallback();
		unlockMixerCallback();
	}
	else
	{
		for (uint8_t i = 0; i < MAX_VOICES; i++)
			playTone(i, 0, 97, -1, 0, 0);
	}

	// if song was playing, update local pattPos (fixes certain glitches)
	if (songWasPlaying)
		editor.pattPos = song.pattPos;

#ifdef HAS_MIDI
	midi.currMIDIVibDepth = 0;
	midi.currMIDIPitch = 0;
#endif

	memset(editor.keyOnTab, 0, sizeof (editor.keyOnTab));

	ui.updatePosSections = true;
	ui.updatePatternEditor = true;

	// certain non-FT2 fixes
	song.timer = editor.timer = 1;
	song.globVol = editor.globalVol = 64;
	ui.drawGlobVolFlag = true;
}

// from keyboard/smp. ed.
void playTone(uint8_t stmm, uint8_t inst, uint8_t ton, int8_t vol, uint16_t midiVibDepth, uint16_t midiPitch)
{
	instrTyp *ins = instr[inst];
	if (ins == NULL)
		return;

	assert(stmm < MAX_VOICES && inst <= MAX_INST && ton <= 97);
	stmTyp *ch = &stm[stmm];

	// FT2 bugfix: Don't play tone if certain requirements are not met
	if (ton != 97)
	{
		if (ton == 0 || ton > 96)
			return;

		sampleTyp *s = &ins->samp[ins->ta[ton-1] & 0xF];

		int16_t newTon = (int16_t)ton + s->relTon;
		if (s->pek == NULL || s->len == 0 || newTon <= 0 || newTon >= 12*10)
			return;
	}
	// -------------------

	lockAudio();

	if (inst != 0 && ton != 97)
	{
		ch->tonTyp = (inst << 8) | (ch->tonTyp & 0xFF);
		ch->instrNr = inst;
	}

	ch->tonTyp = (ch->tonTyp & 0xFF00) | ton;
	ch->effTyp = 0;
	ch->eff = 0;

	startTone(ton, 0, 0, ch);

	if (ton != 97)
	{
		retrigVolume(ch);
		retrigEnvelopeVibrato(ch);

		if (vol != -1) // if jamming note keys, vol -1 = use sample's volume
		{
			ch->realVol = vol;
			ch->outVol = vol;
			ch->oldVol = vol;
		}
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	fixaEnvelopeVibrato(ch);

	unlockAudio();
}

// smp. ed.
void playSample(uint8_t stmm, uint8_t inst, uint8_t smpNr, uint8_t ton, uint16_t midiVibDepth, uint16_t midiPitch)
{
	if (instr[inst] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[stmm].instrNr = 255;
	lastChInstr[stmm].sampleNr = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(stmm < MAX_VOICES && inst <= MAX_INST && smpNr < MAX_SMP_PER_INST && ton <= 97);
	stmTyp *ch = &stm[stmm];

	memcpy(&instr[130]->samp[0], &instr[inst]->samp[smpNr], sizeof (sampleTyp));

	uint8_t vol = instr[inst]->samp[smpNr].vol;
	
	lockAudio();

	ch->instrNr = 130;
	ch->tonTyp = (ch->instrNr << 8) | ton;
	ch->effTyp = 0;

	startTone(ton, 0, 0, ch);

	if (ton != 97)
	{
		retrigVolume(ch);
		retrigEnvelopeVibrato(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	fixaEnvelopeVibrato(ch);

	unlockAudio();

	while (ch->status & IS_NyTon); // wait for sample to latch in mixer

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = editor.curInstr;
	editor.curPlaySmp = editor.curSmp;
}

// smp. ed.
void playRange(uint8_t stmm, uint8_t inst, uint8_t smpNr, uint8_t ton, uint16_t midiVibDepth, uint16_t midiPitch, int32_t offs, int32_t len)
{
	if (instr[inst] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[stmm].instrNr = 255;
	lastChInstr[stmm].sampleNr = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(stmm < MAX_VOICES && inst <= MAX_INST && smpNr < MAX_SMP_PER_INST && ton <= 97);

	stmTyp *ch = &stm[stmm];
	sampleTyp *s = &instr[130]->samp[0];

	memcpy(s, &instr[inst]->samp[smpNr], sizeof (sampleTyp));

	uint8_t vol = instr[inst]->samp[smpNr].vol;

	if (s->typ & 16)
	{
		offs &= 0xFFFFFFFE;
		len &= 0xFFFFFFFE;
	}

	lockAudio();

	s->len = offs + len;
	s->repS = 0;
	s->repL = 0;
	s->typ &= 16; // only keep 8-bit/16-bit flag (disable loop)

	int32_t samplePlayOffset = offs;
	if (s->typ & 16)
		samplePlayOffset >>= 1;

	ch->instrNr = 130;
	ch->tonTyp = (ch->instrNr << 8) | ton;
	ch->effTyp = 0;

	startTone(ton, 0, 0, ch);

	ch->smpStartPos = samplePlayOffset;

	if (ton != 97)
	{
		retrigVolume(ch);
		retrigEnvelopeVibrato(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	fixaEnvelopeVibrato(ch);

	unlockAudio();

	while (ch->status & IS_NyTon); // wait for sample to latch in mixer

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = editor.curInstr;
	editor.curPlaySmp = editor.curSmp;
}

void stopVoices(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	stmTyp *ch = stm;
	for (int32_t i = 0; i < MAX_VOICES; i++, ch++)
	{
		lastChInstr[i].sampleNr = 255;
		lastChInstr[i].instrNr = 255;

		ch->tonTyp = 0;
		ch->relTonNr = 0;
		ch->instrNr = 0;
		ch->instrPtr = instr[0]; // important: set instrument pointer to instr 0 (placeholder instrument)
		ch->status = IS_Vol;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->dFinalVol = 0.0;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;
		ch->vibDepth = 0;
		ch->midiVibDepth = 0;
		ch->midiPitch = 0;
		ch->smpPtr = NULL;
		ch->portaDir = 0; // FT2 bugfix: weird 3xx behavior if not used with note

		stopVoice(i);
	}

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	stopAllScopes();
	resetAudioDither();
	resetCachedMixerVars();
	resetCachedScopeVars();

	// wait for scope thread to finish, so that we know pointers aren't deprecated
	while (editor.scopeThreadMutex);

	if (audioWasntLocked)
		unlockAudio();
}

void resetReplayerState(void)
{
	song.pattDelTime = song.pattDelTime2 = 0;
	song.posJumpFlag = false;
	song.pBreakPos = 0;
	song.pBreakFlag = false;

	if (songPlaying)
	{
		song.globVol = 64;

		stmTyp *ch = stm;
		for (int32_t i = 0; i < song.antChn; i++, ch++)
			ch->status |= IS_Vol;
	}
}

void setNewSongPos(int32_t pos)
{
	resetReplayerState(); // FT2 bugfix
	setPos((int16_t)pos, 0, true);

	// non-FT2 fix: If song speed was 0, set it back to initial speed
	if (song.tempo == 0)
		song.tempo = song.initialTempo;
}

void decSongPos(void)
{
	if (song.songPos == 0)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos > 0)
		setNewSongPos(song.songPos - 1);

	if (audioWasntLocked)
		unlockAudio();
}

void incSongPos(void)
{
	if (song.songPos == song.len-1)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos < song.len-1)
		setNewSongPos(song.songPos + 1);

	if (audioWasntLocked)
		unlockAudio();
}

void decCurIns(void)
{
	if (editor.curInstr <= 1)
		return;

	editor.curInstr--;
	if ((editor.curInstr > 0x40 && !editor.instrBankSwapped) || (editor.curInstr <= 0x40 && editor.instrBankSwapped))
		pbSwapInstrBank();

	editor.instrBankOffset = ((editor.curInstr - 1) / 8) * 8;
 
	updateTextBoxPointers();
	updateNewInstrument();

	if (ui.advEditShown)
		updateAdvEdit();
}

void incCurIns(void)
{
	if (editor.curInstr >= MAX_INST)
		return;

	editor.curInstr++;
	if ((editor.curInstr > 0x40 && !editor.instrBankSwapped) || (editor.curInstr <= 0x40 && editor.instrBankSwapped))
		pbSwapInstrBank();

	editor.instrBankOffset = ((editor.curInstr - 1) / 8) * 8;
 	if (editor.instrBankOffset > MAX_INST-8)
		editor.instrBankOffset = MAX_INST-8;

	updateTextBoxPointers();
	updateNewInstrument();

	if (ui.advEditShown)
		updateAdvEdit();
}

void decCurSmp(void)
{
	if (editor.curSmp == 0)
		return;

	editor.curSmp--;
	editor.sampleBankOffset = (editor.curSmp / 5) * 5;
	setScrollBarPos(SB_SAMPLE_LIST, editor.sampleBankOffset, true);

	updateTextBoxPointers();
	updateNewSample();
}

void incCurSmp(void)
{
	if (editor.curSmp >= MAX_SMP_PER_INST-1)
		return;

	editor.curSmp++;

	editor.sampleBankOffset = (editor.curSmp / 5) * 5;
	if (editor.sampleBankOffset > MAX_SMP_PER_INST-5)
		editor.sampleBankOffset = MAX_SMP_PER_INST-5;

	setScrollBarPos(SB_SAMPLE_LIST, editor.sampleBankOffset, true);

	updateTextBoxPointers();
	updateNewSample();
}

void pbPlaySong(void)
{
	startPlaying(PLAYMODE_SONG, 0);
}

void pbPlayPtn(void)
{
	startPlaying(PLAYMODE_PATT, 0);
}

void pbRecSng(void)
{
	startPlaying(PLAYMODE_RECSONG, 0);
}

void pbRecPtn(void)
{
	startPlaying(PLAYMODE_RECPATT, 0);
}

void setSyncedReplayerVars(void)
{
	uint8_t scopeUpdateStatus[MAX_VOICES];

	pattSyncEntry = NULL;
	chSyncEntry = NULL;

	memset(scopeUpdateStatus, 0, sizeof (scopeUpdateStatus)); // this is needed

	uint64_t frameTime64 = SDL_GetPerformanceCounter();

	// handle channel sync queue

	while (chQueueClearing);
	while (chQueueReadSize() > 0)
	{
		if (frameTime64 < getChQueueTimestamp())
			break; // we have no more stuff to render for now

		chSyncEntry = chQueuePeek();
		if (chSyncEntry == NULL)
			break;

		for (int32_t i = 0; i < song.antChn; i++)
			scopeUpdateStatus[i] |= chSyncEntry->channels[i].status; // yes, OR the status

		if (!chQueuePop())
			break;
	}

	/* Extra validation because of possible issues when the buffer is full
	** and positions are being reset, which is not entirely thread safe.
	*/
	if (chSyncEntry != NULL && chSyncEntry->timestamp == 0)
		chSyncEntry = NULL;

	// handle pattern sync queue

	while (pattQueueClearing);
	while (pattQueueReadSize() > 0)
	{
		if (frameTime64 < getPattQueueTimestamp())
			break; // we have no more stuff to render for now

		pattSyncEntry = pattQueuePeek();
		if (pattSyncEntry == NULL)
			break;

		if (!pattQueuePop())
			break;
	}

	/* Extra validation because of possible issues when the buffer is full
	** and positions are being reset, which is not entirely thread safe.
	*/
	if (pattSyncEntry != NULL && pattSyncEntry->timestamp == 0)
		pattSyncEntry = NULL;

	// do actual updates

	if (chSyncEntry != NULL)
	{
		handleScopesFromChQueue(chSyncEntry, scopeUpdateStatus);
		ui.drawReplayerPianoFlag = true;
	}

	if (!songPlaying || pattSyncEntry == NULL)
		return;

	// we have a new tick

	editor.timer = pattSyncEntry->timer;

	if (editor.speed != pattSyncEntry->speed)
	{
		editor.speed = pattSyncEntry->speed;
		ui.drawBPMFlag = true;
	}

	if (editor.tempo != pattSyncEntry->tempo)
	{
		editor.tempo = pattSyncEntry->tempo;
		ui.drawSpeedFlag = true;
	}

	if (editor.globalVol != pattSyncEntry->globalVol)
	{
		editor.globalVol = pattSyncEntry->globalVol;
		ui.drawGlobVolFlag = true;
	}

	if (editor.songPos != pattSyncEntry->songPos)
	{
		editor.songPos = pattSyncEntry->songPos;
		ui.drawPosEdFlag = true;
	}

	// somewhat of a kludge...
	if (editor.tmpPattern != pattSyncEntry->pattern || editor.pattPos != pattSyncEntry->patternPos)
	{
		// set pattern number
		editor.editPattern = editor.tmpPattern = pattSyncEntry->pattern;
		checkMarkLimits();
		ui.drawPattNumLenFlag = true;

		// set row
		editor.pattPos = (uint8_t)pattSyncEntry->patternPos;
		ui.updatePatternEditor = true;
	}
}
