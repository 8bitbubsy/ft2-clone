// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_scopes.h"
#include "ft2_sample_ed.h"
#include "ft2_gfxdata.h"
#include "ft2_mouse.h"
#include "ft2_video.h"
#include "ft2_sample_loader.h"
#include "ft2_diskop.h"
#include "ft2_module_loader.h"

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct instrPATHeaderTyp_t
{
	char id[22], copyright[60];
	uint8_t antInstr, activeVoices, antChannels;
	int16_t waveForms, masterVol;
	int32_t dataSize;
	char reserved1[36];
	int16_t instrNr;
	char instrName[16];
	int32_t instrSize;
	uint8_t layers;
	char reserved2[40];
	uint8_t layerDuplicate, layerByte;
	int32_t layerSize;
	uint8_t antSamp;
	char reserved3[40];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
instrPATHeaderTyp;

typedef struct instrPATWaveHeaderTyp_t
{
	char name[7];
	uint8_t fractions;
	int32_t waveSize, repS, repE;
	uint16_t sampleRate;
	int32_t lowFrq, highFreq, rootFrq;
	int16_t fineTune;
	uint8_t pan, envRate[6], envOfs[6], tremSweep, tremRate;
	uint8_t tremDepth, vibSweep, vibRate, vibDepth, mode;
	int16_t scaleFrq;
	uint16_t scaleFactor;
	char reserved[36];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
instrPATWaveHeaderTyp;

typedef struct instrXIHeaderTyp_t
{
	char sig[21], name[23], progName[20];
	uint16_t ver;
	uint8_t ta[96];
	int16_t envVP[12][2], envPP[12][2];
	uint8_t envVPAnt, envPPAnt, envVSust, envVRepS, envVRepE, envPSust, envPRepS;
	uint8_t envPRepE, envVTyp, envPTyp, vibTyp, vibSweep, vibDepth, vibRate;
	uint16_t fadeOut;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	uint8_t mute, reserved[15];
	int16_t antSamp;
	sampleHeaderTyp samp[16];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
instrXIHeaderTyp;

#define PIANOKEY_WHITE_W 10
#define PIANOKEY_WHITE_H 46
#define PIANOKEY_BLACK_W 7
#define PIANOKEY_BLACK_H 29

static const bool keyIsBlackTab[12] = { false, true, false, true, false, false, true, false, true, false, true };
static const char sharpNote1Char[12] = { 'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B' };
static const char sharpNote2Char[12] = { '-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-' };
static const char flatNote1Char[12]  = { 'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B' };
static const char flatNote2Char[12]  = { '-', 'b', '-', 'b', '-', '-', 'b', '-', 'b', '-', 'b', '-' };
static const uint8_t whiteKeyIndex[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const uint16_t whiteKeysBmpOrder[12] = { 0, 0, 506, 0, 1012, 0, 0, 506, 0, 506, 0, 1012 };
static const uint8_t keyDigitXPos[12] = { 11, 16, 22, 27, 33, 44, 49, 55, 60, 66, 71, 77 };
static const uint8_t keyXPos[12] = { 8, 15, 19, 26, 30, 41, 48, 52, 59, 63, 70, 74 };
static volatile bool updateVolEnv, updatePanEnv;
static bool pianoKeyStatus[96];
static int32_t lastMouseX, lastMouseY, saveMouseX, saveMouseY;

// thread data
static uint16_t saveInstrNr;
static SDL_Thread *thread;

extern const int16_t *note2Period; // ft2_replayer.c

void updateInstEditor(void);
void updateNewInstrument(void);

static instrTyp *getCurDispInstr(void)
{
	if (instr[editor.curInstr] == NULL)
		return instr[131];

	return instr[editor.curInstr];
}

static int32_t SDLCALL copyInstrThread(void *ptr)
{
	bool error;
	int8_t *p;
	int16_t destIns, sourceIns;
	sampleTyp *src, *dst;
	(void)ptr;

	error = false;

	destIns = editor.curInstr;
	sourceIns = editor.srcInstr;

	pauseAudio();
	
	freeInstr(destIns);

	if (instr[sourceIns] != NULL)
	{
		if (allocateInstr(destIns))
		{
			memcpy(instr[destIns], instr[sourceIns], sizeof (instrTyp));

			for (int16_t i = 0; i < MAX_SMP_PER_INST; i++)
			{
				src = &instr[sourceIns]->samp[i];
				dst = &instr[destIns]->samp[i];

				dst->origPek = NULL;
				dst->pek = NULL;

				if (src->origPek != NULL)
				{
					p = (int8_t *)malloc(src->len + LOOP_FIX_LEN);
					if (p != NULL)
					{
						dst->origPek = p;
						dst->pek = dst->origPek + SMP_DAT_OFFSET;

						memcpy(dst->origPek, src->origPek, src->len + LOOP_FIX_LEN);
					}
					else error = true;
				}
			}
		}
		else error = true;
	}

	resumeAudio();

	if (error)
		okBoxThreadSafe(0, "System message", "Not enough memory!");

	// do not change instrument names!

	editor.updateCurInstr = true;
	setSongModifiedFlag();
	setMouseBusy(false);

	return false;
}

void copyInstr(void) // dstInstr = srcInstr
{
	if (editor.curInstr == 0 || editor.srcInstr == editor.curInstr)
		return;

	mouseAnimOn();
	thread = SDL_CreateThread(copyInstrThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

void xchgInstr(void) // dstInstr <-> srcInstr
{
	instrTyp *dst, *src, dstTmp;

	if (editor.curInstr == 0 || editor.srcInstr == editor.curInstr)
		return;

	lockMixerCallback();

	src = instr[editor.srcInstr];
	dst = instr[editor.curInstr];

	// swap instruments
	dstTmp = *dst;
	*dst = *src;
	*src = dstTmp;

	unlockMixerCallback();

	// do not change instrument names!

	updateNewInstrument();
	setSongModifiedFlag();
}

static void drawMIDICh(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	assert(ins->midiChannel <= 15);
	uint8_t disp = ins->midiChannel + 1;
	sprintf(str, "%02d", disp);
	textOutFixed(156, 132, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawMIDIPrg(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	assert(ins->midiProgram <= 127);
	sprintf(str, "%03d", ins->midiProgram);
	textOutFixed(149, 146, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawMIDIBend(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	assert(ins->midiBend <= 36);
	sprintf(str, "%02d", ins->midiBend);
	textOutFixed(156, 160, PAL_FORGRND, PAL_DESKTOP, str);
}

void midiChDown(void)
{
	scrollBarScrollLeft(SB_INST_EXT_MIDI_CH, 1);
}

void midiChUp(void)
{
	scrollBarScrollRight(SB_INST_EXT_MIDI_CH, 1);
}

void midiPrgDown(void)
{
	scrollBarScrollLeft(SB_INST_EXT_MIDI_PRG, 1);
}

void midiPrgUp(void)
{
	scrollBarScrollRight(SB_INST_EXT_MIDI_PRG, 1);
}

void midiBendDown(void)
{
	scrollBarScrollLeft(SB_INST_EXT_MIDI_BEND, 1);
}

void midiBendUp(void)
{
	scrollBarScrollRight(SB_INST_EXT_MIDI_BEND, 1);
}

void sbMidiChPos(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_EXT_MIDI_CH, 0, false);
		return;
	}

	if (ins->midiChannel != (uint8_t)pos)
	{
		ins->midiChannel = (uint8_t)pos;
		drawMIDICh();
		setSongModifiedFlag();
	}
}

void sbMidiPrgPos(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_EXT_MIDI_PRG, 0, false);
		return;
	}

	if (ins->midiProgram != (int16_t)pos)
	{
		ins->midiProgram = (int16_t)pos;
		drawMIDIPrg();
		setSongModifiedFlag();
	}
}

void sbMidiBendPos(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_EXT_MIDI_BEND, 0, false);
		return;
	}

	if (ins->midiBend != (int16_t)pos)
	{
		ins->midiBend = (int16_t)pos;
		drawMIDIBend();
		setSongModifiedFlag();
	}
}

void updateNewSample(void)
{
	if (editor.ui.instrSwitcherShown)
		updateInstrumentSwitcher();

	updateSampleEditorSample();

	if (editor.ui.sampleEditorShown)
		updateSampleEditor();

	if (editor.ui.instEditorShown || editor.ui.instEditorExtShown)
		updateInstEditor();
}

void updateNewInstrument(void)
{
	updateTextBoxPointers();

	if (editor.ui.instrSwitcherShown)
		updateInstrumentSwitcher();

	editor.currVolEnvPoint = 0;
	editor.currPanEnvPoint = 0;

	updateSampleEditorSample();

	if (editor.ui.sampleEditorShown)
		updateSampleEditor();

	if (editor.ui.instEditorShown || editor.ui.instEditorExtShown)
		updateInstEditor();

	if (editor.ui.advEditShown)
		updateAdvEdit();
}

static void drawVolEnvSus(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envVSust);
	textOutFixed(382, 206, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawVolEnvRepS(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envVRepS);
	textOutFixed(382, 234, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawVolEnvRepE(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envVRepE);
	textOutFixed(382, 247, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawPanEnvSus(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envPSust);
	textOutFixed(382, 294, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawPanEnvRepS(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envPRepS);
	textOutFixed(382, 321, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawPanEnvRepE(void)
{
	char str[8];
	instrTyp *ins = getCurDispInstr();

	sprintf(str, "%02d", ins->envPRepE);
	textOutFixed(382, 335, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawVolume(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->samp[0];
	else
		s = &instr[editor.curInstr]->samp[editor.curSmp];

	hexOutBg(505, 178, PAL_FORGRND, PAL_DESKTOP, s->vol, 2);
}

static void drawPanning(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->samp[0];
	else
		s = &instr[editor.curInstr]->samp[editor.curSmp];

	hexOutBg(505, 192, PAL_FORGRND, PAL_DESKTOP, s->pan, 2);
}

static void drawFineTune(void)
{
	char sign;
	int16_t ftune;
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->samp[0];
	else
		s = &instr[editor.curInstr]->samp[editor.curSmp];

	fillRect(491, 205, 27, 8, PAL_DESKTOP);

	ftune = s->fine;
	if (ftune == 0)
	{
		charOut(512, 205, PAL_FORGRND, '0');
		return;
	}

	sign = (ftune < 0) ? '-' : '+';

	ftune = ABS(ftune);
	if (ftune >= 100)
	{
		charOut(491, 205, PAL_FORGRND, sign);
		charOut(498 + (0 * 7), 205, PAL_FORGRND, '0' + ((ftune / 100) % 10));
		charOut(498 + (1 * 7), 205, PAL_FORGRND, '0' + ((ftune /  10) % 10));
		charOut(498 + (2 * 7), 205, PAL_FORGRND, '0' + (ftune % 10));
	}
	else if (ftune >= 10)
	{
		charOut(498, 205, PAL_FORGRND, sign);
		charOut(505 + (0 * 7), 205, PAL_FORGRND, '0' + ((ftune / 10) % 10));
		charOut(505 + (1 * 7), 205, PAL_FORGRND, '0' + (ftune % 10));
	}
	else
	{
		charOut(505, 205, PAL_FORGRND, sign);
		charOut(512, 205, PAL_FORGRND, '0' + (ftune % 10));
	}
}

static void drawFadeout(void)
{
	hexOutBg(498, 222, PAL_FORGRND, PAL_DESKTOP, getCurDispInstr()->fadeOut, 3);
}

static void drawVibSpeed(void)
{
	hexOutBg(505, 236, PAL_FORGRND, PAL_DESKTOP, getCurDispInstr()->vibRate, 2);
}

static void drawVibDepth(void)
{
	hexOutBg(512, 250, PAL_FORGRND, PAL_DESKTOP, getCurDispInstr()->vibDepth, 1);
}

static void drawVibSweep(void)
{
	hexOutBg(505, 264, PAL_FORGRND, PAL_DESKTOP, getCurDispInstr()->vibSweep, 2);
}

static void drawRelTone(void)
{
	char noteChar1, noteChar2, octaChar;
	int8_t note2, note;

	if (instr[editor.curInstr] == NULL)
	{
		fillRect(598, 299, 8*3, 8, PAL_BCKGRND);
		return;
	}

	if (editor.curInstr == 0)
		note2 = 48;
	else
		note2 = 48 + instr[editor.curInstr]->samp[editor.curSmp].relTon;

	note = note2 % 12;
	if (config.ptnAcc == 0)
	{
		noteChar1 = sharpNote1Char[note];
		noteChar2 = sharpNote2Char[note];
	}
	else
	{
		noteChar1 = flatNote1Char[note];
		noteChar2 = flatNote2Char[note];
	}

	octaChar = '0' + (note2 / 12);

	charOutBg(598, 299, PAL_FORGRND, PAL_BCKGRND, noteChar1);
	charOutBg(606, 299, PAL_FORGRND, PAL_BCKGRND, noteChar2);
	charOutBg(614, 299, PAL_FORGRND, PAL_BCKGRND, octaChar);
}

static void setStdVolEnvelope(instrTyp *ins, uint8_t num)
{
	if (editor.curInstr == 0)
		return;

	pauseMusic();

	ins->fadeOut = config.stdFadeOut[num];
	ins->envVSust = (uint8_t)config.stdVolEnvSust[num];
	ins->envVRepS = (uint8_t)config.stdVolEnvRepS[num];
	ins->envVRepE = (uint8_t)config.stdVolEnvRepE[num];
	ins->envVPAnt = (uint8_t)config.stdVolEnvAnt[num];
	ins->envVTyp = (uint8_t)config.stdVolEnvTyp[num];
	ins->vibRate = (uint8_t)config.stdVibRate[num];
	ins->vibDepth = (uint8_t)config.stdVibDepth[num];
	ins->vibSweep = (uint8_t)config.stdVibSweep[num];
	ins->vibTyp = (uint8_t)config.stdVibTyp[num];

	memcpy(ins->envVP, config.stdEnvP[num][0], sizeof (int16_t) * 12 * 2);

	resumeMusic();
}

static void setStdPanEnvelope(instrTyp *ins, uint8_t num)
{
	if (editor.curInstr == 0)
		return;

	pauseMusic();

	ins->envPPAnt = (uint8_t)config.stdPanEnvAnt[num];
	ins->envPSust = (uint8_t)config.stdPanEnvSust[num];
	ins->envPRepS = (uint8_t)config.stdPanEnvRepS[num];
	ins->envPRepE = (uint8_t)config.stdPanEnvRepE[num];
	ins->envPTyp = (uint8_t)config.stdPanEnvTyp[num];

	memcpy(ins->envPP, config.stdEnvP[num][1], sizeof (int16_t) * 12 * 2);

	resumeMusic();
}

static void setOrStoreVolEnvPreset(uint8_t num)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
		return;

	if (mouse.rightButtonReleased)
	{
		// store preset
		config.stdFadeOut[num] = ins->fadeOut;
		config.stdVolEnvSust[num] = ins->envVSust;
		config.stdVolEnvRepS[num] = ins->envVRepS;
		config.stdVolEnvRepE[num] = ins->envVRepE;
		config.stdVolEnvAnt[num] = ins->envVPAnt;
		config.stdVolEnvTyp[num] = ins->envVTyp;
		config.stdVibRate[num] = ins->vibRate;
		config.stdVibDepth[num] = ins->vibDepth;
		config.stdVibSweep[num] = ins->vibSweep;
		config.stdVibTyp[num] = ins->vibTyp;

		memcpy(config.stdEnvP[num][0], ins->envVP, sizeof (int16_t) * 12 * 2);
	}
	else if (mouse.leftButtonReleased)
	{
		// read preset
		setStdVolEnvelope(ins, num);
		editor.currVolEnvPoint = 0;
		updateInstEditor();
		setSongModifiedFlag();
	}
}

static void setOrStorePanEnvPreset(uint8_t num)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
		return;

	if (mouse.rightButtonReleased)
	{
		// store preset
		config.stdFadeOut[num] = ins->fadeOut;
		config.stdPanEnvSust[num] = ins->envPSust;
		config.stdPanEnvRepS[num] = ins->envPRepS;
		config.stdPanEnvRepE[num] = ins->envPRepE;
		config.stdPanEnvAnt[num] = ins->envPPAnt;
		config.stdPanEnvTyp[num] = ins->envPTyp;
		config.stdVibRate[num] = ins->vibRate;
		config.stdVibDepth[num] = ins->vibDepth;
		config.stdVibSweep[num] = ins->vibSweep;
		config.stdVibTyp[num] = ins->vibTyp;

		memcpy(config.stdEnvP[num][1], ins->envPP, sizeof (int16_t) * 12 * 2);
	}
	else if (mouse.leftButtonReleased)
	{
		// read preset
		setStdPanEnvelope(ins, num);
		editor.currPanEnvPoint = 0;
		updateInstEditor();
		setSongModifiedFlag();
	}
}

void volPreDef1(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(1 - 1);
}

void volPreDef2(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(2 - 1);
}

void volPreDef3(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(3 - 1);
}

void volPreDef4(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(4 - 1);
}

void volPreDef5(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(5 - 1);
}

void volPreDef6(void)
{
	if (editor.curInstr > 0)
		setOrStoreVolEnvPreset(6 - 1);
}

void panPreDef1(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(1 - 1);
}

void panPreDef2(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(2 - 1);
}

void panPreDef3(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(3 - 1);
}

void panPreDef4(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(4 - 1);
}

void panPreDef5(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(5 - 1);
}

void panPreDef6(void)
{
	if (editor.curInstr > 0)
		setOrStorePanEnvPreset(6 - 1);
}

void relToneOctUp(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->relTon <= 71-12)
		s->relTon += 12;
	else
		s->relTon = 71;

	drawRelTone();
	setSongModifiedFlag();
}

void relToneOctDown(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->relTon >= -48+12)
		s->relTon -= 12;
	else
		s->relTon = -48;

	drawRelTone();
	setSongModifiedFlag();
}

void relToneUp(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->relTon < 71)
	{
		s->relTon++;
		drawRelTone();
		setSongModifiedFlag();
	}
}

void relToneDown(void)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->relTon > -48)
	{
		s->relTon--;
		drawRelTone();
		setSongModifiedFlag();
	}
}

void volEnvAdd(void)
{
	int16_t i, ant;
	instrTyp *ins = instr[editor.curInstr];

	ant = ins->envVPAnt;
	if (ins == NULL || editor.curInstr == 0 || ant >= 12)
		return;

	i = (int16_t)editor.currVolEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant-1;
		if (i < 0)
			i = 0;
	}

	if (i < ant-1 && ins->envVP[i+1][0]-ins->envVP[i][0] < 2)
		return;

	if (ins->envVP[i][0] >= 323)
		return;

	for (int16_t j = ant; j > i; j--)
	{
		ins->envVP[j][0] = ins->envVP[j-1][0];
		ins->envVP[j][1] = ins->envVP[j-1][1];
	}

	if (ins->envVSust > i) { ins->envVSust++; drawVolEnvSus();  }
	if (ins->envVRepS > i) { ins->envVRepS++; drawVolEnvRepS(); }
	if (ins->envVRepE > i) { ins->envVRepE++; drawVolEnvRepE(); }

	if (i < ant-1)
	{
		ins->envVP[i+1][0] = (ins->envVP[i][0] + ins->envVP[i+2][0]) / 2;
		ins->envVP[i+1][1] = (ins->envVP[i][1] + ins->envVP[i+2][1]) / 2;
	}
	else
	{
		ins->envVP[i+1][0] = ins->envVP[i][0] + 10;
		ins->envVP[i+1][1] = ins->envVP[i][1];
	}

	if (ins->envVP[i+1][0] > 324)
		ins->envVP[i+1][0] = 324;

	ins->envVPAnt++;

	updateVolEnv = true;
	setSongModifiedFlag();
}

void volEnvDel(void)
{
	uint8_t drawSust, drawRepS, drawRepE;
	int16_t i;
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0 || ins->envVPAnt <= 2)
		return;

	i = (int16_t)editor.currVolEnvPoint;
	if (i < 0 || i >= ins->envVPAnt)
		return;

	for (int16_t j = i; j < ins->envVPAnt; j++)
	{
		ins->envVP[j][0] = ins->envVP[j+1][0];
		ins->envVP[j][1] = ins->envVP[j+1][1];
	}

	drawSust = false;
	drawRepS = false;
	drawRepE = false;

	if (ins->envVSust > i) { ins->envVSust--; drawSust = true; }
	if (ins->envVRepS > i) { ins->envVRepS--; drawRepS = true; }
	if (ins->envVRepE > i) { ins->envVRepE--; drawRepE = true; }

	ins->envVP[0][0] = 0;
	ins->envVPAnt--;

	if (ins->envVSust >= ins->envVPAnt) { ins->envVSust = ins->envVPAnt - 1; drawSust = true; }
	if (ins->envVRepS >= ins->envVPAnt) { ins->envVRepS = ins->envVPAnt - 1; drawRepS = true; }
	if (ins->envVRepE >= ins->envVPAnt) { ins->envVRepE = ins->envVPAnt - 1; drawRepE = true; }

	if (drawSust) drawVolEnvSus();
	if (drawRepS) drawVolEnvRepS();
	if (drawRepE) drawVolEnvRepE();

	if (ins->envVPAnt == 0)
		editor.currVolEnvPoint = 0;
	else if (editor.currVolEnvPoint >= ins->envVPAnt)
		editor.currVolEnvPoint = ins->envVPAnt-1;

	updateVolEnv = true;
	setSongModifiedFlag();
}

void volEnvSusUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVSust < ins->envVPAnt-1)
	{
		ins->envVSust++;
		drawVolEnvSus();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvSusDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVSust > 0)
	{
		ins->envVSust--;
		drawVolEnvSus();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepSUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVRepS < ins->envVRepE)
	{
		ins->envVRepS++;
		drawVolEnvRepS();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepSDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVRepS > 0)
	{
		ins->envVRepS--;
		drawVolEnvRepS();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepEUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVRepE < ins->envVPAnt-1)
	{
		ins->envVRepE++;
		drawVolEnvRepE();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepEDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envVRepE > ins->envVRepS)
	{
		ins->envVRepE--;
		drawVolEnvRepE();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvAdd(void)
{
	int16_t i, ant;
	instrTyp *ins = instr[editor.curInstr];

	ant = ins->envPPAnt;
	if (ins == NULL || editor.curInstr == 0 || ant >= 12)
		return;

	i = (int16_t)editor.currPanEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant-1;
		if (i < 0)
			i = 0;
	}

	if (i < ant-1 && ins->envPP[i+1][0]-ins->envPP[i][0] < 2)
		return;

	if (ins->envPP[i][0] >= 323)
		return;

	for (int16_t j = ant; j > i; j--)
	{
		ins->envPP[j][0] = ins->envPP[j-1][0];
		ins->envPP[j][1] = ins->envPP[j-1][1];
	}

	if (ins->envPSust > i) { ins->envPSust++; drawPanEnvSus();  }
	if (ins->envPRepS > i) { ins->envPRepS++; drawPanEnvRepS(); }
	if (ins->envPRepE > i) { ins->envPRepE++; drawPanEnvRepE(); }

	if (i < ant-1)
	{
		ins->envPP[i+1][0] = (ins->envPP[i][0] + ins->envPP[i+2][0]) / 2;
		ins->envPP[i+1][1] = (ins->envPP[i][1] + ins->envPP[i+2][1]) / 2;
	}
	else
	{
		ins->envPP[i+1][0] = ins->envPP[i][0] + 10;
		ins->envPP[i+1][1] = ins->envPP[i][1];
	}

	if (ins->envPP[i+1][0] > 324)
		ins->envPP[i+1][0] = 324;

	ins->envPPAnt++;

	updatePanEnv = true;
	setSongModifiedFlag();
}

void panEnvDel(void)
{
	uint8_t drawSust, drawRepS, drawRepE;
	int16_t i;
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0 || ins->envPPAnt <= 2)
		return;

	i = (int16_t)editor.currPanEnvPoint;
	if (i < 0 || i >= ins->envPPAnt)
		return;

	for (int16_t j = i; j < ins->envPPAnt; j++)
	{
		ins->envPP[j][0] = ins->envPP[j+1][0];
		ins->envPP[j][1] = ins->envPP[j+1][1];
	}

	drawSust = false;
	drawRepS = false;
	drawRepE = false;

	if (ins->envPSust > i) { ins->envPSust--; drawSust = true; }
	if (ins->envPRepS > i) { ins->envPRepS--; drawRepS = true; }
	if (ins->envPRepE > i) { ins->envPRepE--; drawRepE = true; }

	ins->envPP[0][0] = 0;
	ins->envPPAnt--;

	if (ins->envPSust >= ins->envPPAnt) { ins->envPSust = ins->envPPAnt - 1; drawSust = true; }
	if (ins->envPRepS >= ins->envPPAnt) { ins->envPRepS = ins->envPPAnt - 1; drawRepS = true; }
	if (ins->envPRepE >= ins->envPPAnt) { ins->envPRepE = ins->envPPAnt - 1; drawRepE = true; }

	if (drawSust) drawPanEnvSus();
	if (drawRepS) drawPanEnvRepS();
	if (drawRepE) drawPanEnvRepE();

	if (ins->envPPAnt == 0)
		editor.currPanEnvPoint = 0;
	else if (editor.currPanEnvPoint >= ins->envPPAnt)
		editor.currPanEnvPoint = ins->envPPAnt-1;

	updatePanEnv = true;
	setSongModifiedFlag();
}

void panEnvSusUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPSust < ins->envPPAnt-1)
	{
		ins->envPSust++;
		drawPanEnvSus();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvSusDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPSust > 0)
	{
		ins->envPSust--;
		drawPanEnvSus();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepSUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPRepS < ins->envPRepE)
	{
		ins->envPRepS++;
		drawPanEnvRepS();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepSDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPRepS > 0)
	{
		ins->envPRepS--;
		drawPanEnvRepS();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepEUp(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPRepE < ins->envPPAnt-1)
	{
		ins->envPRepE++;
		drawPanEnvRepE();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepEDown(void)
{
	instrTyp *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->envPRepE > ins->envPRepS)
	{
		ins->envPRepE--;
		drawPanEnvRepE();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void volDown(void)
{
	scrollBarScrollLeft(SB_INST_VOL, 1);
}

void volUp(void)
{
	scrollBarScrollRight(SB_INST_VOL, 1);
}

void panDown(void)
{
	scrollBarScrollLeft(SB_INST_PAN, 1);
}

void panUp(void)
{
	scrollBarScrollRight(SB_INST_PAN, 1);
}

void ftuneDown(void)
{
	scrollBarScrollLeft(SB_INST_FTUNE, 1);
}

void ftuneUp(void)
{
	scrollBarScrollRight(SB_INST_FTUNE, 1);
}

void fadeoutDown(void)
{
	scrollBarScrollLeft(SB_INST_FADEOUT, 1);
}

void fadeoutUp(void)
{
	scrollBarScrollRight(SB_INST_FADEOUT, 1);
}

void vibSpeedDown(void)
{
	scrollBarScrollLeft(SB_INST_VIBSPEED, 1);
}

void vibSpeedUp(void)
{
	scrollBarScrollRight(SB_INST_VIBSPEED, 1);
}

void vibDepthDown(void)
{
	scrollBarScrollLeft(SB_INST_VIBDEPTH, 1);
}

void vibDepthUp(void)
{
	scrollBarScrollRight(SB_INST_VIBDEPTH, 1);
}

void vibSweepDown(void)
{
	scrollBarScrollLeft(SB_INST_VIBSWEEP, 1);
}

void vibSweepUp(void)
{
	scrollBarScrollRight(SB_INST_VIBSWEEP, 1);
}

void setVolumeScroll(uint32_t pos)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		if (editor.curInstr == 0 && editor.curSmp != 0)
			setScrollBarPos(SB_INST_VOL, 0x40, false);
		else
			setScrollBarPos(SB_INST_VOL, 0, false);

		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->vol != (uint8_t)pos)
	{
		s->vol = (uint8_t)pos;
		drawVolume();
		setSongModifiedFlag();
	}
}

void setPanningScroll(uint32_t pos)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_PAN, 0x80, false);
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->pan != (uint8_t)pos)
	{
		s->pan = (uint8_t)pos;
		drawPanning();
		setSongModifiedFlag();
	}
}

void setFinetuneScroll(uint32_t pos)
{
	sampleTyp *s;

	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_FTUNE, 128, false); // finetune 0
		return;
	}

	s = &instr[editor.curInstr]->samp[editor.curSmp];
	if (s->fine != (int8_t)(pos - 128))
	{
		s->fine = (int8_t)(pos - 128);
		drawFineTune();
		setSongModifiedFlag();
	}
}

void setFadeoutScroll(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL)
	{
		setScrollBarPos(SB_INST_FADEOUT, 0, false);
		return;
	}

	if (editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_FADEOUT, 0x80, false);
		return;
	}

	if (ins->fadeOut != (uint16_t)pos)
	{
		ins->fadeOut = (uint16_t)pos;
		drawFadeout();
		setSongModifiedFlag();
	}
}

void setVibSpeedScroll(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_VIBSPEED, 0, false);
		return;
	}

	if (ins->vibRate != (uint8_t)pos)
	{
		ins->vibRate = (uint8_t)pos;
		drawVibSpeed();
		setSongModifiedFlag();
	}
}

void setVibDepthScroll(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_VIBDEPTH, 0, false);
		return;
	}

	if (ins->vibDepth != (uint8_t)pos)
	{
		ins->vibDepth = (uint8_t)pos;
		drawVibDepth();
		setSongModifiedFlag();
	}
}

void setVibSweepScroll(uint32_t pos)
{
	instrTyp *ins = instr[editor.curInstr];

	if (ins == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_VIBSWEEP, 0, false);
		return;
	}

	if (ins->vibSweep != (uint8_t)pos)
	{
		ins->vibSweep = (uint8_t)pos;
		drawVibSweep();
		setSongModifiedFlag();
	}
}

void rbVibWaveSine(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibTyp = 0;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_SINE].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveSquare(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibTyp = 1;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_SQUARE].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveRampDown(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibTyp = 2;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_RAMP_DOWN].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveRampUp(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibTyp = 3;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_RAMP_UP].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void cbVEnv(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_VENV].checked = false;
		drawCheckBox(CB_INST_VENV);
		return;
	}

	instr[editor.curInstr]->envVTyp ^= 1;
	updateVolEnv = true;

	setSongModifiedFlag();
}

void cbVEnvSus(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_VENV_SUS].checked = false;
		drawCheckBox(CB_INST_VENV_SUS);
		return;
	}

	instr[editor.curInstr]->envVTyp ^= 2;
	updateVolEnv = true;

	setSongModifiedFlag();
}

void cbVEnvLoop(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_VENV_LOOP].checked = false;
		drawCheckBox(CB_INST_VENV_LOOP);
		return;
	}

	instr[editor.curInstr]->envVTyp ^= 4;
	updateVolEnv = true;

	setSongModifiedFlag();
}

void cbPEnv(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_PENV].checked = false;
		drawCheckBox(CB_INST_PENV);
		return;
	}

	instr[editor.curInstr]->envPTyp ^= 1;
	updatePanEnv = true;

	setSongModifiedFlag();
}

void cbPEnvSus(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_PENV_SUS].checked = false;
		drawCheckBox(CB_INST_PENV_SUS);
		return;
	}

	instr[editor.curInstr]->envPTyp ^= 2;
	updatePanEnv = true;

	setSongModifiedFlag();
}

void cbPEnvLoop(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		checkBoxes[CB_INST_PENV_LOOP].checked = false;
		drawCheckBox(CB_INST_PENV_LOOP);
		return;
	}

	instr[editor.curInstr]->envPTyp ^= 4;
	updatePanEnv = true;

	setSongModifiedFlag();
}

static void smallHexOutBg(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint8_t val)
{
	const uint8_t *srcPtr;
	uint32_t *dstPtr, fg, bg;

	assert(val <= 0xF);

	fg = video.palette[fgPalette];
	bg = video.palette[bgPalette];
	dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	srcPtr = &smallHexBitmap[val * 5];

	for (uint32_t y = 0; y < 7; y++)
	{
		for (uint32_t x = 0; x < 5; x++)
			dstPtr[x] = srcPtr[x] ? fg : bg;

		dstPtr += SCREEN_W;
		srcPtr += 80;
	}
}

static void writePianoNumber(uint8_t note, uint8_t key, uint8_t octave)
{
	uint8_t number;
	uint16_t x;

	number = 0;
	if (instr[editor.curInstr] != NULL && editor.curInstr > 0)
		number = instr[editor.curInstr]->ta[note];

	x = keyDigitXPos[key] + (octave * 77);

	if (keyIsBlackTab[key])
		smallHexOutBg(x, 361, PAL_FORGRND, PAL_BCKGRND, number);
	else
		smallHexOutBg(x, 385, PAL_BCKGRND, PAL_FORGRND, number);
}

static void drawBlackPianoKey(uint8_t key, uint8_t octave, bool keyDown)
{
	uint16_t x = keyXPos[key] + (octave * 77);
	blit(x, 351, &blackPianoKeysBitmap[keyDown * (7*27)], 7, 27);
}

static void drawWhitePianoKey(uint8_t key, uint8_t octave,  bool keyDown)
{
	uint16_t x = keyXPos[key] + (octave * 77);
	blit(x, 351, &whitePianoKeysBitmap[(keyDown * (11*46*3)) + whiteKeysBmpOrder[key]], 11, 46);
}

void redrawPiano(void)
{
	uint8_t key, octave;

	memset(pianoKeyStatus, 0, sizeof (pianoKeyStatus));
	for (uint8_t i = 0; i < 96; i++)
	{
		key = i % 12;
		octave = i / 12;

		if (keyIsBlackTab[key])
			drawBlackPianoKey(key, octave, false);
		else
			drawWhitePianoKey(key, octave, false);

		writePianoNumber(i, key, octave);
	}
}

bool testPianoKeysMouseDown(bool mouseButtonDown)
{
	uint8_t key, note, octave;
	int32_t mx, my;
	instrTyp *ins;

	if (!editor.ui.instEditorShown)
		return false;

	mx = mouse.x;
	my = mouse.y;

	if (!mouseButtonDown)
	{
		if (my < 351 || my > 396 || mx < 8 || mx > 623)
			return false;

		mouse.lastUsedObjectType = OBJECT_PIANO;
	}
	else
	{
		my = CLAMP(my, 351, 396);
		mx = CLAMP(mx, 8, 623);
	}

	ins = instr[editor.curInstr];
	if (ins == NULL)
		return true;

	mx -= 8;
	if (my < 378)
	{
		// white keys and black keys (top)

		octave = (uint8_t)(mx / 77);
		mx %= 77; // width of all keys in one octave

		// this is pretty disgusting...
		     if (mx >= 69) key = 11;
		else if (mx >= 62) key = 10;
		else if (mx >= 58) key =  9;
		else if (mx >= 51) key =  8;
		else if (mx >= 47) key =  7;
		else if (mx >= 40) key =  6;
		else if (mx >= 33) key =  5;
		else if (mx >= 25) key =  4;
		else if (mx >= 18) key =  3;
		else if (mx >= 14) key =  2;
		else if (mx >=  7) key =  1;
		else               key =  0;

		note = (octave * 12) + key;
		if (ins->ta[note] != editor.curSmp)
		{
			ins->ta[note] = editor.curSmp;
			writePianoNumber(note, key, octave);
			setSongModifiedFlag();
		}
	}
	else
	{
		// white keys only (bottom)

		octave = (uint8_t)(mx / 77);
		key = (uint8_t)(mx % 77) / 11;

		note = (octave * 12) + whiteKeyIndex[key];
		if (ins->ta[note] != editor.curSmp)
		{
			ins->ta[note] = editor.curSmp;
			writePianoNumber(note, note % 12, octave);
			setSongModifiedFlag();
		}
	}

	return true;
}

static uint8_t getNote(uint8_t i) // returns 1..96
{
	int8_t fineTune;
	uint8_t note;
	int32_t period, loPeriod, hiPeriod, tmpPeriod, tableIndex;
	stmTyp *ch;

	ch = &stm[i];

	fineTune = (ch->fineTune >> 3) + 16;
	hiPeriod = 8 * 12 * 16;
	loPeriod = 0;
	period = ch->finalPeriod;

	for (i = 0; i < 8; i++)
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

	if (loPeriod >= ((8*12*16) + 15) - 1) // FT2 bug: off-by-one error
		loPeriod = (8*12*16) + 15;

	note = (uint8_t)(((loPeriod + 8) >> 4) - ch->relTonNr) + 1;
	return note;
}

void drawPiano(void) // draw piano in idle mode
{
	bool keyDown, newStatus[96];
	uint8_t key, note, octave;
	stmTyp *ch;

	memset(newStatus, 0, sizeof (newStatus));

	// find active notes
	if (editor.curInstr > 0)
	{
		for (uint8_t i = 0; i < song.antChn; i++)
		{
			ch = &stm[i];
			if (ch->instrNr == editor.curInstr)
			{
				note = getNote(i);
				if (ch->envSustainActive)
					newStatus[(note - 1) % 96] = true;
			}
		}
	}

	// draw keys
	for (uint8_t i = 0; i < 96; i++)
	{
		keyDown = newStatus[i];
		if (pianoKeyStatus[i] ^ keyDown)
		{
			key = i % 12;
			octave = i / 12;

			if (keyIsBlackTab[key])
				drawBlackPianoKey(key, octave, keyDown);
			else
				drawWhitePianoKey(key, octave, keyDown);

			pianoKeyStatus[i] = keyDown;
		}
	}
}

static uint8_t getNoteReplayer(syncedChannel_t *ch) // returns 1..96
{
	int8_t fineTune;
	uint8_t note;
	int32_t period, loPeriod, hiPeriod, tmpPeriod, tableIndex;

	fineTune = (ch->fineTune >> 3) + 16;
	hiPeriod = 8 * 12 * 16;
	loPeriod = 0;
	period = ch->finalPeriod;

	for (uint8_t i = 0; i < 8; i++)
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

	if (loPeriod >= ((8*12*16) + 15) - 1) // FT2 bug: off-by-one error
		loPeriod = (8*12*16) + 15;

	note = (uint8_t)(((loPeriod + 8) >> 4) - ch->relTonNr) + 1;
	return note;
}

void drawPianoReplayer(chSyncData_t *chSyncData) // draw piano with synced replayer datas
{
	bool keyDown, newStatus[96];
	uint8_t key, note, octave;
	syncedChannel_t *ch;

	memset(newStatus, 0, sizeof (newStatus));

	// find active notes
	if (editor.curInstr > 0)
	{
		for (uint8_t i = 0; i < song.antChn; i++)
		{
			ch = &chSyncData->channels[i];
			if (ch->instrNr == editor.curInstr)
			{
				note = getNoteReplayer(ch);
				if (ch->envSustainActive)
					newStatus[(note - 1) % 96] = true;
			}
		}
	}

	// draw keys
	for (uint8_t i = 0; i < 96; i++)
	{
		keyDown = newStatus[i];
		if (pianoKeyStatus[i] ^ keyDown)
		{
			key = i % 12;
			octave = i / 12;

			if (keyIsBlackTab[key])
				drawBlackPianoKey(key, octave, keyDown);
			else
				drawWhitePianoKey(key, octave, keyDown);

			pianoKeyStatus[i] = keyDown;
		}
	}
}

static void envelopeLine(int32_t nr, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t col)
{
	int16_t d, x, y, sx, sy, dx, dy;
	uint16_t ax, ay;
	int32_t pitch;
	uint32_t pal1, pal2, pixVal, *dst32;

	y1 = CLAMP(y1, 0, 66);
	y2 = CLAMP(y2, 0, 66);
	x1 = CLAMP(x1, 0, 335);
	x2 = CLAMP(x2, 0, 335);

	if (nr == 0)
	{
		y1 += 189;
		y2 += 189;
	}
	else
	{
		y1 += 276;
		y2 += 276;
	}

	// get coefficients
	dx = x2 - x1;
	ax = ABS(dx) * 2;
	sx = SGN(dx);
	dy = y2 - y1;
	ay = ABS(dy) * 2;
	sy = SGN(dy);
	x  = x1;
	y  = y1;

	pal1 = video.palette[PAL_BLCKMRK];
	pal2 = video.palette[PAL_BLCKTXT];
	pixVal = video.palette[col];
	pitch = sy * SCREEN_W;

	dst32 = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		d = ay - (ax / 2);

		while (true)
		{
			// invert certain colors
			if (*dst32 != pal2)
			{
				if (*dst32 == pal1)
					*dst32 = pal2;
				else
					*dst32 = pixVal;
			}

			if (x == x2)
				break;

			if (d >= 0)
			{
				d -= ax;
				dst32 += pitch;
			}

			x += sx;
			d += ay;
			dst32 += sx;
		}
	}
	else
	{
		d = ax - (ay / 2);

		while (true)
		{
			// invert certain colors
			if (*dst32 != pal2)
			{
				if (*dst32 == pal1)
					*dst32 = pal2;
				else
					*dst32 = pixVal;
			}

			if (y == y2)
				break;

			if (d >= 0)
			{
				d -= ay;
				dst32 += sx;
			}

			y += sy;
			d += ax;
			dst32 += pitch;
		}
	}
}

static void envelopePixel(int32_t nr, int16_t x, int16_t y, uint8_t col)
{
	y += (nr == 0) ? 189 : 276;
	video.frameBuffer[(y * SCREEN_W) + x] = video.palette[col];
}

static void envelopeDot(int32_t nr, int16_t x, int16_t y)
{
	uint32_t *dstPtr, pixVal;

	y += (nr == 0) ? 189 : 276;

	pixVal = video.palette[PAL_BLCKTXT];
	dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];

	for (y = 0; y < 3; y++)
	{
		*dstPtr++ = pixVal;
		*dstPtr++ = pixVal;
		*dstPtr++ = pixVal;

		dstPtr += (SCREEN_W - 3);
	}
}

static void envelopeVertLine(int32_t nr, int16_t x, int16_t y, uint8_t col)
{
	uint32_t *dstPtr, pixVal1, pixVal2;

	y += (nr == 0) ? 189 : 276;

	pixVal1 = video.palette[col];
	pixVal2 = video.palette[PAL_BLCKTXT];

	dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (y = 0; y < 33; y++)
	{
		if (*dstPtr != pixVal2)
			*dstPtr = pixVal1;

		dstPtr += (SCREEN_W * 2);
	}
}

static void writeEnvelope(int32_t nr)
{
	uint8_t selected;
	int16_t i, x, y, lx, ly, nd, sp, ls, le, (*curEnvP)[2];
	instrTyp *ins = instr[editor.curInstr];

	// clear envelope area
	if (nr == 0)
		clearRect(5, 189, 331, 67);
	else
		clearRect(5, 276, 331, 67);

	// draw dotted x/y lines
	for (i = 0; i <= 32; i++) envelopePixel(nr, 5, 1 + i * 2, PAL_PATTEXT);
	for (i = 0; i <= 8; i++) envelopePixel(nr, 4, 1 + i * 8, PAL_PATTEXT);
	for (i = 0; i <= 162; i++) envelopePixel(nr, 8 + i *  2, 65, PAL_PATTEXT);
	for (i = 0; i <= 6; i++) envelopePixel(nr, 8 + i * 50, 66, PAL_PATTEXT);

	// draw center line on pan envelope
	if (nr == 1)
		envelopeLine(nr, 8, 33, 332, 33, PAL_BLCKMRK);

	if (ins == NULL)
		return;

	// collect variables
	if (nr == 0)
	{
		nd = ins->envVPAnt;
		if (ins->envVTyp & 2)
			sp = ins->envVSust;
		else
			sp = -1;

		if (ins->envVTyp & 4)
		{
			ls = ins->envVRepS;
			le = ins->envVRepE;
		}
		else
		{
			ls = -1;
			le = -1;
		}

		curEnvP = ins->envVP;
		selected = editor.currVolEnvPoint;
	}
	else
	{
		nd = ins->envPPAnt;
		if (ins->envPTyp & 2)
			sp = ins->envPSust;
		else
			sp = -1;

		if (ins->envPTyp & 4)
		{
			ls = ins->envPRepS;
			le = ins->envPRepE;
		}
		else
		{
			ls = -1;
			le = -1;
		}

		curEnvP = ins->envPP;
		selected = editor.currPanEnvPoint;
	}

	if (nd > 12)
		nd = 12;

	lx = 0;
	ly = 0;

	// draw envelope
	for (i = 0; i < nd; i++)
	{
		x = curEnvP[i][0];
		y = curEnvP[i][1];

		x = CLAMP(x, 0, 324);
		
		if (nr == 0)
			y = CLAMP(y, 0, 64);
		else
			y = CLAMP(y, 0, 63);

		if ((uint16_t)curEnvP[i][0] <= 324)
		{
			envelopeDot(nr, 7 + x, 64 - y);

			// draw "envelope selected" data
			if (i == selected)
			{
				envelopeLine(nr, 5  + x, 64 - y, 5  + x, 66 - y, PAL_BLCKTXT);
				envelopeLine(nr, 11 + x, 64 - y, 11 + x, 66 - y, PAL_BLCKTXT);
				envelopePixel(nr, 5, 65 - y, PAL_BLCKTXT);
				envelopePixel(nr, 8 + x, 65, PAL_BLCKTXT);
			}

			// draw loop start marker
			if (i == ls)
			{
				envelopeLine(nr, x + 6, 1, x + 10, 1, PAL_PATTEXT);
				envelopeLine(nr, x + 7, 2, x +  9, 2, PAL_PATTEXT);
				envelopeVertLine(nr, x + 8, 1, PAL_PATTEXT);
			}

			// draw sustain marker
			if (i == sp)
				envelopeVertLine(nr, x + 8, 1, PAL_BLCKTXT);

			// draw loop end marker
			if (i == le)
			{
				envelopeLine(nr, x + 6, 65, x + 10, 65, PAL_PATTEXT);
				envelopeLine(nr, x + 7, 64, x +  9, 64, PAL_PATTEXT);
				envelopeVertLine(nr, x + 8, 1, PAL_PATTEXT);
			}
		}

		// draw envelope line
		if (i > 0 && lx < x)
			envelopeLine(nr, lx + 8, 65 - ly, x + 8, 65 - y, PAL_PATTEXT);

		lx = x;
		ly = y;
	}
}

void handleInstEditorRedrawing(void)
{
	if (updateVolEnv)
	{
		updateVolEnv = false;
		writeEnvelope(0);
	}

	if (updatePanEnv)
	{
		updatePanEnv = false;
		writeEnvelope(1);
	}
}

void hideInstEditor(void)
{
	editor.ui.instEditorShown = false;

	hideScrollBar(SB_INST_VOL);
	hideScrollBar(SB_INST_PAN);
	hideScrollBar(SB_INST_FTUNE);
	hideScrollBar(SB_INST_FADEOUT);
	hideScrollBar(SB_INST_VIBSPEED);
	hideScrollBar(SB_INST_VIBDEPTH);
	hideScrollBar(SB_INST_VIBSWEEP);

	hidePushButton(PB_INST_VDEF1);
	hidePushButton(PB_INST_VDEF2);
	hidePushButton(PB_INST_VDEF3);
	hidePushButton(PB_INST_VDEF4);
	hidePushButton(PB_INST_VDEF5);
	hidePushButton(PB_INST_VDEF6);
	hidePushButton(PB_INST_PDEF1);
	hidePushButton(PB_INST_PDEF2);
	hidePushButton(PB_INST_PDEF3);
	hidePushButton(PB_INST_PDEF4);
	hidePushButton(PB_INST_PDEF5);
	hidePushButton(PB_INST_PDEF6);
	hidePushButton(PB_INST_VP_ADD);
	hidePushButton(PB_INST_VP_DEL);
	hidePushButton(PB_INST_VS_UP);
	hidePushButton(PB_INST_VS_DOWN);
	hidePushButton(PB_INST_VREPS_UP);
	hidePushButton(PB_INST_VREPS_DOWN);
	hidePushButton(PB_INST_VREPE_UP);
	hidePushButton(PB_INST_VREPE_DOWN);
	hidePushButton(PB_INST_PP_ADD);
	hidePushButton(PB_INST_PP_DEL);
	hidePushButton(PB_INST_PS_UP);
	hidePushButton(PB_INST_PS_DOWN);
	hidePushButton(PB_INST_PREPS_UP);
	hidePushButton(PB_INST_PREPS_DOWN);
	hidePushButton(PB_INST_PREPE_UP);
	hidePushButton(PB_INST_PREPE_DOWN);
	hidePushButton(PB_INST_VOL_DOWN);
	hidePushButton(PB_INST_VOL_UP);
	hidePushButton(PB_INST_PAN_DOWN);
	hidePushButton(PB_INST_PAN_UP);
	hidePushButton(PB_INST_FTUNE_DOWN);
	hidePushButton(PB_INST_FTUNE_UP);
	hidePushButton(PB_INST_FADEOUT_DOWN);
	hidePushButton(PB_INST_FADEOUT_UP);
	hidePushButton(PB_INST_VIBSPEED_DOWN);
	hidePushButton(PB_INST_VIBSPEED_UP);
	hidePushButton(PB_INST_VIBDEPTH_DOWN);
	hidePushButton(PB_INST_VIBDEPTH_UP);
	hidePushButton(PB_INST_VIBSWEEP_DOWN);
	hidePushButton(PB_INST_VIBSWEEP_UP);
	hidePushButton(PB_INST_EXIT);
	hidePushButton(PB_INST_OCT_UP);
	hidePushButton(PB_INST_HALFTONE_UP);
	hidePushButton(PB_INST_OCT_DOWN);
	hidePushButton(PB_INST_HALFTONE_DOWN);

	hideCheckBox(CB_INST_VENV);
	hideCheckBox(CB_INST_VENV_SUS);
	hideCheckBox(CB_INST_VENV_LOOP);
	hideCheckBox(CB_INST_PENV);
	hideCheckBox(CB_INST_PENV_SUS);
	hideCheckBox(CB_INST_PENV_LOOP);

	hideRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
}

void exitInstEditor(void)
{
	hideInstEditor();
	showPatternEditor();
}

void updateInstEditor(void)
{
	uint16_t tmpID;
	sampleTyp *s;
	instrTyp *ins = getCurDispInstr();

	if (instr[editor.curInstr] == NULL)
		s = &ins->samp[0];
	else
		s = &ins->samp[editor.curSmp];

	// update instrument editor extension
	if (editor.ui.instEditorExtShown)
	{
		checkBoxes[CB_INST_EXT_MIDI].checked = ins->midiOn ? true : false;
		checkBoxes[CB_INST_EXT_MUTE].checked = ins->mute ? true : false;

		setScrollBarPos(SB_INST_EXT_MIDI_CH, ins->midiChannel, false);
		setScrollBarPos(SB_INST_EXT_MIDI_PRG, ins->midiProgram, false);
		setScrollBarPos(SB_INST_EXT_MIDI_BEND, ins->midiBend, false);

		drawCheckBox(CB_INST_EXT_MIDI);
		drawCheckBox(CB_INST_EXT_MUTE);

		drawMIDICh();
		drawMIDIPrg();
		drawMIDIBend();
	}

	if (!editor.ui.instEditorShown)
		return;

	drawVolEnvSus();
	drawVolEnvRepS();
	drawVolEnvRepE();
	drawPanEnvSus();
	drawPanEnvRepS();
	drawPanEnvRepE();
	drawVolume();
	drawPanning();
	drawFineTune();
	drawFadeout();
	drawVibSpeed();
	drawVibDepth();
	drawVibSweep();
	drawRelTone();

	// set scroll bars
	setScrollBarPos(SB_INST_VOL, s->vol, false);
	setScrollBarPos(SB_INST_PAN, s->pan, false);
	setScrollBarPos(SB_INST_FTUNE, 128 + s->fine, false);
	setScrollBarPos(SB_INST_FADEOUT, ins->fadeOut, false);
	setScrollBarPos(SB_INST_VIBSPEED, ins->vibRate, false);
	setScrollBarPos(SB_INST_VIBDEPTH, ins->vibDepth, false);
	setScrollBarPos(SB_INST_VIBSWEEP, ins->vibSweep, false);

	// set radio buttons

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	switch (ins->vibTyp)
	{
		default:
		case 0: tmpID = RB_INST_WAVE_SINE;      break;
		case 1: tmpID = RB_INST_WAVE_SQUARE;    break;
		case 2: tmpID = RB_INST_WAVE_RAMP_DOWN; break;
		case 3: tmpID = RB_INST_WAVE_RAMP_UP;   break;
	}

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// set check boxes

	checkBoxes[CB_INST_VENV].checked = (ins->envVTyp & 1) ? true : false;
	checkBoxes[CB_INST_VENV_SUS].checked = (ins->envVTyp & 2) ? true : false;
	checkBoxes[CB_INST_VENV_LOOP].checked = (ins->envVTyp & 4) ? true : false;
	checkBoxes[CB_INST_PENV].checked = (ins->envPTyp & 1) ? true : false;
	checkBoxes[CB_INST_PENV_SUS].checked = (ins->envPTyp & 2) ? true : false;
	checkBoxes[CB_INST_PENV_LOOP].checked = (ins->envPTyp & 4) ? true : false;

	if (editor.currVolEnvPoint >= ins->envVPAnt) editor.currVolEnvPoint = 0;
	if (editor.currPanEnvPoint >= ins->envPPAnt) editor.currPanEnvPoint = 0;

	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);

	drawCheckBox(CB_INST_VENV);
	drawCheckBox(CB_INST_VENV_SUS);
	drawCheckBox(CB_INST_VENV_LOOP);
	drawCheckBox(CB_INST_PENV);
	drawCheckBox(CB_INST_PENV_SUS);
	drawCheckBox(CB_INST_PENV_LOOP);

	updateVolEnv = true;
	updatePanEnv = true;

	redrawPiano();
}

void showInstEditor(void)
{
	if (editor.ui.extended) exitPatternEditorExtended();
	if (editor.ui.sampleEditorShown) hideSampleEditor();
	if (editor.ui.sampleEditorExtShown) hideSampleEditorExt();

	hidePatternEditor();
	editor.ui.instEditorShown = true;

	drawFramework(0,   173, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(0,   260, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(0,   347, 632,  53, FRAMEWORK_TYPE1);
	drawFramework(438, 173, 194,  45, FRAMEWORK_TYPE1);
	drawFramework(438, 218, 194,  76, FRAMEWORK_TYPE1);
	drawFramework(438, 294, 194,  53, FRAMEWORK_TYPE1);
	drawFramework(2,   188, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(2,   275, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(2,   349, 628,  49, FRAMEWORK_TYPE2);
	drawFramework(590, 296,  40,  15, FRAMEWORK_TYPE2);

	textOutShadow(20,  176, PAL_FORGRND, PAL_DSKTOP2, "Volume envelope:");
	textOutShadow(153, 176, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
	textOutShadow(358, 193, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
	textOutShadow(342, 206, PAL_FORGRND, PAL_DSKTOP2, "Point");
	textOutShadow(358, 219, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
	textOutShadow(342, 234, PAL_FORGRND, PAL_DSKTOP2, "Start");
	textOutShadow(342, 247, PAL_FORGRND, PAL_DSKTOP2, "End");
	textOutShadow(20,  263, PAL_FORGRND, PAL_DSKTOP2, "Panning envelope:");
	textOutShadow(152, 263, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
	textOutShadow(358, 280, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
	textOutShadow(342, 293, PAL_FORGRND, PAL_DSKTOP2, "Point");
	textOutShadow(358, 306, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
	textOutShadow(342, 321, PAL_FORGRND, PAL_DSKTOP2, "Start");
	textOutShadow(342, 334, PAL_FORGRND, PAL_DSKTOP2, "End");
	textOutShadow(443, 177, PAL_FORGRND, PAL_DSKTOP2, "Volume");
	textOutShadow(443, 191, PAL_FORGRND, PAL_DSKTOP2, "Panning");
	textOutShadow(443, 205, PAL_FORGRND, PAL_DSKTOP2, "Tune");
	textOutShadow(442, 222, PAL_FORGRND, PAL_DSKTOP2, "Fadeout");
	textOutShadow(442, 236, PAL_FORGRND, PAL_DSKTOP2, "Vib.speed");
	textOutShadow(442, 250, PAL_FORGRND, PAL_DSKTOP2, "Vib.depth");
	textOutShadow(442, 264, PAL_FORGRND, PAL_DSKTOP2, "Vib.sweep");
	textOutShadow(453, 299, PAL_FORGRND, PAL_DSKTOP2, "Tone relative to C-4:");

	showScrollBar(SB_INST_VOL);
	showScrollBar(SB_INST_PAN);
	showScrollBar(SB_INST_FTUNE);
	showScrollBar(SB_INST_FADEOUT);
	showScrollBar(SB_INST_VIBSPEED);
	showScrollBar(SB_INST_VIBDEPTH);
	showScrollBar(SB_INST_VIBSWEEP);

	showPushButton(PB_INST_VDEF1);
	showPushButton(PB_INST_VDEF2);
	showPushButton(PB_INST_VDEF3);
	showPushButton(PB_INST_VDEF4);
	showPushButton(PB_INST_VDEF5);
	showPushButton(PB_INST_VDEF6);
	showPushButton(PB_INST_PDEF1);
	showPushButton(PB_INST_PDEF2);
	showPushButton(PB_INST_PDEF3);
	showPushButton(PB_INST_PDEF4);
	showPushButton(PB_INST_PDEF5);
	showPushButton(PB_INST_PDEF6);
	showPushButton(PB_INST_VP_ADD);
	showPushButton(PB_INST_VP_DEL);
	showPushButton(PB_INST_VS_UP);
	showPushButton(PB_INST_VS_DOWN);
	showPushButton(PB_INST_VREPS_UP);
	showPushButton(PB_INST_VREPS_DOWN);
	showPushButton(PB_INST_VREPE_UP);
	showPushButton(PB_INST_VREPE_DOWN);
	showPushButton(PB_INST_PP_ADD);
	showPushButton(PB_INST_PP_DEL);
	showPushButton(PB_INST_PS_UP);
	showPushButton(PB_INST_PS_DOWN);
	showPushButton(PB_INST_PREPS_UP);
	showPushButton(PB_INST_PREPS_DOWN);
	showPushButton(PB_INST_PREPE_UP);
	showPushButton(PB_INST_PREPE_DOWN);
	showPushButton(PB_INST_VOL_DOWN);
	showPushButton(PB_INST_VOL_UP);
	showPushButton(PB_INST_PAN_DOWN);
	showPushButton(PB_INST_PAN_UP);
	showPushButton(PB_INST_FTUNE_DOWN);
	showPushButton(PB_INST_FTUNE_UP);
	showPushButton(PB_INST_FADEOUT_DOWN);
	showPushButton(PB_INST_FADEOUT_UP);
	showPushButton(PB_INST_VIBSPEED_DOWN);
	showPushButton(PB_INST_VIBSPEED_UP);
	showPushButton(PB_INST_VIBDEPTH_DOWN);
	showPushButton(PB_INST_VIBDEPTH_UP);
	showPushButton(PB_INST_VIBSWEEP_DOWN);
	showPushButton(PB_INST_VIBSWEEP_UP);
	showPushButton(PB_INST_EXIT);
	showPushButton(PB_INST_OCT_UP);
	showPushButton(PB_INST_HALFTONE_UP);
	showPushButton(PB_INST_OCT_DOWN);
	showPushButton(PB_INST_HALFTONE_DOWN);

	showCheckBox(CB_INST_VENV);
	showCheckBox(CB_INST_VENV_SUS);
	showCheckBox(CB_INST_VENV_LOOP);
	showCheckBox(CB_INST_PENV);
	showCheckBox(CB_INST_PENV_SUS);
	showCheckBox(CB_INST_PENV_LOOP);

	// draw auto-vibrato waveforms
	blitFast(455, 279, &vibWaveformBitmap[(12*10)*0], 12, 10);
	blitFast(485, 279, &vibWaveformBitmap[(12*10)*1], 12, 10);
	blitFast(515, 279, &vibWaveformBitmap[(12*10)*2], 12, 10);
	blitFast(545, 279, &vibWaveformBitmap[(12*10)*3], 12, 10);

	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);

	updateInstEditor();
	redrawPiano();
}

void toggleInstEditor(void)
{
	if (editor.ui.sampleEditorShown)
		hideSampleEditor();

	if (editor.ui.instEditorShown)
	{
		exitInstEditor();
	}
	else
	{
		hidePatternEditor();
		showInstEditor();
	}
}

bool testInstrVolEnvMouseDown(bool mouseButtonDown)
{
	uint8_t ant;
	int32_t x, y, mx, my, minX, maxX;
	instrTyp *ins;

	if (!editor.ui.instEditorShown || editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return false;

	ins = instr[editor.curInstr];

	ant = ins->envVPAnt;
	if (ant > 12)
		ant = 12;

	mx = mouse.x;
	my = mouse.y;

	if (!mouseButtonDown)
	{
		if (my < 189 || my > 256 || mx < 7 || mx > 334)
			return false;

		if (ins->envVPAnt == 0)
			return true;

		lastMouseX = mx;
		lastMouseY = my;

		for (uint8_t i = 0; i < ant; i++)
		{
			x = 8 + ins->envVP[i][0];
			y = 190 + (64 - ins->envVP[i][1]);

			if (mx >= x-2 && mx <= x+2 && my >= y-2 && my <= y+2)
			{
				editor.currVolEnvPoint = i;
				mouse.lastUsedObjectType = OBJECT_INSVOLENV;

				saveMouseX = 8 + (lastMouseX - x);
				saveMouseY = 190 + (lastMouseY - y);

				updateVolEnv = true;
				break;
			}
		}

		return true;
	}

	if (ins->envVPAnt == 0)
		return true;

	if (mx != lastMouseX)
	{
		lastMouseX = mx;

		if (ant > 1 && editor.currVolEnvPoint > 0)
		{
			mx -= saveMouseX;
			mx = CLAMP(mx, 0, 324);

			if (editor.currVolEnvPoint == ant-1)
			{
				minX = ins->envVP[editor.currVolEnvPoint-1][0] + 1;
				maxX = 324;
			}
			else
			{
				minX = ins->envVP[editor.currVolEnvPoint-1][0] + 1;
				maxX = ins->envVP[editor.currVolEnvPoint+1][0] - 1;
			}

			minX = CLAMP(minX, 0, 324);
			maxX = CLAMP(maxX, 0, 324);

			ins->envVP[editor.currVolEnvPoint][0] = (int16_t)(CLAMP(mx, minX, maxX));
			updateVolEnv = true;

			setSongModifiedFlag();
		}
	}

	if (my != lastMouseY)
	{
		lastMouseY = my;

		my -= saveMouseY;
		my = 64 - CLAMP(my, 0, 64);

		ins->envVP[editor.currVolEnvPoint][1] = (int16_t)my;
		updateVolEnv = true;

		setSongModifiedFlag();
	}

	return true;
}

bool testInstrPanEnvMouseDown(bool mouseButtonDown)
{
	uint8_t ant;
	int32_t x, y, mx, my, minX, maxX;
	instrTyp *ins;

	if (!editor.ui.instEditorShown || editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return false;

	ins = instr[editor.curInstr];

	ant = ins->envPPAnt;
	if (ant > 12)
		ant = 12;

	mx = mouse.x;
	my = mouse.y;

	if (!mouseButtonDown)
	{
		if (my < 277 || my > 343 || mx < 7 || mx > 334)
			return false;

		if (ins->envPPAnt == 0)
			return true;

		lastMouseX = mx;
		lastMouseY = my;

		for (uint8_t i = 0; i < ant; i++)
		{
			x = 8 + ins->envPP[i][0];
			y = 277 + (63 - ins->envPP[i][1]);

			if (mx >= x-2 && mx <= x+2 && my >= y-2 && my <= y+2)
			{
				editor.currPanEnvPoint = i;
				mouse.lastUsedObjectType = OBJECT_INSPANENV;

				saveMouseX = lastMouseX - x + 8;
				saveMouseY = lastMouseY - y + 277;

				updatePanEnv = true;
				break;
			}
		}

		return true;
	}

	if (ins->envPPAnt == 0)
		return true;

	if (mx != lastMouseX)
	{
		lastMouseX = mx;

		if (ant > 1 && editor.currPanEnvPoint > 0)
		{
			mx -= saveMouseX;
			mx = CLAMP(mx, 0, 324);

			if (editor.currPanEnvPoint == ant-1)
			{
				minX = ins->envPP[editor.currPanEnvPoint-1][0] + 1;
				maxX = 324;
			}
			else
			{
				minX = ins->envPP[editor.currPanEnvPoint-1][0] + 1;
				maxX = ins->envPP[editor.currPanEnvPoint+1][0] - 1;
			}

			minX = CLAMP(minX, 0, 324);
			maxX = CLAMP(maxX, 0, 324);

			ins->envPP[editor.currPanEnvPoint][0] = (int16_t)(CLAMP(mx, minX, maxX));
			updatePanEnv = true;

			setSongModifiedFlag();
		}
	}

	if (my != lastMouseY)
	{
		lastMouseY = my;

		my -= saveMouseY;
		my  = 63 - CLAMP(my, 0, 63);

		ins->envPP[editor.currPanEnvPoint][1] = (int16_t)my;
		updatePanEnv = true;

		setSongModifiedFlag();
	}

	return true;
}

void cbInstMidiEnable(void)
{
	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
	{
		checkBoxes[CB_INST_EXT_MIDI].checked = false;
		drawCheckBox(CB_INST_EXT_MIDI);
		return;
	}

	instr[editor.curInstr]->midiOn ^= 1;
	setSongModifiedFlag();
}

void cbInstMuteComputer(void)
{
	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
	{
		checkBoxes[CB_INST_EXT_MUTE].checked = false;
		drawCheckBox(CB_INST_EXT_MUTE);
		return;
	}

	instr[editor.curInstr]->mute ^= 1;
	setSongModifiedFlag();
}

void drawInstEditorExt(void)
{
	instrTyp *ins = instr[editor.curInstr];

	drawFramework(0,  92, 291, 17, FRAMEWORK_TYPE1);
	drawFramework(0, 109, 291, 19, FRAMEWORK_TYPE1);
	drawFramework(0, 128, 291, 45, FRAMEWORK_TYPE1);

	textOutShadow(4,   96,  PAL_FORGRND, PAL_DSKTOP2, "Instrument Editor Extension:");
	textOutShadow(20,  114, PAL_FORGRND, PAL_DSKTOP2, "Instrument MIDI enable");
	textOutShadow(189, 114, PAL_FORGRND, PAL_DSKTOP2, "Mute computer");
	textOutShadow(4,   133, PAL_FORGRND, PAL_DSKTOP2, "MIDI transmit channel");
	textOutShadow(4,   147, PAL_FORGRND, PAL_DSKTOP2, "MIDI program");
	textOutShadow(4,   160, PAL_FORGRND, PAL_DSKTOP2, "Bender range (halftones)");

	if (ins == NULL)
	{
		checkBoxes[CB_INST_EXT_MIDI].checked = false;
		checkBoxes[CB_INST_EXT_MUTE].checked = false;
		setScrollBarPos(SB_INST_EXT_MIDI_CH, 0, false);
		setScrollBarPos(SB_INST_EXT_MIDI_PRG, 0, false);
		setScrollBarPos(SB_INST_EXT_MIDI_BEND, 0, false);
	}
	else
	{
		checkBoxes[CB_INST_EXT_MIDI].checked = ins->midiOn ? true : false;
		checkBoxes[CB_INST_EXT_MUTE].checked = ins->mute ? true : false;
		setScrollBarPos(SB_INST_EXT_MIDI_CH, ins->midiChannel, false);
		setScrollBarPos(SB_INST_EXT_MIDI_PRG, ins->midiProgram, false);
		setScrollBarPos(SB_INST_EXT_MIDI_BEND, ins->midiBend, false);
	}

	showCheckBox(CB_INST_EXT_MIDI);
	showCheckBox(CB_INST_EXT_MUTE);

	showScrollBar(SB_INST_EXT_MIDI_CH);
	showScrollBar(SB_INST_EXT_MIDI_PRG);
	showScrollBar(SB_INST_EXT_MIDI_BEND);

	showPushButton(PB_INST_EXT_MIDI_CH_DOWN);
	showPushButton(PB_INST_EXT_MIDI_CH_UP);
	showPushButton(PB_INST_EXT_MIDI_PRG_DOWN);
	showPushButton(PB_INST_EXT_MIDI_PRG_UP);
	showPushButton(PB_INST_EXT_MIDI_BEND_DOWN);
	showPushButton(PB_INST_EXT_MIDI_BEND_UP);

	drawMIDICh();
	drawMIDIPrg();
	drawMIDIBend();
}

void showInstEditorExt(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	editor.ui.instEditorExtShown = true;
	editor.ui.scopesShown = false;
	drawInstEditorExt();
}

void hideInstEditorExt(void)
{
	hideScrollBar(SB_INST_EXT_MIDI_CH);
	hideScrollBar(SB_INST_EXT_MIDI_PRG);
	hideScrollBar(SB_INST_EXT_MIDI_BEND);
	hideCheckBox(CB_INST_EXT_MIDI);
	hideCheckBox(CB_INST_EXT_MUTE);
	hidePushButton(PB_INST_EXT_MIDI_CH_DOWN);
	hidePushButton(PB_INST_EXT_MIDI_CH_UP);
	hidePushButton(PB_INST_EXT_MIDI_PRG_DOWN);
	hidePushButton(PB_INST_EXT_MIDI_PRG_UP);
	hidePushButton(PB_INST_EXT_MIDI_BEND_DOWN);
	hidePushButton(PB_INST_EXT_MIDI_BEND_UP);

	editor.ui.instEditorExtShown = false;
	editor.ui.scopesShown = true;
	drawScopeFramework();
}

void toggleInstEditorExt(void)
{
	if (editor.ui.instEditorExtShown)
		hideInstEditorExt();
	else
		showInstEditorExt();
}

static bool testInstrSwitcherNormal(void) // Welcome to the Jungle
{
	uint8_t newEntry;

	if (mouse.x < 424 || mouse.x > 585)
		return false;

	if (mouse.y >= 5 && mouse.y <= 91)
	{
		// instruments
		if (mouse.x >= 446 && mouse.x <= 584)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// destination instrument
			newEntry = (editor.instrBankOffset + 1) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.curInstr != newEntry)
			{
				editor.curInstr = newEntry;
				updateTextBoxPointers();
				updateNewInstrument();
			}

			return true;
		}
		else if (mouse.x >= 424 && mouse.x <= 438)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// source isntrument
			newEntry = (editor.instrBankOffset + 1) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.srcInstr != newEntry)
			{
				editor.srcInstr = newEntry;
				updateInstrumentSwitcher();

				if (editor.ui.advEditShown)
					updateAdvEdit();
			}

			return true;
		}
	}
	else if (mouse.y >= 99 && mouse.y <= 152)
	{
		// samples
		if (mouse.x >= 446 && mouse.x <= 560)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-99) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// destionation sample
			newEntry = editor.sampleBankOffset + (uint8_t)((mouse.y - 99) / 11);
			if (editor.curSmp != newEntry)
			{
				editor.curSmp = newEntry;
				updateInstrumentSwitcher();
				updateSampleEditorSample();

				     if (editor.ui.sampleEditorShown) updateSampleEditor();
				else if (editor.ui.instEditorShown)   updateInstEditor();
			}

			return true;
		}
		else if (mouse.x >= 423 && mouse.x <= 438)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-99) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// source sample
			newEntry = editor.sampleBankOffset + (uint8_t)((mouse.y - 99) / 11);
			if (editor.srcSmp != newEntry)
			{
				editor.srcSmp = newEntry;
				updateInstrumentSwitcher();
			}

			return true;
		}
	}

	return false;
}

static bool testInstrSwitcherExtended(void) // Welcome to the Jungle 2 - The Happening
{
	uint8_t newEntry;

	if (mouse.y < 5 || mouse.y > 47)
		return false;

	if (mouse.x >= 511)
	{
		// right columns
		if (mouse.x <= 525)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// source instrument
			newEntry = (editor.instrBankOffset + 5) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.srcInstr != newEntry)
			{
				editor.srcInstr = newEntry;
				updateInstrumentSwitcher();

				if (editor.ui.advEditShown)
					updateAdvEdit();
			}

			return true;
		}
		else if (mouse.x >= 529 && mouse.x <= 626)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// destination instrument
			newEntry = (editor.instrBankOffset + 5) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.curInstr != newEntry)
			{
				editor.curInstr = newEntry;
				updateTextBoxPointers();
				updateNewInstrument();
			}

			return true;
		}
	}
	else if (mouse.x >= 388)
	{
		// left columns
		if (mouse.x <= 402)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// source instrument
			newEntry = (editor.instrBankOffset + 1) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.srcInstr != newEntry)
			{
				editor.srcInstr = newEntry;
				updateInstrumentSwitcher();

				if (editor.ui.advEditShown)
					updateAdvEdit();
			}

			return true;
		}
		else if (mouse.x >= 406 && mouse.x <= 503)
		{
			mouse.lastUsedObjectType = OBJECT_INSTRSWITCH;

			if ((mouse.y-5) % 11 == 10)
				return true; // we clicked on the one-pixel spacer

			// destination instrument
			newEntry = (editor.instrBankOffset + 1) + (uint8_t)((mouse.y - 5) / 11);
			if (editor.curInstr != newEntry)
			{
				editor.curInstr = newEntry;
				updateTextBoxPointers();
				updateNewInstrument();
			}

			return true;
		}
	}

	return false;
}

bool testInstrSwitcherMouseDown(void)
{
	if (!editor.ui.instrSwitcherShown)
		return false;

	if (editor.ui.extended)
		return testInstrSwitcherExtended();
	else
		return testInstrSwitcherNormal();
}

static int32_t SDLCALL saveInstrThread(void *ptr)
{
	int16_t n;
	int32_t i;
	size_t result;
	FILE *f;
	instrXIHeaderTyp ih;
	instrTyp *ins;
	sampleTyp *s;
	sampleHeaderTyp *dst;

	(void)ptr;

	if (editor.tmpFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	n = getUsedSamples(saveInstrNr);
	if (n == 0 || instr[saveInstrNr] == NULL)
	{
		okBoxThreadSafe(0, "System message", "Instrument is empty!");
		return false;
	}

	f = UNICHAR_FOPEN(editor.tmpFilenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	memset(&ih, 0, sizeof (ih)); // important, also clears reserved stuff

	memcpy(ih.sig, "Extended Instrument: ", 21);
	memset(ih.name, ' ', 22);
	memcpy(ih.name, song.instrName[saveInstrNr], strlen(song.instrName[saveInstrNr]));
	ih.name[22] = 0x1A;
	memcpy(ih.progName, PROG_NAME_STR, 20);
	ih.ver = 0x0102;

	// copy over instrument struct data to instrument header
	ins = instr[saveInstrNr];
	memcpy(ih.ta, ins->ta, 96);
	memcpy(ih.envVP, ins->envVP, 12*2*sizeof(int16_t));
	memcpy(ih.envPP, ins->envPP, 12*2*sizeof(int16_t));
	ih.envVPAnt = ins->envVPAnt;
	ih.envPPAnt = ins->envPPAnt;
	ih.envVSust = ins->envVSust;
	ih.envVRepS = ins->envVRepS;
	ih.envVRepE = ins->envVRepE;
	ih.envPSust = ins->envPSust;
	ih.envPRepS = ins->envPRepS;
	ih.envPRepE = ins->envPRepE;
	ih.envVTyp = ins->envVTyp;
	ih.envPTyp = ins->envPTyp;
	ih.vibTyp = ins->vibTyp;
	ih.vibSweep = ins->vibSweep;
	ih.vibDepth = ins->vibDepth;
	ih.vibRate = ins->vibRate;
	ih.fadeOut = ins->fadeOut;
	ih.midiOn = ins->midiOn ? 1 : 0;
	ih.midiChannel = ins->midiChannel;
	ih.midiProgram = ins->midiProgram;
	ih.midiBend = ins->midiBend;
	ih.mute = ins->mute ? 1 : 0;
	ih.antSamp = n;

	// copy over sample struct datas to sample headers
	for (i = 0; i < n; i++)
	{
		s = &instr[saveInstrNr]->samp[i];
		dst = &ih.samp[i];

		dst->len = s->len;
		dst->repS = s->repS;
		dst->repL = s->repL;
		dst->vol = s->vol;
		dst->fine = s->fine;
		dst->typ = s->typ;
		dst->pan = s->pan;
		dst->relTon = s->relTon;

		dst->nameLen = (uint8_t)strlen(s->name);
		memcpy(dst->name, s->name, 22);

		if (s->pek == NULL)
			dst->len = 0;
	}

	result = fwrite(&ih, INSTR_XI_HEADER_SIZE + (ih.antSamp * sizeof (sampleHeaderTyp)), 1, f);
	if (result != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving instrument: general I/O error!");
		return false;
	}

	pauseAudio();
	for (i = 0; i < n; i++)
	{
		s = &instr[saveInstrNr]->samp[i];
		if (s->pek != NULL && s->len > 0)
		{
			restoreSample(s);
			samp2Delta(s->pek, s->len, s->typ);

			result = fwrite(s->pek, 1, s->len, f);

			delta2Samp(s->pek, s->len, s->typ);
			fixSample(s);

			if (result != (size_t)s->len) // write not OK
			{
				resumeAudio();
				fclose(f);
				okBoxThreadSafe(0, "System message", "Error saving instrument: general I/O error!");
				return false;
			}
		}
	}
	resumeAudio();

	fclose(f);

	editor.diskOpReadDir = true; // force diskop re-read

	setMouseBusy(false);
	return true;
}

void saveInstr(UNICHAR *filenameU, int16_t nr)
{
	if (nr == 0)
		return;

	saveInstrNr = nr;
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(saveInstrThread, NULL, NULL);
	if (thread == NULL)
	{
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

static int16_t getPATNote(int32_t freq)
{
	double dNote = (log2(freq * (1.0 / 440000.0)) * 12.0) + 57.0;
	int32_t note = (int32_t)(dNote + 0.5);

	return (int16_t)note;
}

static int32_t SDLCALL loadInstrThread(void *ptr)
{
	bool stereoWarning;
	int8_t *newPtr;
	int16_t a, b;
	int32_t i, j;
	double dFreq;
	FILE *f;
	instrXIHeaderTyp ih;
	instrPATHeaderTyp ih_PAT;
	instrPATWaveHeaderTyp ih_PATWave;
	sampleHeaderTyp *src;
	sampleTyp *s;
	instrTyp *ins;

	(void)ptr;

	stereoWarning = false;

	if (editor.tmpInstrFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	f = UNICHAR_FOPEN(editor.tmpInstrFilenameU, "rb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	memset(&ih, 0, sizeof (ih));

	fread(&ih, INSTR_XI_HEADER_SIZE, 1, f);
	if (!strncmp(ih.sig, "Extended Instrument: ", 21))
	{
		// XI - Extended Instrument

		if (ih.ver != 0x0101 && ih.ver != 0x0102)
		{
			okBoxThreadSafe(0, "System message", "Incompatible format version!");
			goto loadDone;
		}

		if (ih.ver == 0x0101) // not even FT2.01 can save old v1.01 .XI files, so I have no way to verify this.
		{
			fseek(f, -20, SEEK_CUR);
			ih.antSamp = ih.midiProgram;
			ih.midiProgram = 0;
			ih.midiBend = 0;
			ih.mute = false;
		}

		if (ih.antSamp > MAX_SMP_PER_INST)
		{
			okBoxThreadSafe(0, "System message", "Incompatible instrument!");
			goto loadDone;
		}

		memcpy(song.instrName[editor.curInstr], ih.name, 22);
		song.instrName[editor.curInstr][22] = '\0';

		pauseAudio();

		freeInstr(editor.curInstr);

		if (ih.antSamp > 0)
		{
			if (!allocateInstr(editor.curInstr))
			{
				resumeAudio();
				okBoxThreadSafe(0, "System message", "Not enough memory!");
				goto loadDone;
			}

			// copy instrument header elements to our instrument struct

			ins = instr[editor.curInstr];
			memcpy(ins->ta, ih.ta, 96);
			memcpy(ins->envVP, ih.envVP, 12*2*sizeof(int16_t));
			memcpy(ins->envPP, ih.envPP, 12*2*sizeof(int16_t));
			ins->envVPAnt = ih.envVPAnt;
			ins->envPPAnt = ih.envPPAnt;
			ins->envVSust = ih.envVSust;
			ins->envVRepS = ih.envVRepS;
			ins->envVRepE = ih.envVRepE;
			ins->envPSust = ih.envPSust;
			ins->envPRepS = ih.envPRepS;
			ins->envPRepE = ih.envPRepE;
			ins->envVTyp = ih.envVTyp;
			ins->envPTyp = ih.envPTyp;
			ins->vibTyp = ih.vibTyp;
			ins->vibSweep = ih.vibSweep;
			ins->vibDepth = ih.vibDepth;
			ins->vibRate = ih.vibRate;
			ins->fadeOut = ih.fadeOut;
			ins->midiOn = (ih.midiOn > 0) ? true : false;
			ins->midiChannel = ih.midiChannel;
			ins->midiProgram = ih.midiProgram;
			ins->midiBend = ih.midiBend;
			ins->mute = (ih.mute > 0) ? true : false;
			ins->antSamp = ih.antSamp; // used in loadInstrSample()

			// sanitize stuff for broken/unsupported instruments
			ins->midiProgram = CLAMP(ins->midiProgram, 0, 127);
			ins->midiBend = CLAMP(ins->midiBend, 0, 36);

			if (ins->midiChannel > 15) ins->midiChannel = 15;
			if (ins->vibDepth > 0x0F) ins->vibDepth = 0x0F;
			if (ins->vibRate > 0x3F) ins->vibRate = 0x3F;
			if (ins->vibTyp > 3) ins->vibTyp = 0;

			for (i = 0; i < 96; i++)
			{
				if (ins->ta[i] > 15)
					ins->ta[i] = 15;
			}

			if (ins->envVPAnt > 12) ins->envVPAnt = 12;
			if (ins->envVRepS > 11) ins->envVRepS = 11;
			if (ins->envVRepE > 11) ins->envVRepE = 11;
			if (ins->envVSust > 11) ins->envVSust = 11;
			if (ins->envPPAnt > 12) ins->envPPAnt = 12;
			if (ins->envPRepS > 11) ins->envPRepS = 11;
			if (ins->envPRepE > 11) ins->envPRepE = 11;
			if (ins->envPSust > 11) ins->envPSust = 11;

			for (i = 0; i < 12; i++)
			{
				if ((uint16_t)ins->envVP[i][0] > 32767) ins->envVP[i][0] = 32767;
				if ((uint16_t)ins->envPP[i][0] > 32767) ins->envPP[i][0] = 32767;
				if ((uint16_t)ins->envVP[i][1] > 64) ins->envVP[i][1] = 64;
				if ((uint16_t)ins->envPP[i][1] > 63) ins->envPP[i][1] = 63;
				
			}

			if (fread(ih.samp, sizeof (sampleHeaderTyp) * ih.antSamp, 1, f) != 1)
			{
				freeInstr(editor.curInstr);
				resumeAudio();
				okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
				goto loadDone;
			}

			for (i = 0; i < ih.antSamp; i++)
			{
				s = &instr[editor.curInstr]->samp[i];
				src = &ih.samp[i];

				// copy sample header elements to our sample struct

				s->len = src->len;
				s->repS = src->repS;
				s->repL = src->repL;
				s->vol = src->vol;
				s->fine = src->fine;
				s->typ = src->typ;
				s->pan = src->pan;
				s->relTon = src->relTon;
				memcpy(s->name, src->name, 22);
				s->name[22] = '\0';

				// dst->pek is set up later

				// trim off spaces at end of name
				for (j = 21; j >= 0; j--)
				{
					if (s->name[j] == ' ' || s->name[j] == 0x1A)
						s->name[j] = '\0';
					else
						break;
				}

				// sanitize stuff broken/unsupported samples
				if (s->vol > 64)
					s->vol = 64;

				s->relTon = CLAMP(s->relTon, -48, 71);
			}
		}

		for (i = 0; i < ih.antSamp; i++)
		{
			s = &instr[editor.curInstr]->samp[i];

			// if a sample has both forward loop and pingpong loop set, make it pingpong loop only (FT2 mixer behavior)
			if ((s->typ & 3) == 3)
				s->typ &= 0xFE;

			if (s->len > 0)
			{
				s->pek = NULL;
				s->origPek = (int8_t *)malloc(s->len + LOOP_FIX_LEN);
				if (s->origPek == NULL)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "Not enough memory!");
					goto loadDone;
				}

				s->pek = s->origPek + SMP_DAT_OFFSET;

				if (fread(s->pek, s->len, 1, f) != 1)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				delta2Samp(s->pek, s->len, s->typ); // stereo samples are handled here

				// check if it was a stereo sample
				if (s->typ & 32)
				{
					s->typ &= ~32;

					s->len /= 2;
					s->repL /= 2;
					s->repS /= 2;

					newPtr = (int8_t *)realloc(s->origPek, s->len + LOOP_FIX_LEN);
					if (newPtr != NULL)
					{
						s->origPek = newPtr;
						s->pek = s->origPek + SMP_DAT_OFFSET;
					}

					stereoWarning = true;
				}

				checkSampleRepeat(s);
				fixSample(s);
			}
		}

		resumeAudio();
	}
	else
	{
		rewind(f);

		fread(&ih_PAT, 1, sizeof (instrPATHeaderTyp), f);
		if (!memcmp(ih_PAT.id, "GF1PATCH110\0ID#000002\0", 22))
		{
			// PAT - Gravis Ultrasound GF1 patch

			if (ih_PAT.antSamp == 0)
				ih_PAT.antSamp = 1; // to some patch makers, 0 means 1

			if (ih_PAT.layers > 1 || ih_PAT.antSamp > MAX_SMP_PER_INST)
			{
				okBoxThreadSafe(0, "System message", "Incompatible instrument!");
				goto loadDone;
			}

			pauseAudio();
			freeInstr(editor.curInstr);

			if (!allocateInstr(editor.curInstr))
			{
				okBoxThreadSafe(0, "System message", "Not enough memory!");
				goto loadDone;
			}

			memset(song.instrName[editor.curInstr], 0, 22 + 1);
			memcpy(song.instrName[editor.curInstr], ih_PAT.instrName, 16);

			for (i = 0; i < ih_PAT.antSamp; i++)
			{
				s = &instr[editor.curInstr]->samp[i];
				ins = instr[editor.curInstr];

				if (fread(&ih_PATWave, 1, sizeof (ih_PATWave), f) != sizeof (ih_PATWave))
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				s->pek = NULL;
				s->origPek = (int8_t *)malloc(ih_PATWave.waveSize + LOOP_FIX_LEN);
				if (s->origPek == NULL)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "Not enough memory!");
					goto loadDone;
				}

				s->pek = s->origPek + SMP_DAT_OFFSET;

				if (i == 0)
				{
					ins->vibSweep = ih_PATWave.vibSweep;

					ins->vibRate = (ih_PATWave.vibRate + 2) / 4;
					if (ins->vibRate > 0x3F)
						ins->vibRate = 0x3F;

					ins->vibDepth = (ih_PATWave.vibDepth + 1) / 2;
					if (ins->vibDepth > 0x0F)
						ins->vibDepth = 0x0F;
				}

				s = &instr[editor.curInstr]->samp[i];

				memcpy(s->name, ih_PATWave.name, 7);

				s->typ = (ih_PATWave.mode & 1) << 4; // 16-bit flag
				if (ih_PATWave.mode & 4) // loop enabled?
				{
					if (ih_PATWave.mode & 8)
						s->typ |= 2; // pingpong loop
					else
						s->typ |= 1; // forward loop
				}

				s->pan = ((ih_PATWave.pan << 4) & 0xF0) | (ih_PATWave.pan & 0xF);

				if (s->typ & 16)
				{
					ih_PATWave.waveSize &= 0xFFFFFFFE;
					ih_PATWave.repS &= 0xFFFFFFFE;
					ih_PATWave.repE &= 0xFFFFFFFE;
				}

				s->len = ih_PATWave.waveSize;
				if (s->len > MAX_SAMPLE_LEN)
					s->len = MAX_SAMPLE_LEN;

				s->repS = ih_PATWave.repS;
				if (s->repS > s->len)
					s->repS = 0;

				s->repL = ih_PATWave.repE - ih_PATWave.repS;
				if (s->repL < 0)
					s->repL = 0;

				if (s->repS+s->repL > s->len)
					s->repL = s->len - s->repS;

				dFreq = (1.0 + (ih_PATWave.fineTune / 512.0)) * ih_PATWave.sampleRate;
				int32_t freq = (int32_t)(dFreq + 0.5);
				tuneSample(s, freq);

				a = s->relTon - (getPATNote(ih_PATWave.rootFrq) - (12 * 3));
				s->relTon = (uint8_t)CLAMP(a, -48, 71);

				a = getPATNote(ih_PATWave.lowFrq);
				b = getPATNote(ih_PATWave.highFreq);

				a = CLAMP(a, 0, 95);
				b = CLAMP(b, 0, 95);

				for (j = a; j <= b; j++)
					ins->ta[j] = (uint8_t)i;

				if (fread(s->pek, ih_PATWave.waveSize, 1, f) != 1)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				if (ih_PATWave.mode & 2)
				{
					if (s->typ & 16)
						conv16BitSample(s->pek, s->len, false);
					else
						conv8BitSample(s->pek, s->len, false);
				}

				fixSample(s);
			}

			resumeAudio();
		}
	}

loadDone:
	fclose(f);

	fixSampleName(editor.curInstr);
	editor.updateCurInstr = true; // setMouseBusy(false) is called in the input/video thread when done

	if (stereoWarning)
		okBoxThreadSafe(0, "System message", "The instrument contains stereo samples! They were mixed to mono.");

	return true;
}

static bool fileIsInstr(UNICHAR *filename)
{
	char header[22];
	FILE *f;

	f = UNICHAR_FOPEN(filename, "rb");
	if (f == NULL)
		return false;

	fread(header, 1, sizeof (header), f);
	fclose(f);

	if (!strncmp(header, "Extended Instrument: ", 21) || !memcmp(header, "GF1PATCH110\0ID#000002\0", 22))
		return true;

	return false;
}

void loadInstr(UNICHAR *filenameU)
{
	if (editor.curInstr == 0)
	{
		okBox(0, "System message", "The zero-instrument cannot hold intrument data.");
		return;
	}

	UNICHAR_STRCPY(editor.tmpInstrFilenameU, filenameU);

	if (fileIsInstr(filenameU))
	{
		// load as instrument
		mouseAnimOn();
		thread = SDL_CreateThread(loadInstrThread, NULL, NULL);
		if (thread == NULL)
		{
			okBox(0, "System message", "Couldn't create thread!");
			return;
		}

		SDL_DetachThread(thread);
	}
	else
	{
		// load as sample into sample slot #0 (and clear instrument)
		loadSample(editor.tmpInstrFilenameU, 0, true);
	}
}
