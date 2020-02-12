#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <limits.h> // PATH_MAX
#endif
#include "ft2_replayer.h"

#define PROG_VER_STR "1.09"

// do NOT change these! It will only mess things up...

/* "60Hz" ranges everywhere from 59..61Hz depending on the monitor, so with
** no vsync we will get stuttering because the rate is not perfect... */
#define VBLANK_HZ 60

/* Scopes are clocked at 64Hz instead of 60Hz to prevent possible stutters
** from monitors not being exactly 60Hz (and unstable non-vsync mode). */
#define SCOPE_HZ 64

#define FT2_VBLANK_HZ 70
#define SCREEN_W 632
#define SCREEN_H 400

/* Amount of extra bytes to allocate for every instrument sample,
** this is used for a hack for resampling interpolation to be
** branchless in the inner channel mixer loop.
** Warning: Do not change this! */
#define LOOP_FIX_LEN 4

#ifndef _WIN32
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define DIR_DELIMITER '/'
#else
#define DIR_DELIMITER '\\'
#define PATH_MAX MAX_PATH
#endif

#define SGN(x) (((x) >= 0) ? 1 : -1)
#define ABS(a) (((a) < 0) ? -(a) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

// fast 32-bit -> 8-bit clamp
#define CLAMP8(i) if ((int8_t)(i) != i) i = 0x7F ^ (i >> 31)

// fast 32-bit -> 16-bit clamp
#define CLAMP16(i) if ((int16_t)(i) != i) i = 0x7FFF ^ (i >> 31)

#define ALIGN_PTR(p, x) (((uintptr_t)(p) + ((x)-1)) & ~((x)-1))
#define MALLOC_PAD(size, pad) (malloc((size) + (pad)))

#define SWAP16(value) \
( \
	(((uint16_t)((value) & 0x00FF)) << 8) | \
	(((uint16_t)((value) & 0xFF00)) >> 8)   \
)

#define SWAP32(value) \
( \
	(((uint32_t)((value) & 0x000000FF)) << 24) | \
	(((uint32_t)((value) & 0x0000FF00)) <<  8) | \
	(((uint32_t)((value) & 0x00FF0000)) >>  8) | \
	(((uint32_t)((value) & 0xFF000000)) >> 24)   \
)

struct cpu_t
{
	bool hasSSE, hasSSE2;
} cpu;

struct editor_t
{
	struct ui_t
	{
		volatile bool setMouseBusy, setMouseIdle;
		bool sysReqEnterPressed;
		char fullscreenButtonText[24];

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
	} ui;

	struct cursor_t
	{
		uint8_t ch;
		int8_t object;
	} cursor;

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
} editor;
