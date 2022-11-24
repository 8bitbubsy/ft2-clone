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
#include "scopes/ft2_scopes.h"
#include "ft2_mouse.h"
#include "ft2_sample_loader.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "mixer/ft2_windowed_sinc.h"

static double dLogTab[4*12*16], dExp2MulTab[32];
static bool bxxOverflow;
static note_t nilPatternLine[MAX_CHANNELS];

typedef void (*volColumnEfxRoutine)(channel_t *ch);
typedef void (*volColumnEfxRoutine2)(channel_t *ch, uint8_t *volColumnData);
typedef void (*efxRoutine)(channel_t *ch, uint8_t param);

// globally accessed
int8_t playMode = 0;
bool songPlaying = false, audioPaused = false, musicPaused = false;
volatile bool replayerBusy = false;
const uint16_t *note2Period = NULL;
int16_t patternNumRows[MAX_PATTERNS];
channel_t channel[MAX_CHANNELS];
song_t song;
instr_t *instr[128+4];
note_t *pattern[MAX_PATTERNS];
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

void fixInstrAndSampleNames(int16_t insNum)
{
	fixString(song.instrName[insNum], 21);
	if (instr[insNum] != NULL)
	{
		sample_t *s = instr[insNum]->smp;
		for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++)
			fixString(s->name, 21);
	}
}

void resetChannels(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	memset(channel, 0, sizeof (channel));

	channel_t *ch = channel;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++)
	{
		ch->instrPtr = instr[0];
		ch->status = IS_Vol;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;

		ch->channelOff = !editor.chnMode[i]; // set channel mute flag from global mute flag
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

// used on external sample load and during sample loading (on some module formats)
void tuneSample(sample_t *s, const int32_t midCFreq, bool linearPeriodsFlag)
{
	#define MIN_PERIOD (0)
	#define MAX_PERIOD (((10*12*16)-1)-1) /* -1 (because of bugged amigaPeriods table values) */

	double (*dGetHzFromPeriod)(int32_t) = linearPeriodsFlag ? dLinearPeriod2Hz : dAmigaPeriod2Hz;
	const uint16_t *periodTab = linearPeriodsFlag ? linearPeriods : amigaPeriods;

	if (midCFreq <= 0 || periodTab == NULL)
	{
		s->finetune = s->relativeNote = 0;
		return;
	}

	// handle frequency boundaries first...

	if (midCFreq <= (int32_t)dGetHzFromPeriod(periodTab[MIN_PERIOD]))
	{
		s->finetune = -128;
		s->relativeNote = -48;
		return;
	}

	if (midCFreq >= (int32_t)dGetHzFromPeriod(periodTab[MAX_PERIOD]))
	{
		s->finetune = 127;
		s->relativeNote = 71;
		return;
	}

	// check if midCFreq is matching any of the non-finetuned note frequencies (C-0..B-9)

	for (int8_t i = 0; i < 10*12; i++)
	{
		if (midCFreq == (int32_t)dGetHzFromPeriod(periodTab[16 + (i<<4)]))
		{
			s->finetune = 0;
			s->relativeNote = i - NOTE_C4;
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

	s->finetune = ((period & 31) - 16) << 3;
	s->relativeNote = (int8_t)(((period & ~31) >> 4) - NOTE_C4);
}

void setPatternLen(uint16_t pattNum, int16_t numRows)
{
	assert(pattNum < MAX_PATTERNS);
	if ((numRows < 1 || numRows > MAX_PATT_LEN) || numRows == patternNumRows[pattNum])
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	patternNumRows[pattNum] = numRows;

	if (pattern[pattNum] != NULL)
		killPatternIfUnused(pattNum);

	// non-FT2 security
	song.pattDelTime = 0;
	song.pattDelTime2 = 0;
	song.pBreakFlag = false;
	song.posJumpFlag = false;
	song.pBreakPos = 0;

	song.currNumRows = numRows;
	if (song.row >= song.currNumRows)
	{
		song.row = song.currNumRows - 1;
		editor.row = song.row;
	}

	checkMarkLimits();

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;
}

int16_t getUsedSamples(int16_t smpNum)
{
	if (instr[smpNum] == NULL)
		return 0;

	instr_t *ins = instr[smpNum];

	int16_t i = 16 - 1;
	while (i >= 0 && ins->smp[i].dataPtr == NULL && ins->smp[i].name[0] == '\0')
		i--;

	/* Yes, 'i' can be -1 here, and will be set to at least 0
	** because of ins->ta values. Possibly an FT2 bug...
	*/
	for (int16_t j = 0; j < 96; j++)
	{
		if (ins->note2SampleLUT[j] > i)
			i = ins->note2SampleLUT[j];
	}

	return i+1;
}

int16_t getRealUsedSamples(int16_t smpNum)
{
	if (instr[smpNum] == NULL)
		return 0;

	int8_t i = 16 - 1;
	while (i >= 0 && instr[smpNum]->smp[i].dataPtr == NULL)
		i--;

	return i+1;
}

double dLinearPeriod2Hz(int32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0.0; // in FT2, a period of 0 results in 0Hz

	const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF; // mask needed for FT2 period overflow quirk

	const uint32_t quotient  = invPeriod / (12 * 16 * 4);
	const uint32_t remainder = invPeriod % (12 * 16 * 4);

	return dLogTab[remainder] * dExp2MulTab[(14-quotient) & 31]; // x = y >> ((14-quotient) & 31);
}

double dAmigaPeriod2Hz(int32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0.0; // in FT2, a period of 0 results in 0Hz

	return (double)(8363 * 1712) / period;
}

double dPeriod2Hz(int32_t period)
{
	return audio.linearPeriodsFlag ? dLinearPeriod2Hz(period) : dAmigaPeriod2Hz(period);
}

// returns *exact* FT2 C-4 voice rate (depending on finetune, relativeNote and linear/Amiga period mode)
double getSampleC4Rate(sample_t *s)
{
	int32_t note = NOTE_C4 + s->relativeNote;
	if (note < 0)
		return -1; // shouldn't happen (just in case...)

	if (note >= (10*12)-1)
		return -1; // B-9 (after relativeTone add) = illegal! (won't play in replayer)

	const int32_t C4Period = (note << 4) + (((int8_t)s->finetune >> 3) + 16);

	const int32_t period = audio.linearPeriodsFlag ? linearPeriods[C4Period] : amigaPeriods[C4Period];
	return dPeriod2Hz(period);
}

void setFrequencyTable(bool linearPeriodsFlag)
{
	pauseAudio();

	audio.linearPeriodsFlag = linearPeriodsFlag;

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

static void retrigVolume(channel_t *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= IS_Vol + IS_Pan + IS_QuickVol;
}

static void retrigEnvelopeVibrato(channel_t *ch)
{
	if (!(ch->waveCtrl & 0x04)) ch->vibPos  = 0;
	if (!(ch->waveCtrl & 0x40)) ch->tremPos = 0;

	ch->retrigCnt = 0;
	ch->tremorPos = 0;

	ch->envSustainActive = true;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	if (ins->volEnvFlags & ENV_ENABLED)
	{
		ch->volEnvTick = 65535;
		ch->volEnvPos = 0;
	}

	if (ins->panEnvFlags & ENV_ENABLED)
	{
		ch->panEnvTick = 65535;
		ch->panEnvPos = 0;
	}

	ch->fadeoutSpeed = ins->fadeout; // FT2 doesn't check if fadeout is more than 4095!
	ch->fadeoutVol = 32768;

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

void keyOff(channel_t *ch)
{
	ch->envSustainActive = false;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	if (ins->volEnvFlags & ENV_ENABLED)
	{
		if (ch->volEnvTick >= (uint16_t)ins->volEnvPoints[ch->volEnvPos][0])
			ch->volEnvTick = ins->volEnvPoints[ch->volEnvPos][0] - 1;
	}
	else
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}

	if (!(ins->panEnvFlags & ENV_ENABLED)) // What..? Probably an FT2 bug.
	{
		if (ch->panEnvTick >= (uint16_t)ins->panEnvPoints[ch->panEnvPos][0])
			ch->panEnvTick = ins->panEnvPoints[ch->panEnvPos][0] - 1;
	}
}

void calcReplayerLogTab(void) // for linear period -> hz calculation
{
	for (int32_t i = 0; i < 32; i++)
		dExp2MulTab[i] = 1.0 / exp2(i); // 1/(2^i)

	for (int32_t i = 0; i < 4*12*16; i++)
		dLogTab[i] = (8363.0 * 256.0) * exp2(i / (4.0 * 12.0 * 16.0));
}

void calcReplayerVars(int32_t audioFreq)
{
	assert(audioFreq > 0);
	if (audioFreq <= 0)
		return;

	audio.dHz2MixDeltaMul = (double)MIXER_FRAC_SCALE / audioFreq;
	audio.quickVolRampSamples = (int32_t)round(audioFreq / (double)FT2_QUICKRAMP_SAMPLES);
	audio.fRampQuickVolMul = (float)(1.0 / audio.quickVolRampSamples);

	for (int32_t bpm = MIN_BPM; bpm <= MAX_BPM; bpm++)
	{
		const int32_t i = bpm - MIN_BPM; // index for tables

		const double dBpmHz = bpm / 2.5;
		const double dSamplesPerTick = audioFreq / dBpmHz;

		// convert to 32.32 fixed-point
		audio.samplesPerTick64Tab[i] = (int64_t)((dSamplesPerTick * (UINT32_MAX+1.0)) + 0.5); // rounded

		// BPM Hz -> tick length for performance counter (syncing visuals to audio)
		double dTimeInt;
		double dTimeFrac = modf(editor.dPerfFreq / dBpmHz, &dTimeInt);
		const int32_t timeInt = (int32_t)dTimeInt;

		dTimeFrac = floor((UINT32_MAX+1.0) * dTimeFrac); // fractional part (scaled to 0..2^32-1)

		audio.tickTimeTab[i] = (uint32_t)timeInt;
		audio.tickTimeFracTab[i] = (uint32_t)dTimeFrac;

		// for calculating volume ramp length for tick-lenghted ramps
		const int32_t samplesPerTickRounded = (int32_t)(dSamplesPerTick + 0.5); // must be rounded
		audio.fRampTickMulTab[i] = (float)(1.0 / samplesPerTickRounded);
	}
}

// for piano in Instr. Ed. (values outside 0..95 can happen)
int32_t getPianoKey(uint16_t period, int8_t finetune, int8_t relativeNote) 
{
	assert(note2Period != NULL);
	if (period > note2Period[0])
		return -1; // outside left piano edge

	finetune = ((int8_t)finetune >> 3) + 16; // -128..127 -> 0..31

	int32_t hiPeriod = 10*12*16;
	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 7; i++)
	{
		const int32_t tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + finetune;

		int32_t lookUp = tmpPeriod - 16;
		if (lookUp < 0)
			lookUp = 0;

		if (period >= note2Period[lookUp])
			hiPeriod = (tmpPeriod - finetune) & ~15;
		else
			loPeriod = (tmpPeriod - finetune) & ~15;
	}

	return (loPeriod >> 4) - relativeNote;
}

static void startTone(uint8_t note, uint8_t efx, uint8_t efxData, channel_t *ch)
{
	if (note == NOTE_OFF)
	{
		keyOff(ch);
		return;
	}

	// if we came from Rxy (retrig), we didn't check note (Ton) yet
	if (note == 0)
	{
		note = ch->noteNum;
		if (note == 0)
			return; // if still no note, exit from routine
	}

	ch->noteNum = note;

	assert(ch->instrNum <= 130);
	instr_t *ins = instr[ch->instrNum];
	if (ins == NULL)
		ins = instr[0]; // empty instruments use this placeholder instrument

	ch->instrPtr = ins;
	ch->mute = ins->mute;

	if (note > 96) // non-FT2 security (should never happen because I clamp in the patt. loader now)
		note = 96;

	ch->smpNum = ins->note2SampleLUT[note-1] & 0xF; // FT2 doesn't AND here, but let's do it for safety
	sample_t *s = &ins->smp[ch->smpNum];

	ch->smpPtr = s;
	ch->relativeNote = s->relativeNote;

	note += ch->relativeNote;
	if (note >= 10*12) // unsigned check (also handles note < 0)
		return;

	ch->oldVol = s->volume;
	ch->oldPan = s->panning;

	if (efx == 0xE && (efxData & 0xF0) == 0x50)
		ch->finetune = ((efxData & 0x0F) << 4) - 128;
	else
		ch->finetune = s->finetune;

	if (note != 0)
	{
		const uint16_t noteIndex = ((note-1) << 4) + (((int8_t)ch->finetune >> 3) + 16); // 0..1920

		assert(note2Period != NULL);
		ch->outPeriod = ch->realPeriod = note2Period[noteIndex];
	}

	ch->status |= IS_Period + IS_Vol + IS_Pan + IS_Trigger + IS_QuickVol;

	if (efx == 9)
	{
		if (efxData > 0)
			ch->smpOffset = ch->efxData;

		ch->smpStartPos = ch->smpOffset << 8;
	}
	else
	{
		ch->smpStartPos = 0;
	}
}

static void volume(channel_t *ch, uint8_t param); // volume slide
static void vibrato2(channel_t *ch);
static void tonePorta(channel_t *ch, uint8_t param);

static void dummy(channel_t *ch, uint8_t param)
{
	(void)ch;
	(void)param;
	return;
}

static void finePortaUp(channel_t *ch, uint8_t param)
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

static void finePortaDown(channel_t *ch, uint8_t param)
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

static void setGlissCtrl(channel_t *ch, uint8_t param)
{
	ch->glissFunk = param;
}

static void setVibratoCtrl(channel_t *ch, uint8_t param)
{
	ch->waveCtrl = (ch->waveCtrl & 0xF0) | param;
}

static void jumpLoop(channel_t *ch, uint8_t param)
{
	if (param == 0)
	{
		ch->jumpToRow = song.row & 0xFF;
	}
	else if (ch->patLoopCounter == 0)
	{
		ch->patLoopCounter = param;

		song.pBreakPos = ch->jumpToRow;
		song.pBreakFlag = true;
	}
	else if (--ch->patLoopCounter > 0)
	{
		song.pBreakPos = ch->jumpToRow;
		song.pBreakFlag = true;
	}
}

static void setTremoloCtrl(channel_t *ch, uint8_t param)
{
	ch->waveCtrl = (param << 4) | (ch->waveCtrl & 0x0F);
}

static void volFineUp(channel_t *ch, uint8_t param)
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

static void volFineDown(channel_t *ch, uint8_t param)
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

static void noteCut0(channel_t *ch, uint8_t param)
{
	if (param == 0) // only a parameter of zero is handled here
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void pattDelay(channel_t *ch, uint8_t param)
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

static void E_Effects_TickZero(channel_t *ch, uint8_t param)
{
	const uint8_t efx = param >> 4;
	param &= 0x0F;

	if (ch->channelOff) // channel is muted, only handle some E effects
	{
		     if (efx == 0x6) jumpLoop(ch, param);
		else if (efx == 0xE) pattDelay(ch, param);

		return;
	}

	EJumpTab_TickZero[efx](ch, param);
}

static void posJump(channel_t *ch, uint8_t param)
{
	if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
	{
		const int16_t pos = (int16_t)param - 1;
		if (pos < 0 || pos >= song.songLength)
			bxxOverflow = true; // non-FT2 security fix...
		else
			song.songPos = pos;
	}

	song.pBreakPos = 0;
	song.posJumpFlag = true;

	(void)ch;
}

static void pattBreak(channel_t *ch, uint8_t param)
{
	param = ((param >> 4) * 10) + (param & 0x0F);
	if (param <= 63)
		song.pBreakPos = param;
	else
		song.pBreakPos = 0;

	song.posJumpFlag = true;

	(void)ch;
}

static void setSpeed(channel_t *ch, uint8_t param)
{
	if (param >= 32)
	{
		song.BPM = param;
		setMixerBPM(song.BPM);
	}
	else
	{
		song.tick = song.speed = param;
	}

	(void)ch;
}

static void setGlobalVolume(channel_t *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	song.globalVolume = param;

	channel_t *c = channel;
	for (int32_t i = 0; i < song.numChannels; i++, c++) // update all voice volumes
		c->status |= IS_Vol;

	(void)ch;
}

static void setEnvelopePos(channel_t *ch, uint8_t param)
{
	bool envUpdate;
	int8_t point;
	int16_t tick;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	// (envelope precision has been updated from x.8fp to x.16fp)

	// *** VOLUME ENVELOPE ***
	if (ins->volEnvFlags & ENV_ENABLED)
	{
		ch->volEnvTick = param-1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->volEnvLength > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->volEnvLength-1; i++)
			{
				if (tick < ins->volEnvPoints[point][0])
				{
					point--;

					tick -= ins->volEnvPoints[point][0];
					if (tick == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->volEnvPoints[point+1][0] <= ins->volEnvPoints[point][0])
					{
						envUpdate = true;
						break;
					}

					int32_t delta = (int8_t)((ins->volEnvPoints[point+1][1] - ins->volEnvPoints[point][1]) & 0xFF) << 16;
					delta /= (ins->volEnvPoints[point+1][0]-ins->volEnvPoints[point][0]);

					ch->volEnvDelta = delta;
					ch->volEnvValue = (ch->volEnvDelta * (tick-1)) + ((int8_t)(ins->volEnvPoints[point][1] & 0xFF) << 16);

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
			ch->volEnvDelta = 0;
			ch->volEnvValue = (int8_t)(ins->volEnvPoints[point][1] & 0xFF) << 16;
		}

		if (point >= ins->volEnvLength)
		{
			point = ins->volEnvLength-1;
			if (point < 0)
				point = 0;
		}

		ch->volEnvPos = point;
	}

	// *** PANNING ENVELOPE ***
	if (ins->volEnvFlags & ENV_SUSTAIN) // What..? An FT2 bug!?
	{
		ch->panEnvTick = param-1;

		point = 0;
		envUpdate = true;
		tick = param;

		if (ins->panEnvLength > 1)
		{
			point++;
			for (int32_t i = 0; i < ins->panEnvLength-1; i++)
			{
				if (tick < ins->panEnvPoints[point][0])
				{
					point--;

					tick -= ins->panEnvPoints[point][0];
					if (tick == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->panEnvPoints[point+1][0] <= ins->panEnvPoints[point][0])
					{
						envUpdate = true;
						break;
					}

					int32_t delta = (int8_t)((ins->panEnvPoints[point+1][1] - ins->panEnvPoints[point][1]) & 0xFF) << 16;
					delta /= (ins->panEnvPoints[point+1][0]-ins->panEnvPoints[point][0]);

					ch->panEnvDelta = delta;
					ch->panEnvValue = (ch->panEnvDelta * (tick-1)) + ((int8_t)(ins->panEnvPoints[point][1] & 0xFF) << 16);

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
			ch->panEnvDelta = 0;
			ch->panEnvValue = (int8_t)(ins->panEnvPoints[point][1] & 0xFF) << 16;
		}

		if (point >= ins->panEnvLength)
		{
			point = ins->panEnvLength-1;
			if (point < 0)
				point = 0;
		}

		ch->panEnvPos = point;
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
	setGlobalVolume, // G
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

static void handleMoreEffects_TickZero(channel_t *ch) // called even if channel is muted
{
	if (ch->efx > 35)
		return;

	JumpTab_TickZero[ch->efx](ch, ch->efxData);
}

/* -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy command (multiretrig)
*/

static void v_SetVibSpeed(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (ch->volColumnVol & 0x0F) << 2;
	if (*volColumnData != 0)
		ch->vibSpeed = *volColumnData;
}

static void v_Volume(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData -= 16;
	if (*volColumnData > 64) // no idea why FT2 has this check...
		*volColumnData = 64;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void v_FineSlideDown(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (uint8_t)(0 - (ch->volColumnVol & 0x0F)) + ch->realVol;
	if ((int8_t)*volColumnData < 0)
		*volColumnData = 0;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= IS_Vol;
}

static void v_FineSlideUp(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (ch->volColumnVol & 0x0F) + ch->realVol;
	if (*volColumnData > 64)
		*volColumnData = 64;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= IS_Vol;
}

static void v_SetPan(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData <<= 4;

	ch->outPan = *volColumnData;
	ch->status |= IS_Pan;
}

// -- non-tick-zero volume column effects --

static void v_SlideDown(channel_t *ch)
{
	uint8_t newVol = (uint8_t)(0 - (ch->volColumnVol & 0x0F)) + ch->realVol;
	if ((int8_t)newVol < 0)
		newVol = 0;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_SlideUp(channel_t *ch)
{
	uint8_t newVol = (ch->volColumnVol & 0x0F) + ch->realVol;
	if (newVol > 64)
		newVol = 64;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_Vibrato(channel_t *ch)
{
	const uint8_t param = ch->volColumnVol & 0xF;
	if (param > 0)
		ch->vibDepth = param;

	vibrato2(ch);
}

static void v_PanSlideLeft(channel_t *ch)
{
	uint16_t tmp16 = (uint8_t)(0 - (ch->volColumnVol & 0x0F)) + ch->outPan;
	if (tmp16 < 256) // includes an FT2 bug: pan-slide-left of 0 = set pan to 0
		tmp16 = 0;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_PanSlideRight(channel_t *ch)
{
	uint16_t tmp16 = (ch->volColumnVol & 0x0F) + ch->outPan;
	if (tmp16 > 255)
		tmp16 = 255;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_TonePorta(channel_t *ch)
{
	tonePorta(ch, 0); // the last parameter is actually not used in tonePorta()
}

static void v_dummy(channel_t *ch)
{
	(void)ch;
	return;
}

static void v_dummy2(channel_t *ch, uint8_t *volColumnData)
{
	(void)ch;
	(void)volColumnData;
	return;
}

static const volColumnEfxRoutine VJumpTab_TickNonZero[16] =
{
	v_dummy,        v_dummy,         v_dummy,  v_dummy,
	v_dummy,        v_dummy,     v_SlideDown, v_SlideUp,
	v_dummy,        v_dummy,         v_dummy, v_Vibrato,
	v_dummy, v_PanSlideLeft, v_PanSlideRight, v_TonePorta
};

static const volColumnEfxRoutine2 VJumpTab_TickZero[16] =
{
	       v_dummy2,      v_Volume,      v_Volume, v_Volume,
	       v_Volume,      v_Volume,      v_dummy2, v_dummy2,
	v_FineSlideDown, v_FineSlideUp, v_SetVibSpeed, v_dummy2,
	       v_SetPan,      v_dummy2,      v_dummy2, v_dummy2
};

static void setPan(channel_t *ch, uint8_t param)
{
	ch->outPan = param;
	ch->status |= IS_Pan;
}

static void setVol(channel_t *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	ch->outVol = ch->realVol = param;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void xFinePorta(channel_t *ch, uint8_t param)
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

static void doMultiRetrig(channel_t *ch, uint8_t param) // "param" is never used (needed for efx jumptable structure)
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

	if (ch->volColumnVol >= 0x10 && ch->volColumnVol <= 0x50)
	{
		ch->outVol = ch->volColumnVol - 0x10;
		ch->realVol = ch->outVol;
	}
	else if (ch->volColumnVol >= 0xC0 && ch->volColumnVol <= 0xCF)
	{
		ch->outPan = (ch->volColumnVol & 0x0F) << 4;
	}

	startTone(0, 0, 0, ch);

	(void)param;
}

static void multiRetrig(channel_t *ch, uint8_t param, uint8_t volumeColumnData)
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

static void handleEffects_TickZero(channel_t *ch)
{
	// volume column effects
	uint8_t newVolCol = ch->volColumnVol; // manipulated by vol. column effects, then used for multiretrig check (FT2 quirk)
	VJumpTab_TickZero[ch->volColumnVol >> 4](ch, &newVolCol);

	// normal effects
	const uint8_t param = ch->efxData;
	if (ch->efx == 0 && param == 0)
		return; // no effect

	     if (ch->efx ==  8) setPan(ch, param);
	else if (ch->efx == 12) setVol(ch, param);
	else if (ch->efx == 27) multiRetrig(ch, param, newVolCol);
	else if (ch->efx == 33) xFinePorta(ch, param);

	handleMoreEffects_TickZero(ch);
}

static void fixTonePorta(channel_t *ch, const note_t *p, uint8_t inst)
{
	if (p->note > 0)
	{
		if (p->note == NOTE_OFF)
		{
			keyOff(ch);
		}
		else
		{
			const uint16_t note = (((p->note-1) + ch->relativeNote) << 4) + (((int8_t)ch->finetune >> 3) + 16);
			if (note < MAX_NOTES)
			{
				assert(note2Period != NULL);
				ch->wantPeriod = note2Period[note];

				if (ch->wantPeriod == ch->realPeriod)
					ch->portaDirection = 0;
				else if (ch->wantPeriod > ch->realPeriod)
					ch->portaDirection = 1;
				else
					ch->portaDirection = 2;
			}
		}
	}

	if (inst > 0)
	{
		retrigVolume(ch);
		if (p->note != NOTE_OFF)
			retrigEnvelopeVibrato(ch);
	}
}

static void getNewNote(channel_t *ch, const note_t *p)
{
	ch->volColumnVol = p->vol;

	if (ch->efx == 0)
	{
		if (ch->efxData > 0) // we have an arpeggio running, set period back
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}
	else
	{
		// if we have a vibrato on previous row (ch) that ends at current row (p), set period back
		if ((ch->efx == 4 || ch->efx == 6) && (p->efx != 4 && p->efx != 6))
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}

	ch->efx = p->efx;
	ch->efxData = p->efxData;
	ch->noteData = (p->instr << 8) | p->note;

	if (ch->channelOff) // channel is muted, only handle some effects
	{
		handleMoreEffects_TickZero(ch);
		return;
	}

	// 'inst' var is used for later if checks...
	uint8_t inst = p->instr;
	if (inst > 0)
	{
		if (inst <= MAX_INST)
			ch->instrNum = inst;
		else
			inst = 0;
	}

	bool checkEfx = true;
	if (p->efx == 0x0E)
	{
		if (p->efxData >= 0xD1 && p->efxData <= 0xDF)
			return; // we have a note delay (ED1..EDF)
		else if (p->efxData == 0x90)
			checkEfx = false;
	}

	if (checkEfx)
	{
		if ((ch->volColumnVol & 0xF0) == 0xF0) // gxx
		{
			const uint8_t volColumnData = ch->volColumnVol & 0x0F;
			if (volColumnData > 0)
				ch->portaSpeed = volColumnData << 6;

			fixTonePorta(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->efx == 3 || p->efx == 5) // 3xx or 5xx
		{
			if (p->efx != 5 && p->efxData != 0)
				ch->portaSpeed = p->efxData << 2;

			fixTonePorta(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->efx == 0x14 && p->efxData == 0) // K00 (KeyOff - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				retrigVolume(ch);

			handleEffects_TickZero(ch);
			return;
		}

		if (p->note == 0)
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

	if (p->note == NOTE_OFF)
		keyOff(ch);
	else
		startTone(p->note, p->efx, p->efxData, ch);

	if (inst > 0)
	{
		retrigVolume(ch);
		if (p->note != NOTE_OFF)
			retrigEnvelopeVibrato(ch);
	}

	handleEffects_TickZero(ch);
}

static void updateChannel(channel_t *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	int16_t autoVibVal;
	uint16_t autoVibAmp;
	int32_t envVal;
	float fVol;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	// *** FADEOUT ***
	if (!ch->envSustainActive)
	{
		ch->status |= IS_Vol;

		// unsigned clamp + reset
		if (ch->fadeoutVol >= ch->fadeoutSpeed)
		{
			ch->fadeoutVol -= ch->fadeoutSpeed;
		}
		else
		{
			ch->fadeoutVol = 0;
			ch->fadeoutSpeed = 0;
		}
	}

	if (!ch->mute)
	{
		// (envelope precision has been updated from x.8fp to x.16fp)

		// *** VOLUME ENVELOPE ***
		envVal = 0;
		if (ins->volEnvFlags & ENV_ENABLED)
		{
			envDidInterpolate = false;
			envPos = ch->volEnvPos;

			if (++ch->volEnvTick == ins->volEnvPoints[envPos][0])
			{
				ch->volEnvValue = (int8_t)(ins->volEnvPoints[envPos][1] & 0xFF) << 16;

				envPos++;
				if (ins->volEnvFlags & ENV_LOOP)
				{
					envPos--;

					if (envPos == ins->volEnvLoopEnd)
					{
						if (!(ins->volEnvFlags & ENV_SUSTAIN) || envPos != ins->volEnvSustain || ch->envSustainActive)
						{
							envPos = ins->volEnvLoopStart;
							ch->volEnvTick = ins->volEnvPoints[envPos][0];
							ch->volEnvValue = (int8_t)(ins->volEnvPoints[envPos][1] & 0xFF) << 16;
						}
					}

					envPos++;
				}

				if (envPos < ins->volEnvLength)
				{
					envInterpolateFlag = true;
					if ((ins->volEnvFlags & ENV_SUSTAIN) && ch->envSustainActive)
					{
						if (envPos-1 == ins->volEnvSustain)
						{
							envPos--;
							ch->volEnvDelta = 0;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag)
					{
						ch->volEnvPos = envPos;

						ch->volEnvDelta = 0;
						if (ins->volEnvPoints[envPos][0] > ins->volEnvPoints[envPos-1][0])
						{
							int32_t delta = (int8_t)((ins->volEnvPoints[envPos][1] - ins->volEnvPoints[envPos-1][1]) & 0xFF) << 16;
							delta /= (ins->volEnvPoints[envPos][0]-ins->volEnvPoints[envPos-1][0]);
							ch->volEnvDelta = delta;

							envVal = ch->volEnvValue;
							envDidInterpolate = true;
						}
					}
				}
				else
				{
					ch->volEnvDelta = 0;
				}
			}

			if (!envDidInterpolate)
			{
				ch->volEnvValue += ch->volEnvDelta;

				envVal = ch->volEnvValue;
				if (envVal > 64<<16)
				{
					if (envVal > 128<<16)
						envVal = 64<<16;
					else
						envVal = 0;

					ch->volEnvDelta = 0;
				}
			}

			const int32_t vol = song.globalVolume * ch->outVol * ch->fadeoutVol;

			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
			fVol *= (int32_t)envVal * (1.0f / (64.0f * (1 << 16))); // volume envelope value

			ch->status |= IS_Vol; // update mixer vol every tick when vol envelope is enabled
		}
		else
		{
			const int32_t vol = song.globalVolume * ch->outVol * ch->fadeoutVol;
			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
		}

		/* FT2 doesn't clamp here, but it's actually important if you
		** move envelope points with the mouse while playing the instrument.
		*/
		ch->fFinalVol = CLAMP(fVol, 0.0f, 1.0f);
	}
	else
	{
		ch->fFinalVol = 0.0f;
	}

	// *** PANNING ENVELOPE ***

	envVal = 0;
	if (ins->panEnvFlags & ENV_ENABLED)
	{
		envDidInterpolate = false;
		envPos = ch->panEnvPos;

		if (++ch->panEnvTick == ins->panEnvPoints[envPos][0])
		{
			ch->panEnvValue = (int8_t)(ins->panEnvPoints[envPos][1] & 0xFF) << 16;

			envPos++;
			if (ins->panEnvFlags & ENV_LOOP)
			{
				envPos--;

				if (envPos == ins->panEnvLoopEnd)
				{
					if (!(ins->panEnvFlags & ENV_SUSTAIN) || envPos != ins->panEnvSustain || ch->envSustainActive)
					{
						envPos = ins->panEnvLoopStart;

						ch->panEnvTick = ins->panEnvPoints[envPos][0];
						ch->panEnvValue = (int8_t)(ins->panEnvPoints[envPos][1] & 0xFF) << 16;
					}
				}

				envPos++;
			}

			if (envPos < ins->panEnvLength)
			{
				envInterpolateFlag = true;
				if ((ins->panEnvFlags & ENV_SUSTAIN) && ch->envSustainActive)
				{
					if (envPos-1 == ins->panEnvSustain)
					{
						envPos--;
						ch->panEnvDelta = 0;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag)
				{
					ch->panEnvPos = envPos;

					ch->panEnvDelta = 0;
					if (ins->panEnvPoints[envPos][0] > ins->panEnvPoints[envPos-1][0])
					{
						int32_t delta = (int8_t)((ins->panEnvPoints[envPos][1] - ins->panEnvPoints[envPos-1][1]) & 0xFF) << 16;
						delta /= (ins->panEnvPoints[envPos][0]-ins->panEnvPoints[envPos-1][0]);
						ch->panEnvDelta = delta;

						envVal = ch->panEnvValue;
						envDidInterpolate = true;
					}
				}
			}
			else
			{
				ch->panEnvDelta = 0;
			}
		}

		if (!envDidInterpolate)
		{
			ch->panEnvValue += ch->panEnvDelta;

			envVal = ch->panEnvValue;
			if (envVal > 64<<16)
			{
				if (envVal > 128<<16)
					envVal = 64<<16;
				else
					envVal = 0;

				ch->panEnvDelta = 0;
			}
		}

		envVal -= 32<<16; // center panning envelope value

		const int32_t pan = 128 - ABS(ch->outPan-128);
		const int32_t panAdd = (pan * envVal) >> (16+5);
		ch->finalPan = (uint8_t)(ch->outPan + panAdd);

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

		     if (ins->vibType == 1) autoVibVal = (ch->eVibPos > 127) ? 64 : -64; // square
		else if (ins->vibType == 2) autoVibVal = (((ch->eVibPos >> 1) + 64) & 127) - 64; // ramp up
		else if (ins->vibType == 3) autoVibVal = ((-(ch->eVibPos >> 1) + 64) & 127) - 64; // ramp down
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
static uint16_t relocateTon(uint16_t period, uint8_t arpNote, channel_t *ch)
{
	int32_t tmpPeriod;

	const int32_t fineTune = ((int8_t)ch->finetune >> 3) + 16;

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
			lookUp = 0; // safety fix (C-0 w/ f.tune <= -65). This seems to result in 0 in FT2 (TODO: verify)

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

static void vibrato2(channel_t *ch)
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

static void arp(channel_t *ch, uint8_t param)
{
	uint8_t note;

	const uint8_t tick = arpTab[song.tick & 0xFF]; // non-FT2 protection (we have 248 extra overflow bytes in LUT, but not more!)
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

static void portaUp(channel_t *ch, uint8_t param)
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

static void portaDown(channel_t *ch, uint8_t param)
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

static void tonePorta(channel_t *ch, uint8_t param)
{
	if (ch->portaDirection == 0)
		return;

	if (ch->portaDirection > 1)
	{
		ch->realPeriod -= ch->portaSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->wantPeriod)
		{
			ch->portaDirection = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}
	else
	{
		ch->realPeriod += ch->portaSpeed;
		if (ch->realPeriod >= ch->wantPeriod)
		{
			ch->portaDirection = 1;
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

static void vibrato(channel_t *ch, uint8_t param)
{
	uint8_t tmp8;

	if (param > 0)
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

static void tonePlusVol(channel_t *ch, uint8_t param)
{
	tonePorta(ch, 0); // the last parameter is actually not used in tonePorta()
	volume(ch, param);

	(void)param;
}

static void vibratoPlusVol(channel_t *ch, uint8_t param)
{
	vibrato2(ch);
	volume(ch, param);

	(void)param;
}

static void tremolo(channel_t *ch, uint8_t param)
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

static void volume(channel_t *ch, uint8_t param) // volume slide
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

static void globalVolSlide(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->globVolSlideSpeed;

	ch->globVolSlideSpeed = param;

	uint8_t newVol = (uint8_t)song.globalVolume;
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

	song.globalVolume = newVol;

	channel_t *c = channel;
	for (int32_t i = 0; i < song.numChannels; i++, c++) // update all voice volumes
		c->status |= IS_Vol;
}

static void keyOffCmd(channel_t *ch, uint8_t param)
{
	if ((uint8_t)(song.speed-song.tick) == (param & 31))
		keyOff(ch);
}

static void panningSlide(channel_t *ch, uint8_t param)
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

static void tremor(channel_t *ch, uint8_t param)
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

static void retrigNote(channel_t *ch, uint8_t param)
{
	if (param == 0) // E9x with a param of zero is handled in getNewNote()
		return;

	if ((song.speed-song.tick) % param == 0)
	{
		startTone(0, 0, 0, ch);
		retrigEnvelopeVibrato(ch);
	}
}

static void noteCut(channel_t *ch, uint8_t param)
{
	if ((uint8_t)(song.speed-song.tick) == param)
	{
		ch->outVol = ch->realVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void noteDelay(channel_t *ch, uint8_t param)
{
	if ((uint8_t)(song.speed-song.tick) == param)
	{
		startTone(ch->noteData & 0xFF, 0, 0, ch);

		if ((ch->noteData & 0xFF00) > 0)
			retrigVolume(ch);

		retrigEnvelopeVibrato(ch);

		if (ch->volColumnVol >= 0x10 && ch->volColumnVol <= 0x50)
		{
			ch->outVol = ch->volColumnVol - 16;
			ch->realVol = ch->outVol;
		}
		else if (ch->volColumnVol >= 0xC0 && ch->volColumnVol <= 0xCF)
		{
			ch->outPan = (ch->volColumnVol & 0x0F) << 4;
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

static void E_Effects_TickNonZero(channel_t *ch, uint8_t param)
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

static void handleEffects_TickNonZero(channel_t *ch)
{
	if (ch->channelOff)
		return; // muted

	// volume column effects
	VJumpTab_TickNonZero[ch->volColumnVol >> 4](ch);

	// normal effects
	if ((ch->efx == 0 && ch->efxData == 0) || ch->efx > 35)
		return; // no effect

	JumpTab_TickNonZero[ch->efx](ch, ch->efxData);
}

static void getNextPos(void)
{
	if (song.tick != 1)
		return;

	song.row++;

	if (song.pattDelTime > 0)
	{
		song.pattDelTime2 = song.pattDelTime;
		song.pattDelTime = 0;
	}

	if (song.pattDelTime2 > 0)
	{
		song.pattDelTime2--;
		if (song.pattDelTime2 > 0)
			song.row--;
	}

	if (song.pBreakFlag)
	{
		song.pBreakFlag = false;
		song.row = song.pBreakPos;
	}

	if (song.row >= song.currNumRows || song.posJumpFlag)
	{
		song.row = song.pBreakPos;
		song.pBreakPos = 0;
		song.posJumpFlag = false;

		if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
		{
			if (bxxOverflow)
			{
				song.songPos = 0;
				bxxOverflow = false;
			}
			else if (++song.songPos >= song.songLength)
			{
				editor.wavReachedEndFlag = true;
				song.songPos = song.songLoopStart;
			}

			assert(song.songPos <= 255);
			song.pattNum = song.orders[song.songPos & 0xFF];
			song.currNumRows = patternNumRows[song.pattNum & 0xFF];
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
	channel_t *ch;

	if (musicPaused || !songPlaying)
	{
		ch = channel;
		for (i = 0; i < song.numChannels; i++, ch++)
			updateChannel(ch);

		return;
	}

	// for song playback counter (hh:mm:ss)
	if (song.BPM >= MIN_BPM && song.BPM <= MAX_BPM)
		song.musicTime64 += musicTimeTab64[song.BPM-MIN_BPM];

	bool tickZero = false;
	if (--song.tick == 0)
	{
		song.tick = song.speed;
		tickZero = true;
	}

	song.curReplayerTick = (uint8_t)song.tick; // for audio/video syncing (and recording)

	const bool readNewNote = tickZero && (song.pattDelTime2 == 0);
	if (readNewNote)
	{
		// set audio/video syncing variables
		song.curReplayerRow = (uint8_t)song.row;
		song.curReplayerPattNum = (uint8_t)song.pattNum;
		song.curReplayerSongPos = (uint8_t)song.songPos;
		// ----------------------------------------------

		const note_t *p = nilPatternLine;
		if (pattern[song.pattNum] != NULL)
			p = &pattern[song.pattNum][song.row * MAX_CHANNELS];

		ch = channel;
		for (i = 0; i < song.numChannels; i++, ch++, p++)
		{
			getNewNote(ch, p);
			updateChannel(ch);
		}
	}
	else
	{
		ch = channel;
		for (i = 0; i < song.numChannels; i++, ch++)
		{
			handleEffects_TickNonZero(ch);
			updateChannel(ch);
		}
	}

	getNextPos();
}

void resetMusic(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.tick = 1;
	stopVoices();

	if (audioWasntLocked)
		unlockAudio();

	setPos(0, 0, false);

	if (!songPlaying)
	{
		setScrollBarEnd(SB_POS_ED, (song.songLength - 1) + 5);
		setScrollBarPos(SB_POS_ED, 0, false);
	}
}

void setPos(int16_t songPos, int16_t row, bool resetTimer)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPos > -1)
	{
		song.songPos = songPos;
		if (song.songLength > 0 && song.songPos >= song.songLength)
			song.songPos = song.songLength - 1;

		song.pattNum = song.orders[song.songPos];
		assert(song.pattNum < MAX_PATTERNS);
		song.currNumRows = patternNumRows[song.pattNum];

		checkMarkLimits(); // non-FT2 safety
	}

	if (row > -1)
	{
		song.row = row;
		if (song.row >= song.currNumRows)
			song.row = song.currNumRows-1;
	}

	// if not playing, update local position variables
	if (!songPlaying)
	{
		if (row > -1)
		{
			editor.row = (uint8_t)row;
			ui.updatePatternEditor = true;
		}

		if (songPos > -1)
		{
			editor.editPattern = (uint8_t)song.pattNum;
			editor.songPos = song.songPos;
			ui.updatePosSections = true;
		}
	}

	if (resetTimer)
		song.tick = 1;

	if (audioWasntLocked)
		unlockAudio();
}

void delta2Samp(int8_t *p, int32_t length, uint8_t smpFlags)
{
	bool sample16Bit = !!(smpFlags & SAMPLE_16BIT);
	bool stereo = !!(smpFlags & SAMPLE_STEREO);

	if (stereo)
	{
		length >>= 1;

		if (sample16Bit)
		{
			int16_t *p16L = (int16_t *)p;
			int16_t *p16R = (int16_t *)p + length;

			int16_t olds16L = 0;
			int16_t olds16R = 0;

			for (int32_t i = 0; i < length; i++)
			{
				const int16_t news16L = p16L[i] + olds16L;
				p16L[i] = news16L;
				olds16L = news16L;

				const int16_t news16R = p16R[i] + olds16R;
				p16R[i] = news16R;
				olds16R = news16R;

				p16L[i] = (int16_t)((olds16L + olds16R) >> 1);
			}
		}
		else // 8-bit
		{
			int8_t *p8L = (int8_t *)p;
			int8_t *p8R = (int8_t *)p + length;
			int8_t olds8L = 0;
			int8_t olds8R = 0;

			for (int32_t i = 0; i < length; i++)
			{
				const int8_t news8L = p8L[i] + olds8L;
				p8L[i] = news8L;
				olds8L = news8L;

				const int8_t news8R = p8R[i] + olds8R;
				p8R[i] = news8R;
				olds8R = news8R;

				p8L[i] = (int8_t)((olds8L + olds8R) >> 1);
			}
		}
	}
	else // mono (normal sample)
	{
		if (sample16Bit)
		{
			int16_t *p16 = (int16_t *)p;

			int16_t olds16L = 0;
			for (int32_t i = 0; i < length; i++)
			{
				const int16_t news16 = p16[i] + olds16L;
				p16[i] = news16;
				olds16L = news16;
			}
		}
		else // 8-bit
		{
			int8_t *p8 = (int8_t *)p;

			int8_t olds8L = 0;
			for (int32_t i = 0; i < length; i++)
			{
				const int8_t news8 = p8[i] + olds8L;
				p8[i] = news8;
				olds8L = news8;
			}
		}
	}
}

void samp2Delta(int8_t *p, int32_t length, uint8_t smpFlags)
{
	if (smpFlags & SAMPLE_16BIT)
	{
		int16_t *p16 = (int16_t *)p;

		int16_t newS16 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int16_t oldS16 = p16[i];
			p16[i] -= newS16;
			newS16 = oldS16;
		}
	}
	else // 8-bit
	{
		int8_t *p8 = (int8_t *)p;

		int8_t newS8 = 0;
		for (int32_t i = 0; i < length; i++)
		{
			const int8_t oldS8 = p8[i];
			p8[i] -= newS8;
			newS8 = oldS8;
		}
	}
}

bool allocateInstr(int16_t insNum)
{
	if (instr[insNum] != NULL)
		return false; // already allocated

	instr_t *p = (instr_t *)malloc(sizeof (instr_t));
	if (p == NULL)
		return false;

	memset(p, 0, sizeof (instr_t));
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		p->smp[i].panning = 128;
		p->smp[i].volume = 64;
	}

	setStdEnvelope(p, 0, 3);

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	instr[insNum] = p;

	if (audioWasntLocked)
		unlockAudio();

	return true;
}

void freeInstr(int32_t insNum)
{
	if (instr[insNum] == NULL)
		return; // not allocated

	pauseAudio(); // channel instrument pointers are now cleared

	sample_t *s = instr[insNum]->smp;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++) // free sample data
		freeSmpData(s);

	free(instr[insNum]);
	instr[insNum] = NULL;
	
	resumeAudio();
}

void freeAllInstr(void)
{
	pauseAudio(); // channel instrument pointers are now cleared
	for (int32_t i = 1; i <= MAX_INST; i++)
	{
		if (instr[i] != NULL)
		{
			sample_t *s = instr[i]->smp;
			for (int32_t j = 0; j < MAX_SMP_PER_INST; j++, s++) // free sample data
				freeSmpData(s);

			free(instr[i]);
			instr[i] = NULL;
		}
	}
	resumeAudio();
}

void freeSample(int16_t insNum, int16_t smpNum)
{
	if (instr[insNum] == NULL)
		return; // instrument not allocated

	pauseAudio(); // voice sample pointers are now cleared

	sample_t *s = &instr[insNum]->smp[smpNum];
	freeSmpData(s);

	memset(s, 0, sizeof (sample_t));
	s->panning = 128;
	s->volume = 64;

	resumeAudio();
}

void freeAllPatterns(void)
{
	pauseAudio();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (pattern[i] != NULL)
		{
			free(pattern[i]);
			pattern[i] = NULL;
		}
	}
	resumeAudio();
}

void setStdEnvelope(instr_t *ins, int16_t i, uint8_t type)
{
	if (ins == NULL)
		return;

	pauseMusic();

	if (type & 1)
	{
		memcpy(ins->volEnvPoints, config.stdEnvPoints[i][0], 2*2*12);
		ins->volEnvLength = (uint8_t)config.stdVolEnvLength[i];
		ins->volEnvSustain = (uint8_t)config.stdVolEnvSustain[i];
		ins->volEnvLoopStart = (uint8_t)config.stdVolEnvLoopStart[i];
		ins->volEnvLoopEnd = (uint8_t)config.stdVolEnvLoopEnd[i];
		ins->fadeout = config.stdFadeout[i];
		ins->vibRate = (uint8_t)config.stdVibRate[i];
		ins->vibDepth = (uint8_t)config.stdVibDepth[i];
		ins->vibSweep = (uint8_t)config.stdVibSweep[i];
		ins->vibType = (uint8_t)config.stdVibType[i];
		ins->volEnvFlags = (uint8_t)config.stdVolEnvFlags[i];
	}

	if (type & 2)
	{
		memcpy(ins->panEnvPoints, config.stdEnvPoints[i][1], 2*2*12);
		ins->panEnvLength = (uint8_t)config.stdPanEnvLength[0];
		ins->panEnvSustain = (uint8_t)config.stdPanEnvSustain[0];
		ins->panEnvLoopStart = (uint8_t)config.stdPanEnvLoopStart[0];
		ins->panEnvLoopEnd = (uint8_t)config.stdPanEnvLoopEnd[0];
		ins->panEnvFlags  = (uint8_t)config.stdPanEnvFlags[0];
	}

	resumeMusic();
}

void setNoEnvelope(instr_t *ins)
{
	if (ins == NULL)
		return;

	pauseMusic();

	memcpy(ins->volEnvPoints, config.stdEnvPoints[0][0], 2*2*12);
	ins->volEnvLength = (uint8_t)config.stdVolEnvLength[0];
	ins->volEnvSustain = (uint8_t)config.stdVolEnvSustain[0];
	ins->volEnvLoopStart = (uint8_t)config.stdVolEnvLoopStart[0];
	ins->volEnvLoopEnd = (uint8_t)config.stdVolEnvLoopEnd[0];
	ins->volEnvFlags = 0;

	memcpy(ins->panEnvPoints, config.stdEnvPoints[0][1], 2*2*12);
	ins->panEnvLength = (uint8_t)config.stdPanEnvLength[0];
	ins->panEnvSustain = (uint8_t)config.stdPanEnvSustain[0];
	ins->panEnvLoopStart = (uint8_t)config.stdPanEnvLoopStart[0];
	ins->panEnvLoopEnd = (uint8_t)config.stdPanEnvLoopEnd[0];
	ins->panEnvFlags = 0;

	ins->fadeout = 0;
	ins->vibRate = 0;
	ins->vibDepth = 0;
	ins->vibSweep = 0;
	ins->vibType = 0;

	resumeMusic();
}

bool patternEmpty(uint16_t pattNum)
{
	if (pattern[pattNum] == NULL)
		return true;

	const uint8_t *scanPtr = (const uint8_t *)pattern[pattNum];
	const uint32_t scanLen = patternNumRows[pattNum] * TRACK_WIDTH;

	for (uint32_t i = 0; i < scanLen; i++)
	{
		if (scanPtr[i] != 0)
			return false;
	}

	return true;
}

void updateChanNums(void)
{
	assert(!(song.numChannels & 1));

	const int32_t maxChannelsShown = getMaxVisibleChannels();

	int32_t channelsShown = song.numChannels;
	if (channelsShown > maxChannelsShown)
		channelsShown = maxChannelsShown;

	ui.numChannelsShown = (uint8_t)channelsShown;
	ui.pattChanScrollShown = (song.numChannels > maxChannelsShown);

	if (ui.patternEditorShown)
	{
		if (ui.channelOffset > song.numChannels-ui.numChannelsShown)
			setScrollBarPos(SB_CHAN_SCROLL, song.numChannels - ui.numChannelsShown, true);
	}

	if (ui.pattChanScrollShown)
	{
		if (ui.patternEditorShown)
		{
			showScrollBar(SB_CHAN_SCROLL);
			showPushButton(PB_CHAN_SCROLL_LEFT);
			showPushButton(PB_CHAN_SCROLL_RIGHT);
		}

		setScrollBarEnd(SB_CHAN_SCROLL, song.numChannels);
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

void conv8BitSample(int8_t *p, int32_t length, bool stereo) // changes sample sign
{
	if (stereo)
	{
		length >>= 1;

		int8_t *p2 = &p[length];
		for (int32_t i = 0; i < length; i++)
		{
			const int8_t l = p[i] ^ 0x80;
			const int8_t r = p2[i] ^ 0x80;

			p[i] = (int8_t)((l + r) >> 1);
		}
	}
	else
	{
		for (int32_t i = 0; i < length; i++)
			p[i] ^= 0x80;
	}
}

void conv16BitSample(int8_t *p, int32_t length, bool stereo) // changes sample sign
{
	int16_t *p16_1 = (int16_t *)p;

	if (stereo)
	{
		length >>= 1;

		int16_t *p16_2 = (int16_t *)p + length;
		for (int32_t i = 0; i < length; i++)
		{
			const int16_t l = p16_1[i] ^ 0x8000;
			const int16_t r = p16_2[i] ^ 0x8000;

			p16_1[i] = (int16_t)((l + r) >> 1);
		}
	}
	else
	{
		for (int32_t i = 0; i < length; i++)
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
		patternNumRows[i] = 64;

	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	// unmute all channels (must be done before resetChannels() call)
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		editor.chnMode[i] = 1;

	resetChannels();

	song.songLength = 1;
	song.numChannels = 8;
	editor.BPM = song.BPM = 125;
	editor.speed = song.initialSpeed = song.speed = 6;
	editor.globalVolume = song.globalVolume = 64;
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
	instr[0]->smp[0].volume = 0;

	if (!allocateInstr(130))
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}
	memset(instr[130], 0, sizeof (instr_t));

	if (!allocateInstr(131)) // Instr. Ed. display instrument for unallocated/empty instruments
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	memset(instr[131], 0, sizeof (instr_t));
	for (int32_t i = 0; i < 16; i++)
		instr[131]->smp[i].panning = 128;

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
	if (song.speed == 0)
		song.speed = song.initialSpeed;

	audio.tickSampleCounter64 = 0; // zero tick sample counter so that it will instantly initiate a tick

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
		for (uint8_t i = 0; i < MAX_CHANNELS; i++)
			playTone(i, 0, NOTE_OFF, -1, 0, 0);
	}

	// if song was playing, update local row (fixes certain glitches)
	if (songWasPlaying)
		editor.row = song.row;

#ifdef HAS_MIDI
	midi.currMIDIVibDepth = 0;
	midi.currMIDIPitch = 0;
#endif

	memset(editor.keyOnTab, 0, sizeof (editor.keyOnTab));

	// kludge to prevent UI from being out of sync with replayer vars on stop
	song.songPos = editor.songPos;
	song.row = editor.row;
	song.pattNum = editor.editPattern;

	ui.updatePosSections = true;
	ui.updatePatternEditor = true;

	// certain non-FT2 fixes
	song.tick = editor.tick = 1;
	song.globalVolume = editor.globalVolume = 64;
	ui.drawGlobVolFlag = true;
}

// from keyboard/smp. ed.
void playTone(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol, uint16_t midiVibDepth, uint16_t midiPitch)
{
	instr_t *ins = instr[insNum];
	if (ins == NULL)
		return;

	assert(chNum < MAX_CHANNELS && insNum <= MAX_INST && note <= NOTE_OFF);
	channel_t *ch = &channel[chNum];

	// FT2 bugfix: Don't play tone if certain requirements are not met
	if (note != NOTE_OFF)
	{
		if (note == 0 || note > 96)
			return;

		sample_t *s = &ins->smp[ins->note2SampleLUT[note-1] & 0xF];

		int16_t finalNote = (int16_t)note + s->relativeNote;
		if (s->dataPtr == NULL || s->length == 0 || finalNote <= 0 || finalNote >= 12*10)
			return;
	}
	// -------------------

	lockAudio();

	if (insNum != 0 && note != NOTE_OFF)
	{
		ch->noteData = (insNum << 8) | (ch->noteData & 0xFF);
		ch->instrNum = insNum;
	}

	ch->noteData = (ch->noteData & 0xFF00) | note;
	ch->efx = 0;
	ch->efxData = 0;

	startTone(note, 0, 0, ch);

	if (note != NOTE_OFF)
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

	updateChannel(ch);

	unlockAudio();
}

// smp. ed.
void playSample(uint8_t chNum, uint8_t insNum, uint8_t smpNum, uint8_t note, uint16_t midiVibDepth, uint16_t midiPitch)
{
	if (instr[insNum] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[chNum].instrNum = 255;
	lastChInstr[chNum].smpNum = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(chNum < MAX_CHANNELS && insNum <= MAX_INST && smpNum < MAX_SMP_PER_INST && note <= NOTE_OFF);
	channel_t *ch = &channel[chNum];

	memcpy(&instr[130]->smp[0], &instr[insNum]->smp[smpNum], sizeof (sample_t));

	uint8_t vol = instr[insNum]->smp[smpNum].volume;
	
	lockAudio();

	ch->instrNum = 130;
	ch->noteData = (ch->instrNum << 8) | note;
	ch->efx = 0;

	startTone(note, 0, 0, ch);

	if (note != NOTE_OFF)
	{
		retrigVolume(ch);
		retrigEnvelopeVibrato(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	updateChannel(ch);

	unlockAudio();

	while (ch->status & IS_Trigger); // wait for sample to latch in mixer

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = editor.curInstr;
	editor.curPlaySmp = editor.curSmp;
}

// smp. ed.
void playRange(uint8_t chNum, uint8_t insNum, uint8_t smpNum, uint8_t note, uint16_t midiVibDepth, uint16_t midiPitch, int32_t smpOffset, int32_t length)
{
	if (instr[insNum] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[chNum].instrNum = 255;
	lastChInstr[chNum].smpNum = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(chNum < MAX_CHANNELS && insNum <= MAX_INST && smpNum < MAX_SMP_PER_INST && note <= NOTE_OFF);

	channel_t *ch = &channel[chNum];
	sample_t *s = &instr[130]->smp[0];

	memcpy(s, &instr[insNum]->smp[smpNum], sizeof (sample_t));

	uint8_t vol = instr[insNum]->smp[smpNum].volume;

	lockAudio();

	s->length = smpOffset + length;
	s->loopStart = 0;
	s->loopLength = 0;
	DISABLE_LOOP(s->flags); // disable loop on sample #129 (placeholder)

	int32_t samplePlayOffset = smpOffset;

	ch->instrNum = 130;
	ch->noteData = (ch->instrNum << 8) | note;
	ch->efx = 0;
	ch->efxData = 0;

	startTone(note, 0, 0, ch);

	ch->smpStartPos = samplePlayOffset;

	if (note != NOTE_OFF)
	{
		retrigVolume(ch);
		retrigEnvelopeVibrato(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	updateChannel(ch);

	unlockAudio();

	while (ch->status & IS_Trigger); // wait for sample to latch in mixer

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = editor.curInstr;
	editor.curPlaySmp = editor.curSmp;
}

void stopVoices(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	channel_t *ch = channel;
	for (int32_t i = 0; i < MAX_CHANNELS; i++, ch++)
	{
		lastChInstr[i].smpNum = 255;
		lastChInstr[i].instrNum = 255;

		ch->noteData = 0;
		ch->relativeNote = 0;
		ch->smpNum = 0;
		ch->smpPtr = NULL;
		ch->instrNum = 0;
		ch->instrPtr = instr[0]; // important: set instrument pointer to instr 0 (placeholder instrument)
		ch->status = IS_Vol;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->fFinalVol = 0.0f;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;
		ch->vibDepth = 0;
		ch->midiVibDepth = 0;
		ch->midiPitch = 0;
		ch->portaDirection = 0; // FT2 bugfix: weird 3xx behavior if not used with note

		stopVoice(i);
	}

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	stopAllScopes();
	resetAudioDither();
	resetCachedMixerVars();

	// wait for scope thread to finish, so that we know pointers aren't deprecated
	while (editor.scopeThreadBusy);

	if (audioWasntLocked)
		unlockAudio();
}

void resetReplayerState(void)
{
	song.pattDelTime = song.pattDelTime2 = 0;
	song.posJumpFlag = false;
	song.pBreakPos = 0;
	song.pBreakFlag = false;

	// reset pattern loops (E6x)
	channel_t *ch = channel;
	for (int32_t i = 0; i < song.numChannels; i++, ch++)
	{
		ch->jumpToRow = 0;
		ch->patLoopCounter = 0;
	}
	
	// reset global volume (if song was playing)
	if (songPlaying)
	{
		song.globalVolume = 64;

		ch = channel;
		for (int32_t i = 0; i < song.numChannels; i++, ch++)
			ch->status |= IS_Vol;
	}
}

void setNewSongPos(int32_t pos)
{
	resetReplayerState(); // FT2 bugfix
	setPos((int16_t)pos, 0, true);

	// non-FT2 fix: If song speed was 0, set it back to initial speed
	if (song.speed == 0)
		song.speed = song.initialSpeed;
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
	if (song.songPos == song.songLength-1)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos < song.songLength-1)
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
	uint8_t scopeUpdateStatus[MAX_CHANNELS];

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

		for (int32_t i = 0; i < song.numChannels; i++)
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

	editor.tick = pattSyncEntry->tick;

	if (editor.BPM != pattSyncEntry->BPM)
	{
		editor.BPM = pattSyncEntry->BPM;
		ui.drawBPMFlag = true;
	}

	if (editor.speed != pattSyncEntry->speed)
	{
		editor.speed = pattSyncEntry->speed;
		ui.drawSpeedFlag = true;
	}

	if (editor.globalVolume != pattSyncEntry->globalVolume)
	{
		editor.globalVolume = pattSyncEntry->globalVolume;
		ui.drawGlobVolFlag = true;
	}

	if (editor.songPos != pattSyncEntry->songPos)
	{
		editor.songPos = pattSyncEntry->songPos;
		ui.drawPosEdFlag = true;
	}

	// somewhat of a kludge...
	if (editor.tmpPattern != pattSyncEntry->pattNum || editor.row != pattSyncEntry->row)
	{
		// set pattern number
		editor.editPattern = editor.tmpPattern = pattSyncEntry->pattNum;
		checkMarkLimits();
		ui.drawPattNumLenFlag = true;

		// set row
		editor.row = (uint8_t)pattSyncEntry->row;
		ui.updatePatternEditor = true;
	}
}
