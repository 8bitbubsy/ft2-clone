// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "ft2_audio.h"
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

/* This is a *huge* mess, directly ported from the original FT2 code (and modified).
** You will experience a lot of headaches if you dig into it...
** If something looks to be wrong, it's probably right! */

// TABLES AND VARIABLES

// defined at the bottom of this file
extern const int8_t vibSineTab[256];
extern const uint8_t vibTab[32];
extern const uint16_t amigaFinePeriod[12 * 8];
extern const uint16_t amigaPeriod[12 * 8];

static bool bxxOverflow;
static int16_t *linearPeriods, *amigaPeriods, oldPeriod;
static uint32_t *logTab, oldRate;
static uint32_t frequenceDivFactor, frequenceMulFactor;
static tonTyp nilPatternLine;

// globally accessed

int8_t playMode = 0;
bool linearFrqTab = false, songPlaying = false, audioPaused = false, musicPaused = false;
volatile bool replayerBusy = false;
int16_t *note2Period = NULL, pattLens[MAX_PATTERNS];
stmTyp stm[MAX_VOICES];
songTyp song;
instrTyp *instr[132];
tonTyp *patt[MAX_PATTERNS];

// CODE START

void fixSongName(void) // removes spaces from right side of song name
{
	for (int16_t i = 20; i >= 0; i--)
	{
		if (song.name[i] == ' ')
			song.name[i] = '\0';
		else
			break;
	}
}

void fixSampleName(int16_t nr) // removes spaces from right side of ins/smp names
{
	int16_t i, j;
	sampleTyp *s;

	for (i = 21; i >= 0; i--)
	{
		if (song.instrName[nr][i] == ' ')
			song.instrName[nr][i] = '\0';
		else
			break;
	}

	if (instr[nr] != NULL)
	{
		for (i = 0; i < MAX_SMP_PER_INST; i++)
		{
			s = &instr[nr]->samp[i];
			for (j = 21; j >= 0; j--)
			{
				if (s->name[j] == ' ')
					s->name[j] = '\0';
				else
					break;
			}
			s->name[22] = '\0'; // just in case (for tracker, not present in sample header when saving)
		}
	}
}

void resetChannels(void)
{
	bool audioWasntLocked;
	stmTyp *ch;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	memset(stm, 0, sizeof (stm));
	for (uint8_t i = 0; i < MAX_VOICES; i++)
	{
		ch = &stm[i];

		ch->instrSeg = instr[0];
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

void tuneSample(sampleTyp *s, uint32_t midCFreq)
{
	int32_t linearFreq, relativeNote;

	if (midCFreq == 0)
	{
		s->fine = 0;
		s->relTon = 0;
	}
	else
	{
		linearFreq = (int32_t)round(log(midCFreq / 8363.0) * ((12.0 * 128.0) / M_LN2));
		s->fine = ((linearFreq + 128) & 255) - 128;

		relativeNote = (int32_t)round((linearFreq - s->fine) / 128.0);
		s->relTon = (int8_t)relativeNote;
	}
}

void setPatternLen(uint16_t nr, int16_t len)
{
	bool audioWasntLocked;

	assert(nr < MAX_PATTERNS);

	if ((len < 1 || len > MAX_PATT_LEN) || len == pattLens[nr])
		return;

	audioWasntLocked = !audio.locked;
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

	editor.ui.updatePatternEditor = true;
	editor.ui.updatePosSections = true;
}

int16_t getUsedSamples(int16_t nr)
{
	int16_t i, j;
	instrTyp *ins;

	if (instr[nr] == NULL)
		return 0;

	ins = instr[nr];

	i = 16 - 1;
	while (i >= 0 && ins->samp[i].pek == NULL && ins->samp[i].name[0] == '\0')
		i--;

	/* Yes, 'i' can be -1 here, and will be set to at least 0
	** because of ins->ta values. Possibly an FT2 bug... */
	for (j = 0; j < 96; j++)
	{
		if (ins->ta[j] > i)
			i = ins->ta[j];
	}

	return i+1;
}

int16_t getRealUsedSamples(int16_t nr)
{
	int8_t i;

	if (instr[nr] == NULL)
		return 0;

	i = 16 - 1;
	while (i >= 0 && instr[nr]->samp[i].pek == NULL)
		i--;

	return i+1;
}

void setFrqTab(bool linear)
{
	linearFrqTab = linear;

	if (linearFrqTab)
	{
		audio.linearFreqTable = true;
		note2Period = linearPeriods;
	}
	else
	{
		audio.linearFreqTable = false;
		note2Period = amigaPeriods;
	}

	// update frequency type radiobutton if it's shown
	if (editor.ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
		setConfigIORadioButtonStates();
}

static void retrigVolume(stmTyp *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= (IS_Vol + IS_Pan + IS_QuickVol);
}

static void retrigEnvelopeVibrato(stmTyp *ch)
{
	instrTyp *ins;

	if (!(ch->waveCtrl & 0x04)) ch->vibPos  = 0;
	if (!(ch->waveCtrl & 0x40)) ch->tremPos = 0;

	ch->retrigCnt = 0;
	ch->tremorPos = 0;

	ch->envSustainActive = true;

	ins = ch->instrSeg;
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
	instrTyp *ins;

	ch->envSustainActive = false;

	ins = ch->instrSeg;
	assert(ins != NULL);

	if (!(ins->envPTyp & 1)) // yes, FT2 does this (!)
	{
		if (ch->envPCnt >= ins->envPP[ch->envPPos][0])
			ch->envPCnt = ins->envPP[ch->envPPos][0] - 1;
	}

	if (ins->envVTyp & 1)
	{
		if (ch->envVCnt >= ins->envVP[ch->envVPos][0])
			ch->envVCnt = ins->envVP[ch->envVPos][0] - 1;
	}
	else
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= (IS_Vol + IS_QuickVol);
	}
}

// 100% FT2-accurate routine, do not touch!
void calcReplayRate(uint32_t rate)
{
	if (rate == 0)
		return;

	// for voice delta calculation
	frequenceDivFactor = (uint32_t)round(65536.0 * 1712.0 / rate * 8363.0);
	frequenceMulFactor = (uint32_t)round(256.0 * 65536.0 / rate * 8363.0);
	audio.dScopeFreqMul = rate / (double)SCOPE_HZ;

	// for volume ramping (FT2 doesn't round here)
	audio.quickVolSizeVal = rate / 200;

	// for audio/video sync
	audio.dSpeedValMul = (1.0 / rate) * editor.dPerfFreq;
}

// 100% FT2-accurate routine, do not touch!
uint32_t getFrequenceValue(uint16_t period)
{
	uint8_t shift;
	uint16_t index;
	uint32_t rate;

	if (period == 0)
		return 0;

	if (period == oldPeriod)
		return oldRate; // added check: prevent this calculation if it would yield the same

	if (linearFrqTab)
	{
		index = (12 * 192 * 4) - period;
		shift = (14 - (index / 768)) & 0x1F;

		// this converts to fast code even on x86 (imul + shrd)
		rate = ((uint64_t)logTab[index % 768] * frequenceMulFactor) >> 24;
		if (shift > 0)
			rate >>= shift;
	}
	else
	{
		rate = frequenceDivFactor / period;
	}

	oldPeriod = period;
	oldRate = rate;

	return rate;
}

void resetOldRates(void)
{
	oldPeriod = 0;
	oldRate = 0;

	resetOldScopeRates();
	resetOldRevFreqs();
}

static void startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch)
{
	uint8_t smp;
	uint16_t tmpTon;
	sampleTyp *s;
	instrTyp *ins;

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

	ins = instr[ch->instrNr];
	if (ins == NULL)
		ins = instr[0];

	ch->instrSeg = ins;
	ch->mute = ins->mute;

	if (ton > 96) // non-FT2 security (should never happen because I clamp in the patt. loader now)
		ton = 96;

	smp = ins->ta[ton-1] & 0xF;
	ch->sampleNr = smp;

	s = &ins->samp[smp];
	ch->smpPtr = s;
	ch->relTonNr = s->relTon;

	ton += ch->relTonNr;
	if (ton >= 12*10)
		return;

	ch->oldVol = s->vol;
	ch->oldPan = s->pan;

	if (effTyp == 0x0E && (eff & 0xF0) == 0x50)
		ch->fineTune = ((eff & 0x0F) * 16) - 128; // result is now -128 .. 127
	else
		ch->fineTune = s->fine;

	if (ton > 0)
	{
		tmpTon = ((ton - 1) * 16) + (((ch->fineTune >> 3) + 16) & 0xFF);
		if (tmpTon < MAX_NOTES) // should always happen, but FT2 does this check
		{
			assert(note2Period != NULL);
			ch->realPeriod = note2Period[tmpTon];

			ch->outPeriod = ch->realPeriod;
			ch->midiCurPeriod = ch->realPeriod;
			ch->midiPortaPeriod = ch->realPeriod;
		}
	}

	ch->status |= (IS_Period + IS_Vol + IS_Pan + IS_NyTon + IS_QuickVol);

	if (effTyp == 9)
	{
		if (eff)
			ch->smpOffset = ch->eff;

		ch->smpStartPos = ch->smpOffset * 256;
	}
	else
	{
		ch->smpStartPos = 0;
	}
}

static void multiRetrig(stmTyp *ch)
{
	uint8_t cnt;
	int16_t vol;

	cnt = ch->retrigCnt + 1;
	if (cnt < ch->retrigSpeed)
	{
		ch->retrigCnt = cnt;
		return;
	}

	ch->retrigCnt = 0;

	vol = ch->realVol;
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
}

static void checkMoreEffects(stmTyp *ch) // called even if channel is muted
{
	int8_t envPos;
	bool envUpdate;
	uint8_t tmpEff;
	int16_t newEnvPos;
	uint16_t i;
	instrTyp *ins;

	ins = ch->instrSeg;
	assert(ins != NULL);

	// Bxx - position jump
	if (ch->effTyp == 11)
	{
		if (playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT)
		{
			if (ch->eff-1 < 0 || ch->eff-1 >= song.len)
				bxxOverflow = true; // non-FT2 security fix...
			else
				song.songPos = ch->eff - 1;
		}

		song.pBreakPos = 0;
		song.posJumpFlag = true;
	}

	// Dxx - pattern break
	else if (ch->effTyp == 13)
	{
		song.posJumpFlag = true;

		tmpEff = ((ch->eff >> 4) * 10) + (ch->eff & 0x0F);
		if (tmpEff <= 63)
			song.pBreakPos = tmpEff;
		else
			song.pBreakPos = 0;
	}

	// Exx - E effects
	else if (ch->effTyp == 14)
	{
		if (ch->stOff) // channel is muted
		{
			// E6x - pattern loop
			if ((ch->eff & 0xF0) == 0x60)
			{
				if (ch->eff == 0x60) // E60, empty param
				{
					ch->pattPos = song.pattPos & 0x00FF;
				}
				else
				{
					if (ch->loopCnt == 0)
					{
						ch->loopCnt = ch->eff & 0x0F;

						song.pBreakPos = ch->pattPos;
						song.pBreakFlag = true;
					}
					else
					{
						if (--ch->loopCnt > 0)
						{
							song.pBreakPos = ch->pattPos;
							song.pBreakFlag = true;
						}
					}
				}
			}

			// EEx - pattern delay
			else if ((ch->eff & 0xF0) == 0xE0)
			{
				if (song.pattDelTime2 == 0)
					song.pattDelTime = (ch->eff & 0x0F) + 1;
			}

			return;
		}

		// E1x - fine period slide up
		if ((ch->eff & 0xF0) == 0x10)
		{
			tmpEff = ch->eff & 0x0F;
			if (tmpEff == 0)
				tmpEff = ch->fPortaUpSpeed;

			ch->fPortaUpSpeed = tmpEff;

			ch->realPeriod -= (tmpEff * 4);
			if (ch->realPeriod < 1)
				ch->realPeriod = 1;

			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}

		// E2x - fine period slide down
		else if ((ch->eff & 0xF0) == 0x20)
		{
			tmpEff = ch->eff & 0x0F;
			if (tmpEff == 0)
				tmpEff = ch->fPortaDownSpeed;

			ch->fPortaDownSpeed = tmpEff;

			ch->realPeriod += (tmpEff * 4);
			if (ch->realPeriod > (32000 - 1))
				ch->realPeriod =  32000 - 1;

			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}

		// E3x - set glissando type
		else if ((ch->eff & 0xF0) == 0x30) ch->glissFunk = ch->eff & 0x0F;

		// E4x - set vibrato waveform
		else if ((ch->eff & 0xF0) == 0x40) ch->waveCtrl = (ch->waveCtrl & 0xF0) | (ch->eff & 0x0F);

		// E5x (set finetune) is handled in startTone()

		// E6x - pattern loop
		else if ((ch->eff & 0xF0) == 0x60)
		{
			if (ch->eff == 0x60) // E60, empty param
			{
				ch->pattPos = song.pattPos & 0xFF;
			}
			else
			{
				if (ch->loopCnt == 0)
				{
					ch->loopCnt = ch->eff & 0x0F;

					song.pBreakPos = ch->pattPos;
					song.pBreakFlag = true;
				}
				else
				{
					if (--ch->loopCnt > 0)
					{
						song.pBreakPos = ch->pattPos;
						song.pBreakFlag = true;
					}
				}
			}
		}

		// E7x - set tremolo waveform
		else if ((ch->eff & 0xF0) == 0x70) ch->waveCtrl = ((ch->eff & 0x0F) << 4) | (ch->waveCtrl & 0x0F);

		// EAx - fine volume slide up
		else if ((ch->eff & 0xF0) == 0xA0)
		{
			tmpEff = ch->eff & 0x0F;
			if (tmpEff == 0)
				tmpEff = ch->fVolSlideUpSpeed;

			ch->fVolSlideUpSpeed = tmpEff;

			// unsigned clamp
			if (ch->realVol <= (64 - tmpEff))
				ch->realVol += tmpEff;
			else
				ch->realVol = 64;

			ch->outVol = ch->realVol;
			ch->status |= IS_Vol;
		}

		// EBx - fine volume slide down
		else if ((ch->eff & 0xF0) == 0xB0)
		{
			tmpEff = ch->eff & 0x0F;
			if (tmpEff == 0)
				tmpEff = ch->fVolSlideDownSpeed;

			ch->fVolSlideDownSpeed = tmpEff;

			// unsigned clamp
			if (ch->realVol >= tmpEff)
				ch->realVol -= tmpEff;
			else
				ch->realVol = 0;

			ch->outVol = ch->realVol;
			ch->status |= IS_Vol;
		}

		// ECx - note cut
		else if ((ch->eff & 0xF0) == 0xC0)
		{
			if (ch->eff == 0xC0) // empty param
			{
				ch->realVol = 0;
				ch->outVol = 0;
				ch->status |= (IS_Vol + IS_QuickVol);
			}
		}

		// EEx - pattern delay
		else if ((ch->eff & 0xF0) == 0xE0)
		{
			if (song.pattDelTime2 == 0)
				song.pattDelTime = (ch->eff & 0x0F) + 1;
		}
	}

	// Fxx - set speed/tempo
	else if (ch->effTyp == 15)
	{
		if (ch->eff >= 32)
		{
			song.speed = ch->eff;
			setSpeed(song.speed);
		}
		else
		{
			song.tempo = ch->eff;
			song.timer = ch->eff;
		}
	}

	// Gxx - set global volume
	else if (ch->effTyp == 16)
	{
		song.globVol = ch->eff;
		if (song.globVol > 64)
			song.globVol = 64;

		for (i = 0; i < song.antChn; i++) // update all voice volumes
			stm[i].status |= IS_Vol;
	}

	// Lxx - set vol and pan envelope position
	else if (ch->effTyp == 21)
	{
		// *** VOLUME ENVELOPE ***
		if (ins->envVTyp & 1)
		{
			ch->envVCnt = ch->eff - 1;

			envPos = 0;
			envUpdate = true;
			newEnvPos = ch->eff;

			if (ins->envVPAnt > 1)
			{
				envPos++;
				for (i = 0; i < ins->envVPAnt-1; i++)
				{
					if (newEnvPos < ins->envVP[envPos][0])
					{
						envPos--;

						newEnvPos -= ins->envVP[envPos][0];
						if (newEnvPos == 0)
						{
							envUpdate = false;
							break;
						}

						if (ins->envVP[envPos+1][0] <= ins->envVP[envPos][0])
						{
							envUpdate = true;
							break;
						}

						ch->envVIPValue = ((ins->envVP[envPos+1][1] - ins->envVP[envPos][1]) & 0xFF) << 8;
						ch->envVIPValue /= (ins->envVP[envPos+1][0] - ins->envVP[envPos][0]);

						ch->envVAmp = (ch->envVIPValue * (newEnvPos - 1)) + ((ins->envVP[envPos][1] & 0xFF) << 8);

						envPos++;

						envUpdate = false;
						break;
					}

					envPos++;
				}

				if (envUpdate)
					envPos--;
			}

			if (envUpdate)
			{
				ch->envVIPValue = 0;
				ch->envVAmp = (ins->envVP[envPos][1] & 0xFF) << 8;
			}

			if (envPos >= ins->envVPAnt)
			{
				envPos = ins->envVPAnt - 1;
				if (envPos < 0)
					envPos = 0;
			}

			ch->envVPos = envPos;
		}

		// *** PANNING ENVELOPE ***
		if (ins->envVTyp & 2) // probably an FT2 bug
		{
			ch->envPCnt = ch->eff - 1;

			envPos = 0;
			envUpdate = true;
			newEnvPos = ch->eff;

			if (ins->envPPAnt > 1)
			{
				envPos++;
				for (i = 0; i < ins->envPPAnt-1; i++)
				{
					if (newEnvPos < ins->envPP[envPos][0])
					{
						envPos--;

						newEnvPos -= ins->envPP[envPos][0];
						if (newEnvPos == 0)
						{
							envUpdate = false;
							break;
						}

						if (ins->envPP[envPos + 1][0] <= ins->envPP[envPos][0])
						{
							envUpdate = true;
							break;
						}

						ch->envPIPValue = ((ins->envPP[envPos+1][1] - ins->envPP[envPos][1]) & 0xFF) << 8;
						ch->envPIPValue /= (ins->envPP[envPos+1][0] - ins->envPP[envPos][0]);

						ch->envPAmp = (ch->envPIPValue * (newEnvPos - 1)) + ((ins->envPP[envPos][1] & 0xFF) << 8);

						envPos++;

						envUpdate = false;
						break;
					}

					envPos++;
				}

				if (envUpdate)
					envPos--;
			}

			if (envUpdate)
			{
				ch->envPIPValue = 0;
				ch->envPAmp = (ins->envPP[envPos][1] & 0xFF) << 8;
			}

			if (envPos >= ins->envPPAnt)
			{
				envPos = ins->envPPAnt - 1;
				if (envPos < 0)
					envPos = 0;
			}

			ch->envPPos = envPos;
		}
	}
}

static void checkEffects(stmTyp *ch)
{
	uint8_t tmpEff, tmpEffHi, volKol;

	// this one is manipulated by vol column effects, then used for multiretrig (FT2 quirk)
	volKol = ch->volKolVol;

	// *** VOLUME COLUMN EFFECTS (TICK 0) ***

	// set volume
	if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50)
	{
		volKol -= 16;

		ch->outVol = volKol;
		ch->realVol = volKol;

		ch->status |= (IS_Vol + IS_QuickVol);
	}

	// fine volume slide down
	else if ((ch->volKolVol & 0xF0) == 0x80)
	{
		volKol = ch->volKolVol & 0x0F;

		// unsigned clamp
		if (ch->realVol >= volKol)
			ch->realVol -= volKol;
		else
			ch->realVol = 0;

		ch->outVol  = ch->realVol;
		ch->status |= IS_Vol;
	}

	// fine volume slide up
	else if ((ch->volKolVol & 0xF0) == 0x90)
	{
		volKol = ch->volKolVol & 0x0F;

		// unsigned clamp
		if (ch->realVol <= 64-volKol)
			ch->realVol += volKol;
		else
			ch->realVol = 64;

		ch->outVol  = ch->realVol;
		ch->status |= IS_Vol;
	}

	// set vibrato speed
	else if ((ch->volKolVol & 0xF0) == 0xA0)
	{
		volKol = (ch->volKolVol & 0x0F) << 2;
		ch->vibSpeed = volKol;
	}

	// set panning
	else if ((ch->volKolVol & 0xF0) == 0xC0)
	{
		volKol <<= 4;

		ch->outPan = volKol;
		ch->status |= IS_Pan;
	}

	// *** MAIN EFFECTS (TICK 0) ***

	if (ch->effTyp == 0 && ch->eff == 0) return; // no effect

	// Cxx - set volume
	if (ch->effTyp == 12)
	{
		ch->realVol = ch->eff;
		if (ch->realVol > 64)
			ch->realVol = 64;

		ch->outVol = ch->realVol;
		ch->status |= (IS_Vol + IS_QuickVol);

		return;
	}

	// 8xx - set panning
	else if (ch->effTyp == 8)
	{
		ch->outPan = ch->eff;
		ch->status |= IS_Pan;

		return;
	}

	// Rxy - note multi retrigger
	else if (ch->effTyp == 27)
	{
		tmpEff = ch->eff & 0x0F;
		if (tmpEff == 0)
			tmpEff = ch->retrigSpeed;

		ch->retrigSpeed = tmpEff;

		tmpEffHi = ch->eff >> 4;
		if (tmpEffHi == 0)
			tmpEffHi = ch->retrigVol;

		ch->retrigVol = tmpEffHi;

		if (volKol == 0)
			multiRetrig(ch);

		return;
	}

	// X1x - extra fine period slide up
	else if (ch->effTyp == 33 && (ch->eff & 0xF0) == 0x10)
	{
		tmpEff = ch->eff & 0x0F;
		if (tmpEff == 0)
			tmpEff = ch->ePortaUpSpeed;

		ch->ePortaUpSpeed = tmpEff;

		ch->realPeriod -= tmpEff;
		if (ch->realPeriod < 1)
			ch->realPeriod = 1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= IS_Period;

		return;
	}

	// X2x - extra fine period slide down
	else if (ch->effTyp == 33 && (ch->eff & 0xF0) == 0x20)
	{
		tmpEff = ch->eff & 0x0F;
		if (tmpEff == 0)
			tmpEff = ch->ePortaDownSpeed;

		ch->ePortaDownSpeed = tmpEff;

		ch->realPeriod += tmpEff;
		if (ch->realPeriod > 32000-1)
			ch->realPeriod = 32000-1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= IS_Period;

		return;
	}

	checkMoreEffects(ch);
}

static void fixTonePorta(stmTyp *ch, tonTyp *p, uint8_t inst)
{
	uint16_t portaTmp;

	if (p->ton > 0)
	{
		if (p->ton == 97)
		{
			keyOff(ch);
		}
		else
		{
			portaTmp = ((((p->ton - 1) + ch->relTonNr) & 0xFF) * 16) + (((ch->fineTune >> 3) + 16) & 0xFF);
			if (portaTmp < MAX_NOTES)
			{
				assert(note2Period != NULL);
				ch->wantPeriod = note2Period[portaTmp];

				     if (ch->wantPeriod == ch->realPeriod) ch->portaDir = 0;
				else if (ch->wantPeriod  > ch->realPeriod) ch->portaDir = 1;
				else ch->portaDir = 2;
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

static void getNewNote(stmTyp *ch, tonTyp *p)
{
	uint8_t inst;
	bool checkEfx;

	ch->volKolVol = p->vol;

	if (ch->effTyp == 0)
	{
		if (ch->eff > 0)
		{
			// we have an arpeggio running, set period back
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}
	else
	{
		if (ch->effTyp == 4 || ch->effTyp == 6)
		{
			// we have a vibrato running
			if (p->effTyp != 4 && p->effTyp != 6)
			{
				// but it's ending at the next (this) row, so set period back
				ch->outPeriod = ch->realPeriod;
				ch->status |= IS_Period;
			}
		}
	}

	ch->effTyp = p->effTyp;
	ch->eff = p->eff;
	ch->tonTyp = (p->instr << 8) | p->ton;

	if (ch->stOff)
	{
		checkMoreEffects(ch);
		return;
	}

	// 'inst' var is used for later if checks...
	inst = p->instr;
	if (inst > 0)
	{
		if (inst <= MAX_INST)
			ch->instrNr = inst;
		else
			inst = 0;
	}

	checkEfx = true;
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
			if ((ch->volKolVol & 0x0F) > 0)
				ch->portaSpeed = (ch->volKolVol & 0x0F) << 6;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);

			return;
		}

		if (p->effTyp == 3 || p->effTyp == 5) // 3xx or 5xx
		{
			if (p->effTyp != 5 && p->eff != 0)
				ch->portaSpeed = p->eff << 2;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);

			return;
		}

		if (p->effTyp == 0x14 && p->eff == 0) // K00 (KeyOff - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				retrigVolume(ch);

			checkEffects(ch);
			return;
		}

		if (p->ton == 0)
		{
			if (inst > 0)
			{
				retrigVolume(ch);
				retrigEnvelopeVibrato(ch);
			}

			checkEffects(ch);
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

	checkEffects(ch);
}

static void fixaEnvelopeVibrato(stmTyp *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	int16_t autoVibVal, panTmp;
	uint16_t autoVibAmp, tmpPeriod, envVal;
	int32_t tmp32;
	uint32_t vol;
	instrTyp *ins;

	ins = ch->instrSeg;

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
				ch->envVAmp = (ins->envVP[envPos][1] & 0xFF) << 8;

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
							ch->envVAmp = ins->envVP[envPos][1] << 8;
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
							ch->envVIPValue = (ins->envVP[envPos][1] - ins->envVP[envPos-1][1]) << 8;
							ch->envVIPValue /= (ins->envVP[envPos][0] - ins->envVP[envPos-1][0]);

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
				if (envVal > 64*256)
				{
					if (envVal > 128*256)
						envVal = 64*256;
					else
						envVal = 0;

					ch->envVIPValue = 0;
				}
			}

			/* original FT2 method with lower precision (finalVol = 0..256)
			envVal >>= 8;
			ch->finalVol = (song.globVol * (((envVal * ch->outVol) * ch->fadeOutAmp) >> (16 + 2))) >> 7;
			*/

			/* calculate with four times more precision (finalVol = 0..2048)
			** Also, vol envelope range is now 0..16384 instead of being shifted to 0..64
			*/

			uint32_t vol1 = song.globVol * ch->outVol * ch->fadeOutAmp; // 0..64 * 0..64 * 0..32768 = 0..134217728
			uint32_t vol2 = envVal << 2; // 0..16384 * 2^2 = 0..65536

			vol = ((uint64_t)vol1 * vol2) >> 32; // 0..2048

			ch->status |= IS_Vol;
		}
		else
		{
			/* original FT2 method with lower precision (finalVol = 0..256)
			ch->finalVol = (song.globVol * (((ch->outVol << 4) * ch->fadeOutAmp) >> 16)) >> 7;
			*/

			// calculate with four times more precision (finalVol = 0..2048)
			vol = (song.globVol * ch->outVol * ch->fadeOutAmp) >> 16; // 0..64 * 0..64 * 0..32768 = 0..2048
		}

		if (vol > 2047)
			vol = 2047; // range is now 0..2047 to prevent MUL overflow when voice volume is calculated

		ch->finalVol = (uint16_t)vol;
	}
	else
	{
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
			ch->envPAmp = ins->envPP[envPos][1] << 8;

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
						ch->envPAmp = ins->envPP[envPos][1] << 8;
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
						ch->envPIPValue = (ins->envPP[envPos][1] - ins->envPP[envPos-1][1]) << 8;
						ch->envPIPValue /= (ins->envPP[envPos][0] - ins->envPP[envPos-1][0]);

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
			if (envVal > 64*256)
			{
				if (envVal > 128*256)
					envVal = 64*256;
				else
					envVal = 0;

				ch->envPIPValue = 0;
			}
		}

		panTmp = ch->outPan - 128;
		if (panTmp > 0)
			panTmp = 0 - panTmp;
		panTmp += 128;

		envVal -= 32*256;

		ch->finalPan = ch->outPan + (uint8_t)(((int16_t)envVal * panTmp) >> 13);
		ch->status |= IS_Pan;
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
		else if (ins->vibTyp == 3) autoVibVal = (((0 - (ch->eVibPos >> 1)) + 64) & 127) - 64; // ramp down
		else autoVibVal = vibSineTab[ch->eVibPos]; // sine

		autoVibVal <<= 2;

		tmp32 = ((autoVibVal * (int16_t)autoVibAmp) >> 16) & 0x8000FFFF;
		tmpPeriod = ch->outPeriod + (int16_t)tmp32;
		if (tmpPeriod > 32000-1)
			tmpPeriod = 0;

		ch->finalPeriod = tmpPeriod - ch->midiPitch;
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

int16_t relocateTon(int16_t period, int8_t relativeNote, stmTyp *ch)
{
	int8_t fineTune;
	int32_t loPeriod, hiPeriod, tmpPeriod, tableIndex;

	fineTune = (ch->fineTune >> 3) + 16;
	hiPeriod = 8 * 12 * 16;
	loPeriod = 0;

	for (int8_t i = 0; i < 8; i++)
	{
		tmpPeriod = (((loPeriod + hiPeriod) >> 1) & 0xFFFFFFF0) + fineTune;

		tableIndex = tmpPeriod - 8;
		if (tableIndex < 0) // added security check
			tableIndex = 0;

		if (period >= note2Period[tableIndex])
			hiPeriod = tmpPeriod - fineTune;
		else
			loPeriod = tmpPeriod - fineTune;
	}

	tmpPeriod = loPeriod + fineTune + (relativeNote << 4);
	if (tmpPeriod < 0) // added security check
		tmpPeriod = 0;

	if (tmpPeriod >= ((8*12*16)+15)-1) // FT2 bug: off-by-one edge case
		tmpPeriod = (8*12*16)+15;

	return note2Period[tmpPeriod];
}

static void tonePorta(stmTyp *ch)
{
	if (ch->portaDir == 0)
		return;

	if (ch->portaDir > 1)
	{
		ch->realPeriod -= ch->portaSpeed;
		if (ch->realPeriod <= ch->wantPeriod)
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

	if (ch->glissFunk) // semi-tone slide flag
		ch->outPeriod = relocateTon(ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= IS_Period;
}

static void volume(stmTyp *ch) // actually volume slide
{
	uint8_t tmpEff = ch->eff;
	if (tmpEff == 0)
		tmpEff = ch->volSlideSpeed;

	ch->volSlideSpeed = tmpEff;

	if ((tmpEff & 0xF0) == 0)
	{
		// unsigned clamp
		if (ch->realVol >= tmpEff)
			ch->realVol -= tmpEff;
		else
			ch->realVol = 0;
	}
	else
	{
		// unsigned clamp
		if (ch->realVol <= 64-(tmpEff>>4))
			ch->realVol += tmpEff>>4;
		else
			ch->realVol = 64;
	}

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

static void vibrato2(stmTyp *ch)
{
	uint8_t tmpVib = (ch->vibPos / 4) & 0x1F;

	switch (ch->waveCtrl & 3)
	{
		// 0: sine
		case 0: tmpVib = vibTab[tmpVib]; break;

		// 1: ramp
		case 1:
		{
			tmpVib *= 8;
			if (ch->vibPos >= 128)
				tmpVib ^= 0xFF;
		}
		break;

		// 2/3: square
		default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibDepth) / 32;

	if (ch->vibPos >= 128)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= IS_Period;
	ch->vibPos += ch->vibSpeed;
}

static void vibrato(stmTyp *ch)
{
	if (ch->eff > 0)
	{
		if ((ch->eff & 0x0F) > 0) ch->vibDepth = ch->eff & 0x0F;
		if ((ch->eff & 0xF0) > 0) ch->vibSpeed = (ch->eff >> 4) * 4;
	}

	vibrato2(ch);
}

static void doEffects(stmTyp *ch)
{
	int8_t note;
	uint8_t tmpEff, tremorData, tremorSign, tmpTrem;
	int16_t tremVol;
	uint16_t i, tick;

	if (ch->stOff)
		return;

	// *** VOLUME COLUMN EFFECTS (TICKS >0) ***

	// volume slide down
	if ((ch->volKolVol & 0xF0) == 0x60)
	{
		// unsigned clamp
		if (ch->realVol >= (ch->volKolVol & 0x0F))
			ch->realVol -= ch->volKolVol & 0x0F;
		else
			ch->realVol = 0;

		ch->outVol = ch->realVol;
		ch->status |= IS_Vol;
	}

	// volume slide up
	else if ((ch->volKolVol & 0xF0) == 0x70)
	{
		// unsigned clamp
		if (ch->realVol <= 64-(ch->volKolVol & 0x0F))
			ch->realVol += ch->volKolVol & 0x0F;
		else
			ch->realVol = 64;

		ch->outVol = ch->realVol;
		ch->status |= IS_Vol;
	}

	// vibrato (+ set vibrato depth)
	else if ((ch->volKolVol & 0xF0) == 0xB0)
	{
		if (ch->volKolVol != 0xB0)
			ch->vibDepth = ch->volKolVol & 0x0F;

		vibrato2(ch);
	}

	// pan slide left
	else if ((ch->volKolVol & 0xF0) == 0xD0)
	{
		// unsigned clamp + a bug when the parameter is 0
		if ((ch->volKolVol & 0x0F) == 0 || ch->outPan < (ch->volKolVol & 0x0F))
			ch->outPan = 0;
		else
			ch->outPan -= ch->volKolVol & 0x0F;

		ch->status |= IS_Pan;
	}

	// pan slide right
	else if ((ch->volKolVol & 0xF0) == 0xE0)
	{
		// unsigned clamp
		if (ch->outPan <= 255-(ch->volKolVol & 0x0F))
			ch->outPan += ch->volKolVol & 0x0F;
		else
			ch->outPan = 255;

		ch->status |= IS_Pan;
	}

	// tone portamento
	else if ((ch->volKolVol & 0xF0) == 0xF0) tonePorta(ch);

	// *** MAIN EFFECTS (TICKS >0) ***

	if ((ch->eff == 0 && ch->effTyp == 0) || ch->effTyp >= 36) return; // no effect

	// 0xy - Arpeggio
	if (ch->effTyp == 0)
	{
		tick = song.timer;

		// FT2 'out of boundary LUT read' arp simulation
		     if (tick < 16) tick %= 3;
		else if (tick == 16) tick = 0;
		else tick = 2;

		if (tick == 0)
		{
			ch->outPeriod = ch->realPeriod;
		}
		else
		{
			if (tick == 1)
				note = ch->eff >> 4;
			else
				note = ch->eff & 0xF; // tick 2

			ch->outPeriod = relocateTon(ch->realPeriod, note, ch);
		}

		ch->status |= IS_Period;
	}

	// 1xx - period slide up
	else if (ch->effTyp == 1)
	{
		tmpEff = ch->eff;
		if (tmpEff == 0)
			tmpEff = ch->portaUpSpeed;

		ch->portaUpSpeed = tmpEff;

		ch->realPeriod -= tmpEff * 4;
		if (ch->realPeriod < 1)
			ch->realPeriod = 1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= IS_Period;
	}

	// 2xx - period slide down
	else if (ch->effTyp == 2)
	{
		tmpEff = ch->eff;
		if (tmpEff == 0)
			tmpEff = ch->portaDownSpeed;

		ch->portaDownSpeed = tmpEff;

		ch->realPeriod += tmpEff * 4;
		if (ch->realPeriod > 32000-1)
			ch->realPeriod = 32000-1;

		ch->outPeriod = ch->realPeriod;
		ch->status |= IS_Period;
	}

	// 3xx - tone portamento
	else if (ch->effTyp == 3) tonePorta(ch);

	// 4xy - vibrato
	else if (ch->effTyp == 4) vibrato(ch);

	// 5xy - tone portamento + volume slide
	else if (ch->effTyp == 5)
	{
		tonePorta(ch);
		volume(ch);
	}

	// 6xy - vibrato + volume slide
	else if (ch->effTyp == 6)
	{
		vibrato2(ch);
		volume(ch);
	}

	// 7xy - tremolo
	else if (ch->effTyp == 7)
	{
		tmpEff = ch->eff;
		if (tmpEff > 0)
		{
			if ((tmpEff & 0x0F) > 0) ch->tremDepth = tmpEff & 0x0F;
			if ((tmpEff & 0xF0) > 0) ch->tremSpeed = (tmpEff >> 4) * 4;
		}

		tmpTrem = (ch->tremPos / 4) & 0x1F;
		switch ((ch->waveCtrl >> 4) & 3)
		{
			// 0: sine
			case 0: tmpTrem = vibTab[tmpTrem]; break;

			// 1: ramp
			case 1:
			{
				tmpTrem *= 8;
				if (ch->vibPos >= 128) tmpTrem ^= 0xFF; // FT2 bug, should've been TremPos
			}
			break;

			// 2/3: square
			default: tmpTrem = 255; break;
		}
		tmpTrem = (tmpTrem * ch->tremDepth) / 64;

		if (ch->tremPos >= 128)
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

		ch->outVol = tremVol & 0xFF;
		ch->tremPos += ch->tremSpeed;
		ch->status |= IS_Vol;
	}

	// Axy - volume slide
	else if (ch->effTyp == 10) volume(ch); // actually volume slide

	// Exy - E effects
	else if (ch->effTyp == 14)
	{
		// E9x - note retrigger
		if ((ch->eff & 0xF0) == 0x90)
		{
			if (ch->eff != 0x90) // E90 is handled in getNewNote()
			{
				if ((song.tempo-song.timer) % (ch->eff & 0x0F) == 0)
				{
					startTone(0, 0, 0, ch);
					retrigEnvelopeVibrato(ch);
				}
			}
		}

		// ECx - note cut
		else if ((ch->eff & 0xF0) == 0xC0)
		{
			if (((song.tempo-song.timer) & 0xFF) == (ch->eff & 0x0F))
			{
				ch->outVol = 0;
				ch->realVol = 0;
				ch->status |= (IS_Vol + IS_QuickVol);
			}
		}

		// EDx - note delay
		else if ((ch->eff & 0xF0) == 0xD0)
		{
			if (((song.tempo-song.timer) & 0xFF) == (ch->eff & 0x0F))
			{
				startTone(ch->tonTyp & 0xFF, 0, 0, ch);

				if ((ch->tonTyp & 0xFF00) > 0)
					retrigVolume(ch);

				retrigEnvelopeVibrato(ch);

				if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50)
				{
					ch->outVol  = ch->volKolVol - 16;
					ch->realVol = ch->outVol;
				}
				else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF)
				{
					ch->outPan = (ch->volKolVol & 0x0F) << 4;
				}
			}
		}
	}

	// Hxy - global volume slide
	else if (ch->effTyp == 17)
	{
		tmpEff = ch->eff;
		if (tmpEff == 0)
			tmpEff = ch->globVolSlideSpeed;

		ch->globVolSlideSpeed = tmpEff;

		if ((tmpEff & 0xF0) == 0)
		{
			// unsigned clamp
			if (song.globVol >= tmpEff)
				song.globVol -= tmpEff;
			else
				song.globVol = 0;
		}
		else
		{
			// unsigned clamp
			if (song.globVol <= 64-(tmpEff >> 4))
				song.globVol += tmpEff >> 4;
			else
				song.globVol = 64;
		}

		for (i = 0; i < song.antChn; i++) // update all voice volumes
			stm[i].status |= IS_Vol;
	}

	// Kxx - key off
	else if (ch->effTyp == 20)
	{
		if (((song.tempo-song.timer) & 31) == (ch->eff & 0x0F))
			keyOff(ch);
	}

	// Pxy - panning slide
	else if (ch->effTyp == 25)
	{
		tmpEff = ch->eff;
		if (tmpEff == 0)
			tmpEff = ch->panningSlideSpeed;

		ch->panningSlideSpeed = tmpEff;

		if ((tmpEff & 0xF0) == 0)
		{
			// unsigned clamp
			if (ch->outPan >= tmpEff)
				ch->outPan -= tmpEff;
			else
				ch->outPan = 0;
		}
		else
		{
			tmpEff >>= 4;

			// unsigned clamp
			if (ch->outPan <= 255-tmpEff)
				ch->outPan += tmpEff;
			else
				ch->outPan = 255;
		}

		ch->status |= IS_Pan;
	}

	// Rxy - multi note retrig
	else if (ch->effTyp == 27) multiRetrig(ch);

	// Txy - tremor
	else if (ch->effTyp == 29)
	{
		tmpEff = ch->eff;
		if (tmpEff == 0)
			tmpEff = ch->tremorSave;

		ch->tremorSave = tmpEff;

		tremorSign = ch->tremorPos & 0x80;
		tremorData = ch->tremorPos & 0x7F;

		tremorData--;
		if ((tremorData & 0x80) > 0)
		{
			if (tremorSign == 0x80)
			{
				tremorSign = 0x00;
				tremorData = tmpEff & 0x0F;
			}
			else
			{
				tremorSign = 0x80;
				tremorData = tmpEff >> 4;
			}
		}

		ch->tremorPos = tremorData | tremorSign;

		ch->outVol = tremorSign ? ch->realVol : 0;
		ch->status |= (IS_Vol + IS_QuickVol);
	}
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
		if (--song.pattDelTime2 > 0)
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
			else
			{
				if (++song.songPos >= song.len)
				{
					editor.wavReachedEndFlag = true;
					song.songPos = song.repS;
				}
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

static void noNewAllChannels(void)
{
	for (uint8_t i = 0; i < song.antChn; i++)
	{
		doEffects(&stm[i]);
		fixaEnvelopeVibrato(&stm[i]);
	}
}

void mainPlayer(void) // periodically called from audio callback
{
	uint8_t i;
	bool readNewNote;

	if (musicPaused || !songPlaying)
	{
		for (i = 0; i < song.antChn; i++)
			fixaEnvelopeVibrato(&stm[i]);

		return;
	}

	if (song.speed > 0)
		song.musicTime += 65536 / song.speed; // for playback counter

	readNewNote = false;
	if (--song.timer == 0)
	{
		song.timer = song.tempo;
		readNewNote = true;
	}

	// for visuals
	song.curReplayerTimer   = (uint8_t)song.timer;
	song.curReplayerPattPos = (uint8_t)song.pattPos;
	song.curReplayerPattNr  = (uint8_t)song.pattNr;
	song.curReplayerSongPos = (uint8_t)song.songPos;

	if (readNewNote)
	{
		if (song.pattDelTime2 == 0)
		{
			for (i = 0; i < song.antChn; i++)
			{
				if (patt[song.pattNr] == NULL)
					getNewNote(&stm[i], &nilPatternLine);
				else
					getNewNote(&stm[i], &patt[song.pattNr][(song.pattPos * MAX_VOICES) + i]);

				fixaEnvelopeVibrato(&stm[i]);
			}
		}
		else
		{
			noNewAllChannels();
		}
	}
	else
	{
		noNewAllChannels();
	}

	getNextPos();
}

void resetMusic(void)
{
	bool audioWasntLocked = !audio.locked;
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
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPos > -1)
	{
		song.songPos = songPos;
		if (song.len > 0 && song.songPos >= song.len)
			song.songPos = song.len - 1;

		song.pattNr = song.songTab[songPos];
		assert(song.pattNr < MAX_PATTERNS);
		song.pattLen = pattLens[song.pattNr];

		checkMarkLimits(); // non-FT2 safety
	}

	if (pattPos > -1)
	{
		song.pattPos = pattPos;
		if (song.pattPos >= song.pattLen)
			song.pattPos = song.pattLen - 1;
	}

	// if not playing, update local position variables
	if (!songPlaying)
	{
		if (pattPos > -1)
		{
			editor.pattPos = (uint8_t)pattPos;
			editor.ui.updatePatternEditor = true;
		}

		if (songPos > -1)
		{
			editor.editPattern = (uint8_t)song.pattNr;
			editor.songPos = song.songPos;
			editor.ui.updatePosSections = true;
		}
	}

	if (resetTimer)
		song.timer = 1;

	if (audioWasntLocked)
		unlockAudio();
}

void delta2Samp(int8_t *p, int32_t len, uint8_t typ)
{
	int8_t *p8, news8, olds8L, olds8R;
	int16_t *p16, news16, olds16L, olds16R, tmp16;
	int32_t i, tmp32;

	if (typ & 16) len /= 2; // 16-bit
	if (typ & 32) len /= 2; // stereo

	if (typ & 32)
	{
		if (typ & 16)
		{
			p16 = (int16_t *)p;

			olds16L = 0;
			olds16R = 0;

			for (i = 0; i < len; i++)
			{
				news16 = p16[i] + olds16L;
				p16[i] = news16;
				olds16L = news16;

				news16 = p16[len+i] + olds16R;
				p16[len+i] = news16;
				olds16R = news16;

				tmp32 = olds16L + olds16R;
				p16[i] = (int16_t)(tmp32 >> 1);
			}
		}
		else
		{
			p8 = (int8_t *)p;

			olds8L = 0;
			olds8R = 0;

			for (i = 0; i < len; i++)
			{
				news8 = p8[i] + olds8L;
				p8[i] = news8;
				olds8L = news8;

				news8 = p8[len+i] + olds8R;
				p8[len+i] = news8;
				olds8R = news8;

				tmp16 = olds8L + olds8R;
				p8[i] = (int8_t)(tmp16 >> 1);
			}
		}
	}
	else
	{
		if (typ & 16)
		{
			p16 = (int16_t *)p;

			olds16L = 0;
			for (i = 0; i < len; i++)
			{
				news16 = p16[i] + olds16L;
				p16[i] = news16;
				olds16L = news16;
			}
		}
		else
		{
			p8 = (int8_t *)p;

			olds8L = 0;
			for (i = 0; i < len; i++)
			{
				news8 = p8[i] + olds8L;
				p8[i] = news8;
				olds8L = news8;
			}
		}
	}
}

void samp2Delta(int8_t *p, int32_t len, uint8_t typ)
{
	int8_t *p8, news8, olds8;
	int16_t *p16, news16, olds16;
	int32_t i;

	if (typ & 16) len /= 2; // 16-bit

	if (typ & 16)
	{
		p16 = (int16_t *)p;

		news16 = 0;
		for (i = 0; i < len; i++)
		{
			olds16 = p16[i];
			p16[i] -= news16;
			news16 = olds16;
		}
	}
	else
	{
		p8 = (int8_t *)p;

		news8 = 0;
		for (i = 0; i < len; i++)
		{
			olds8 = p8[i];
			p8[i] -= news8;
			news8 = olds8;
		}
	}
}

bool allocateInstr(int16_t nr)
{
	bool audioWasntLocked;
	instrTyp *p;

	if (instr[nr] != NULL)
		return false; // already allocated

	p = (instrTyp *)malloc(sizeof (instrTyp));
	if (p == NULL)
		return false;

	memset(p, 0, sizeof (instrTyp));

	for (int8_t i = 0; i < 16; i++) // set standard sample pan/vol
	{
		p->samp[i].pan = 128;
		p->samp[i].vol = 64;
	}

	setStdEnvelope(p, 0, 3);

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	instr[nr] = p;

	if (audioWasntLocked)
		unlockAudio();

	return true;
}

void freeInstr(int16_t nr)
{
	if (instr[nr] == NULL)
		return; // not allocated

	pauseAudio(); // channel instrument pointers are now cleared

	for (int8_t i = 0; i < 16; i++) // free sample data
	{
		if (instr[nr]->samp[i].pek != NULL)
			free(instr[nr]->samp[i].pek);
	}

	free(instr[nr]);
	instr[nr] = NULL;
	
	resumeAudio();
}

void freeAllInstr(void)
{
	pauseAudio(); // channel instrument pointers are now cleared
	for (int16_t i = 1; i <= MAX_INST; i++)
	{
		if (instr[i] != NULL)
		{
			for (int8_t j = 0; j < MAX_SMP_PER_INST; j++) // free sample data
			{
				if (instr[i]->samp[j].pek != NULL)
					free(instr[i]->samp[j].pek);
			}

			free(instr[i]);
			instr[i] = NULL;
		}
	}
	resumeAudio();
}

void freeSample(int16_t nr, int16_t nr2)
{
	sampleTyp *s;

	if (instr[nr] == NULL)
		return; // instrument not allocated

	pauseAudio(); // voice sample pointers are now cleared

	s = &instr[nr]->samp[nr2];
	if (s->pek != NULL)
		free(s->pek);

	memset(s, 0, sizeof (sampleTyp));

	s->pan = 128;
	s->vol = 64;

	resumeAudio();
}

void freeAllPatterns(void)
{
	pauseAudio();
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
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
	uint8_t *scanPtr;
	uint32_t scanLen;

	if (patt[nr] == NULL)
		return true;

	scanPtr = (uint8_t *)patt[nr];
	scanLen = pattLens[nr] * TRACK_WIDTH;

	for (uint32_t i = 0; i < scanLen; i++)
	{
		if (scanPtr[i] != 0)
			return false;
	}

	return true;
}

void updateChanNums(void)
{
	uint8_t pageLen;

	assert(!(song.antChn & 1));

	pageLen = 8;
	if (config.ptnS3M)
	{
		     if (song.antChn == 2) pageLen = 4;
		else if (song.antChn == 4) pageLen = 4;
		else if (song.antChn == 6) pageLen = 6;
		else if (song.antChn >= 8) pageLen = 8;
	}
	else
	{
		     if (song.antChn ==  2) pageLen = 4;
		else if (song.antChn ==  4) pageLen = 4;
		else if (song.antChn ==  6) pageLen = 6;
		else if (song.antChn ==  8) pageLen = 8;
		else if (song.antChn == 10) pageLen = 10;
		else if (song.antChn >= 12) pageLen = 12;
	}

	editor.ui.numChannelsShown = pageLen;
	if (song.antChn == 2)
		editor.ui.numChannelsShown = 2;

	if (config.ptnMaxChannels == 0)
	{
		if (editor.ui.numChannelsShown > 4)
			editor.ui.numChannelsShown = 4;
	}
	else if (config.ptnMaxChannels == 1)
	{
		if (editor.ui.numChannelsShown > 6)
			editor.ui.numChannelsShown = 6;
	}
	else if (config.ptnMaxChannels == 2)
	{
		if (editor.ui.numChannelsShown > 8)
			editor.ui.numChannelsShown = 8;
	}
	else if (config.ptnMaxChannels == 3)
	{
		if (config.ptnS3M)
		{
			if (editor.ui.numChannelsShown > 8)
				editor.ui.numChannelsShown = 8;
		}
		else
		{
			if (editor.ui.numChannelsShown > 12)
				editor.ui.numChannelsShown = 12;
		}
	}

	editor.ui.pattChanScrollShown = song.antChn > getMaxVisibleChannels();

	if (editor.ui.patternEditorShown)
	{
		if (editor.ui.channelOffset > song.antChn-editor.ui.numChannelsShown)
			setScrollBarPos(SB_CHAN_SCROLL, song.antChn - editor.ui.numChannelsShown, true);
	}

	if (editor.ui.pattChanScrollShown)
	{
		if (editor.ui.patternEditorShown)
		{
			showScrollBar(SB_CHAN_SCROLL);
			showPushButton(PB_CHAN_SCROLL_LEFT);
			showPushButton(PB_CHAN_SCROLL_RIGHT);
		}

		setScrollBarEnd(SB_CHAN_SCROLL, song.antChn);
		setScrollBarPageLength(SB_CHAN_SCROLL, editor.ui.numChannelsShown);
	}
	else
	{
		hideScrollBar(SB_CHAN_SCROLL);
		hidePushButton(PB_CHAN_SCROLL_LEFT);
		hidePushButton(PB_CHAN_SCROLL_RIGHT);

		setScrollBarPos(SB_CHAN_SCROLL, 0, false);

		editor.ui.channelOffset = 0;
	}

	if (editor.cursor.ch >= editor.ui.channelOffset+editor.ui.numChannelsShown)
		editor.cursor.ch = editor.ui.channelOffset+editor.ui.numChannelsShown - 1;
}

void conv8BitSample(int8_t *p, int32_t len, bool stereo)
{
	int8_t *p2, l, r;
	int16_t tmp16;
	int32_t i;

	if (stereo)
	{
		len /= 2;

		p2 = &p[len];
		for (i = 0; i < len; i++)
		{
			l = p[i]  - 128;
			r = p2[i] - 128;

			tmp16 = l + r;
			p[i] = (int8_t)(tmp16 >> 1);
		}
	}
	else
	{
		for (i = 0; i < len; i++)
			p[i] -= 128;
	}
}

void conv16BitSample(int8_t *p, int32_t len, bool stereo)
{
	int16_t *p16_1, *p16_2, l, r;
	int32_t i, tmp32;

	p16_1 = (int16_t *)p;

	len /= 2;

	if (stereo)
	{
		len /= 2;

		p16_2 = (int16_t *)&p[len * 2];
		for (i = 0; i < len; i++)
		{
			l = p16_1[i] - 32768;
			r = p16_2[i] - 32768;

			tmp32 = l + r;
			p16_1[i] = (int16_t)(tmp32 >> 1);
		}
	}
	else
	{
		for (i = 0; i < len; i++)
			p16_1[i] -= 32768;
	}
}

void closeReplayer(void)
{
	freeAllInstr();
	freeAllPatterns();

	if (logTab != NULL)
	{
		free(logTab);
		logTab = NULL;
	}

	if (amigaPeriods != NULL)
	{
		free(amigaPeriods);
		amigaPeriods = NULL;
	}

	if (linearPeriods != NULL)
	{
		free(linearPeriods);
		linearPeriods = NULL;
	}

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
}

bool setupReplayer(void)
{
	uint16_t i, k;
	int16_t noteVal;

	// allocate memory for pointers

	for (i = 0; i < MAX_PATTERNS; i++)
		pattLens[i] = 64;

	if (linearPeriods == NULL)
		linearPeriods = (int16_t *)malloc(sizeof (int16_t) * ((12 * 10 * 16) + 16));

	if (amigaPeriods == NULL)
		amigaPeriods = (int16_t *)malloc(sizeof (int16_t) * ((12 * 10 * 16) + 16));

	if (logTab == NULL)
		logTab = (uint32_t *)malloc(sizeof (int32_t) * 768);

	if (linearPeriods == NULL || amigaPeriods == NULL || logTab == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// generate tables, bit-exact to original FT2

	// log table
	for (i = 0; i < 768; i++)
		logTab[i] = (uint32_t)round(16777216.0 * exp((i / 768.0) * M_LN2));

	// linear table
	for (i = 0; i < (12*10*16)+16; i++)
		linearPeriods[i] = (((12 * 10 * 16) + 16) * 4) - (i * 4);

	/* Amiga period table
	** This has a LUT read overflow in real FT2 making the last 17 values trash. We patch those later. */
	k = 0;
	for (i = 0; i < 10; i++)
	{
		for (uint16_t j = 0; j < 96; j++)
		{
			noteVal = ((amigaFinePeriod[j] * 64) + (-1 + (1 << i))) >> (i + 1);

			amigaPeriods[k++] = noteVal;
			amigaPeriods[k++] = noteVal; // copy for interpolation applied later
		}
	}

	// interpolate between points
	for (i = 0; i < (12*10*8)+7; i++)
		amigaPeriods[(i * 2) + 1] = (amigaPeriods[i * 2] + amigaPeriods[(i * 2) + 2]) / 2;

	// the following 17 values are confirmed to be the correct table LUT overflow values in real FT2
	amigaPeriods[1919] = 22; amigaPeriods[1920] = 16; amigaPeriods[1921] =  8; amigaPeriods[1922] =  0;
	amigaPeriods[1923] = 16; amigaPeriods[1924] = 32; amigaPeriods[1925] = 24; amigaPeriods[1926] = 16;
	amigaPeriods[1927] =  8; amigaPeriods[1928] =  0; amigaPeriods[1929] = 16; amigaPeriods[1930] = 32;
	amigaPeriods[1931] = 24; amigaPeriods[1932] = 16; amigaPeriods[1933] =  8; amigaPeriods[1934] =  0;
	amigaPeriods[1935] =  0;

	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	resetChannels();

	song.len = 1;
	song.antChn = 8;

	editor.speed = song.speed = 125;
	editor.tempo = song.tempo = 6;
	editor.globalVol = song.globVol = 64;
	song.initialTempo = song.tempo;

	setFrqTab(true);
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
	for (i = 0; i < 16; i++)
		instr[131]->samp[i].pan = 128;

	// unmute all channels
	for (i = 0; i < MAX_VOICES; i++)
		editor.chnMode[i] = true;

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
	song.globVol = 64;
	song.musicTime = 0;
	song.pattDelTime2 = 0;
	song.pattDelTime = 0;

	// non-FT2 fix: If song speed was 0, set it back to initial speed on play
	if (song.tempo == 0)
		song.tempo = song.initialTempo;

	unlockMixerCallback();

	editor.ui.updatePosSections = true;
	editor.ui.updatePatternEditor = true;
}

void stopPlaying(void)
{
	uint8_t i;
	bool songWasPlaying;

	songWasPlaying = songPlaying;
	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	if (config.killNotesOnStopPlay)
	{
		// safely kills all voices
		lockMixerCallback();
		unlockMixerCallback();

		// prevent getFrequenceValue() from calculating the rates forever
		for (i = 0; i < MAX_VOICES; i++)
			stm[i].outPeriod = 0;
	}
	else
	{
		for (i = 0; i < MAX_VOICES; i++)
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

	editor.ui.updatePosSections = true;
	editor.ui.updatePatternEditor = true;

	// certain non-FT2 fixes
	song.timer = editor.timer = 1;
	song.globVol = editor.globalVol = 64;
	editor.ui.drawGlobVolFlag = true;
}

// from keyboard/smp. ed.
void playTone(uint8_t stmm, uint8_t inst, uint8_t ton, int8_t vol, uint16_t midiVibDepth, uint16_t midiPitch)
{
	sampleTyp *s;
	stmTyp *ch;
	instrTyp *ins = instr[inst];

	if (ins == NULL)
		return;

	assert(stmm < MAX_VOICES && inst < MAX_INST && ton <= 97);
	ch = &stm[stmm];

	if (ton != 97)
	{
		if (ton < 1 || ton > 96)
			return;

		s = &ins->samp[ins->ta[ton-1] & 0x0F];
		if (s->pek == NULL || s->len == 0 || ton+s->relTon <= 0 || ton+s->relTon >= 12*10)
			return;
	}

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
	uint8_t vol;
	stmTyp *ch;

	if (instr[inst] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[stmm].instrNr = 255;
	lastChInstr[stmm].sampleNr = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(stmm < MAX_VOICES && inst < MAX_INST && smpNr < MAX_SMP_PER_INST && ton <= 97);
	ch = &stm[stmm];

	memcpy(&instr[130]->samp[0], &instr[inst]->samp[smpNr], sizeof (sampleTyp));

	vol = instr[inst]->samp[smpNr].vol;
	
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
	uint8_t vol;
	int32_t samplePlayOffset;
	stmTyp *ch;
	sampleTyp *s;

	if (instr[inst] == NULL)
		return;

	// for sampling playback line in Smp. Ed.
	lastChInstr[stmm].instrNr = 255;
	lastChInstr[stmm].sampleNr = 255;
	editor.curPlayInstr = 255;
	editor.curPlaySmp = 255;

	assert(stmm < MAX_VOICES && inst < MAX_INST && smpNr < MAX_SMP_PER_INST && ton <= 97);

	ch = &stm[stmm];
	s = &instr[130]->samp[0];

	memcpy(s, &instr[inst]->samp[smpNr], sizeof (sampleTyp));

	vol = instr[inst]->samp[smpNr].vol;

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

	samplePlayOffset = offs;
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
	bool audioWasntLocked;
	stmTyp *ch;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	for (uint8_t i = 0; i < MAX_VOICES; i++)
	{
		ch = &stm[i];

		lastChInstr[i].sampleNr = 255;
		lastChInstr[i].instrNr = 255;

		ch->tonTyp = 0;
		ch->relTonNr = 0;
		ch->instrNr = 0;
		ch->instrSeg = instr[0]; // important: set instrument pointer to instr 0 (placeholder instrument)
		ch->status = IS_Vol;
		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->finalVol = 0;
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
	resetDitherSeed();
	resetOldRates();

	// wait for scope thread to finish, so that we know pointers aren't deprecated
	while (editor.scopeThreadMutex);

	if (audioWasntLocked)
		unlockAudio();
}

void decSongPos(void)
{
	if (song.songPos == 0)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos > 0)
		setPos(song.songPos - 1, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void incSongPos(void)
{
	if (song.songPos == song.len-1)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos < song.len-1)
		setPos(song.songPos + 1, 0, true);

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

	if (editor.ui.advEditShown)
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
 
	updateTextBoxPointers();
	updateNewInstrument();

	if (editor.ui.advEditShown)
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
	uint64_t frameTime64;

	pattSyncEntry = NULL;
	chSyncEntry = NULL;

	memset(scopeUpdateStatus, 0, sizeof (scopeUpdateStatus)); // this is needed

	frameTime64 = SDL_GetPerformanceCounter();

	// handle channel sync queue

	while (chQueueClearing);

	chQueueReading = true;
	while (chQueueReadSize() > 0)
	{
		if (frameTime64 < getChQueueTimestamp())
			break; // we have no more stuff to render for now

		chSyncEntry = chQueuePeek();
		if (chSyncEntry == NULL)
			break;

		for (uint32_t i = 0; i < song.antChn; i++)
			scopeUpdateStatus[i] |= chSyncEntry->channels[i].status; // yes, OR the status

		if (!chQueuePop())
			break;
	}
	chQueueReading = false;

	/* extra validation because of possible issues when the buffer is full
	** and positions are being reset, which is not entirely thread safe. */
	if (chSyncEntry != NULL && chSyncEntry->timestamp == 0)
		chSyncEntry = NULL;

	// handle pattern sync queue

	while (pattQueueClearing);

	pattQueueReading = true;
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
	pattQueueReading = false;

	/* extra validation because of possible issues when the buffer is full
	** and positions are being reset, which is not entirely thread safe. */
	if (pattSyncEntry != NULL && pattSyncEntry->timestamp == 0)
		pattSyncEntry = NULL;

	// do actual updates

	if (chSyncEntry != NULL)
	{
		handleScopesFromChQueue(chSyncEntry, scopeUpdateStatus);
		editor.ui.drawReplayerPianoFlag = true;
	}

	if (!songPlaying || pattSyncEntry == NULL)
		return;

	// we have a new tick

	editor.timer = pattSyncEntry->timer;

	if (editor.speed != pattSyncEntry->speed)
	{
		editor.speed = pattSyncEntry->speed;
		editor.ui.drawBPMFlag = true;
	}

	if (editor.tempo != pattSyncEntry->tempo)
	{
		editor.tempo = pattSyncEntry->tempo;
		editor.ui.drawSpeedFlag = true;
	}

	if (editor.globalVol != pattSyncEntry->globalVol)
	{
		editor.globalVol = pattSyncEntry->globalVol;
		editor.ui.drawGlobVolFlag = true;
	}

	if (editor.songPos != pattSyncEntry->songPos)
	{
		editor.songPos = pattSyncEntry->songPos;
		editor.ui.drawPosEdFlag = true;
	}

	// somewhat of a kludge...
	if (editor.tmpPattern != pattSyncEntry->pattern || editor.pattPos != pattSyncEntry->patternPos)
	{
		// set pattern number
		editor.editPattern = editor.tmpPattern = pattSyncEntry->pattern;
		checkMarkLimits();
		editor.ui.drawPattNumLenFlag = true;

		// set row
		editor.pattPos = (uint8_t)pattSyncEntry->patternPos;
		editor.ui.updatePatternEditor = true;
	}
}

// TABLES

const char modSig[32][5] =
{
	"1CHN", "2CHN", "3CHN", "4CHN", "5CHN", "6CHN", "7CHN", "8CHN",
	"9CHN", "10CH", "11CH", "12CH", "13CH", "14CH", "15CH", "16CH",
	"17CH", "18CH", "19CH", "20CH", "21CH", "22CH", "23CH", "24CH",
	"25CH", "26CH", "27CH", "28CH", "29CH", "30CH", "31CH", "32CH"
};

const int8_t vibSineTab[256] = 
{
	 0,-2,-3,-5,-6,-8,-9,-11,-12,-14,-16,-17,-19,-20,-22,-23,-24,-26,-27,
   -29,-30,-32,-33,-34,-36,-37,-38,-39,-41,-42,-43,-44,-45,-46,-47,-48,
   -49,-50,-51,-52,-53,-54,-55,-56,-56,-57,-58,-59,-59,-60,-60,-61,-61,
   -62,-62,-62,-63,-63,-63,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,-64,
   -63,-63,-63,-62,-62,-62,-61,-61,-60,-60,-59,-59,-58,-57,-56,-56,-55,
   -54,-53,-52,-51,-50,-49,-48,-47,-46,-45,-44,-43,-42,-41,-39,-38,-37,
   -36,-34,-33,-32,-30,-29,-27,-26,-24,-23,-22,-20,-19,-17,-16,-14,-12,
   -11,-9,-8,-6,-5,-3,-2,0,2,3,5,6,8,9,11,12,14,16,17,19,20,22,23,24,26,
	27,29,30,32,33,34,36,37,38,39,41,42,43,44,45,46,47,48,49,50,51,52,53,
	54,55,56,56,57,58,59,59,60,60,61,61,62,62,62,63,63,63,64,64,64,64,64,
	64,64,64,64,64,64,63,63,63,62,62,62,61,61,60,60,59,59,58,57,56,56,55,
	54,53,52,51,50,49,48,47,46,45,44,43,42,41,39,38,37,36,34,33,32,30,29,
	27,26,24,23,22,20,19,17,16,14,12,11,9,8,6,5,3,2
};

const uint8_t vibTab[32] =
{
	  0, 24, 49, 74, 97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120, 97, 74, 49, 24
};

const uint16_t amigaPeriod[12 * 8] =
{
	4*1712,4*1616,4*1524,4*1440,4*1356,4*1280,4*1208,4*1140,4*1076,4*1016,4*960,4*906,
	2*1712,2*1616,2*1524,2*1440,2*1356,2*1280,2*1208,2*1140,2*1076,2*1016,2*960,2*906,
	1712,1616,1524,1440,1356,1280,1208,1140,1076,1016,960,906,
	856,808,762,720,678,640,604,570,538,508,480,453,
	428,404,381,360,339,320,302,285,269,254,240,226,
	214,202,190,180,170,160,151,143,135,127,120,113,
	107,101,95,90,85,80,75,71,67,63,60,56,
	53,50,47,45,42,40,37,35,33,31,30,28
};

const uint16_t amigaFinePeriod[12 * 8] =
{
	907,900,894,887,881,875,868,862,856,850,844,838,
	832,826,820,814,808,802,796,791,785,779,774,768,
	762,757,752,746,741,736,730,725,720,715,709,704,
	699,694,689,684,678,675,670,665,660,655,651,646,
	640,636,632,628,623,619,614,610,604,601,597,592,
	588,584,580,575,570,567,563,559,555,551,547,543,
	538,535,532,528,524,520,516,513,508,505,502,498,
	494,491,487,484,480,477,474,470,467,463,460,457
};
