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
#include "scopes/ft2_scopes.h"
#include "ft2_sample_ed.h"
#include "ft2_mouse.h"
#include "ft2_video.h"
#include "ft2_sample_loader.h"
#include "ft2_diskop.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_bmp.h"

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct patHdr_t
{
	char ID[22], junk1[60];
	uint8_t numInstrs, junk2, numChannels;
	int16_t waveforms, masterVol;
	int32_t dataSize;
	char junk4[36];
	int16_t junk5;
	char instrName[16];
	int32_t instrSize;
	uint8_t layers;
	char junk6[40];
	uint8_t junk7, junk8;
	int32_t junk9;
	uint8_t numSamples;
	char junk10[40];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
patHdr_t;

typedef struct patWaveHdr_t
{
	char name[7];
	uint8_t fractions;
	int32_t sampleLength, loopStart, loopEnd;
	uint16_t sampleRate;
	int32_t lowFrq, highFreq, rootFrq;
	int16_t finetune;
	uint8_t panning, envRate[6], envOfs[6], tremSweep, tremRate;
	uint8_t tremDepth, vibSweep, vibRate, vibDepth, flags;
	int16_t junk1;
	uint16_t junk2;
	char junk3[36];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
patWaveHdr_t;

typedef struct xiHdr_t
{
	char ID[21], name[23], progName[20];
	uint16_t version;
	uint8_t note2SampleLUT[96];
	int16_t volEnvPoints[12][2], panEnvPoints[12][2];
	uint8_t volEnvLength, panEnvLength, volEnvSustain, volEnvLoopStart, volEnvLoopEnd, panEnvSustain, panEnvLoopStart;
	uint8_t panEnvLoopEnd, volEnvFlags, panEnvFlags, vibType, vibSweep, vibDepth, vibRate;
	uint16_t fadeout;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	uint8_t mute, reserved[15];
	int16_t numSamples;
	xmSmpHdr_t smp[16];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
xiHdr_t;

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

static const uint8_t mx2PianoKey[77] =
{
	0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4,
	4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,
	9,9,9,9,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11
};

// thread data
static uint16_t saveInstrNum;
static SDL_Thread *thread;

extern const uint16_t *note2Period; // ft2_replayer.c

void updateInstEditor(void);
void updateNewInstrument(void);

void sanitizeInstrument(instr_t *ins)
{
	if (ins == NULL)
		return;

	ins->midiProgram = CLAMP(ins->midiProgram, 0, 127);
	ins->midiBend = CLAMP(ins->midiBend, 0, 36);

	if (ins->midiChannel > 15) ins->midiChannel = 15;
	if (ins->vibDepth > 0x0F) ins->vibDepth = 0x0F;
	if (ins->vibRate > 0x3F) ins->vibRate = 0x3F;
	if (ins->vibType > 3) ins->vibType = 0;

	for (int32_t i = 0; i < 96; i++)
	{
		if (ins->note2SampleLUT[i] >= MAX_SMP_PER_INST)
			ins->note2SampleLUT[i] = MAX_SMP_PER_INST-1;
	}

	if (ins->volEnvLength > 12) ins->volEnvLength = 12;
	if (ins->volEnvLoopStart > 11) ins->volEnvLoopStart = 11;
	if (ins->volEnvLoopEnd > 11) ins->volEnvLoopEnd = 11;
	if (ins->volEnvSustain > 11) ins->volEnvSustain = 11;
	if (ins->panEnvLength > 12) ins->panEnvLength = 12;
	if (ins->panEnvLoopStart > 11) ins->panEnvLoopStart = 11;
	if (ins->panEnvLoopEnd > 11) ins->panEnvLoopEnd = 11;
	if (ins->panEnvSustain > 11) ins->panEnvSustain = 11;

	for (int32_t i = 0; i < 12; i++)
	{
		if ((uint16_t)ins->volEnvPoints[i][0] > 32767) ins->volEnvPoints[i][0] = 32767;
		if ((uint16_t)ins->panEnvPoints[i][0] > 32767) ins->panEnvPoints[i][0] = 32767;
		if ((uint16_t)ins->volEnvPoints[i][1] > 64) ins->volEnvPoints[i][1] = 64;
		if ((uint16_t)ins->panEnvPoints[i][1] > 63) ins->panEnvPoints[i][1] = 63;
	}
}

static instr_t *getCurDispInstr(void)
{
	if (instr[editor.curInstr] == NULL)
		return instr[131];

	return instr[editor.curInstr];
}

static int32_t SDLCALL copyInstrThread(void *ptr)
{
	const int16_t dstIns = editor.curInstr;
	const int16_t srcIns = editor.srcInstr;

	pauseAudio();
	freeInstr(dstIns);

	bool error = true;
	if (instr[srcIns] != NULL)
	{
		if (allocateInstr(dstIns))
		{
			memcpy(instr[dstIns], instr[srcIns], sizeof (instr_t));

			sample_t *srcSmp = instr[srcIns]->smp;
			sample_t *dstSmp = instr[dstIns]->smp;

			for (int16_t i = 0; i < MAX_SMP_PER_INST; i++, srcSmp++, dstSmp++)
			{
				if (!cloneSample(srcSmp, dstSmp))
					error = false;
			}
		}
	}

	resumeAudio();

	if (error)
		okBoxThreadSafe(0, "System message", "Not enough memory!");

	// do not change instrument names!

	if (!error)
	{
		editor.updateCurInstr = true;
		setSongModifiedFlag();
	}

	setMouseBusy(false);

	return false;
	(void)ptr;
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
	if (editor.curInstr == 0 || editor.srcInstr == editor.curInstr)
		return;

	lockMixerCallback();

	instr_t *src = instr[editor.srcInstr];
	instr_t *dst = instr[editor.curInstr];

	// swap instruments
	instr_t dstTmp = *dst;
	*dst = *src;
	*src = dstTmp;

	unlockMixerCallback();

	// do not change instrument names!

	updateNewInstrument();
	setSongModifiedFlag();
}

static void drawMIDICh(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->midiChannel <= 15);
	const uint8_t val = ins->midiChannel + 1;
	textOutFixed(156, 132, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[val]);
}

static void drawMIDIPrg(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->midiProgram <= 127);
	textOutFixed(149, 146, PAL_FORGRND, PAL_DESKTOP, dec3StrTab[ins->midiProgram]);
}

static void drawMIDIBend(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->midiBend <= 36);
	textOutFixed(156, 160, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->midiBend]);
}

void midiChDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_EXT_MIDI_CH, 1);
}

void midiChUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_EXT_MIDI_CH, 1);
}

void midiPrgDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_EXT_MIDI_PRG, 1);
}

void midiPrgUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_EXT_MIDI_PRG, 1);
}

void midiBendDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_EXT_MIDI_BEND, 1);
}

void midiBendUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_EXT_MIDI_BEND, 1);
}

void sbMidiChPos(uint32_t pos)
{
	instr_t *ins = instr[editor.curInstr];
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
	instr_t *ins = instr[editor.curInstr];
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
	instr_t *ins = instr[editor.curInstr];
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
	if (ui.instrSwitcherShown)
		updateInstrumentSwitcher();

	updateSampleEditorSample();

	if (ui.sampleEditorShown)
		updateSampleEditor();

	if (ui.instEditorShown || ui.instEditorExtShown)
		updateInstEditor();
}

void updateNewInstrument(void)
{
	updateTextBoxPointers();

	if (ui.instrSwitcherShown)
		updateInstrumentSwitcher();

	editor.currVolEnvPoint = 0;
	editor.currPanEnvPoint = 0;

	updateSampleEditorSample();

	if (ui.sampleEditorShown)
		updateSampleEditor();

	if (ui.instEditorShown || ui.instEditorExtShown)
		updateInstEditor();

	if (ui.advEditShown)
		updateAdvEdit();
}

static void drawVolEnvSus(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->volEnvSustain < 100);
	textOutFixed(382, 206, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->volEnvSustain]);
}

static void drawVolEnvRepS(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->volEnvLoopStart < 100);
	textOutFixed(382, 233, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->volEnvLoopStart]);
}

static void drawVolEnvRepE(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->volEnvLoopEnd < 100);
	textOutFixed(382, 247, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->volEnvLoopEnd]);
}

static void drawPanEnvSus(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->panEnvSustain < 100);
	textOutFixed(382, 293, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->panEnvSustain]);
}

static void drawPanEnvRepS(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->panEnvLoopStart < 100);
	textOutFixed(382, 320, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->panEnvLoopStart]);
}

static void drawPanEnvRepE(void)
{
	instr_t *ins = getCurDispInstr();
	assert(ins->panEnvLoopEnd < 100);
	textOutFixed(382, 334, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[ins->panEnvLoopEnd]);
}

static void drawVolume(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->smp[0];
	else
		s = &instr[editor.curInstr]->smp[editor.curSmp];

	hexOutBg(505, 177, PAL_FORGRND, PAL_DESKTOP, s->volume, 2);
}

static void drawPanning(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->smp[0];
	else
		s = &instr[editor.curInstr]->smp[editor.curSmp];

	hexOutBg(505, 191, PAL_FORGRND, PAL_DESKTOP, s->panning, 2);
}

void drawC4Rate(void)
{
	fillRect(465, 299, 71, 8, PAL_DESKTOP);

	int32_t C4Hz = 0;
	if (editor.curInstr != 0)
	{
		instr_t *ins = instr[editor.curInstr];
		if (ins != NULL)
			C4Hz = (int32_t)(getSampleC4Rate(&ins->smp[editor.curSmp]) + 0.5); // rounded
	}

	char str[64];
	sprintf(str, "%dHz", C4Hz);
	textOut(465, 299, PAL_FORGRND, str);
}

static void drawFineTune(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL)
		s = &instr[0]->smp[0];
	else
		s = &instr[editor.curInstr]->smp[editor.curSmp];

	fillRect(491, 205, 27, 8, PAL_DESKTOP);

	int16_t  ftune = s->finetune;
	if (ftune == 0)
	{
		charOut(512, 205, PAL_FORGRND, '0');
		return;
	}

	const char sign = (ftune < 0) ? '-' : '+';

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
	hexOutBg(498, 222, PAL_FORGRND, PAL_DESKTOP, getCurDispInstr()->fadeout, 3);
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

static void drawRelativeNote(void)
{
	char noteChar1, noteChar2;
	int8_t note2;

	if (instr[editor.curInstr] == NULL)
	{
		fillRect(600, 299, 8*3, 8, PAL_BCKGRND);
		return;
	}

	if (editor.curInstr == 0)
		note2 = 48;
	else
		note2 = 48 + instr[editor.curInstr]->smp[editor.curSmp].relativeNote;

	const int8_t note = note2 % 12;
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

	const char octaChar = '0' + (note2 / 12);

	charOutBg(600, 299, PAL_FORGRND, PAL_BCKGRND, noteChar1);
	charOutBg(608, 299, PAL_FORGRND, PAL_BCKGRND, noteChar2);
	charOutBg(616, 299, PAL_FORGRND, PAL_BCKGRND, octaChar);
}

static void setStdVolEnvelope(instr_t *ins, uint8_t num)
{
	if (editor.curInstr == 0 || ins == NULL)
		return;

	pauseMusic();

	ins->fadeout = config.stdFadeout[num];
	ins->volEnvSustain = (uint8_t)config.stdVolEnvSustain[num];
	ins->volEnvLoopStart = (uint8_t)config.stdVolEnvLoopStart[num];
	ins->volEnvLoopEnd = (uint8_t)config.stdVolEnvLoopEnd[num];
	ins->volEnvLength = (uint8_t)config.stdVolEnvLength[num];
	ins->volEnvFlags = (uint8_t)config.stdVolEnvFlags[num];
	ins->vibRate = (uint8_t)config.stdVibRate[num];
	ins->vibDepth = (uint8_t)config.stdVibDepth[num];
	ins->vibSweep = (uint8_t)config.stdVibSweep[num];
	ins->vibType = (uint8_t)config.stdVibType[num];

	memcpy(ins->volEnvPoints, config.stdEnvPoints[num][0], sizeof (int16_t) * 12 * 2);

	resumeMusic();
}

static void setStdPanEnvelope(instr_t *ins, uint8_t num)
{
	if (editor.curInstr == 0 || ins == NULL)
		return;

	pauseMusic();

	ins->panEnvLength = (uint8_t)config.stdPanEnvLength[num];
	ins->panEnvSustain = (uint8_t)config.stdPanEnvSustain[num];
	ins->panEnvLoopStart = (uint8_t)config.stdPanEnvLoopStart[num];
	ins->panEnvLoopEnd = (uint8_t)config.stdPanEnvLoopEnd[num];
	ins->panEnvFlags = (uint8_t)config.stdPanEnvFlags[num];

	memcpy(ins->panEnvPoints, config.stdEnvPoints[num][1], sizeof (int16_t) * 12 * 2);

	resumeMusic();
}

static void setOrStoreVolEnvPreset(uint8_t num)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (mouse.rightButtonReleased)
	{
		// store preset
		config.stdFadeout[num] = ins->fadeout;
		config.stdVolEnvSustain[num] = ins->volEnvSustain;
		config.stdVolEnvLoopStart[num] = ins->volEnvLoopStart;
		config.stdVolEnvLoopEnd[num] = ins->volEnvLoopEnd;
		config.stdVolEnvLength[num] = ins->volEnvLength;
		config.stdVolEnvFlags[num] = ins->volEnvFlags;
		config.stdVibRate[num] = ins->vibRate;
		config.stdVibDepth[num] = ins->vibDepth;
		config.stdVibSweep[num] = ins->vibSweep;
		config.stdVibType[num] = ins->vibType;

		memcpy(config.stdEnvPoints[num][0], ins->volEnvPoints, sizeof (int16_t) * 12 * 2);
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
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (mouse.rightButtonReleased)
	{
		// store preset
		config.stdFadeout[num] = ins->fadeout;
		config.stdPanEnvSustain[num] = ins->panEnvSustain;
		config.stdPanEnvLoopStart[num] = ins->panEnvLoopStart;
		config.stdPanEnvLoopEnd[num] = ins->panEnvLoopEnd;
		config.stdPanEnvLength[num] = ins->panEnvLength;
		config.stdPanEnvFlags[num] = ins->panEnvFlags;
		config.stdVibRate[num] = ins->vibRate;
		config.stdVibDepth[num] = ins->vibDepth;
		config.stdVibSweep[num] = ins->vibSweep;
		config.stdVibType[num] = ins->vibType;

		memcpy(config.stdEnvPoints[num][1], ins->panEnvPoints, sizeof (int16_t) * 12 * 2);
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
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(1 - 1);
}

void volPreDef2(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(2 - 1);
}

void volPreDef3(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(3 - 1);
}

void volPreDef4(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(4 - 1);
}

void volPreDef5(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(5 - 1);
}

void volPreDef6(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStoreVolEnvPreset(6 - 1);
}

void panPreDef1(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(1 - 1);
}

void panPreDef2(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(2 - 1);
}

void panPreDef3(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(3 - 1);
}

void panPreDef4(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(4 - 1);
}

void panPreDef5(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(5 - 1);
}

void panPreDef6(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		setOrStorePanEnvPreset(6 - 1);
}

void relativeNoteOctUp(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->relativeNote <= 71-12)
		s->relativeNote += 12;
	else
		s->relativeNote = 71;

	drawRelativeNote();
	drawC4Rate();
	setSongModifiedFlag();
}

void relativeNoteOctDown(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->relativeNote >= -48+12)
		s->relativeNote -= 12;
	else
		s->relativeNote = -48;

	drawRelativeNote();
	drawC4Rate();
	setSongModifiedFlag();
}

void relativeNoteUp(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->relativeNote < 71)
	{
		s->relativeNote++;
		drawRelativeNote();
		drawC4Rate();
		setSongModifiedFlag();
	}
}

void relativeNoteDown(void)
{
	sample_t *s;
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->relativeNote > -48)
	{
		s->relativeNote--;
		drawRelativeNote();
		drawC4Rate();
		setSongModifiedFlag();
	}
}

void volEnvAdd(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (editor.curInstr == 0 || ins == NULL)
		return;

	const int16_t ant = ins->volEnvLength;
	if (ant >= 12)
		return;

	int16_t i = (int16_t)editor.currVolEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant-1;
		if (i < 0)
			i = 0;
	}

	if (i < ant-1 && ins->volEnvPoints[i+1][0]-ins->volEnvPoints[i][0] < 2)
		return;

	if (ins->volEnvPoints[i][0] >= 323)
		return;

	for (int16_t j = ant; j > i; j--)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j-1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j-1][1];
	}

	if (ins->volEnvSustain > i) { ins->volEnvSustain++; drawVolEnvSus();  }
	if (ins->volEnvLoopStart > i) { ins->volEnvLoopStart++; drawVolEnvRepS(); }
	if (ins->volEnvLoopEnd > i) { ins->volEnvLoopEnd++; drawVolEnvRepE(); }

	if (i < ant-1)
	{
		ins->volEnvPoints[i+1][0] = (ins->volEnvPoints[i][0] + ins->volEnvPoints[i+2][0]) / 2;
		ins->volEnvPoints[i+1][1] = (ins->volEnvPoints[i][1] + ins->volEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->volEnvPoints[i+1][0] = ins->volEnvPoints[i][0] + 10;
		ins->volEnvPoints[i+1][1] = ins->volEnvPoints[i][1];
	}

	if (ins->volEnvPoints[i+1][0] > 324)
		ins->volEnvPoints[i+1][0] = 324;

	ins->volEnvLength++;

	updateVolEnv = true;
	setSongModifiedFlag();
}

void volEnvDel(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0 || ins->volEnvLength <= 2)
		return;

	int16_t i = (int16_t)editor.currVolEnvPoint;
	if (i < 0 || i >= ins->volEnvLength)
		return;

	for (int16_t j = i; j < ins->volEnvLength; j++)
	{
		ins->volEnvPoints[j][0] = ins->volEnvPoints[j+1][0];
		ins->volEnvPoints[j][1] = ins->volEnvPoints[j+1][1];
	}

	bool drawSust = false;
	bool drawRepS = false;
	bool drawRepE = false;

	if (ins->volEnvSustain > i) { ins->volEnvSustain--; drawSust = true; }
	if (ins->volEnvLoopStart > i) { ins->volEnvLoopStart--; drawRepS = true; }
	if (ins->volEnvLoopEnd > i) { ins->volEnvLoopEnd--; drawRepE = true; }

	ins->volEnvPoints[0][0] = 0;
	ins->volEnvLength--;

	if (ins->volEnvSustain >= ins->volEnvLength) { ins->volEnvSustain = ins->volEnvLength - 1; drawSust = true; }
	if (ins->volEnvLoopStart >= ins->volEnvLength) { ins->volEnvLoopStart = ins->volEnvLength - 1; drawRepS = true; }
	if (ins->volEnvLoopEnd >= ins->volEnvLength) { ins->volEnvLoopEnd = ins->volEnvLength - 1; drawRepE = true; }

	if (drawSust) drawVolEnvSus();
	if (drawRepS) drawVolEnvRepS();
	if (drawRepE) drawVolEnvRepE();

	if (ins->volEnvLength == 0)
		editor.currVolEnvPoint = 0;
	else if (editor.currVolEnvPoint >= ins->volEnvLength)
		editor.currVolEnvPoint = ins->volEnvLength-1;

	updateVolEnv = true;
	setSongModifiedFlag();
}

void volEnvSusUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvSustain < ins->volEnvLength-1)
	{
		ins->volEnvSustain++;
		drawVolEnvSus();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvSusDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvSustain > 0)
	{
		ins->volEnvSustain--;
		drawVolEnvSus();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepSUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvLoopStart < ins->volEnvLoopEnd)
	{
		ins->volEnvLoopStart++;
		drawVolEnvRepS();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepSDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvLoopStart > 0)
	{
		ins->volEnvLoopStart--;
		drawVolEnvRepS();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepEUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvLoopEnd < ins->volEnvLength-1)
	{
		ins->volEnvLoopEnd++;
		drawVolEnvRepE();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void volEnvRepEDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->volEnvLoopEnd > ins->volEnvLoopStart)
	{
		ins->volEnvLoopEnd--;
		drawVolEnvRepE();
		updateVolEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvAdd(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	const int16_t ant = ins->panEnvLength;
	if (ant >= 12)
		return;

	int16_t i = (int16_t)editor.currPanEnvPoint;
	if (i < 0 || i >= ant)
	{
		i = ant-1;
		if (i < 0)
			i = 0;
	}

	if (i < ant-1 && ins->panEnvPoints[i+1][0]-ins->panEnvPoints[i][0] < 2)
		return;

	if (ins->panEnvPoints[i][0] >= 323)
		return;

	for (int16_t j = ant; j > i; j--)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j-1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j-1][1];
	}

	if (ins->panEnvSustain > i) { ins->panEnvSustain++; drawPanEnvSus();  }
	if (ins->panEnvLoopStart > i) { ins->panEnvLoopStart++; drawPanEnvRepS(); }
	if (ins->panEnvLoopEnd > i) { ins->panEnvLoopEnd++; drawPanEnvRepE(); }

	if (i < ant-1)
	{
		ins->panEnvPoints[i+1][0] = (ins->panEnvPoints[i][0] + ins->panEnvPoints[i+2][0]) / 2;
		ins->panEnvPoints[i+1][1] = (ins->panEnvPoints[i][1] + ins->panEnvPoints[i+2][1]) / 2;
	}
	else
	{
		ins->panEnvPoints[i+1][0] = ins->panEnvPoints[i][0] + 10;
		ins->panEnvPoints[i+1][1] = ins->panEnvPoints[i][1];
	}

	if (ins->panEnvPoints[i+1][0] > 324)
		ins->panEnvPoints[i+1][0] = 324;

	ins->panEnvLength++;

	updatePanEnv = true;
	setSongModifiedFlag();
}

void panEnvDel(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0 || ins->panEnvLength <= 2)
		return;

	int16_t i = (int16_t)editor.currPanEnvPoint;
	if (i < 0 || i >= ins->panEnvLength)
		return;

	for (int16_t j = i; j < ins->panEnvLength; j++)
	{
		ins->panEnvPoints[j][0] = ins->panEnvPoints[j+1][0];
		ins->panEnvPoints[j][1] = ins->panEnvPoints[j+1][1];
	}

	bool drawSust = false;
	bool drawRepS = false;
	bool drawRepE = false;

	if (ins->panEnvSustain > i) { ins->panEnvSustain--; drawSust = true; }
	if (ins->panEnvLoopStart > i) { ins->panEnvLoopStart--; drawRepS = true; }
	if (ins->panEnvLoopEnd > i) { ins->panEnvLoopEnd--; drawRepE = true; }

	ins->panEnvPoints[0][0] = 0;
	ins->panEnvLength--;

	if (ins->panEnvSustain >= ins->panEnvLength) { ins->panEnvSustain = ins->panEnvLength - 1; drawSust = true; }
	if (ins->panEnvLoopStart >= ins->panEnvLength) { ins->panEnvLoopStart = ins->panEnvLength - 1; drawRepS = true; }
	if (ins->panEnvLoopEnd >= ins->panEnvLength) { ins->panEnvLoopEnd = ins->panEnvLength - 1; drawRepE = true; }

	if (drawSust) drawPanEnvSus();
	if (drawRepS) drawPanEnvRepS();
	if (drawRepE) drawPanEnvRepE();

	if (ins->panEnvLength == 0)
		editor.currPanEnvPoint = 0;
	else if (editor.currPanEnvPoint >= ins->panEnvLength)
		editor.currPanEnvPoint = ins->panEnvLength-1;

	updatePanEnv = true;
	setSongModifiedFlag();
}

void panEnvSusUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvSustain < ins->panEnvLength-1)
	{
		ins->panEnvSustain++;
		drawPanEnvSus();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvSusDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvSustain > 0)
	{
		ins->panEnvSustain--;
		drawPanEnvSus();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepSUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvLoopStart < ins->panEnvLoopEnd)
	{
		ins->panEnvLoopStart++;
		drawPanEnvRepS();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepSDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvLoopStart > 0)
	{
		ins->panEnvLoopStart--;
		drawPanEnvRepS();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepEUp(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvLoopEnd < ins->panEnvLength-1)
	{
		ins->panEnvLoopEnd++;
		drawPanEnvRepE();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void panEnvRepEDown(void)
{
	instr_t *ins = instr[editor.curInstr];
	if (ins == NULL || editor.curInstr == 0)
		return;

	if (ins->panEnvLoopEnd > ins->panEnvLoopStart)
	{
		ins->panEnvLoopEnd--;
		drawPanEnvRepE();
		updatePanEnv = true;
		setSongModifiedFlag();
	}
}

void volDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_VOL, 1);
}

void volUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_VOL, 1);
}

void panDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_PAN, 1);
}

void panUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_PAN, 1);
}

void ftuneDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_FTUNE, 1);
}

void ftuneUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_FTUNE, 1);
}

void fadeoutDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_FADEOUT, 1);
}

void fadeoutUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_FADEOUT, 1);
}

void vibSpeedDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_VIBSPEED, 1);
}

void vibSpeedUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_VIBSPEED, 1);
}

void vibDepthDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_VIBDEPTH, 1);
}

void vibDepthUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_VIBDEPTH, 1);
}

void vibSweepDown(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollLeft(SB_INST_VIBSWEEP, 1);
}

void vibSweepUp(void)
{
	if (editor.curInstr != 0 && instr[editor.curInstr] != NULL)
		scrollBarScrollRight(SB_INST_VIBSWEEP, 1);
}

void setVolumeScroll(uint32_t pos)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		if (editor.curInstr == 0 && editor.curSmp != 0)
			setScrollBarPos(SB_INST_VOL, 0x40, false);
		else
			setScrollBarPos(SB_INST_VOL, 0, false);

		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->volume != (uint8_t)pos)
	{
		s->volume = (uint8_t)pos;
		drawVolume();
		setSongModifiedFlag();
	}
}

void setPanningScroll(uint32_t pos)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_PAN, 0x80, false);
		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->panning != (uint8_t)pos)
	{
		s->panning = (uint8_t)pos;
		drawPanning();
		setSongModifiedFlag();
	}
}

void setFinetuneScroll(uint32_t pos)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
	{
		setScrollBarPos(SB_INST_FTUNE, 128, false); // finetune 0
		return;
	}

	sample_t *s = &instr[editor.curInstr]->smp[editor.curSmp];
	if (s->finetune != (int8_t)(pos - 128))
	{
		s->finetune = (int8_t)(pos - 128);
		drawFineTune();
		drawC4Rate();
		setSongModifiedFlag();
	}
}

void setFadeoutScroll(uint32_t pos)
{
	instr_t *ins = instr[editor.curInstr];
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

	if (ins->fadeout != (uint16_t)pos)
	{
		ins->fadeout = (uint16_t)pos;
		drawFadeout();
		setSongModifiedFlag();
	}
}

void setVibSpeedScroll(uint32_t pos)
{
	instr_t *ins = instr[editor.curInstr];
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
	instr_t *ins = instr[editor.curInstr];
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
	instr_t *ins = instr[editor.curInstr];
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

	instr[editor.curInstr]->vibType = 0;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_SINE].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveSquare(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibType = 1;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_SQUARE].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveRampDown(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibType = 2;

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	radioButtons[RB_INST_WAVE_RAMP_DOWN].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	setSongModifiedFlag();
}

void rbVibWaveRampUp(void)
{
	if (instr[editor.curInstr] == NULL || editor.curInstr == 0)
		return;

	instr[editor.curInstr]->vibType = 3;

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

	instr[editor.curInstr]->volEnvFlags ^= 1;
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

	instr[editor.curInstr]->volEnvFlags ^= 2;
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

	instr[editor.curInstr]->volEnvFlags ^= 4;
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

	instr[editor.curInstr]->panEnvFlags ^= 1;
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

	instr[editor.curInstr]->panEnvFlags ^= 2;
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

	instr[editor.curInstr]->panEnvFlags ^= 4;
	updatePanEnv = true;

	setSongModifiedFlag();
}

static void pinoaNumberOut(uint16_t xPos, uint16_t yPos, uint8_t fgPalette, uint8_t bgPalette, uint8_t val)
{
	assert(val <= 0xF);

	const uint32_t fg = video.palette[fgPalette];
	const uint32_t bg = video.palette[bgPalette];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];
	const uint8_t *srcPtr = &bmp.font8[val * 5];

	for (int32_t y = 0; y < 7; y++)
	{
		for (int32_t x = 0; x < 5; x++)
			dstPtr[x] = srcPtr[x] ? fg : bg;

		dstPtr += SCREEN_W;
		srcPtr += 80;
	}
}

static void writePianoNumber(uint8_t note, uint8_t key, uint8_t octave)
{
	uint8_t number = 0;
	if (instr[editor.curInstr] != NULL && editor.curInstr != 0)
		number = instr[editor.curInstr]->note2SampleLUT[note];

	const uint16_t x = keyDigitXPos[key] + (octave * 77);

	if (keyIsBlackTab[key])
		pinoaNumberOut(x, 361, PAL_FORGRND, PAL_BCKGRND, number);
	else
		pinoaNumberOut(x, 385, PAL_BCKGRND, PAL_FORGRND, number);
}

static void drawBlackPianoKey(uint8_t key, uint8_t octave, bool keyDown)
{
	const uint16_t x = keyXPos[key] + (octave * 77);
	blit(x, 351, &bmp.blackPianoKeys[keyDown * (7*27)], 7, 27);
}

static void drawWhitePianoKey(uint8_t key, uint8_t octave,  bool keyDown)
{
	const uint16_t x = keyXPos[key] + (octave * 77);
	blit(x, 351, &bmp.whitePianoKeys[(keyDown * (11*46*3)) + whiteKeysBmpOrder[key]], 11, 46);
}

void redrawPiano(void)
{
	memset(pianoKeyStatus, 0, sizeof (pianoKeyStatus));
	for (uint8_t i = 0; i < 96; i++)
	{
		const uint8_t key = noteTab1[i];
		const uint8_t octave = noteTab2[i];

		if (keyIsBlackTab[key])
			drawBlackPianoKey(key, octave, false);
		else
			drawWhitePianoKey(key, octave, false);

		writePianoNumber(i, key, octave);
	}
}

bool testPianoKeysMouseDown(bool mouseButtonDown)
{
	uint8_t key, octave;

	if (!ui.instEditorShown)
		return false; // area not clicked

	if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return true; // area clicked, but don't do anything

	int32_t mx = mouse.x;
	int32_t my = mouse.y;

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

	mx -= 8;

	const int32_t quotient  = mx / 77;
	const int32_t remainder = mx % 77;

	if (my < 378)
	{
		// white keys and black keys (top)

		octave = (uint8_t)quotient;
		key = mx2PianoKey[remainder];
	}
	else
	{
		// white keys only (bottom)
		const int32_t whiteKeyWidth = 11;

		octave = (uint8_t)(quotient);
		key = whiteKeyIndex[remainder / whiteKeyWidth];
	}

	const uint8_t note = (octave * 12) + key;
	if (instr[editor.curInstr]->note2SampleLUT[note] != editor.curSmp)
	{
		instr[editor.curInstr]->note2SampleLUT[note] = editor.curSmp;
		writePianoNumber(note, key, octave);
		setSongModifiedFlag();
	}

	return true;
}

void drawPiano(chSyncData_t *chSyncData)
{
	bool newStatus[96];
	memset(newStatus, 0, sizeof (newStatus));

	// find active notes
	if (editor.curInstr > 0)
	{
		if (chSyncData != NULL) // song is playing, use replayer channel state
		{
			syncedChannel_t *c = chSyncData->channels;
			for (int32_t i = 0; i < song.numChannels; i++, c++)
			{
				if (c->instrNum == editor.curInstr && c->pianoNoteNum <= 95)
					newStatus[c->pianoNoteNum] = true;
			}
		}
		else // song is not playing (jamming from keyboard/MIDI)
		{
			channel_t *c = channel;
			for (int32_t i = 0; i < song.numChannels; i++, c++)
			{
				if (c->instrNum == editor.curInstr && c->envSustainActive)
				{
					const int32_t note = getPianoKey(c->finalPeriod, c->finetune, c->relativeNote);
					if (note >= 0 && note <= 95)
						newStatus[note] = true;
				}
			}
		}
	}

	// draw keys
	for (int32_t i = 0; i < 96; i++)
	{
		const bool keyDown = newStatus[i];
		if (pianoKeyStatus[i] ^ keyDown)
		{
			const uint8_t key = noteTab1[i];
			const uint8_t octave = noteTab2[i];

			if (keyIsBlackTab[key])
				drawBlackPianoKey(key, octave, keyDown);
			else
				drawWhitePianoKey(key, octave, keyDown);

			pianoKeyStatus[i] = keyDown;
		}
	}
}

static void envelopeLine(int32_t envNum, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t pal)
{
	y1 = CLAMP(y1, 0, 66);
	y2 = CLAMP(y2, 0, 66);
	x1 = CLAMP(x1, 0, 335);
	x2 = CLAMP(x2, 0, 335);

	if (envNum == 0) // volume envelope
	{
		y1 += 189;
		y2 += 189;
	}
	else // panning envelope
	{
		y1 += 276;
		y2 += 276;
	}

	const int16_t dx = x2 - x1;
	const uint16_t ax = ABS(dx) << 1;
	const int16_t sx = SGN(dx);
	const int16_t dy = y2 - y1;
	const uint16_t ay = ABS(dy) << 1;
	const int16_t sy = SGN(dy);
	int16_t x = x1;
	int16_t y = y1;

	const uint32_t pal1 = video.palette[PAL_BLCKMRK];
	const uint32_t pal2 = video.palette[PAL_BLCKTXT];
	const uint32_t pixVal = video.palette[pal];
	const int32_t pitch = sy * SCREEN_W;

	uint32_t *dst32 = &video.frameBuffer[(y * SCREEN_W) + x];

	// draw line
	if (ax > ay)
	{
		int16_t d = ay - (ax >> 1);
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
		int16_t d = ax - (ay >> 1);
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

static void envelopePixel(int32_t envNum, int16_t x, int16_t y, uint8_t pal)
{
	y += (envNum == 0) ? 189 : 276;
	video.frameBuffer[(y * SCREEN_W) + x] = video.palette[pal];
}

static void envelopeDot(int32_t envNum, int16_t x, int16_t y)
{
	y += (envNum == 0) ? 189 : 276;

	const uint32_t pixVal = video.palette[PAL_BLCKTXT];
	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];

	for (y = 0; y < 3; y++)
	{
		*dstPtr++ = pixVal;
		*dstPtr++ = pixVal;
		*dstPtr++ = pixVal;

		dstPtr += SCREEN_W-3;
	}
}

static void envelopeVertLine(int32_t envNum, int16_t x, int16_t y, uint8_t pal)
{
	y += (envNum == 0) ? 189 : 276;

	const uint32_t pixVal1 = video.palette[pal];
	const uint32_t pixVal2 = video.palette[PAL_BLCKTXT];

	uint32_t *dstPtr = &video.frameBuffer[(y * SCREEN_W) + x];
	for (y = 0; y < 33; y++)
	{
		if (*dstPtr != pixVal2)
			*dstPtr = pixVal1;

		dstPtr += SCREEN_W*2;
	}
}

static void writeEnvelope(int32_t envNum)
{
	uint8_t selected;
	int16_t i, nd, sp, ls, le, (*curEnvP)[2];

	instr_t *ins = instr[editor.curInstr];

	// clear envelope area
	if (envNum == 0)
		clearRect(5, 189, 333, 67);
	else
		clearRect(5, 276, 333, 67);

	// draw dotted x/y lines
	for (i = 0; i <= 32; i++) envelopePixel(envNum, 5, 1 + i * 2, PAL_PATTEXT);
	for (i = 0; i <= 8; i++) envelopePixel(envNum, 4, 1 + i * 8, PAL_PATTEXT);
	for (i = 0; i <= 162; i++) envelopePixel(envNum, 8 + i *  2, 65, PAL_PATTEXT);
	for (i = 0; i <= 6; i++) envelopePixel(envNum, 8 + i * 50, 66, PAL_PATTEXT);

	// draw center line on pan envelope
	if (envNum == 1)
		envelopeLine(envNum, 8, 33, 332, 33, PAL_BLCKMRK);

	if (ins == NULL)
		return;

	// collect variables

	if (envNum == 0) // volume envelope
	{
		nd = ins->volEnvLength;
		if (ins->volEnvFlags & ENV_SUSTAIN)
			sp = ins->volEnvSustain;
		else
			sp = -1;

		if (ins->volEnvFlags & ENV_LOOP)
		{
			ls = ins->volEnvLoopStart;
			le = ins->volEnvLoopEnd;
		}
		else
		{
			ls = -1;
			le = -1;
		}

		curEnvP = ins->volEnvPoints;
		selected = editor.currVolEnvPoint;
	}
	else // panning envelope
	{
		nd = ins->panEnvLength;
		if (ins->panEnvFlags & ENV_SUSTAIN)
			sp = ins->panEnvSustain;
		else
			sp = -1;

		if (ins->panEnvFlags & ENV_LOOP)
		{
			ls = ins->panEnvLoopStart;
			le = ins->panEnvLoopEnd;
		}
		else
		{
			ls = -1;
			le = -1;
		}

		curEnvP = ins->panEnvPoints;
		selected = editor.currPanEnvPoint;
	}

	if (nd > 12)
		nd = 12;

	int16_t lx = 0;
	int16_t ly = 0;

	// draw envelope
	for (i = 0; i < nd; i++)
	{
		int16_t x = curEnvP[i][0];
		int16_t y = curEnvP[i][1];

		x = CLAMP(x, 0, 324);
		
		if (envNum == 0) // volume envelope
			y = CLAMP(y, 0, 64);
		else // panning envelope
			y = CLAMP(y, 0, 63);

		if ((uint16_t)curEnvP[i][0] <= 324)
		{
			envelopeDot(envNum, 7 + x, 64 - y);

			// draw "envelope selected" data
			if (i == selected)
			{
				envelopeLine(envNum, 5  + x, 64 - y, 5  + x, 66 - y, PAL_BLCKTXT);
				envelopeLine(envNum, 11 + x, 64 - y, 11 + x, 66 - y, PAL_BLCKTXT);
				envelopePixel(envNum, 5, 65 - y, PAL_BLCKTXT);
				envelopePixel(envNum, 8 + x, 65, PAL_BLCKTXT);
			}

			// draw loop start marker
			if (i == ls)
			{
				envelopeLine(envNum, x + 6, 1, x + 10, 1, PAL_PATTEXT);
				envelopeLine(envNum, x + 7, 2, x +  9, 2, PAL_PATTEXT);
				envelopeVertLine(envNum, x + 8, 1, PAL_PATTEXT);
			}

			// draw sustain marker
			if (i == sp)
				envelopeVertLine(envNum, x + 8, 1, PAL_BLCKTXT);

			// draw loop end marker
			if (i == le)
			{
				envelopeLine(envNum, x + 6, 65, x + 10, 65, PAL_PATTEXT);
				envelopeLine(envNum, x + 7, 64, x +  9, 64, PAL_PATTEXT);
				envelopeVertLine(envNum, x + 8, 1, PAL_PATTEXT);
			}
		}

		// draw envelope line
		if (i > 0 && lx < x)
			envelopeLine(envNum, lx + 8, 65 - ly, x + 8, 65 - y, PAL_PATTEXT);

		lx = x;
		ly = y;
	}
}

static void drawVolEnvCoords(int16_t tick, int16_t val)
{
	char str[4];

	tick = CLAMP(tick, 0, 324);
	sprintf(str, "%03d", tick);
	textOutTinyOutline(326, 190, str);

	val = CLAMP(val, 0, 64);
	sprintf(str, "%02d", val);
	textOutTinyOutline(330, 198, str);
}

static void drawPanEnvCoords(int16_t tick, int16_t val)
{
	bool negative = false;
	char str[4];

	tick = CLAMP(tick, 0, 324);
	sprintf(str, "%03d", tick);
	textOutTinyOutline(326, 277, str);
	
	val -= 32;
	val = CLAMP(val, -32, 31);
	if (val < 0)
	{
		negative = true;
		val = -val;
	}

	if (negative) // draw minus sign
	{
		// outline
		hLine(326, 287, 3, PAL_BCKGRND);
		hLine(326, 289, 3, PAL_BCKGRND);
		video.frameBuffer[(288 * SCREEN_W) + 325] = video.palette[PAL_BCKGRND];
		video.frameBuffer[(288 * SCREEN_W) + 329] = video.palette[PAL_BCKGRND];

		hLine(326, 288, 3, PAL_FORGRND);
	}
	
	sprintf(str, "%02d", val);
	textOutTinyOutline(330, 285, str);
}

void handleInstEditorRedrawing(void)
{
	int16_t tick, val;

	instr_t *ins = instr[editor.curInstr];

	if (updateVolEnv)
	{
		updateVolEnv = false;
		writeEnvelope(0);

		tick = 0;
		val = 0;

		if (ins != NULL && ins->volEnvLength > 0)
		{
			tick = ins->volEnvPoints[editor.currVolEnvPoint][0];
			val = ins->volEnvPoints[editor.currVolEnvPoint][1];
		}

		drawVolEnvCoords(tick, val);
	}

	if (updatePanEnv)
	{
		updatePanEnv = false;
		writeEnvelope(1);

		tick = 0;
		val = 32;

		if (ins != NULL && ins->panEnvLength > 0)
		{
			tick = ins->panEnvPoints[editor.currPanEnvPoint][0];
			val = ins->panEnvPoints[editor.currPanEnvPoint][1];
		}

		drawPanEnvCoords(tick, val);
	}
}

void hideInstEditor(void)
{
	ui.instEditorShown = false;

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
	sample_t *s;
	instr_t *ins = getCurDispInstr();

	if (instr[editor.curInstr] == NULL)
		s = &ins->smp[0];
	else
		s = &ins->smp[editor.curSmp];

	// update instrument editor extension
	if (ui.instEditorExtShown)
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

	if (!ui.instEditorShown)
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
	drawC4Rate();
	drawRelativeNote();

	// set scroll bars
	setScrollBarPos(SB_INST_VOL, s->volume, false);
	setScrollBarPos(SB_INST_PAN, s->panning, false);
	setScrollBarPos(SB_INST_FTUNE, 128 + s->finetune, false);
	setScrollBarPos(SB_INST_FADEOUT, ins->fadeout, false);
	setScrollBarPos(SB_INST_VIBSPEED, ins->vibRate, false);
	setScrollBarPos(SB_INST_VIBDEPTH, ins->vibDepth, false);
	setScrollBarPos(SB_INST_VIBSWEEP, ins->vibSweep, false);

	// set radio buttons

	uncheckRadioButtonGroup(RB_GROUP_INST_WAVEFORM);
	switch (ins->vibType)
	{
		default:
		case 0: tmpID = RB_INST_WAVE_SINE;      break;
		case 1: tmpID = RB_INST_WAVE_SQUARE;    break;
		case 2: tmpID = RB_INST_WAVE_RAMP_DOWN; break;
		case 3: tmpID = RB_INST_WAVE_RAMP_UP;   break;
	}

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// set checkboxes

	checkBoxes[CB_INST_VENV].checked      = (ins->volEnvFlags & ENV_ENABLED) ? true : false;
	checkBoxes[CB_INST_VENV_SUS].checked  = (ins->volEnvFlags & ENV_SUSTAIN) ? true : false;
	checkBoxes[CB_INST_VENV_LOOP].checked = (ins->volEnvFlags & ENV_LOOP)    ? true : false;

	checkBoxes[CB_INST_PENV].checked      = (ins->panEnvFlags & ENV_ENABLED) ? true : false;
	checkBoxes[CB_INST_PENV_SUS].checked  = (ins->panEnvFlags & ENV_SUSTAIN) ? true : false;
	checkBoxes[CB_INST_PENV_LOOP].checked = (ins->panEnvFlags & ENV_LOOP)    ? true : false;

	if (editor.currVolEnvPoint >= ins->volEnvLength) editor.currVolEnvPoint = 0;
	if (editor.currPanEnvPoint >= ins->panEnvLength) editor.currPanEnvPoint = 0;

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
	if (ui.extended) exitPatternEditorExtended();
	if (ui.sampleEditorShown) hideSampleEditor();
	if (ui.sampleEditorExtShown) hideSampleEditorExt();

	hidePatternEditor();
	ui.instEditorShown = true;

	drawFramework(0,   173, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(0,   260, 438,  87, FRAMEWORK_TYPE1);
	drawFramework(0,   347, 632,  53, FRAMEWORK_TYPE1);
	drawFramework(438, 173, 194,  45, FRAMEWORK_TYPE1);
	drawFramework(438, 218, 194,  76, FRAMEWORK_TYPE1);
	drawFramework(438, 294, 194,  53, FRAMEWORK_TYPE1);
	drawFramework(2,   188, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(2,   275, 337,  70, FRAMEWORK_TYPE2);
	drawFramework(2,   349, 628,  49, FRAMEWORK_TYPE2);
	drawFramework(593, 296,  36,  15, FRAMEWORK_TYPE2);

	textOutShadow(20,  176, PAL_FORGRND, PAL_DSKTOP2, "Volume envelope:");
	textOutShadow(153, 176, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
	textOutShadow(358, 194, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
	textOutShadow(342, 206, PAL_FORGRND, PAL_DSKTOP2, "Point");
	textOutShadow(358, 219, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
	textOutShadow(342, 233, PAL_FORGRND, PAL_DSKTOP2, "Start");
	textOutShadow(342, 247, PAL_FORGRND, PAL_DSKTOP2, "End");
	textOutShadow(20,  263, PAL_FORGRND, PAL_DSKTOP2, "Panning envelope:");
	textOutShadow(152, 263, PAL_FORGRND, PAL_DSKTOP2, "Predef.");
	textOutShadow(358, 281, PAL_FORGRND, PAL_DSKTOP2, "Sustain:");
	textOutShadow(342, 293, PAL_FORGRND, PAL_DSKTOP2, "Point");
	textOutShadow(358, 306, PAL_FORGRND, PAL_DSKTOP2, "Env.loop:");
	textOutShadow(342, 320, PAL_FORGRND, PAL_DSKTOP2, "Start");
	textOutShadow(342, 334, PAL_FORGRND, PAL_DSKTOP2, "End");
	textOutShadow(443, 177, PAL_FORGRND, PAL_DSKTOP2, "Volume");
	textOutShadow(443, 191, PAL_FORGRND, PAL_DSKTOP2, "Panning");
	textOutShadow(443, 205, PAL_FORGRND, PAL_DSKTOP2, "F.tune");
	textOutShadow(442, 222, PAL_FORGRND, PAL_DSKTOP2, "Fadeout");
	textOutShadow(442, 236, PAL_FORGRND, PAL_DSKTOP2, "Vib.speed");
	textOutShadow(442, 250, PAL_FORGRND, PAL_DSKTOP2, "Vib.depth");
	textOutShadow(442, 264, PAL_FORGRND, PAL_DSKTOP2, "Vib.sweep");
	textOutShadow(442, 299, PAL_FORGRND, PAL_DSKTOP2, "C4=");
	textOutShadow(537, 299, PAL_FORGRND, PAL_DSKTOP2, "Rel. note");

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
	blitFast(455, 279, &bmp.vibratoWaveforms[0*(12*10)], 12, 10);
	blitFast(485, 279, &bmp.vibratoWaveforms[1*(12*10)], 12, 10);
	blitFast(515, 279, &bmp.vibratoWaveforms[2*(12*10)], 12, 10);
	blitFast(545, 279, &bmp.vibratoWaveforms[3*(12*10)], 12, 10);

	showRadioButtonGroup(RB_GROUP_INST_WAVEFORM);

	updateInstEditor();
	redrawPiano();
}

void toggleInstEditor(void)
{
	if (ui.sampleEditorShown)
		hideSampleEditor();

	if (ui.instEditorShown)
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
	int32_t minX, maxX;

	if (!ui.instEditorShown || editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return false;

	instr_t *ins = instr[editor.curInstr];

	uint8_t ant = ins->volEnvLength;
	if (ant > 12)
		ant = 12;

	int32_t mx = mouse.x;
	int32_t my = mouse.y;

	if (!mouseButtonDown)
	{
		if (my < 189 || my > 256 || mx < 7 || mx > 334)
			return false;

		if (ins->volEnvLength == 0)
			return true;

		lastMouseX = mx;
		lastMouseY = my;

		for (uint8_t i = 0; i < ant; i++)
		{
			const int32_t x = 8 + ins->volEnvPoints[i][0];
			const int32_t y = 190 + (64 - ins->volEnvPoints[i][1]);

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

	if (ins->volEnvLength == 0)
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
				minX = ins->volEnvPoints[editor.currVolEnvPoint-1][0] + 1;
				maxX = 324;
			}
			else
			{
				minX = ins->volEnvPoints[editor.currVolEnvPoint-1][0] + 1;
				maxX = ins->volEnvPoints[editor.currVolEnvPoint+1][0] - 1;
			}

			minX = CLAMP(minX, 0, 324);
			maxX = CLAMP(maxX, 0, 324);

			ins->volEnvPoints[editor.currVolEnvPoint][0] = (int16_t)(CLAMP(mx, minX, maxX));
			updateVolEnv = true;

			setSongModifiedFlag();
		}
	}

	if (my != lastMouseY)
	{
		lastMouseY = my;

		my -= saveMouseY;
		my = 64 - CLAMP(my, 0, 64);

		ins->volEnvPoints[editor.currVolEnvPoint][1] = (int16_t)my;
		updateVolEnv = true;

		setSongModifiedFlag();
	}

	return true;
}

bool testInstrPanEnvMouseDown(bool mouseButtonDown)
{
	int32_t minX, maxX;

	if (!ui.instEditorShown || editor.curInstr == 0 || instr[editor.curInstr] == NULL)
		return false;

	instr_t *ins = instr[editor.curInstr];

	uint8_t ant = ins->panEnvLength;
	if (ant > 12)
		ant = 12;

	int32_t mx = mouse.x;
	int32_t my = mouse.y;

	if (!mouseButtonDown)
	{
		if (my < 277 || my > 343 || mx < 7 || mx > 334)
			return false;

		if (ins->panEnvLength == 0)
			return true;

		lastMouseX = mx;
		lastMouseY = my;

		for (uint8_t i = 0; i < ant; i++)
		{
			const int32_t x = 8 + ins->panEnvPoints[i][0];
			const int32_t y = 277 + (63 - ins->panEnvPoints[i][1]);

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

	if (ins->panEnvLength == 0)
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
				minX = ins->panEnvPoints[editor.currPanEnvPoint-1][0] + 1;
				maxX = 324;
			}
			else
			{
				minX = ins->panEnvPoints[editor.currPanEnvPoint-1][0] + 1;
				maxX = ins->panEnvPoints[editor.currPanEnvPoint+1][0] - 1;
			}

			minX = CLAMP(minX, 0, 324);
			maxX = CLAMP(maxX, 0, 324);

			ins->panEnvPoints[editor.currPanEnvPoint][0] = (int16_t)(CLAMP(mx, minX, maxX));
			updatePanEnv = true;

			setSongModifiedFlag();
		}
	}

	if (my != lastMouseY)
	{
		lastMouseY = my;

		my -= saveMouseY;
		my  = 63 - CLAMP(my, 0, 63);

		ins->panEnvPoints[editor.currPanEnvPoint][1] = (int16_t)my;
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
	instr_t *ins = instr[editor.curInstr];

	drawFramework(0,  92, 291, 17, FRAMEWORK_TYPE1);
	drawFramework(0, 109, 291, 19, FRAMEWORK_TYPE1);
	drawFramework(0, 128, 291, 45, FRAMEWORK_TYPE1);

	textOutShadow(4,   96,  PAL_FORGRND, PAL_DSKTOP2, "Instrument Editor Extension:");
	textOutShadow(20,  114, PAL_FORGRND, PAL_DSKTOP2, "Instrument MIDI enable");
	textOutShadow(189, 114, PAL_FORGRND, PAL_DSKTOP2, "Mute computer");
	textOutShadow(4,   132, PAL_FORGRND, PAL_DSKTOP2, "MIDI transmit channel");
	textOutShadow(4,   146, PAL_FORGRND, PAL_DSKTOP2, "MIDI program");
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
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.instEditorExtShown = true;
	ui.scopesShown = false;
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

	ui.instEditorExtShown = false;
	ui.scopesShown = true;
	drawScopeFramework();
}

void toggleInstEditorExt(void)
{
	if (ui.instEditorExtShown)
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

				if (ui.advEditShown)
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

				     if (ui.sampleEditorShown) updateSampleEditor();
				else if (ui.instEditorShown)   updateInstEditor();
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

				if (ui.advEditShown)
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

				if (ui.advEditShown)
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
	if (!ui.instrSwitcherShown)
		return false;

	if (ui.extended)
		return testInstrSwitcherExtended();
	else
		return testInstrSwitcherNormal();
}

static int32_t SDLCALL saveInstrThread(void *ptr)
{
	xiHdr_t ih;
	sample_t *s;

	if (editor.tmpFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	const int32_t numSamples = getUsedSamples(saveInstrNum);
	if (numSamples == 0 || instr[saveInstrNum] == NULL)
	{
		okBoxThreadSafe(0, "System message", "Instrument is empty!");
		return false;
	}

	FILE *f = UNICHAR_FOPEN(editor.tmpFilenameU, "wb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	memset(&ih, 0, sizeof (ih)); // important, also clears reserved stuff

	memcpy(ih.ID, "Extended Instrument: ", 21);
	ih.version = 0x0102;

	// song name
	int32_t nameLength = (int32_t)strlen(song.instrName[saveInstrNum]);
	if (nameLength > 22)
		nameLength = 22;

	memset(ih.name, ' ', 22); // yes, FT2 pads the name with spaces
	if (nameLength > 0)
		memcpy(ih.name, song.instrName[saveInstrNum], nameLength);

	ih.name[22] = 0x1A;

	// program/tracker name
	nameLength = (int32_t)strlen(PROG_NAME_STR);
	if (nameLength > 20)
		nameLength = 20;

	memset(ih.progName, ' ', 20); // yes, FT2 pads the name with spaces
	if (nameLength > 0)
		memcpy(ih.progName, PROG_NAME_STR, nameLength);

	// copy over instrument struct data to instrument header
	instr_t *ins = instr[saveInstrNum];
	memcpy(ih.note2SampleLUT, ins->note2SampleLUT, 96);
	memcpy(ih.volEnvPoints, ins->volEnvPoints, 12*2*sizeof(int16_t));
	memcpy(ih.panEnvPoints, ins->panEnvPoints, 12*2*sizeof(int16_t));
	ih.volEnvLength = ins->volEnvLength;
	ih.panEnvLength = ins->panEnvLength;
	ih.volEnvSustain = ins->volEnvSustain;
	ih.volEnvLoopStart = ins->volEnvLoopStart;
	ih.volEnvLoopEnd = ins->volEnvLoopEnd;
	ih.panEnvSustain = ins->panEnvSustain;
	ih.panEnvLoopStart = ins->panEnvLoopStart;
	ih.panEnvLoopEnd = ins->panEnvLoopEnd;
	ih.volEnvFlags = ins->volEnvFlags;
	ih.panEnvFlags = ins->panEnvFlags;
	ih.vibType = ins->vibType;
	ih.vibSweep = ins->vibSweep;
	ih.vibDepth = ins->vibDepth;
	ih.vibRate = ins->vibRate;
	ih.fadeout = ins->fadeout;
	ih.midiOn = ins->midiOn ? 1 : 0;
	ih.midiChannel = ins->midiChannel;
	ih.midiProgram = ins->midiProgram;
	ih.midiBend = ins->midiBend;
	ih.mute = ins->mute ? 1 : 0;
	ih.numSamples = (uint16_t)numSamples;

	// copy over sample struct datas to sample headers
	s = instr[saveInstrNum]->smp;
	for (int32_t i = 0; i < numSamples; i++, s++)
	{
		xmSmpHdr_t *dst = &ih.smp[i];

		bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

		dst->length = s->length;
		dst->loopStart = s->loopStart;
		dst->loopLength = s->loopLength;

		if (sample16Bit)
		{
			dst->length <<= 1;
			dst->loopStart <<= 1;
			dst->loopLength <<= 1;
		}

		dst->volume = s->volume;
		dst->finetune = s->finetune;
		dst->flags = s->flags;
		dst->panning = s->panning;
		dst->relativeNote = s->relativeNote;

		dst->nameLength = (uint8_t)strlen(s->name);
		if (dst->nameLength > 22)
			dst->nameLength = 22;

		memset(dst->name, 0, 22);
		if (dst->nameLength > 0)
			memcpy(dst->name, s->name, dst->nameLength);

		if (s->dataPtr == NULL)
			dst->length = 0;
	}

	size_t result = fwrite(&ih, INSTR_XI_HEADER_SIZE + (ih.numSamples * sizeof (xmSmpHdr_t)), 1, f);
	if (result != 1)
	{
		fclose(f);
		okBoxThreadSafe(0, "System message", "Error saving instrument: general I/O error!");
		return false;
	}

	pauseAudio();
	s = instr[saveInstrNum]->smp;
	for (int32_t i = 0; i < numSamples; i++, s++)
	{
		if (s->dataPtr != NULL && s->length > 0)
		{
			unfixSample(s);
			samp2Delta(s->dataPtr, s->length, s->flags);

			result = fwrite(s->dataPtr, 1, SAMPLE_LENGTH_BYTES(s), f);

			delta2Samp(s->dataPtr, s->length, s->flags);
			fixSample(s);

			if (result != (size_t)SAMPLE_LENGTH_BYTES(s)) // write not OK
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

	(void)ptr;
}

void saveInstr(UNICHAR *filenameU, int16_t insNum)
{
	if (insNum == 0)
		return;

	saveInstrNum = insNum;
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
	const double dNote = (log2(freq / 440000.0) * 12.0) + 57.0;
	const int32_t note = (const int32_t)(dNote + 0.5); // rounded

	return (int16_t)note;
}

static int32_t SDLCALL loadInstrThread(void *ptr)
{
	int16_t a, b;
	int32_t i, j, numLoadedSamples;
	xiHdr_t xi_h;
	patHdr_t pat_h;
	patWaveHdr_t patWave_h;
	xmSmpHdr_t *src;
	sample_t *s;
	instr_t *ins;

	bool stereoWarning = false;
	numLoadedSamples = 0;

	if (editor.tmpInstrFilenameU == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	FILE *f = UNICHAR_FOPEN(editor.tmpInstrFilenameU, "rb");
	if (f == NULL)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	memset(&xi_h, 0, sizeof (xi_h));
	memset(&pat_h, 0, sizeof (pat_h));
	memset(&patWave_h, 0, sizeof (patWave_h));

	fread(&xi_h, INSTR_XI_HEADER_SIZE, 1, f);
	if (!strncmp(xi_h.ID, "Extended Instrument: ", 21))
	{
		// XI - Extended Instrument

		if (xi_h.version != 0x0101 && xi_h.version != 0x0102)
		{
			okBoxThreadSafe(0, "System message", "Incompatible format version!");
			goto loadDone;
		}

		// not even FT2.01 can save old v1.01 .XI files, so I have no way to verify this
		if (xi_h.version == 0x0101) 
		{
			fseek(f, -20, SEEK_CUR);
			xi_h.numSamples = xi_h.midiProgram;
			xi_h.midiProgram = 0;
			xi_h.midiBend = 0;
			xi_h.mute = false;
		}

		numLoadedSamples = xi_h.numSamples;

		memcpy(song.instrName[editor.curInstr], xi_h.name, 22);
		song.instrName[editor.curInstr][22] = '\0';

		pauseAudio();

		freeInstr(editor.curInstr);

		if (xi_h.numSamples > 0)
		{
			if (!allocateInstr(editor.curInstr))
			{
				resumeAudio();
				okBoxThreadSafe(0, "System message", "Not enough memory!");
				goto loadDone;
			}

			// copy instrument header elements to our instrument struct

			ins = instr[editor.curInstr];
			memcpy(ins->note2SampleLUT, xi_h.note2SampleLUT, 96);
			memcpy(ins->volEnvPoints, xi_h.volEnvPoints, 12*2*sizeof(int16_t));
			memcpy(ins->panEnvPoints, xi_h.panEnvPoints, 12*2*sizeof(int16_t));
			ins->volEnvLength = xi_h.volEnvLength;
			ins->panEnvLength = xi_h.panEnvLength;
			ins->volEnvSustain = xi_h.volEnvSustain;
			ins->volEnvLoopStart = xi_h.volEnvLoopStart;
			ins->volEnvLoopEnd = xi_h.volEnvLoopEnd;
			ins->panEnvSustain = xi_h.panEnvSustain;
			ins->panEnvLoopStart = xi_h.panEnvLoopStart;
			ins->panEnvLoopEnd = xi_h.panEnvLoopEnd;
			ins->volEnvFlags = xi_h.volEnvFlags;
			ins->panEnvFlags = xi_h.panEnvFlags;
			ins->vibType = xi_h.vibType;
			ins->vibSweep = xi_h.vibSweep;
			ins->vibDepth = xi_h.vibDepth;
			ins->vibRate = xi_h.vibRate;
			ins->fadeout = xi_h.fadeout;
			ins->midiOn = (xi_h.midiOn == 1) ? true : false;
			ins->midiChannel = xi_h.midiChannel;
			ins->midiProgram = xi_h.midiProgram;
			ins->midiBend = xi_h.midiBend;
			ins->mute = (xi_h.mute == 1) ? true : false; // correct logic! Don't mess with this
			ins->numSamples = xi_h.numSamples; // used in loadInstrSample()

			int32_t sampleHeadersToRead = xi_h.numSamples;
			if (sampleHeadersToRead > MAX_SMP_PER_INST)
				sampleHeadersToRead = MAX_SMP_PER_INST;

			if (fread(xi_h.smp, sampleHeadersToRead * sizeof (xmSmpHdr_t), 1, f) != 1)
			{
				freeInstr(editor.curInstr);
				resumeAudio();
				okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
				goto loadDone;
			}

			if (xi_h.numSamples > MAX_SMP_PER_INST)
			{
				const int32_t sampleHeadersToSkip = xi_h.numSamples - MAX_SMP_PER_INST;
				fseek(f, sampleHeadersToSkip * sizeof (xmSmpHdr_t), SEEK_CUR);
			}

			for (i = 0; i < sampleHeadersToRead; i++)
			{
				s = &instr[editor.curInstr]->smp[i];
				src = &xi_h.smp[i];

				// copy sample header elements to our sample struct

				s->length = src->length;
				s->loopStart = src->loopStart;
				s->loopLength = src->loopLength;
				s->volume = src->volume;
				s->finetune = src->finetune;
				s->flags = src->flags;
				s->panning = src->panning;
				s->relativeNote = src->relativeNote;
				memcpy(s->name, src->name, 22);
				s->name[22] = '\0';

				// dst->dataPtr is set up later
			}
		}

		for (i = 0; i < xi_h.numSamples; i++)
		{
			s = &instr[editor.curInstr]->smp[i];

			if (s->length <= 0)
			{
				s->length = 0;
				s->loopStart = 0;
				s->loopLength = 0;
				s->flags = 0;
			}
			else
			{
				const int32_t lengthInFile = s->length;
				bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
				bool stereoSample = !!(s->flags & SAMPLE_STEREO);

				if (sample16Bit) // we use units of samples (not bytes like in FT2)
				{
					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}

				if (s->length > MAX_SAMPLE_LEN)
					s->length = MAX_SAMPLE_LEN;

				if (!allocateSmpData(s, s->length, sample16Bit))
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "Not enough memory!");
					goto loadDone;
				}

				const int32_t sampleLengthInBytes = SAMPLE_LENGTH_BYTES(s);
				if (fread(s->dataPtr, sampleLengthInBytes, 1, f) != 1)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				if (sampleLengthInBytes < lengthInFile)
					fseek(f, lengthInFile-sampleLengthInBytes, SEEK_CUR);

				delta2Samp(s->dataPtr, s->length, s->flags); // stereo samples are handled here

				// check if it was a stereo sample
				if (stereoSample)
				{
					s->flags &= ~SAMPLE_STEREO;

					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;

					reallocateSmpData(s, s->length, sample16Bit);
					stereoWarning = true;
				}
			}
		}

		resumeAudio();
	}
	else
	{
		rewind(f);

		fread(&pat_h, 1, sizeof (patHdr_t), f);
		if (!memcmp(pat_h.ID, "GF1PATCH110\0ID#000002\0", 22))
		{
			// PAT - Gravis Ultrasound patch

			if (pat_h.numSamples == 0)
				pat_h.numSamples = 1; // to some patch makers, 0 means 1

			if (pat_h.layers > 1 || pat_h.numSamples > MAX_SMP_PER_INST)
			{
				okBoxThreadSafe(0, "System message", "Incompatible instrument!");
				goto loadDone;
			}

			numLoadedSamples = pat_h.numSamples;

			pauseAudio();
			freeInstr(editor.curInstr);

			if (!allocateInstr(editor.curInstr))
			{
				okBoxThreadSafe(0, "System message", "Not enough memory!");
				goto loadDone;
			}

			memcpy(song.instrName[editor.curInstr], pat_h.instrName, 16);
			song.instrName[editor.curInstr][16] = '\0';

			for (i = 0; i < pat_h.numSamples; i++)
			{
				s = &instr[editor.curInstr]->smp[i];
				ins = instr[editor.curInstr];

				if (fread(&patWave_h, 1, sizeof (patWave_h), f) != sizeof (patWave_h))
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				const int32_t lengthInFile = patWave_h.sampleLength;

				bool sample16Bit = !!(patWave_h.flags & 1);

				s->length = lengthInFile;
				if (sample16Bit)
				{
					s->flags |= SAMPLE_16BIT;
					s->length >>= 1;
				}

				if (s->length > MAX_SAMPLE_LEN)
					s->length = MAX_SAMPLE_LEN;

				if (!allocateSmpData(s, s->length, sample16Bit))
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "Not enough memory!");
					goto loadDone;
				}

				if (i == 0)
				{
					ins->vibSweep = patWave_h.vibSweep;
					ins->vibRate = (patWave_h.vibRate + 2) >> 2; // rounded
					ins->vibDepth = (patWave_h.vibDepth + 1) >> 1; // rounded
				}

				s = &instr[editor.curInstr]->smp[i];

				memcpy(s->name, patWave_h.name, 7);
				s->name[7] = '\0';

				if (patWave_h.flags & 4) // loop enabled?
				{
					if (patWave_h.flags & 8)
						s->flags |= LOOP_BIDI;
					else
						s->flags |= LOOP_FWD;
				}

				s->panning = ((patWave_h.panning << 4) & 0xF0) | (patWave_h.panning & 0xF);
				s->loopStart = patWave_h.loopStart;
				s->loopLength = patWave_h.loopEnd - patWave_h.loopStart;

				if (sample16Bit)
				{
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}

				const double dFreq = (1.0 + (patWave_h.finetune / 512.0)) * patWave_h.sampleRate;
				tuneSample(s, (int32_t)(dFreq + 0.5), audio.linearPeriodsFlag);

				a = getPATNote(patWave_h.rootFrq) - (12 * 3);
				s->relativeNote -= (uint8_t)a;

				a = getPATNote(patWave_h.lowFrq);
				b = getPATNote(patWave_h.highFreq);

				a = CLAMP(a, 0, 95);
				b = CLAMP(b, 0, 95);

				for (j = a; j <= b; j++)
					ins->note2SampleLUT[j] = (uint8_t)i;

				const int32_t sampleLengthInBytes = SAMPLE_LENGTH_BYTES(s);
				size_t bytesRead = fread(s->dataPtr, sampleLengthInBytes, 1, f);
				if (bytesRead != 1)
				{
					freeInstr(editor.curInstr);
					resumeAudio();
					okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
					goto loadDone;
				}

				if (sampleLengthInBytes < lengthInFile)
					fseek(f, lengthInFile-sampleLengthInBytes, SEEK_CUR);

				if (patWave_h.flags & 2) // unsigned samples?
				{
					if (sample16Bit)
						conv16BitSample(s->dataPtr, s->length, false);
					else
						conv8BitSample(s->dataPtr, s->length, false);
				}
			}

			resumeAudio();
		}
	}

loadDone:
	fclose(f);

	numLoadedSamples = CLAMP(numLoadedSamples, 1, MAX_SMP_PER_INST);

	ins = instr[editor.curInstr];
	if (ins != NULL)
	{
		sanitizeInstrument(ins);
		for (i = 0; i < numLoadedSamples; i++)
		{
			s = &ins->smp[i];
			sanitizeSample(s);

			if (s->dataPtr != NULL)
				fixSample(s);
		}

		fixInstrAndSampleNames(editor.curInstr);
	}
	editor.updateCurInstr = true; // setMouseBusy(false) is called in the input/video thread when done

	if (numLoadedSamples > MAX_SMP_PER_INST)
		okBoxThreadSafe(0, "System message", "Warning: The instrument contained >16 samples. The extra samples were discarded!");

	if (stereoWarning)
		okBoxThreadSafe(0, "System message", "Warning: The instrument contained stereo sample(s). They were mixed to mono!");

	return true;
	(void)ptr;
}

bool fileIsInstr(UNICHAR *filenameU)
{
	FILE *f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
		return false;

	char header[22];
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
