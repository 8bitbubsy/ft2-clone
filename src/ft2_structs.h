#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"

typedef struct cpu_t
{
	bool hasSSE, hasSSE2;
} cpu_t;

typedef struct editor_t
{
	UNICHAR binaryPathU[PATH_MAX + 2];
	UNICHAR *tmpFilenameU, *tmpInstrFilenameU; // used by saving/loading threads
	UNICHAR *configFileLocation, *audioDevConfigFileLocation, *midiConfigFileLocation;

	volatile bool mainLoopOngoing;
	volatile bool busy, scopeThreadMutex, programRunning, wavIsRendering, wavReachedEndFlag;
	volatile bool updateCurSmp, updateCurInstr, diskOpReadDir, diskOpReadDone, updateWindowTitle;
	volatile uint8_t loadMusicEvent;
	volatile FILE *wavRendererFileHandle;

	bool autoPlayOnDrop, trimThreadWasDone, throwExit, editTextFlag;
	bool copyMaskEnable, diskOpReadOnOpen, samplingAudioFlag, editSampleFlag;
	bool instrBankSwapped, chnMode[MAX_VOICES], NI_Play;

	uint8_t curPlayInstr, curPlaySmp, curSmpChannel, currPanEnvPoint, currVolEnvPoint;
	uint8_t copyMask[5], pasteMask[5], transpMask[5], smpEd_NoteNr, instrBankOffset, sampleBankOffset;
	uint8_t srcInstr, curInstr, srcSmp, curSmp, currHelpScreen, currConfigScreen, textCursorBlinkCounter;
	uint8_t keyOnTab[MAX_VOICES], ID_Add, curOctave;
	uint8_t sampleSaveMode, moduleSaveMode, ptnJumpPos[4];
	int16_t globalVol, songPos, pattPos;
	uint16_t tmpPattern, editPattern, speed, tempo, timer, ptnCursorY;
	int32_t keyOffNr, keyOffTime[MAX_VOICES];
	uint32_t framesPassed, wavRendererTime;
	double dPerfFreq, dPerfFreqMulMicro, dPerfFreqMulMs;
} editor_t;

typedef struct ui_t
{
	volatile bool setMouseBusy, setMouseIdle;
	bool sysReqEnterPressed;

	// all screens
	bool extended, sysReqShown;

	// top screens
	bool instrSwitcherShown, aboutScreenShown, helpScreenShown, configScreenShown;
	bool scopesShown, diskOpShown, nibblesShown, transposeShown, instEditorExtShown;
	bool sampleEditorExtShown, advEditShown, wavRendererShown, trimScreenShown;
	bool drawBPMFlag, drawSpeedFlag, drawGlobVolFlag, drawPosEdFlag, drawPattNumLenFlag;
	bool updatePosSections, updatePosEdScrollBar;
	uint8_t oldTopLeftScreen;

	// bottom screens
	bool patternEditorShown, instEditorShown, sampleEditorShown, pattChanScrollShown;
	bool leftLoopPinMoving, rightLoopPinMoving;
	bool drawReplayerPianoFlag, drawPianoFlag, updatePatternEditor;
	uint8_t channelOffset, numChannelsShown, maxVisibleChannels;
	uint16_t patternChannelWidth;
	int32_t sampleDataOrLoopDrag;

	// backup flag for when entering/exiting extended pattern editor (TODO: this is lame and shouldn't be hardcoded)
	bool _aboutScreenShown, _helpScreenShown, _configScreenShown, _diskOpShown;
	bool _nibblesShown, _transposeShown, _instEditorShown;
	bool _instEditorExtShown, _sampleEditorExtShown, _patternEditorShown;
	bool _sampleEditorShown, _advEditShown, _wavRendererShown, _trimScreenShown;
	// -------------------------------------------------------------------------
} ui_t;

typedef struct cursor_t
{
	uint8_t ch;
	int8_t object;
} cursor_t;

extern cpu_t cpu;
extern editor_t editor;
extern ui_t ui;
extern cursor_t cursor;
