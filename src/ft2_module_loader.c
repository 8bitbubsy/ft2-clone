// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "ft2_header.h"
#include "ft2_scopes.h"
#include "ft2_trim.h"
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_wav_renderer.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_diskop.h"
#include "ft2_sample_loader.h"
#include "ft2_mouse.h"
#include "ft2_midi.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_sysreqs.h"

bool loadDIGI(FILE *f, uint32_t filesize);
bool loadMOD(FILE *f, uint32_t filesize);
bool loadS3M(FILE *f, uint32_t filesize);
bool loadSTK(FILE *f, uint32_t filesize);
bool loadSTM(FILE *f, uint32_t filesize);
bool loadXM(FILE *f, uint32_t filesize);

enum
{
	FORMAT_UNKNOWN = 0,
	FORMAT_POSSIBLY_STK = 1,
	FORMAT_XM = 2,
	FORMAT_MOD = 3,
	FORMAT_S3M = 4,
	FORMAT_STM = 5,
	FORMAT_DIGI = 6
};

// file extensions accepted by Disk Op. in module mode
char *supportedModExtensions[] =
{
	"xm", "ft", "nst", "stk", "mod", "s3m", "stm", "fst",
	"digi",

	// IMPORTANT: Remember comma after last entry above
	"END_OF_LIST" // do NOT move, remove or edit this line!
};

// globals for module loaders
volatile bool tmpLinearPeriodsFlag;
int16_t pattLensTmp[MAX_PATTERNS];
tonTyp *pattTmp[MAX_PATTERNS];
instrTyp *instrTmp[1+256];
songTyp songTmp;
// --------------------------

static volatile bool musicIsLoading, moduleLoaded, moduleFailedToLoad;
static SDL_Thread *thread;
static uint8_t oldPlayMode;
static void setupLoadedModule(void);
static void freeTmpModule(void);

// Crude module detection routine. These aren't always accurate detections!
static int8_t detectModule(FILE *f)
{
	uint8_t D[256], I[4];

	fseek(f, 0, SEEK_END);
	uint32_t fileLength = (uint32_t)ftell(f);
	rewind(f);

	memset(D, 0, sizeof (D));
	fread(D, 1, sizeof (D), f);
	fseek(f, 1080, SEEK_SET); // MOD ID
	I[0] = I[1] = I[2] = I[3] = 0;
	fread(I, 1, 4, f);
	rewind(f);

	// DIGI Booster (non-Pro)
	if (!memcmp("DIGI Booster module", &D[0x00], 19+1) && D[0x19] >= 1 && D[0x19] <= 8)
		return FORMAT_DIGI;

	// Scream Tracker 3 S3M (and compatible trackers)
	if (!memcmp("SCRM", &D[0x2C], 4) && D[0x1D] == 16) // XXX: byte=16 in all cases?
		return FORMAT_S3M;

	// Scream Tracker 2 STM
	if ((!memcmp("!Scream!", &D[0x14], 8) || !memcmp("BMOD2STM", &D[0x14], 8) ||
		 !memcmp("WUZAMOD!", &D[0x14], 8) || !memcmp("SWavePro", &D[0x14], 8)) && D[0x1D] == 2) // XXX: byte=2 for "WUZAMOD!"/"SWavePro" ?
	{
		return FORMAT_STM;
	}

	// Generic multi-channel MOD (1..9 channels)
	if (isdigit(I[0]) && I[0] != '0' && I[1] == 'C' && I[2] == 'H' && I[3] == 'N') // xCHN
		return FORMAT_MOD;

	// Digital Tracker (Atari Falcon)
	if (I[0] == 'F' && I[1] == 'A' && I[2] == '0' && I[3] >= '4' && I[3] <= '8') // FA0x (x=4..8)
		return FORMAT_MOD;

	// Generic multi-channel MOD (10..99 channels)
	if (isdigit(I[0]) && isdigit(I[1]) && I[0] != '0' && I[2] == 'C' && I[3] == 'H') // xxCH
		return FORMAT_MOD;

	// Generic multi-channel MOD (10..99 channels)
	if (isdigit(I[0]) && isdigit(I[1]) && I[0] != '0' && I[2] == 'C' && I[3] == 'N') // xxCN (same as xxCH)
		return FORMAT_MOD;
	
	// ProTracker and generic MOD formats
	if (!memcmp("M.K.", I, 4) || !memcmp("M!K!", I, 4) || !memcmp("NSMS", I, 4) ||
		!memcmp("LARD", I, 4) || !memcmp("PATT", I, 4) || !memcmp("FLT4", I, 4) ||
		!memcmp("FLT8", I, 4) || !memcmp("EXO4", I, 4) || !memcmp("EXO8", I, 4) ||
		!memcmp("N.T.", I, 4) || !memcmp("M&K!", I, 4) || !memcmp("FEST", I, 4) ||
		!memcmp("CD61", I, 4) || !memcmp("CD81", I, 4) || !memcmp("OKTA", I, 4) ||
		!memcmp("OCTA", I, 4))
	{
		return FORMAT_MOD;
	}

	/* Check if the file is a .it module (Impulse Tracker, not supported).
	** Some people may attempt to load .IT files in the FT2 clone, so
	** reject them here instead of accidentally loading them as .STK
	*/
	if (!memcmp("IMPM", D, 4) && D[0x16] == 0)
		return FORMAT_UNKNOWN;

	/* Fasttracker II XM and compatible trackers.
	** Note: This test can falsely be true for STK modules (and non-supported files) where the
	** first 17 bytes start with "Extended Module: ". This is unlikely to happen.
	*/
	if (!memcmp("Extended Module: ", &D[0x00], 17))
		return FORMAT_XM;

	/* Lastly, we assume that the file is either a 15-sample STK or an unsupported file.
	** Let's assume it's an STK and do some sanity checks. If they fail, we have an
	** unsupported file.
	*/

	// minimum and maximum (?) possible size for a supported STK
	if (fileLength < 1624 || fileLength > 984634)
		return FORMAT_UNKNOWN;

	// test STK numOrders+BPM for illegal values
	fseek(f, 470, SEEK_SET);
	D[0] = D[1] = 0;
	fread(D, 1, 2, f);
	rewind(f);

	if (D[0] <= 128 && D[1] <= 220)
		return FORMAT_POSSIBLY_STK;

	return FORMAT_UNKNOWN;
}

static bool doLoadMusic(bool externalThreadFlag)
{
	// setup message box functions
	loaderMsgBox = externalThreadFlag ? myLoaderMsgBoxThreadSafe : myLoaderMsgBox;
	loaderSysReq = externalThreadFlag ? okBoxThreadSafe : okBox;

	if (editor.tmpFilenameU == NULL)
	{
		loaderMsgBox("Generic memory fault during loading!");
		goto loadError;
	}

	FILE *f = UNICHAR_FOPEN(editor.tmpFilenameU, "rb");
	if (f == NULL)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use? Does it exist?");
		goto loadError;
	}

	int8_t format = detectModule(f);
	fseek(f, 0, SEEK_END);
	uint32_t filesize = ftell(f);

	rewind(f);
	switch (format)
	{
		case FORMAT_XM: moduleLoaded = loadXM(f, filesize); break;
		case FORMAT_S3M: moduleLoaded = loadS3M(f, filesize); break;
		case FORMAT_STM: moduleLoaded = loadSTM(f, filesize); break;
		case FORMAT_MOD: moduleLoaded = loadMOD(f, filesize); break;
		case FORMAT_POSSIBLY_STK: moduleLoaded = loadSTK(f, filesize); break;
		case FORMAT_DIGI: moduleLoaded = loadDIGI(f, filesize); break;

		default:
			loaderMsgBox("This file is not a supported module!");
		break;
	}
	fclose(f);

	if (!moduleLoaded)
		goto loadError;

	moduleLoaded = true;
	return true;

loadError:
	freeTmpModule();
	moduleFailedToLoad = true;
	return false;
}

static void clearTmpModule(void)
{
	memset(pattTmp, 0, sizeof (pattTmp));
	memset(instrTmp, 0, sizeof (instrTmp));
	memset(&songTmp, 0, sizeof (songTmp));

	for (uint32_t i = 0; i < MAX_PATTERNS; i++)
		pattLensTmp[i] = 64;
}

static int32_t SDLCALL loadMusicThread(void *ptr)
{
	return doLoadMusic(true);
	(void)ptr;
}

void loadMusic(UNICHAR *filenameU)
{
	if (musicIsLoading || filenameU == NULL)
		return;

	mouseAnimOn();

	musicIsLoading = true;
	moduleLoaded = false;
	moduleFailedToLoad = false;

	clearTmpModule(); // clear stuff from last loading session (very important)
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	thread = SDL_CreateThread(loadMusicThread, NULL, NULL);
	if (thread == NULL)
	{
		editor.loadMusicEvent = EVENT_NONE;
		okBox(0, "System message", "Couldn't create thread!");
		musicIsLoading = false;
		return;
	}

	SDL_DetachThread(thread);
}

bool loadMusicUnthreaded(UNICHAR *filenameU, bool autoPlay)
{
	if (filenameU == NULL)
		return false;

	clearTmpModule(); // clear stuff from last loading session (very important)
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	editor.loadMusicEvent = EVENT_NONE;
	doLoadMusic(false);

	if (moduleLoaded)
	{
		setupLoadedModule();
		if (autoPlay)
			startPlaying(PLAYMODE_SONG, 0);

		return true;
	}

	return false;
}

bool allocateTmpPatt(int32_t nr, uint16_t rows)
{
	pattTmp[nr] = (tonTyp *)calloc((MAX_PATT_LEN * TRACK_WIDTH) + 16, 1);
	if (pattTmp[nr] == NULL)
		return false;

	pattLensTmp[nr] = rows;
	return true;
}

bool allocateTmpInstr(int16_t nr)
{
	if (instrTmp[nr] != NULL)
		return false; // already allocated

	instrTyp *p = (instrTyp *)calloc(1, sizeof (instrTyp));
	if (p == NULL)
		return false;

	sampleTyp *s = p->samp;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++, s++)
	{
		s->pan = 128;
		s->vol = 64;
	}

	instrTmp[nr] = p;
	return true;
}

static void freeTmpModule(void) // called on module load error
{
	// free all patterns
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (pattTmp[i] != NULL)
		{
			free(pattTmp[i]);
			pattTmp[i] = NULL;
		}
	}

	// free all instruments and samples
	for (int32_t i = 1; i <= 256; i++) // if >128 instruments, we fake-load up to 128 extra (and discard them later)
	{
		if (instrTmp[i] == NULL)
			continue;

		sampleTyp *s = instrTmp[i]->samp;
		for (int32_t j = 0; j < MAX_SMP_PER_INST; j++, s++)
		{
			if (s->origPek != NULL)
				free(s->origPek);
		}

		free(instrTmp[i]);
		instrTmp[i] = NULL;
	}
}

bool tmpPatternEmpty(uint16_t nr)
{
	if (pattTmp[nr] == NULL)
		return true;

	uint8_t *scanPtr = (uint8_t *)pattTmp[nr];
	const uint32_t scanLen = pattLensTmp[nr] * TRACK_WIDTH;

	for (uint32_t i = 0; i < scanLen; i++)
	{
		if (scanPtr[i] != 0)
			return false;
	}

	return true;
}

void clearUnusedChannels(tonTyp *p, int16_t pattLen, int32_t antChn)
{
	if (p == NULL || antChn >= MAX_VOICES)
		return;

	const int32_t width = sizeof (tonTyp) * (MAX_VOICES - antChn);

	tonTyp *ptr = &p[antChn];
	for (int32_t i = 0; i < pattLen; i++, ptr += MAX_VOICES)
		memset(ptr, 0, width);
}

// called from input/video thread after the module was done loading
static void setupLoadedModule(void)
{
	lockMixerCallback();

	freeAllInstr();
	freeAllPatterns();

	oldPlayMode = playMode;
	playMode = PLAYMODE_IDLE;
	songPlaying = false;

#ifdef HAS_MIDI
	midi.currMIDIVibDepth = 0;
	midi.currMIDIPitch = 0;
#endif

	memset(editor.keyOnTab, 0, sizeof (editor.keyOnTab));

	// copy over new pattern pointers and lengths
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		patt[i] = pattTmp[i];
		pattLens[i] = pattLensTmp[i];
	}

	// copy over song struct
	memcpy(&song, &songTmp, sizeof (songTyp));
	fixSongName();

	// copy over new instruments (includes sample pointers)
	for (int16_t i = 1; i <= MAX_INST; i++)
	{
		instr[i] = instrTmp[i];
		fixInstrAndSampleNames(i);

		if (instr[i] != NULL)
		{
			for (int32_t j = 0; j < MAX_SMP_PER_INST; j++)
			{
				sampleTyp *s = &instr[i]->samp[j];

				checkSampleRepeat(s);
				if (s->pek != NULL)
					fixSample(s); // prepare sample for branchless linear interpolation
			}
		}
	}

	// we are the owners of the allocated memory ptrs set by the loader thread now

	if (song.antChn == 0)
		song.antChn = 2;

	// support non-even channel numbers
	if (song.antChn & 1)
	{
		song.antChn++;
		if (song.antChn > MAX_VOICES)
			song.antChn = MAX_VOICES;
	}

	if (song.len == 0)
		song.len = 1;

	if (song.repS >= song.len)
		song.repS = 0;

	song.globVol = 64;

	// remove overflown stuff in pattern data (FT2 doesn't do this)
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (pattLens[i] <= 0)
			pattLens[i] = 64;

		if (pattLens[i] > MAX_PATT_LEN)
			pattLens[i] = MAX_PATT_LEN;

		tonTyp *p = patt[i];
		if (p == NULL)
			continue;

		tonTyp *note = p;
		for (int32_t j = 0; j < MAX_PATT_LEN * MAX_VOICES; j++, note++)
		{
			if (note->ton > 97)
				note->ton = 0;

			if (note->instr > 128)
				note->instr = 0;

			if (note->effTyp > 35)
			{
				note->effTyp = 0;
				note->eff = 0;
			}
		}
	}

	setScrollBarEnd(SB_POS_ED, (song.len - 1) + 5);
	setScrollBarPos(SB_POS_ED, 0, false);

	resetChannels();
	setPos(0, 0, true);
	P_SetSpeed(song.speed);

	editor.tmpPattern = editor.editPattern; // set kludge variable
	editor.speed = song.speed;
	editor.tempo = song.tempo;
	editor.timer = song.timer;
	editor.globalVol = song.globVol;

	setFrqTab(tmpLinearPeriodsFlag);

	unlockMixerCallback();

	editor.currVolEnvPoint = 0;
	editor.currPanEnvPoint = 0;

	refreshScopes();
	exitTextEditing();
	updateTextBoxPointers();
	resetChannelOffset();
	updateChanNums();
	resetWavRenderer();
	clearPattMark();
	resetTrimSizes();
	resetPlaybackTime();

	diskOpSetFilename(DISKOP_ITEM_MODULE, editor.tmpFilenameU);

	// redraw top part of screen
	if (ui.extended)
	{
		togglePatternEditorExtended(); // exit
		togglePatternEditorExtended(); // re-enter (force redrawing)
	}
	else
	{
		// redraw top screen
		hideTopScreen();
		showTopScreen(true);
	}

	updateSampleEditorSample();
	showBottomScreen(); // redraw bottom screen (also redraws pattern editor)

	if (ui.instEditorShown)
		drawPiano(NULL); // redraw piano now (since if playing = wait for next tick update)

	removeSongModifiedFlag();

	moduleFailedToLoad = false;
	moduleLoaded = false;
	editor.loadMusicEvent = EVENT_NONE;
}

bool handleModuleLoadFromArg(int argc, char **argv)
{
	// we always expect only one parameter, and that it is the module

	if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0')
		return false;

#ifdef __APPLE__
	if (argc == 2 && !strncmp(argv[1], "-psn_", 5))
		return false; // OS X < 10.9 passes a -psn_x_xxxxx parameter on double-click launch
#endif

	const uint32_t filenameLen = (const uint32_t)strlen(argv[1]);

	UNICHAR *tmpPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	if (tmpPathU == NULL)
	{
		okBox(0, "System message", "Not enough memory!");
		return false;
	}

	UNICHAR *filenameU = (UNICHAR *)malloc((filenameLen + 1) * sizeof (UNICHAR));
	if (filenameU == NULL)
	{
		free(tmpPathU);
		okBox(0, "System message", "Not enough memory!");
		return false;
	}

	tmpPathU[0] = 0;
	filenameU[0] = 0;

#ifdef _WIN32
	MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, filenameU, filenameLen+1);
#else
	strcpy(filenameU, argv[1]);
#endif

	// store old path
	UNICHAR_GETCWD(tmpPathU, PATH_MAX);

	// set path to where the main executable is
	UNICHAR_CHDIR(editor.binaryPathU);

	const int32_t filesize = getFileSize(filenameU);
	if (filesize == -1 || filesize >= 512L*1024*1024) // 1) >=2GB   2) >=512MB
	{
		free(filenameU);
		UNICHAR_CHDIR(tmpPathU); // set old path back
		free(tmpPathU);

		okBox(0, "System message", "Error: The module is too big to be loaded!");
		return false;
	}

	bool result = loadMusicUnthreaded(filenameU, true);

	free(filenameU);
	UNICHAR_CHDIR(tmpPathU); // set old path back
	free(tmpPathU);

	return result;
}

static bool fileIsModule(UNICHAR *pathU)
{
	FILE *f = UNICHAR_FOPEN(pathU, "rb");
	if (f == NULL)
		return false;

	int8_t modFormat = detectModule(f);
	fclose(f);

	/* If the module was not identified (possibly STK type),
	** check the file extension and handle it as a module only
	** if it starts with "mod."/"stk." or ends with ".mod"/".stk" (case insensitive).
	*/
	if (modFormat == FORMAT_POSSIBLY_STK)
	{
		char *path = unicharToCp437(pathU, false);
		if (path == NULL)
			return false;

		int32_t pathLen = (int32_t)strlen(path);

		// get filename from path
		int32_t i = pathLen;
		while (i--)
		{
			if (path[i] == DIR_DELIMITER)
				break;
		}

		char *filename = path;
		if (i > 0)
			filename += i + 1;

		int32_t filenameLen = (int32_t)strlen(filename);
		// --------------------------

		if (filenameLen > 5)
		{
			if (!_strnicmp("mod.", filename, 4) || !_strnicmp("stk.", filename, 4))
			{
				free(path);
				return true;
			}

			if (!_strnicmp(".mod", &filename[filenameLen-4], 4) || !_strnicmp(".stk", &filename[filenameLen-4], 4))
			{
				free(path);
				return true;
			}
		}

		free(path);
		return false;
	}

	return (modFormat != FORMAT_UNKNOWN);
}

void loadDroppedFile(char *fullPathUTF8, bool songModifiedCheck)
{
	if (ui.sysReqShown || fullPathUTF8 == NULL)
		return;

	const int32_t fullPathLen = (const int32_t)strlen(fullPathUTF8);
	if (fullPathLen == 0)
		return;

	UNICHAR *fullPathU = (UNICHAR *)malloc((fullPathLen + 1) * sizeof (UNICHAR));
	if (fullPathU == NULL)
	{
		okBox(0, "System message", "Not enough memory!");
		return;
	}

	fullPathU[0] = 0;

#ifdef _WIN32
	MultiByteToWideChar(CP_UTF8, 0, fullPathUTF8, -1, fullPathU, fullPathLen+1);
#else
	strcpy(fullPathU, fullPathUTF8);
#endif

	const int32_t filesize = getFileSize(fullPathU);

	if (filesize == -1) // >2GB
	{
		okBox(0, "System message", "The file is too big and can't be loaded (over 2GB).");
		free(fullPathU);
		return;
	}

	if (filesize >= 128L*1024*1024) // 128MB
	{
		if (okBox(2, "System request", "Are you sure you want to load such a big file?") != 1)
		{
			free(fullPathU);
			return;
		}
	}

	// pass UTF8 to these tests so that we can test file ending in ASCII/ANSI

	if (fileIsInstr(fullPathU))
	{
		loadInstr(fullPathU);
	}
	else if (fileIsModule(fullPathU))
	{
		SDL_RestoreWindow(video.window);

		if (songModifiedCheck && song.isModified)
		{
			// de-minimize window and set focus so that the user sees the message box
			SDL_RestoreWindow(video.window);
			SDL_RaiseWindow(video.window);

			if (!askUnsavedChanges(ASK_TYPE_LOAD_SONG))
			{
				free(fullPathU);
				return;
			}
		}

		editor.loadMusicEvent = EVENT_LOADMUSIC_DRAGNDROP;
		loadMusic(fullPathU);
	}
	else
	{
		loadSample(fullPathU, editor.curSmp, false);
	}

	free(fullPathU);
}

static void handleOldPlayMode(void)
{
	playMode = oldPlayMode;
	if (oldPlayMode != PLAYMODE_IDLE && oldPlayMode != PLAYMODE_EDIT)
		startPlaying(oldPlayMode, 0);

	songPlaying = (playMode >= PLAYMODE_SONG);
}

// called from input/video thread after module load thread was finished
void handleLoadMusicEvents(void)
{
	if (!moduleLoaded && !moduleFailedToLoad)
		return; // no event to handle

	if (moduleFailedToLoad)
	{
		// module failed to load from loading thread
		musicIsLoading = false;
		moduleFailedToLoad = false;
		moduleLoaded = false;
		editor.loadMusicEvent = EVENT_NONE;
		setMouseBusy(false);
		return;
	}

	if (moduleLoaded)
	{
		// module was successfully loaded from loading thread

		switch (editor.loadMusicEvent)
		{
			// module dragged and dropped *OR* user double clicked a file associated with FT2 clone
			case EVENT_LOADMUSIC_DRAGNDROP:
			{
				setupLoadedModule();
				if (editor.autoPlayOnDrop)
					startPlaying(PLAYMODE_SONG, 0);
				else
					handleOldPlayMode();
			}
			break;

			// filename passed as an exe argument *OR* user double clicked a file associated with FT2 clone
			case EVENT_LOADMUSIC_ARGV:
			{
				setupLoadedModule();
				startPlaying(PLAYMODE_SONG, 0);
			}
			break;

			// module filename pressed in Disk Op.
			case EVENT_LOADMUSIC_DISKOP:
			{
				setupLoadedModule();
				handleOldPlayMode();
			}
			break;

			default: break;
		}

		moduleLoaded = false;
		editor.loadMusicEvent = EVENT_NONE;
		musicIsLoading = false;
		mouseAnimOff();
	}
}
