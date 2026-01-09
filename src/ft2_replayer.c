/*
** C port of FT2 replayer
**
** Some stuff is done differently, but the outcome is the same
** (except for volume and mixer deltas being calculated with higher precision).
*/

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
#include "mixer/ft2_mix_interpolation.h"

static uint64_t logTab[4*12*16], scopeLogTab[4*12*16], scopeDrawLogTab[4*12*16];
static uint64_t amigaPeriodDiv, scopeAmigaPeriodDiv, scopeDrawAmigaPeriodDiv;
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
const uint16_t *note2PeriodLUT = NULL;
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

void resetReplayerState(void)
{
	song.pattDelTime = song.pattDelTime2 = 0;
	song.posJumpFlag = false;
	song.pBreakPos = 0;
	song.pBreakFlag = false;

	channel_t *ch = channel;
	for (int32_t i = 0; i < song.numChannels; i++, ch++)
	{
		ch->patternLoopStartRow = 0;
		ch->patternLoopCounter = 0;
	}
	
	// reset global volume (if song was playing)
	if (songPlaying)
	{
		song.globalVolume = 64;

		ch = channel;
		for (int32_t i = 0; i < song.numChannels; i++, ch++)
			ch->status |= CS_UPDATE_VOL;
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
		ch->status = CS_UPDATE_VOL;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;

		ch->channelOff = editor.channelMuted[i]; // set channel mute flag from global mute flag
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

void setSampleC4Hz(sample_t *s, double dC4Hz)
{
	/* Sets a sample's relative note and finetune according to input C-4 rate.
	**
	** Note:
	** This algorithm uses only 5 finetune bits (like FT2 internally),
	** so that the resulting finetune is the same when loading it in a
	** tracker that does support the full 8 finetune bits.
	*/

	const double dC4PeriodOffset = (NOTE_C4 * 16) + 16;
	int32_t period = (int32_t)round(dC4PeriodOffset + (log2(dC4Hz / C4_FREQ) * 12.0 * 16.0));

	// Hi-limit is A#9 at highest finetune. B-9 is bugged in FT2, don't include it.
	period = CLAMP(period, 0, (12 * 16 * 10) - 1);

	s->finetune = ((period & 31) - 16) << 3; // 0..31 -> -128..120
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

double dPeriod2Hz(uint32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0.0; // in FT2, a period of 0 results in 0Hz

	if (audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF; // mask needed for FT2 period overflow quirk

		const uint32_t quotient  = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);

		return dLogTab[remainder] * dExp2MulTab[(14-quotient) & 31]; // x = y >> ((14-quotient) & 31);
	}
	else
	{
		return (8363.0 * 1712.0) / (int32_t)period;
	}
}

uint64_t period2VoiceDelta(uint32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0; // in FT2, a period of 0 results in 0Hz

	if (audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF; // mask needed for FT2 period overflow quirk

		const uint32_t quotient  = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);

		return logTab[remainder] >> ((14-quotient) & 31);
	}
	else
	{
		return amigaPeriodDiv / period;
	}
}

uint64_t period2ScopeDelta(uint32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0; // in FT2, a period of 0 results in 0Hz

	if (audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF; // mask needed for FT2 period overflow quirk

		const uint32_t quotient  = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);

		return scopeLogTab[remainder] >> ((14-quotient) & 31);
	}
	else
	{
		return scopeAmigaPeriodDiv / period;
	}
}

uint64_t period2ScopeDrawDelta(uint32_t period)
{
	period &= 0xFFFF; // just in case (actual period range is 0..65535)

	if (period == 0)
		return 0; // in FT2, a period of 0 results in 0Hz

	if (audio.linearPeriodsFlag)
	{
		const uint32_t invPeriod = ((12 * 192 * 4) - period) & 0xFFFF; // mask needed for FT2 period overflow quirk

		const uint32_t quotient  = invPeriod / (12 * 16 * 4);
		const uint32_t remainder = invPeriod % (12 * 16 * 4);

		return scopeDrawLogTab[remainder] >> ((14-quotient) & 31);
	}
	else
	{
		return scopeDrawAmigaPeriodDiv / period;
	}
}

// returns *exact* FT2 C-4 voice rate (depending on finetune, relativeNote and linear/Amiga period mode)
double getSampleC4Rate(sample_t *s)
{
	int32_t note = NOTE_C4 + s->relativeNote;
	if (note < 0)
		return -1; // shouldn't happen (just in case...)

	if (note >= (10*12)-1)
		return -1; // B-9 (after relativeNote calculation) = illegal! (won't play in replayer)

	const int32_t C4Period = (note << 4) + (((int8_t)s->finetune >> 3) + 16);

	const int32_t period = audio.linearPeriodsFlag ? linearPeriodLUT[C4Period] : amigaPeriodLUT[C4Period];
	return dPeriod2Hz(period);
}

void setLinearPeriods(bool linearPeriodsFlag)
{
	pauseAudio();

	audio.linearPeriodsFlag = linearPeriodsFlag;

	if (audio.linearPeriodsFlag)
		note2PeriodLUT = linearPeriodLUT;
	else
		note2PeriodLUT = amigaPeriodLUT;

	resumeAudio();

	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
	{
		// update "frequency slides" radiobutton, if it's shown
		setConfigAudioRadioButtonStates();
	}

	// update mid-C freq. in instr. editor (it can slightly differ between Amiga/linear)
	if (ui.instEditorShown)
		drawC4Rate();
}

void resetVolumes(channel_t *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;

	ch->status |= CS_UPDATE_VOL + CS_UPDATE_PAN + CS_USE_QUICK_VOLRAMP;
}

void triggerInstrument(channel_t *ch)
{
	if (!(ch->vibTremCtrl & 0x04)) ch->vibratoPos = 0;
	if (!(ch->vibTremCtrl & 0x40)) ch->tremoloPos = 0;

	ch->noteRetrigCounter = 0;
	ch->tremorPos = 0;
	ch->keyOff = false;

	instr_t *ins = ch->instrPtr;
	if (ins != NULL) // just in case
	{
		// reset volume envelope
		if (ins->volEnvFlags & ENV_ENABLED)
		{
			ch->volEnvTick = 65535; // will be increased to 0 on envelope handling
			ch->volEnvPos = 0;
		}

		// reset panning envelope
		if (ins->panEnvFlags & ENV_ENABLED)
		{
			ch->panEnvTick = 65535; // will be increased to 0 on envelope handling
			ch->panEnvPos = 0;
		}

		// reset fadeout
		ch->fadeoutSpeed = ins->fadeout; // warning: FT2 doesn't check if fadeout is more than 4095!
		ch->fadeoutVol = 32768;

		// reset auto-vibrato
		if (ins->autoVibDepth > 0)
		{
			ch->autoVibPos = 0;

			if (ins->autoVibSweep > 0)
			{
				ch->autoVibAmp = 0;
				ch->autoVibSweep = (ins->autoVibDepth << 8) / ins->autoVibSweep;
			}
			else
			{
				ch->autoVibAmp = ins->autoVibDepth << 8;
				ch->autoVibSweep = 0;
			}
		}
	}
}

void keyOff(channel_t *ch)
{
	ch->keyOff = true;

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
		ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
	}

	if (!(ins->panEnvFlags & ENV_ENABLED)) // FT2 logic bug!
	{
		if (ch->panEnvTick >= (uint16_t)ins->panEnvPoints[ch->panEnvPos][0])
			ch->panEnvTick = ins->panEnvPoints[ch->panEnvPos][0] - 1;
	}
}

void calcReplayerVars(int32_t audioFreq)
{
	assert(audioFreq > 0);
	if (audioFreq <= 0)
		return;

	const double logTabMul = (UINT32_MAX+1.0) / audioFreq;
	for (int32_t i = 0; i < 4*12*16; i++)
		logTab[i] = (uint64_t)round(dLogTab[i] * logTabMul);

	amigaPeriodDiv = (uint64_t)round((MIXER_FRAC_SCALE * (1712.0*8363.0)) / audioFreq);

	audio.quickVolRampSamples = (uint32_t)round(audioFreq / (1000.0 / FT2_QUICK_VOLRAMP_MILLISECONDS));
	audio.fQuickVolRampSamplesMul = (float)(1.0 / audio.quickVolRampSamples);

	for (int32_t bpm = MIN_BPM; bpm <= MAX_BPM; bpm++)
	{
		const int32_t i = bpm - MIN_BPM; // index for tables

		const double dBpmHz = bpm / 2.5;
		const double dSamplesPerTick = audioFreq / dBpmHz;

		double dSamplesPerTickInt;
		double dSamplesPerTickFrac = modf(dSamplesPerTick, &dSamplesPerTickInt);

		audio.samplesPerTickIntTab[i] = (uint32_t)dSamplesPerTickInt;
		audio.samplesPerTickFracTab[i] = (uint64_t)((dSamplesPerTickFrac * BPM_FRAC_SCALE) + 0.5); // rounded

		// BPM Hz -> tick length for performance counter (syncing visuals to audio)
		double dTimeInt;
		double dTimeFrac = modf(editor.dPerfFreq / dBpmHz, &dTimeInt);

		audio.tickTimeIntTab[i] = (uint32_t)dTimeInt;
		audio.tickTimeFracTab[i] = (uint64_t)((dTimeFrac * TICK_TIME_FRAC_SCALE) + 0.5); // rounded
	}
}

// for piano in Instr. Ed. (values outside 0..95 can and will happen)
int32_t getPianoKey(int32_t period, int8_t finetune, int8_t relativeNote)
{
	if (audio.linearPeriodsFlag)
	{
		period >>= 2;
		period += (int8_t)finetune >> 3;

		return (((10*12*16) - period) >> 4) - relativeNote;
	}
	else
	{
		// amiga periods, requires slower method (binary search in amiga period LUT)

		if (period > amigaPeriodLUT[0])
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

			if (period >= amigaPeriodLUT[lookUp])
				hiPeriod = (tmpPeriod - finetune) & ~15;
			else
				loPeriod = (tmpPeriod - finetune) & ~15;
		}

		return (loPeriod >> 4) - relativeNote;
	}
}

void triggerNote(uint8_t note, uint8_t efx, uint8_t efxData, channel_t *ch)
{
	if (note == NOTE_OFF)
	{
		keyOff(ch);
		return;
	}

	// If we came from effect Rxy (retrig), we didn't check note yet. Do it here.
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

	if (note > 96) // non-FT2 sanity check
		note = 96;

	ch->smpNum = ins->note2SampleLUT[note-1] & 0xF; // FT2 doesn't mask here, but let's do it anyway
	sample_t *s = &ins->smp[ch->smpNum];

	ch->smpPtr = s;
	ch->relativeNote = s->relativeNote;

	note += ch->relativeNote;
	if (note >= 10*12) // unsigned check (also handles note < 0)
		return;

	ch->oldVol = s->volume;
	ch->oldPan = s->panning;

	if (efx == 0xE && (efxData & 0xF0) == 0x50) // E5x (Set Finetune)
		ch->finetune = ((efxData & 0x0F) * 16) - 128;
	else
		ch->finetune = s->finetune;

	if (note != 0)
	{
		const uint16_t noteIndex = ((note-1) * 16) + (((int8_t)ch->finetune >> 3) + 16); // 0..1920

		assert(note2PeriodLUT != NULL);
		ch->outPeriod = ch->realPeriod = note2PeriodLUT[noteIndex];
	}

	ch->status |= CF_UPDATE_PERIOD + CS_UPDATE_VOL + CS_UPDATE_PAN + CS_TRIGGER_VOICE + CS_USE_QUICK_VOLRAMP;

	if (efx == 9) // 9xx (Set Sample Offset)
	{
		if (efxData > 0)
			ch->sampleOffset = ch->efxData;

		ch->smpStartPos = ch->sampleOffset << 8;
	}
	else
	{
		ch->smpStartPos = 0; // no 9xx, reset sample offset
	}
}

static void volSlide(channel_t *ch, uint8_t param);
static void doVibrato(channel_t *ch);
static void portamento(channel_t *ch, uint8_t param);

static void dummy(channel_t *ch, uint8_t param)
{
	(void)ch;
	(void)param;
	return;
}

static void finePitchSlideUp(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPitchSlideUpSpeed;

	ch->fPitchSlideUpSpeed = param;

	ch->realPeriod -= param * 4;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= CF_UPDATE_PERIOD;
}

static void finePitchSlideDown(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPitchSlideDownSpeed;

	ch->fPitchSlideDownSpeed = param;

	ch->realPeriod += param * 4;
	if ((int16_t)ch->realPeriod >= 32000) // FT2 bug, should've been unsigned comparison
		ch->realPeriod = 32000-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= CF_UPDATE_PERIOD;
}

static void setPortamentoCtrl(channel_t *ch, uint8_t param)
{
	ch->semitonePortaMode = (param != 0);
}

static void setVibratoCtrl(channel_t *ch, uint8_t param)
{
	ch->vibTremCtrl = (ch->vibTremCtrl & 0xF0) | param;
}

static void patternLoop(channel_t *ch, uint8_t param)
{
	if (param == 0)
	{
		ch->patternLoopStartRow = song.row & 0xFF;
	}
	else if (ch->patternLoopCounter == 0)
	{
		ch->patternLoopCounter = param;

		song.pBreakPos = ch->patternLoopStartRow;
		song.pBreakFlag = true;
	}
	else if (--ch->patternLoopCounter > 0)
	{
		song.pBreakPos = ch->patternLoopStartRow;
		song.pBreakFlag = true;
	}
}

static void setTremoloCtrl(channel_t *ch, uint8_t param)
{
	ch->vibTremCtrl = (param << 4) | (ch->vibTremCtrl & 0x0F);
}

static void fineVolSlideUp(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideUpSpeed;

	ch->fVolSlideUpSpeed = param;

	ch->realVol += param;
	if (ch->realVol > 64)
		ch->realVol = 64;

	ch->outVol = ch->realVol;
	ch->status |= CS_UPDATE_VOL;
}

static void fineVolFineDown(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideDownSpeed;

	ch->fVolSlideDownSpeed = param;

	ch->realVol -= param;
	if ((int8_t)ch->realVol < 0)
		ch->realVol = 0;

	ch->outVol = ch->realVol;
	ch->status |= CS_UPDATE_VOL;
}

static void noteCut0(channel_t *ch, uint8_t param)
{
	if (param == 0) // only a parameter of zero is handled here
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
	}
}

static void patternDelay(channel_t *ch, uint8_t param)
{
	if (song.pattDelTime2 == 0)
		song.pattDelTime = param + 1;

	(void)ch;
}

static const efxRoutine EJumpTab_TickZero[16] =
{
	dummy,				// 0
	finePitchSlideUp,	// 1
	finePitchSlideDown,	// 2
	setPortamentoCtrl,	// 3
	setVibratoCtrl,		// 4
	dummy,				// 5
	patternLoop,		// 6
	setTremoloCtrl,		// 7
	dummy,				// 8
	dummy,				// 9
	fineVolSlideUp,		// A
	fineVolFineDown,	// B
	noteCut0,			// C
	dummy,				// D
	patternDelay,		// E
	dummy				// F
};

static void E_Effects_TickZero(channel_t *ch, uint8_t param)
{
	const uint8_t efx = param >> 4;
	param &= 0x0F;

	if (ch->channelOff) // channel is muted, only handle certain E effects
	{
		if (efx == 0x6)
			patternLoop(ch, param);
		else if (efx == 0xE)
			patternDelay(ch, param);

		return;
	}

	EJumpTab_TickZero[efx](ch, param);
}

static void positionJump(channel_t *ch, uint8_t param)
{
	if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
	{
		const int16_t pos = (int16_t)param - 1;
		if (pos < 0 || pos >= song.songLength)
			bxxOverflow = true; // security fix not present in FT2...
		else
			song.songPos = pos;
	}

	song.pBreakPos = 0;
	song.posJumpFlag = true;

	(void)ch;
}

static void patternBreak(channel_t *ch, uint8_t param)
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

	// update all voice volumes
	channel_t *c = channel;
	for (int32_t i = 0; i < song.numChannels; i++, c++)
		c->status |= CS_UPDATE_VOL;

	(void)ch;
}

static void setEnvelopePos(channel_t *ch, uint8_t param)
{
	bool envUpdate;
	int8_t point;
	int32_t tick;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	// (envelope precision has been upgraded from .8fp to single-precision float)

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
					if (tick == 0) // FT2 doesn't test for <= 0 here
					{
						envUpdate = false;
						break;
					}

					const int32_t xDiff = ins->volEnvPoints[point+1][0] - ins->volEnvPoints[point+0][0];
					if (xDiff <= 0)
					{
						envUpdate = true;
						break;
					}

					const int32_t y0 = ins->volEnvPoints[point+0][1] & 0xFF;
					const int32_t y1 = ins->volEnvPoints[point+1][1] & 0xFF;
					const int32_t yDiff = y1 - y0;

					ch->fVolEnvDelta = (float)yDiff / (float)xDiff;
					ch->fVolEnvValue = (float)y0 + (ch->fVolEnvDelta * (tick-1));

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
			ch->fVolEnvDelta = 0.0f;
			ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[point][1] & 0xFF);
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
	if (ins->volEnvFlags & ENV_SUSTAIN) // FT2 logic bug: should've been ins->panEnvFlags
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
					if (tick == 0) // FT2 doesn't test for <= 0 here
					{
						envUpdate = false;
						break;
					}

					const int32_t xDiff = ins->panEnvPoints[point+1][0] - ins->panEnvPoints[point+0][0];
					if (xDiff <= 0)
					{
						envUpdate = true;
						break;
					}

					const int32_t y0 = ins->panEnvPoints[point+0][1] & 0xFF;
					const int32_t y1 = ins->panEnvPoints[point+1][1] & 0xFF;
					const int32_t yDiff = y1 - y0;

					ch->fPanEnvDelta = (float)yDiff / (float)xDiff;
					ch->fPanEnvValue = (float)y0 + (ch->fPanEnvDelta * (tick-1));

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
			ch->fPanEnvDelta = 0.0f;
			ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[point][1] & 0xFF);
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
	dummy,				// 0
	dummy,				// 1
	dummy,				// 2
	dummy,				// 3
	dummy,				// 4
	dummy,				// 5
	dummy,				// 6
	dummy,				// 7
	dummy,				// 8
	dummy,				// 9
	dummy,				// A
	positionJump,		// B
	dummy,				// C
	patternBreak,		// D
	E_Effects_TickZero,	// E
	setSpeed,			// F
	setGlobalVolume,	// G
	dummy,				// H
	dummy,				// I
	dummy,				// J
	dummy,				// K
	setEnvelopePos,		// L
	dummy,				// M
	dummy,				// N
	dummy,				// O
	dummy,				// P
	dummy,				// Q
	dummy,				// R
	dummy,				// S
	dummy,				// T
	dummy,				// U
	dummy,				// V
	dummy,				// W
	dummy,				// X
	dummy,				// Y
	dummy 				// Z
};

static void handleMoreEffects_TickZero(channel_t *ch) // called even if channel is muted!
{
	if (ch->efx > 35)
		return;

	JumpTab_TickZero[ch->efx](ch, ch->efxData);
}

/* -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy effect (multiNoteRetrig)
*/

static void v_SetVibSpeed(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (ch->volColumnVol & 0x0F) * 4;
	if (*volColumnData != 0)
		ch->vibratoSpeed = *volColumnData;
}

static void v_SetVolume(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData -= 16;
	if (*volColumnData > 64) // unnecessary check, but keep it just in case...
		*volColumnData = 64;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
}

static void v_FineVolSlideDown(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (uint8_t)(0 - (ch->volColumnVol & 0x0F)) + ch->realVol;
	if ((int8_t)*volColumnData < 0)
		*volColumnData = 0;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= CS_UPDATE_VOL;
}

static void v_FineVolSlideUp(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData = (ch->volColumnVol & 0x0F) + ch->realVol;
	if (*volColumnData > 64)
		*volColumnData = 64;

	ch->outVol = ch->realVol = *volColumnData;
	ch->status |= CS_UPDATE_VOL;
}

static void v_SetPan(channel_t *ch, uint8_t *volColumnData)
{
	*volColumnData <<= 4;

	ch->outPan = *volColumnData;
	ch->status |= CS_UPDATE_PAN;
}

// -- non-tick-zero volume column effects --

static void v_VolSlideDown(channel_t *ch)
{
	uint8_t newVol = (uint8_t)(0 - (ch->volColumnVol & 0x0F)) + ch->realVol;
	if ((int8_t)newVol < 0)
		newVol = 0;

	ch->outVol = ch->realVol = newVol;
	ch->status |= CS_UPDATE_VOL;
}

static void v_VolSlideUp(channel_t *ch)
{
	uint8_t newVol = (ch->volColumnVol & 0x0F) + ch->realVol;
	if (newVol > 64)
		newVol = 64;

	ch->outVol = ch->realVol = newVol;
	ch->status |= CS_UPDATE_VOL;
}

static void v_Vibrato(channel_t *ch)
{
	const uint8_t param = ch->volColumnVol & 0xF;
	if (param > 0)
		ch->vibratoDepth = param;

	doVibrato(ch);
}

static void v_PanSlideLeft(channel_t *ch)
{
	uint16_t tmp16 = ch->outPan + (uint8_t)(0 - (ch->volColumnVol & 0x0F));
	if (tmp16 < 256) // includes an FT2 bug: pan-slide-left of 0 = set pan to 0
		tmp16 = 0;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= CS_UPDATE_PAN;
}

static void v_PanSlideRight(channel_t *ch)
{
	uint16_t tmp16 = ch->outPan + (ch->volColumnVol & 0x0F);
	if (tmp16 > 255)
		tmp16 = 255;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= CS_UPDATE_PAN;
}

static void v_Portamento(channel_t *ch)
{
	portamento(ch, 0); // the last parameter is actually not used in portamento()
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
	v_dummy,        v_dummy,         v_dummy, v_dummy,
	v_dummy,        v_dummy,  v_VolSlideDown, v_VolSlideUp,
	v_dummy,        v_dummy,         v_dummy, v_Vibrato,
	v_dummy, v_PanSlideLeft, v_PanSlideRight, v_Portamento
};

static const volColumnEfxRoutine2 VJumpTab_TickZero[16] =
{
	          v_dummy2,      v_SetVolume,   v_SetVolume, v_SetVolume,
	       v_SetVolume,      v_SetVolume,      v_dummy2, v_dummy2,
	v_FineVolSlideDown, v_FineVolSlideUp, v_SetVibSpeed, v_dummy2,
	          v_SetPan,         v_dummy2,      v_dummy2, v_dummy2
};

static void setPan(channel_t *ch, uint8_t param)
{
	ch->outPan = param;
	ch->status |= CS_UPDATE_PAN;
}

static void setVol(channel_t *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	ch->outVol = ch->realVol = param;
	ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
}

static void extraFinePitchSlide(channel_t *ch, uint8_t param)
{
	const uint8_t slideType = param >> 4;
	param &= 0x0F;

	if (slideType == 1) // slide up
	{
		if (param == 0)
			param = ch->efPitchSlideUpSpeed;

		ch->efPitchSlideUpSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod -= param;
		if ((int16_t)newPeriod < 1)
			newPeriod = 1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= CF_UPDATE_PERIOD;
	}
	else if (slideType == 2) // slide down
	{
		if (param == 0)
			param = ch->efPitchSlideDownSpeed;

		ch->efPitchSlideDownSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod += param;
		if ((int16_t)newPeriod >= 32000) // FT2 bug, should've been unsigned comparison
			newPeriod = 32000-1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= CF_UPDATE_PERIOD;
	}
}

// "param" is never used (needed for efx jumptable structure)
static void doMultiNoteRetrig(channel_t *ch, uint8_t param)
{
	uint8_t cnt = ch->noteRetrigCounter + 1;
	if (cnt < ch->noteRetrigSpeed)
	{
		ch->noteRetrigCounter = cnt;
		return;
	}

	ch->noteRetrigCounter = 0;

	int16_t vol = ch->realVol;
	switch (ch->noteRetrigVol)
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

	triggerNote(0, 0, 0, ch);

	(void)param;
}

static void multiNoteRetrig(channel_t *ch, uint8_t param, uint8_t volumeColumnData)
{
	uint8_t tmpParam;

	tmpParam = param & 0x0F;
	if (tmpParam == 0)
		tmpParam = ch->noteRetrigSpeed;

	ch->noteRetrigSpeed = tmpParam;

	tmpParam = param >> 4;
	if (tmpParam == 0)
		tmpParam = ch->noteRetrigVol;

	ch->noteRetrigVol = tmpParam;

	if (volumeColumnData == 0)
		doMultiNoteRetrig(ch, 0); // the second parameter is never used (needed for efx jumptable structure)
}

static void handleEffects_TickZero(channel_t *ch)
{
	// volume column effects

	// FT2 quirk: this one is changed by vol column effects, then used for a Rxy (multiNoteRetrig) check
	uint8_t newVolCol = ch->volColumnVol;

	VJumpTab_TickZero[ch->volColumnVol >> 4](ch, &newVolCol);

	// normal effects
	const uint8_t param = ch->efxData;
	if (ch->efx == 0 && param == 0)
		return; // no effect

	     if (ch->efx ==  8) setPan(ch, param);
	else if (ch->efx == 12) setVol(ch, param);
	else if (ch->efx == 27) multiNoteRetrig(ch, param, newVolCol);
	else if (ch->efx == 33) extraFinePitchSlide(ch, param);

	handleMoreEffects_TickZero(ch);
}

static void preparePortamento(channel_t *ch, const note_t *p, uint8_t inst)
{
	if (p->note > 0)
	{
		if (p->note == NOTE_OFF)
		{
			keyOff(ch);
		}
		else
		{
			const uint16_t note = (((p->note-1) + ch->relativeNote) * 16) + (((int8_t)ch->finetune >> 3) + 16);
			if (note < MAX_NOTES)
			{
				assert(note2PeriodLUT != NULL);
				ch->portamentoTargetPeriod = note2PeriodLUT[note];

				if (ch->portamentoTargetPeriod == ch->realPeriod)
					ch->portamentoDirection = 0;
				else if (ch->portamentoTargetPeriod > ch->realPeriod)
					ch->portamentoDirection = 1;
				else
					ch->portamentoDirection = 2;
			}
		}
	}

	if (inst > 0)
	{
		resetVolumes(ch);
		if (p->note != NOTE_OFF)
			triggerInstrument(ch);
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
			ch->status |= CF_UPDATE_PERIOD;
		}
	}
	else
	{
		// if we have a vibrato on previous row (ch) that ends at current row (p), set period back
		if ((ch->efx == 4 || ch->efx == 6) && (p->efx != 4 && p->efx != 6))
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= CF_UPDATE_PERIOD;
		}
	}

	ch->efx = p->efx;
	ch->efxData = p->efxData;
	ch->copyOfInstrAndNote = (p->instr << 8) | p->note;

	if (ch->channelOff) // channel is muted, only handle certain effects
	{
		handleMoreEffects_TickZero(ch);
		return;
	}

	// this "inst" variable is used for later if-checks...
	uint8_t inst = p->instr;
	if (inst > 0)
	{
		if (inst <= MAX_INST)
			ch->instrNum = inst;
		else
			inst = 0;
	}

	if (p->efx == 0x0E && p->efxData >= 0xD1 && p->efxData <= 0xDF)
		return; // we have a note delay (ED1..EDF)

	// only handle effects here while no E90 (retrigger note w/ parameter zero)
	if (p->efx != 0x0E || p->efxData != 0x90)
	{
		if ((ch->volColumnVol & 0xF0) == 0xF0) // gxx
		{
			const uint8_t param = ch->volColumnVol & 0x0F;
			if (param > 0)
				ch->portamentoSpeed = (param << 4) * 4;

			preparePortamento(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->efx == 3 || p->efx == 5) // 3xx or 5xx
		{
			if (p->efx != 5 && p->efxData != 0)
				ch->portamentoSpeed = p->efxData * 4;

			preparePortamento(ch, p, inst);
			handleEffects_TickZero(ch);
			return;
		}

		if (p->efx == 0x14 && p->efxData == 0) // K00 (KeyOff - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				resetVolumes(ch);

			handleEffects_TickZero(ch);
			return;
		}

		if (p->note == 0)
		{
			if (inst > 0)
			{
				resetVolumes(ch);
				triggerInstrument(ch);
			}

			handleEffects_TickZero(ch);
			return;
		}
	}

	if (p->note == NOTE_OFF)
		keyOff(ch);
	else
		triggerNote(p->note, p->efx, p->efxData, ch);

	if (inst > 0)
	{
		resetVolumes(ch);
		if (p->note != NOTE_OFF)
			triggerInstrument(ch);
	}

	handleEffects_TickZero(ch);
}

void updateVolPanAutoVib(channel_t *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	float fEnvVal, fVol;

	instr_t *ins = ch->instrPtr;
	assert(ins != NULL);

	// *** FADEOUT ON KEY OFF ***
	if (ch->keyOff)
	{
		if (ch->fadeoutSpeed > 0) // 0..4095
		{
			ch->fadeoutVol -= ch->fadeoutSpeed;
			if (ch->fadeoutVol <= 0)
			{
				ch->fadeoutVol = 0;
				ch->fadeoutSpeed = 0;
			}
		}

		ch->status |= CS_UPDATE_VOL; // always update volume, even if fadeout has reached 0
	}
	
	// handle volumes

	if (!ch->mute)
	{
		// (envelope precision has been upgraded from .8fp to single-precision float)

		// *** VOLUME ENVELOPE ***
		fEnvVal = 0.0f;
		if (ins->volEnvFlags & ENV_ENABLED)
		{
			envDidInterpolate = false;
			envPos = ch->volEnvPos;

			ch->volEnvTick++;

			if (ch->volEnvTick == ins->volEnvPoints[envPos][0])
			{
				ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[envPos][1] & 0xFF);

				envPos++;
				if (ins->volEnvFlags & ENV_LOOP)
				{
					envPos--;

					if (envPos == ins->volEnvLoopEnd)
					{
						if (!(ins->volEnvFlags & ENV_SUSTAIN) || envPos != ins->volEnvSustain || !ch->keyOff)
						{
							envPos = ins->volEnvLoopStart;
							ch->volEnvTick = ins->volEnvPoints[envPos][0];
							ch->fVolEnvValue = (float)(int32_t)(ins->volEnvPoints[envPos][1] & 0xFF);
						}
					}

					envPos++;
				}

				if (envPos < ins->volEnvLength)
				{
					envInterpolateFlag = true;
					if ((ins->volEnvFlags & ENV_SUSTAIN) && !ch->keyOff)
					{
						if (envPos-1 == ins->volEnvSustain)
						{
							envPos--;
							ch->fVolEnvDelta = 0.0f;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag)
					{
						ch->volEnvPos = envPos;

						const int32_t x0 = ins->volEnvPoints[envPos-1][0];
						const int32_t x1 = ins->volEnvPoints[envPos-0][0];

						const int32_t xDiff = x1 - x0;
						if (xDiff > 0)
						{
							const int32_t y0 = ins->volEnvPoints[envPos-1][1] & 0xFF;
							const int32_t y1 = ins->volEnvPoints[envPos-0][1] & 0xFF;

							const int32_t yDiff = y1 - y0;
							ch->fVolEnvDelta = (float)yDiff / (float)xDiff;

							fEnvVal = ch->fVolEnvValue;
							envDidInterpolate = true;
						}
						else
						{
							ch->fVolEnvDelta = 0.0f;
						}
					}
				}
				else
				{
					ch->fVolEnvDelta = 0.0f;
				}
			}

			if (!envDidInterpolate)
			{
				ch->fVolEnvValue += ch->fVolEnvDelta;

				fEnvVal = ch->fVolEnvValue;
				if (fEnvVal < 0.0f || fEnvVal > 64.0f)
				{
					fEnvVal = CLAMP(fEnvVal, 0.0f, 64.0f);
					ch->fVolEnvDelta = 0.0f;
				}
			}

			const int32_t vol = song.globalVolume * ch->outVol * ch->fadeoutVol;

			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
			fVol *= fEnvVal * (1.0f / 64.0f); // volume envelope value

			ch->status |= CS_UPDATE_VOL; // update mixer vol every tick when vol envelope is enabled
		}
		else
		{
			const int32_t vol = song.globalVolume * ch->outVol * ch->fadeoutVol;

			fVol = vol * (1.0f / (64.0f * 64.0f * 32768.0f));
		}

		// FT2 doesn't clamp the volume, but let's do it anyway
		ch->fFinalVol = CLAMP(fVol, 0.0f, 1.0f);
	}
	else
	{
		ch->fFinalVol = 0.0f;
	}

	// *** PANNING ENVELOPE ***

	fEnvVal = 0.0f;
	if (ins->panEnvFlags & ENV_ENABLED)
	{
		envDidInterpolate = false;
		envPos = ch->panEnvPos;

		ch->panEnvTick++;

		if (ch->panEnvTick == ins->panEnvPoints[envPos][0])
		{
			ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[envPos][1] & 0xFF);

			envPos++;
			if (ins->panEnvFlags & ENV_LOOP)
			{
				envPos--;

				if (envPos == ins->panEnvLoopEnd)
				{
					if (!(ins->panEnvFlags & ENV_SUSTAIN) || envPos != ins->panEnvSustain || !ch->keyOff)
					{
						envPos = ins->panEnvLoopStart;

						ch->panEnvTick = ins->panEnvPoints[envPos][0];
						ch->fPanEnvValue = (float)(int32_t)(ins->panEnvPoints[envPos][1] & 0xFF);
					}
				}

				envPos++;
			}

			if (envPos < ins->panEnvLength)
			{
				envInterpolateFlag = true;
				if ((ins->panEnvFlags & ENV_SUSTAIN) && !ch->keyOff)
				{
					if (envPos-1 == ins->panEnvSustain)
					{
						envPos--;
						ch->fPanEnvDelta = 0.0f;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag)
				{
					ch->panEnvPos = envPos;

					const int32_t x0 = ins->panEnvPoints[envPos-1][0];
					const int32_t x1 = ins->panEnvPoints[envPos-0][0];

					const int32_t xDiff = x1 - x0;
					if (xDiff > 0)
					{
						const int32_t y0 = ins->panEnvPoints[envPos-1][1] & 0xFF;
						const int32_t y1 = ins->panEnvPoints[envPos-0][1] & 0xFF;

						const int32_t yDiff = y1 - y0;
						ch->fPanEnvDelta = (float)yDiff / (float)xDiff;

						fEnvVal = ch->fPanEnvValue;
						envDidInterpolate = true;
					}
					else
					{
						ch->fPanEnvDelta = 0.0f;
					}
				}
			}
			else
			{
				ch->fPanEnvDelta = 0.0f;
			}
		}

		if (!envDidInterpolate)
		{
			ch->fPanEnvValue += ch->fPanEnvDelta;

			fEnvVal = ch->fPanEnvValue;
			if (fEnvVal < 0.0f || fEnvVal > 64.0f)
			{
				fEnvVal = CLAMP(fEnvVal, 0.0f, 64.0f);
				ch->fPanEnvDelta = 0.0f;
			}
		}

		fEnvVal -= 32.0f; // center panning envelope value (0..64 -> -32..32)

		const int32_t pan = 128 - ABS(ch->outPan - 128);
		const float fPanAdd = (pan * fEnvVal) * (1.0f / 32.0f);
		const int32_t newPan = (int32_t)(ch->outPan + fPanAdd); // truncate here, do not round

		ch->finalPan = (uint8_t)CLAMP(newPan, 0, 255); // FT2 doesn't clamp the pan, but let's do it anyway

		ch->status |= CS_UPDATE_PAN; // update pan every tick because pan envelope is enabled
	}
	else
	{
		ch->finalPan = ch->outPan;
	}

	// *** AUTO VIBRATO ***
#ifdef HAS_MIDI
	if (ch->midiVibDepth > 0 || ins->autoVibDepth > 0)
#else
	if (ins->autoVibDepth > 0)
#endif
	{
		uint16_t autoVibAmp;

		if (ch->autoVibSweep > 0)
		{
			autoVibAmp = ch->autoVibSweep;
			if (!ch->keyOff)
			{
				autoVibAmp += ch->autoVibAmp;
				if ((autoVibAmp >> 8) > ins->autoVibDepth)
				{
					autoVibAmp = ins->autoVibDepth << 8;
					ch->autoVibSweep = 0;
				}

				ch->autoVibAmp = autoVibAmp;
			}
		}
		else
		{
			autoVibAmp = ch->autoVibAmp;
		}

#ifdef HAS_MIDI
		// non-FT2 hack to make modulation wheel work when auto vibrato rate is zero
		if (ch->midiVibDepth > 0 && ins->autoVibRate == 0)
			ins->autoVibRate = 0x20;

		autoVibAmp += ch->midiVibDepth;
#endif
		ch->autoVibPos += ins->autoVibRate;

		int16_t autoVibVal;
		     if (ins->autoVibType == 1) // square
			autoVibVal = (ch->autoVibPos > 127) ? 64 : -64;
		else if (ins->autoVibType == 2) // ramp up
			autoVibVal = (((ch->autoVibPos >> 1) + 64) & 127) - 64;
		else if (ins->autoVibType == 3) // ramp down
			autoVibVal = ((-(ch->autoVibPos >> 1) + 64) & 127) - 64;
		else // sine
			autoVibVal = autoVibSineTab[ch->autoVibPos];

		autoVibVal = (autoVibVal * (int16_t)autoVibAmp) >> (6+8);

		uint16_t tmpPeriod = ch->outPeriod + autoVibVal;
		if (tmpPeriod >= 32000) // unsigned comparison!
			tmpPeriod = 0;

#ifdef HAS_MIDI
		if (midi.enable)
			tmpPeriod -= ch->midiPitch;
#endif

		ch->finalPeriod = tmpPeriod;
		ch->status |= CF_UPDATE_PERIOD;
	}
	else
	{
		ch->finalPeriod = ch->outPeriod;

#ifdef HAS_MIDI
		if (midi.enable)
		{
			ch->finalPeriod -= ch->midiPitch;
			ch->status |= CF_UPDATE_PERIOD;
		}
#endif
	}
}

// for arpeggio and portamento (semitone-slide mode)
static uint16_t period2NotePeriod(uint16_t period, uint8_t outputNoteOffset, channel_t *ch)
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

		if (period >= note2PeriodLUT[lookUp])
			hiPeriod = (tmpPeriod - fineTune) & ~15;
		else
			loPeriod = (tmpPeriod - fineTune) & ~15;
	}

	tmpPeriod = loPeriod + fineTune + (outputNoteOffset << 4);
	if (tmpPeriod >= (8*12*16+15)-1) // FT2 bug, should've been 10*12*16+16 (also notice the +2 difference)
		tmpPeriod = (8*12*16+16)-1;

	return note2PeriodLUT[tmpPeriod];
}

static void doVibrato(channel_t *ch)
{
	uint8_t tmpVib = (ch->vibratoPos >> 2) & 0x1F;

	switch (ch->vibTremCtrl & 3)
	{
		// 0: sine
		case 0: tmpVib = vibratoTab[tmpVib]; break;

		// 1: ramp
		case 1:
		{
			tmpVib <<= 3;
			if ((int8_t)ch->vibratoPos < 0)
				tmpVib = ~tmpVib;
		}
		break;

		// 2/3: square
		default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibratoDepth) >> 5; // logical shift (unsigned calc.), not arithmetic shift

	if ((int8_t)ch->vibratoPos < 0)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= CF_UPDATE_PERIOD;
	ch->vibratoPos += ch->vibratoSpeed;
}

static void arpeggio(channel_t *ch, uint8_t param)
{
	uint8_t noteOffset;

	const uint8_t tick = arpeggioTab[song.tick & 31];
	if (tick == 0)
	{
		ch->outPeriod = ch->realPeriod;
	}
	else
	{
		if (tick == 1)
			noteOffset = param >> 4;
		else
			noteOffset = param & 0x0F; // tick 2

		ch->outPeriod = period2NotePeriod(ch->realPeriod, noteOffset, ch);
	}

	ch->status |= CF_UPDATE_PERIOD;
}

static void pitchSlideUp(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->pitchSlideUpSpeed;

	ch->pitchSlideUpSpeed = param;

	ch->realPeriod -= param * 4;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= CF_UPDATE_PERIOD;
}

static void pitchSlideDown(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->pitchSlideDownSpeed;

	ch->pitchSlideDownSpeed = param;

	ch->realPeriod += param * 4;
	if ((int16_t)ch->realPeriod >= 32000) // FT2 bug, should've been unsigned comparison
		ch->realPeriod = 32000-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= CF_UPDATE_PERIOD;
}

static void portamento(channel_t *ch, uint8_t param)
{
	if (ch->portamentoDirection == 0)
		return;

	if (ch->portamentoDirection > 1)
	{
		ch->realPeriod -= ch->portamentoSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->portamentoTargetPeriod)
		{
			ch->portamentoDirection = 1;
			ch->realPeriod = ch->portamentoTargetPeriod;
		}
	}
	else
	{
		ch->realPeriod += ch->portamentoSpeed;
		if (ch->realPeriod >= ch->portamentoTargetPeriod)
		{
			ch->portamentoDirection = 1;
			ch->realPeriod = ch->portamentoTargetPeriod;
		}
	}

	if (ch->semitonePortaMode)
		ch->outPeriod = period2NotePeriod(ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= CF_UPDATE_PERIOD;

	(void)param;
}

static void vibrato(channel_t *ch, uint8_t param)
{
	if (param > 0)
	{
		const uint8_t vibratoDepth = param & 0x0F;
		if (vibratoDepth > 0)
			ch->vibratoDepth = vibratoDepth;

		const uint8_t vibratoSpeed = (param & 0xF0) >> 2;
		if (vibratoSpeed > 0)
			ch->vibratoSpeed = vibratoSpeed;
	}

	doVibrato(ch);
}

static void portamentoPlusVolSlide(channel_t *ch, uint8_t param)
{
	portamento(ch, 0); // the last parameter is actually not used in portamento()
	volSlide(ch, param);

	(void)param;
}

static void vibratoPlusVolSlide(channel_t *ch, uint8_t param)
{
	doVibrato(ch);
	volSlide(ch, param);

	(void)param;
}

static void tremolo(channel_t *ch, uint8_t param)
{
	int16_t tremVol;

	if (param > 0)
	{
		uint8_t tremoloDepth = param & 0x0F;
		if (tremoloDepth > 0)
			ch->tremoloDepth = tremoloDepth;

		uint8_t tremoloSpeed = (param & 0xF0) >> 2;
		if (tremoloSpeed > 0)
			ch->tremoloSpeed = tremoloSpeed;
	}

	uint8_t tmpTrem = (ch->tremoloPos >> 2) & 0x1F;
	switch ((ch->vibTremCtrl >> 4) & 3)
	{
		// 0: sine
		case 0: tmpTrem = vibratoTab[tmpTrem]; break;

		// 1: ramp
		case 1:
		{
			tmpTrem <<= 3;
			if ((int8_t)ch->vibratoPos < 0) // FT2 bug, should've been ch->tremoloPos
				tmpTrem = ~tmpTrem;
		}
		break;

		// 2/3: square
		default: tmpTrem = 255; break;
	}
	tmpTrem = (tmpTrem * ch->tremoloDepth) >> 6; // logical shift (unsigned calc.), not arithmetic shift

	if ((int8_t)ch->tremoloPos < 0)
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
	ch->status |= CS_UPDATE_VOL;

	ch->tremoloPos += ch->tremoloSpeed;
}

static void volSlide(channel_t *ch, uint8_t param)
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
	ch->status |= CS_UPDATE_VOL;
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
	
	// update all voice volumes

	channel_t *c = channel;
	for (int32_t i = 0; i < song.numChannels; i++, c++)
		c->status |= CS_UPDATE_VOL;
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
	ch->status |= CS_UPDATE_PAN;
}

static void tremor(channel_t *ch, uint8_t param)
{
	if (param == 0)
		param = ch->tremorParam;

	ch->tremorParam = param;

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
	ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
}

static void retrigNote(channel_t *ch, uint8_t param)
{
	if (param == 0) // E9x with a param of zero is handled in getNewNote()
		return;

	if ((song.speed-song.tick) % param == 0)
	{
		triggerNote(0, 0, 0, ch);
		triggerInstrument(ch);
	}
}

static void noteCut(channel_t *ch, uint8_t param)
{
	if ((uint8_t)(song.speed-song.tick) == param)
	{
		ch->outVol = ch->realVol = 0;
		ch->status |= CS_UPDATE_VOL + CS_USE_QUICK_VOLRAMP;
	}
}

static void noteDelay(channel_t *ch, uint8_t param)
{
	if ((uint8_t)(song.speed-song.tick) == param)
	{
		const uint8_t note = ch->copyOfInstrAndNote & 0x00FF;
		triggerNote(note, 0, 0, ch);

		const uint8_t instrument = ch->copyOfInstrAndNote >> 8;
		if (instrument > 0)
			resetVolumes(ch);

		triggerInstrument(ch);

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
	dummy,		// 0
	dummy,		// 1
	dummy,		// 2
	dummy,		// 3
	dummy,		// 4
	dummy,		// 5
	dummy,		// 6
	dummy,		// 7
	dummy,		// 8
	retrigNote,	// 9
	dummy,		// A
	dummy,		// B
	noteCut,	// C
	noteDelay,	// D
	dummy,		// E
	dummy		// F
};

static void E_Effects_TickNonZero(channel_t *ch, uint8_t param)
{
	EJumpTab_TickNonZero[param >> 4](ch, param & 0xF);
}

static const efxRoutine JumpTab_TickNonZero[36] =
{
	arpeggio,		// 0
	pitchSlideUp,		// 1
	pitchSlideDown,		// 2
	portamento,		// 3
	vibrato,		// 4
	portamentoPlusVolSlide,	// 5
	vibratoPlusVolSlide,	// 6
	tremolo,		// 7
	dummy,			// 8
	dummy,			// 9
	volSlide,		// A
	dummy,			// B
	dummy,			// C
	dummy,			// D
	E_Effects_TickNonZero,	// E
	dummy,			// F
	dummy,			// G
	globalVolSlide,		// H
	dummy,			// I
	dummy,			// J
	keyOffCmd,		// K
	dummy,			// L
	dummy,			// M
	dummy,			// N
	dummy,			// O
	panningSlide,		// P
	dummy,			// Q
	doMultiNoteRetrig,	// R
	dummy,			// S
	tremor,			// T
	dummy,			// U
	dummy,			// V
	dummy,			// W
	dummy,			// X
	dummy,			// Y
	dummy			// Z
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

		/*
		** Because of a bug in FT2, pattern loop commands will manipulate
		** the row the next pattern will begin at (should be 0).
		** However, this can overflow the number of rows (length) for that
		** pattern and cause out-of-bounds reads. Set to row 0 in this case.
		*/
		if (song.row >= song.currNumRows)
			song.row = 0;
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

	if (!songPlaying)
	{
		ch = channel;
		for (i = 0; i < song.numChannels; i++, ch++)
			updateVolPanAutoVib(ch);

		return;
	}

	// for song playback counter (hh:mm:ss)
	if (song.BPM >= MIN_BPM && song.BPM <= MAX_BPM) // just in case
	{
		song.playbackSecondsFrac += songTickDuration35fp[song.BPM-MIN_BPM];
		if (song.playbackSecondsFrac >= 1ULL << 35)
		{
			song.playbackSecondsFrac &= (1ULL << 35)-1;
			song.playbackSeconds++;
		}
	}

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
			updateVolPanAutoVib(ch);
		}
	}
	else
	{
		ch = channel;
		for (i = 0; i < song.numChannels; i++, ch++)
		{
			handleEffects_TickNonZero(ch);
			updateVolPanAutoVib(ch);
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
	sample_t *s = p->smp;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++)
	{
		s->panning = 128;
		s->volume = 64;
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
		ins->autoVibRate = (uint8_t)config.stdVibRate[i];
		ins->autoVibDepth = (uint8_t)config.stdVibDepth[i];
		ins->autoVibSweep = (uint8_t)config.stdVibSweep[i];
		ins->autoVibType = (uint8_t)config.stdVibType[i];
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
	ins->autoVibRate = 0;
	ins->autoVibDepth = 0;
	ins->autoVibSweep = 0;
	ins->autoVibType = 0;

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
	
	freeMixerInterpolationTables();
}

void calcMiscReplayerVars(void)
{
	for (int32_t i = 0; i < 32; i++)
		dExp2MulTab[i] = 1.0 / exp2(i); // 1/(2^i)

	for (int32_t i = 0; i < 4*12*16; i++)
	{
		dLogTab[i] = (8363.0 * 256.0) * exp2(i / (4.0 * 12.0 * 16.0));
		scopeLogTab[i] = (uint64_t)round(dLogTab[i] * (SCOPE_FRAC_SCALE / SCOPE_HZ));
		scopeDrawLogTab[i] = (uint64_t)round(dLogTab[i] * (SCOPE_FRAC_SCALE / (C4_FREQ/2.0)));
	}

	scopeAmigaPeriodDiv = (uint64_t)round((SCOPE_FRAC_SCALE * (1712.0*8363.0)) / SCOPE_HZ);
	scopeDrawAmigaPeriodDiv = (uint64_t)round((SCOPE_FRAC_SCALE * (1712.0*8363.0)) / (C4_FREQ/2.0));
}

bool setupReplayer(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		patternNumRows[i] = 64;

	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	// unmute all channels (must be done before resetChannels() call)
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		editor.channelMuted[i] = false;

	resetChannels();

	song.songLength = 1;
	song.numChannels = 8;
	editor.BPM = song.BPM = 125;
	editor.speed = song.initialSpeed = song.speed = 6;
	editor.globalVolume = song.globalVolume = 64;
	audio.linearPeriodsFlag = true;
	note2PeriodLUT = linearPeriodLUT;

	calcPanningTable();

	setPos(0, 0, true); // important!

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

	// zero tick sample counter so that it will instantly initiate a tick
	audio.tickSampleCounterFrac = audio.tickSampleCounter = 0;

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

	// FT2 bugfix: don't play note if certain requirements are not met
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
		ch->copyOfInstrAndNote = (insNum << 8) | (ch->copyOfInstrAndNote & 0xFF);
		ch->instrNum = insNum;
	}

	ch->copyOfInstrAndNote = (ch->copyOfInstrAndNote & 0xFF00) | note;
	ch->efx = 0;
	ch->efxData = 0;

	triggerNote(note, 0, 0, ch);

	if (note != NOTE_OFF)
	{
		resetVolumes(ch);
		triggerInstrument(ch);

		if (vol != -1) // if jamming note keys, vol -1 = use sample's volume
		{
			ch->realVol = vol;
			ch->outVol = vol;
			ch->oldVol = vol;
		}
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	updateVolPanAutoVib(ch);

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
	ch->copyOfInstrAndNote = (ch->instrNum << 8) | note;
	ch->efx = 0;

	triggerNote(note, 0, 0, ch);

	if (note != NOTE_OFF)
	{
		resetVolumes(ch);
		triggerInstrument(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	updateVolPanAutoVib(ch);

	unlockAudio();

	while (ch->status & CS_TRIGGER_VOICE); // wait for voice to trigger in mixer

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
	ch->copyOfInstrAndNote = (ch->instrNum << 8) | note;
	ch->efx = 0;
	ch->efxData = 0;

	triggerNote(note, 0, 0, ch);

	ch->smpStartPos = samplePlayOffset;

	if (note != NOTE_OFF)
	{
		resetVolumes(ch);
		triggerInstrument(ch);

		ch->realVol = vol;
		ch->outVol = vol;
		ch->oldVol = vol;
	}

	ch->midiVibDepth = midiVibDepth;
	ch->midiPitch = midiPitch;

	updateVolPanAutoVib(ch);

	unlockAudio();

	while (ch->status & CS_TRIGGER_VOICE); // wait for voice to trigger in mixer

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

		ch->copyOfInstrAndNote = 0;
		ch->relativeNote = 0;
		ch->smpNum = 0;
		ch->smpPtr = NULL;
		ch->instrNum = 0;
		ch->instrPtr = instr[0]; // important: set instrument pointer to instr 0 (placeholder instrument)
		ch->status = CS_UPDATE_VOL;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->fFinalVol = 0.0f;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;
		ch->vibratoDepth = 0;
		ch->midiVibDepth = 0;
		ch->midiPitch = 0;
		ch->portamentoDirection = 0; // FT2 bugfix: fixes weird portamento behavior

		stopVoice(i);
	}

	// for sampling playback line in Smp. Ed.
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	stopAllScopes();
	resetAudioDither();

	// wait for scope thread to finish, making sure pointers aren't illegal
	while (editor.scopeThreadBusy);

	if (audioWasntLocked)
		unlockAudio();
}

void setNewSongPos(int32_t pos)
{
	resetReplayerState(); // FT2 bugfix
	setPos((int16_t)pos, 0, true);

	// FT2 fix: if song speed was 0, set it back to initial speed
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

	memset(scopeUpdateStatus, 0, sizeof (scopeUpdateStatus));

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
			scopeUpdateStatus[i] |= chSyncEntry->channels[i].status;

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
