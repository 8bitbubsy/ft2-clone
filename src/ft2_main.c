// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <math.h> // modf()
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#else
#include <unistd.h> // chdir()
#endif
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_keyboard.h"
#include "ft2_config.h"
#include "ft2_sample_ed.h"
#include "ft2_diskop.h"
#include "ft2_scopes.h"
#include "ft2_about.h"
#include "ft2_pattern_ed.h"
#include "ft2_module_loader.h"
#include "ft2_sampling.h"
#include "ft2_audioselector.h"
#include "ft2_help.h"
#include "ft2_midi.h"
#include "ft2_events.h"

#ifdef HAS_MIDI
static SDL_Thread *initMidiThread;
#endif

static void setupPerfFreq(void);
static void initializeVars(void);
static void cleanUpAndExit(void); // never call this inside the main loop
#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv);
#endif

#ifdef _WIN32
static void disableWasapi(void);
#endif

int main(int argc, char *argv[])
{
#if defined _WIN32 || defined __APPLE__
	SDL_version sdlVer;
#endif

	// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#if SDL_PATCHLEVEL < 5
#pragma message("WARNING: The SDL2 dev lib is older than ver 2.0.5. You'll get fullscreen mode issues and no audio input sampling.")
#pragma message("At least version 2.0.7 is recommended.")
#endif

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

	initializeVars();
	setupCrashHandler();

	// on Windows and macOS, test what version SDL2.DLL is (against library version used in compilation)
#if defined _WIN32 || defined __APPLE__
	SDL_GetVersion(&sdlVer);
	if (sdlVer.major != SDL_MAJOR_VERSION || sdlVer.minor != SDL_MINOR_VERSION || sdlVer.patch != SDL_PATCHLEVEL)
	{
#ifdef _WIN32
		showErrorMsgBox("SDL2.dll is not the expected version, the program will terminate.\n\n" \
		                "Loaded dll version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d\n\n" \
		                "The needed SDL2.dll is located in the .zip from 16-bits.org/ft2.php\n",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#else
		showErrorMsgBox("The loaded SDL2 library is not the expected version, the program will terminate.\n\n" \
		                "Loaded library version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#endif
		return 0;
	}
#endif

#if SDL_PATCHLEVEL >= 4 // SDL 2.0.4 or later
	SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1"); // windows only - prevent ALT+F4 from exiting (FT2 uses ALT+F4)
#endif

#ifdef _WIN32
	if (!cpu.hasSSE)
	{
		showErrorMsgBox("Your computer's processor doesn't have the SSE instruction set\n" \
		                "which is needed for this program to run. Sorry!");
		return 0;
	}

	if (!cpu.hasSSE2)
	{
		showErrorMsgBox("Your computer's processor doesn't have the SSE2 instruction set\n" \
		                "which is needed for this program to run. Sorry!");
		return 0;
	}

	setupWin32Usleep();
	disableWasapi(); // disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)
#endif

	/* SDL 2.0.9 for Windows has a serious bug where you need to initialize the joystick subsystem
	** (even if you don't use it) or else weird things happen like random stutters, keyboard (rarely) being
	** reinitialized in Windows and what not.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4391 */
#if defined _WIN32 && SDL_PATCHLEVEL == 9
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
#else
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0)
#endif
	{
		showErrorMsgBox("Couldn't initialize SDL:\n%s", SDL_GetError());
		return 1;
	}
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	/* Text input is started by default in SDL2, turn it off to remove ~2ms spikes per key press.
	** We manuallay start it again when a text edit box is activated, and stop it when done.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4166 */
	SDL_StopTextInput();

#ifdef __APPLE__
	osxSetDirToProgramDirFromArgs(argv);
#endif
	UNICHAR_GETCWD(editor.binaryPathU, PATH_MAX);

	loadConfigOrSetDefaults();
	if (!setupWindow() || !setupRenderer())
	{
		cleanUpAndExit();
		return 1;
	}

#ifdef _WIN32
	// allow only one instance, and send arguments to it (what song to play)
	if (handleSingleInstancing(argc, argv))
	{
		cleanUpAndExit();
		return 0; // close current instance, the main instance got a message now
	}
#endif

	if (!setupDiskOp())
	{
		cleanUpAndExit();
		return 1;
	}

	audio.currOutputDevice = getAudioOutputDeviceFromConfig();
	audio.currInputDevice = getAudioInputDeviceFromConfig();

	setupPerfFreq();

	if (!setupAudio(CONFIG_HIDE_ERRORS))
	{
		// one LAST attempt (with default audio device and settings)
#ifdef __APPLE__
		config.audioFreq = 44100;
#else
		config.audioFreq = 48000;
#endif
		// try 16-bit audio at 1024 samples (44.1kHz/48kHz)
		config.specialFlags &= ~(BITDEPTH_24 + BUFFSIZE_512 + BUFFSIZE_2048);
		config.specialFlags |=  (BITDEPTH_16 + BUFFSIZE_1024);

		setToDefaultAudioOutputDevice();
		if (!setupAudio(CONFIG_SHOW_ERRORS))
		{
			cleanUpAndExit(); // still no go...
			return 1;
		}
	}

	if (!setupReplayer() || !setupGUI() || !initScopes())
	{
		cleanUpAndExit();
		return 1;
	}

	pauseAudio();
	resumeAudio();
	rescanAudioDevices();

#ifdef _WIN32 // on Windows we show the window at this point
	SDL_ShowWindow(video.window);
#endif

	if (config.windowFlags & START_IN_FULLSCR)
	{
		video.fullscreen = true;
		enterFullscreen();
	}

#ifdef HAS_MIDI
	// set up MIDI input (in a thread because it can take quite a while on f.ex. macOS)
	initMidiThread = SDL_CreateThread(initMidiFunc, NULL, NULL);
	if (initMidiThread == NULL)
	{
		showErrorMsgBox("Couldn't create MIDI initialization thread!");
		cleanUpAndExit();
		return 1;
	}
	SDL_DetachThread(initMidiThread); // don't wait for this thread, let it clean up when done
#endif

	setupWaitVBL();
	handleModuleLoadFromArg(argc, argv);

	editor.mainLoopOngoing = true;
	while (editor.programRunning)
	{
		beginFPSCounter();
		handleThreadEvents();
		readInput();
		handleEvents();
		handleRedrawing();
		flipFrame();
		endFPSCounter();
	}

	if (config.cfg_AutoSave)
		saveConfig(CONFIG_HIDE_ERRORS);

	cleanUpAndExit();
	return 0;
}

static void initializeVars(void)
{
	int32_t i;

	cpu.hasSSE = SDL_HasSSE();
	cpu.hasSSE2 = SDL_HasSSE2();

	// clear common structs
	memset(&video, 0, sizeof (video));
	memset(&keyb, 0, sizeof (keyb));
	memset(&mouse, 0, sizeof (mouse));
	memset(&editor, 0, sizeof (editor));
	memset(&pattMark, 0, sizeof (pattMark));
	memset(&pattSync, 0, sizeof (pattSync));
	memset(&chSync, 0, sizeof (chSync));
	memset(&song, 0, sizeof (song));

	for (i = 0; i < MAX_VOICES; i++)
	{
		lastChInstr[i].instrNr = 255;
		lastChInstr[i].sampleNr = 255;
	}

	// now set data that must be initialized to non-zero values...

	audio.locked = true;
	audio.rescanAudioDevicesSupported = true;

	strcpy(editor.ui.fullscreenButtonText, "Go fullscreen");

	// set non-zero values

	editor.moduleSaveMode = MOD_SAVE_MODE_XM;
	editor.sampleSaveMode = SMP_SAVE_MODE_WAV;

	editor.ui.sampleDataOrLoopDrag = -1;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;

	editor.ID_Add = 1;
	editor.srcInstr = 1;
	editor.curInstr = 1;
	editor.curOctave = 4;
	editor.smpEd_NoteNr = 48 + 1; // middle-C

	editor.ptnJumpPos[0] = 0x00;
	editor.ptnJumpPos[1] = 0x10;
	editor.ptnJumpPos[2] = 0x20;
	editor.ptnJumpPos[3] = 0x30;

	editor.copyMaskEnable = true;
	memset(editor.copyMask, 1, sizeof (editor.copyMask));
	memset(editor.pasteMask, 1, sizeof (editor.pasteMask));

#ifdef HAS_MIDI
	midi.enable = true;
#endif

	editor.diskOpReadOnOpen = true;
	editor.programRunning = true;
}

static void cleanUpAndExit(void) // never call this inside the main loop!
{
#ifdef HAS_MIDI
	if (midi.closeMidiOnExit)
	{
		closeMidiInDevice();
		freeMidiIn();
	}
#endif

	closeAudio();
	closeReplayer();
	closeVideo();
	freeSprites();
	freeDiskOp();
	clearCopyBuffer();
	freeAudioDeviceSelectorBuffers();
#ifdef HAS_MIDI
	freeMidiInputDeviceList();
#endif
	windUpFTHelp();
	freeTextBoxes();
	freeMouseCursors();

#ifdef HAS_MIDI
	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}
#endif

	if (editor.audioDevConfigFileLocation != NULL)
	{
		free(editor.audioDevConfigFileLocation);
		editor.audioDevConfigFileLocation = NULL;
	}

	if (editor.configFileLocation != NULL)
	{
		free(editor.configFileLocation);
		editor.configFileLocation = NULL;
	}

	if (editor.midiConfigFileLocation != NULL)
	{
		free(editor.midiConfigFileLocation);
		editor.midiConfigFileLocation = NULL;
	}

#ifdef _WIN32
	freeWin32Usleep();
	closeSingleInstancing();
#endif

	SDL_Quit();
}

#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv)
{
	char *tmpPath;
	int32_t i, tmpPathLen;

	/* OS X/macOS: hackish way of setting the current working directory to the place where we double clicked
	** on the icon (for protracker.ini loading)
	*/

	// if we launched from the terminal, argv[0][0] would be '.'
	if (argv[0] != NULL && argv[0][0] == DIR_DELIMITER) // don't do the hack if we launched from the terminal
	{
		tmpPath = strdup(argv[0]);
		if (tmpPath != NULL)
		{
			// cut off program filename
			tmpPathLen = strlen(tmpPath);
			for (i = tmpPathLen - 1; i >= 0; i--)
			{
				if (tmpPath[i] == DIR_DELIMITER)
				{
					tmpPath[i] = '\0';
					break;
				}
			}

			chdir(tmpPath); // path to binary
			chdir("../../../"); // we should now be in the directory where the config can be

			free(tmpPath);
		}
	}
}
#endif

static void setupPerfFreq(void)
{
	uint64_t perfFreq64;
	double dInt, dFrac;

	perfFreq64 = SDL_GetPerformanceFrequency(); assert(perfFreq64 != 0);
	editor.dPerfFreq = (double)perfFreq64;
	editor.dPerfFreqMulMicro = 1000000.0 / editor.dPerfFreq;
	editor.dPerfFreqMulMs = 1.0 / (editor.dPerfFreq / 1000.0);

	// calculate vblank time for performance counters and split into int/frac
	dFrac = modf(editor.dPerfFreq / VBLANK_HZ, &dInt);

	// integer part
	video.vblankTimeLen = (uint32_t)dInt;

	// fractional part scaled to 0..2^32-1
	dFrac *= UINT32_MAX;
	video.vblankTimeLenFrac = (uint32_t)(dFrac + 0.5);
}

#ifdef _WIN32
static void disableWasapi(void)
{
	const char *audioDriver;
	int32_t i, numAudioDrivers;

	// disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)

	numAudioDrivers = SDL_GetNumAudioDrivers();
	for (i = 0; i < numAudioDrivers; i++)
	{
		audioDriver = SDL_GetAudioDriver(i);
		if (audioDriver != NULL && strcmp("directsound", audioDriver) == 0)
		{
			SDL_setenv("SDL_AUDIODRIVER", "directsound", true);
			audio.rescanAudioDevicesSupported = false;
			break;
		}
	}

	if (i == numAudioDrivers)
	{
		// directsound is not available, try winmm
		for (i = 0; i < numAudioDrivers; i++)
		{
			audioDriver = SDL_GetAudioDriver(i);
			if (audioDriver != NULL && strcmp("winmm", audioDriver) == 0)
			{
				SDL_setenv("SDL_AUDIODRIVER", "winmm", true);
				audio.rescanAudioDevicesSupported = false;
				break;
			}
		}
	}

	// maybe we didn't find directsound or winmm, let's use wasapi after all then...
}
#endif
