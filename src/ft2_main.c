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
#include "scopes/ft2_scopes.h"
#include "scopes/ft2_scopedraw.h"
#include "ft2_about.h"
#include "ft2_pattern_ed.h"
#include "ft2_module_loader.h"
#include "ft2_sampling.h"
#include "ft2_audioselector.h"
#include "ft2_help.h"
#include "ft2_midi.h"
#include "ft2_events.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_hpc.h"
#include "ft2_smpfx.h"

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

#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL < 5
#pragma message("WARNING: The SDL2 dev lib is older than ver 2.0.5. You'll get fullscreen mode issues and no audio input sampling.")
#pragma message("At least version 2.0.7 is recommended.")
#endif

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
	SDL_EnableScreenSaver(); // allow screensaver to activate

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
		                "Required (compiled with) version: %d.%d.%d\n\n",
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

	// ALT+F4 is used in FT2, but is "close program" in some cases...
#if SDL_MINOR_VERSION >= 24 || (SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL >= 4)
	SDL_SetHint("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4", "1");
#endif

#ifdef _WIN32
#ifndef _MSC_VER
	SetProcessDPIAware();
#endif

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

	disableWasapi(); // disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)
	                 // 13.03.2020: This is still needed with SDL 2.0.12...
#endif

	/* SDL 2.0.9 for Windows has a serious bug where you need to initialize the joystick subsystem
	** (even if you don't use it) or else weird things happen like random stutters, keyboard (rarely) being
	** reinitialized in Windows and what not.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4391
	*/
#if defined _WIN32 && SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL == 9
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
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4166
	*/
	SDL_StopTextInput();

	hpc_Init();
	hpc_SetDurationInHz(&video.vblankHpc, VBLANK_HZ);

#ifdef __APPLE__
	osxSetDirToProgramDirFromArgs(argv);
#endif
	if (!setupExecutablePath() || !loadBMPs() || !setupMixerInterpolationTables())
	{
		cleanUpAndExit();
		return 1;
	}

	loadConfigOrSetDefaults(); // config must be loaded at this exact point

	if (!setupWindow() || !setupRenderer())
	{
		// error message was shown in the functions above
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

	if (!setupAudio(CONFIG_HIDE_ERRORS)) // can we open the audio device?
	{
		// nope, try with the default audio device
		setToDefaultAudioOutputDevice();

		if (!setupAudio(CONFIG_HIDE_ERRORS)) // does it work this time?
		{
			// nope, try safe values (44.1kHz 16-bit @ 1024 samples)
			config.audioFreq = 44100;
			config.specialFlags &= ~(BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_2048);
			config.specialFlags |=  (BITDEPTH_16 + BUFFSIZE_1024);

			if (!setupAudio(CONFIG_SHOW_ERRORS)) // this time it surely must work?!
			{
				cleanUpAndExit(); // well, nope!
				return 1;
			}
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
#ifdef __APPLE__
	// MIDI init can take several seconds on Mac, use thread
	midi.initMidiThread = SDL_CreateThread(initMidiFunc, NULL, NULL);
	if (midi.initMidiThread == NULL)
	{
		showErrorMsgBox("Couldn't create MIDI initialization thread!");
		cleanUpAndExit();
		return 1;
	}
#else
	initMidiFunc(NULL);
#endif
#endif

	hpc_ResetCounters(&video.vblankHpc); // quirk: this is needed for potential okBox() calls in handleModuleLoadFromArg()
	handleModuleLoadFromArg(argc, argv);

	editor.mainLoopOngoing = true;
	hpc_ResetCounters(&video.vblankHpc); // this must be the last thing we do before entering the main loop

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
	cpu.hasSSE = SDL_HasSSE();
	cpu.hasSSE2 = SDL_HasSSE2();

	// clear common structs
#ifdef HAS_MIDI
	memset(&midi, 0, sizeof (midi));
#endif
	memset(&video, 0, sizeof (video));
	memset(&keyb, 0, sizeof (keyb));
	memset(&mouse, 0, sizeof (mouse));
	memset(&editor, 0, sizeof (editor));
	memset((void *)&pattMark, 0, sizeof (pattMark));
	memset(&pattSync, 0, sizeof (pattSync));
	memset(&chSync, 0, sizeof (chSync));
	memset(&song, 0, sizeof (song));

	calcMiscReplayerVars();

	// used for scopes and sampling position line (sampler screen)
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		lastChInstr[i].instrNum = 255;
		lastChInstr[i].smpNum = 255;
	}

	// now set data that must be initialized to non-zero values...

	audio.locked = true; // XXX: Why..?
	audio.rescanAudioDevicesSupported = true;

	// set non-zero values

	editor.moduleSaveMode = MOD_SAVE_MODE_XM;
	editor.sampleSaveMode = SMP_SAVE_MODE_WAV;

	ui.sampleDataOrLoopDrag = -1;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;

	editor.editRowSkip = 1;
	editor.srcInstr = 1;
	editor.curInstr = 1;
	editor.curOctave = 4;
	editor.smpEd_NoteNr = 1+NOTE_C4;

	editor.ptnJumpPos[0] = 0x00;
	editor.ptnJumpPos[1] = 0x10;
	editor.ptnJumpPos[2] = 0x20;
	editor.ptnJumpPos[3] = 0x30;

	editor.copyMaskEnable = true;
	memset(editor.copyMask, 1, sizeof (editor.copyMask));
	memset(editor.pasteMask, 1, sizeof (editor.pasteMask));

	editor.diskOpReadOnOpen = true;

	audio.linearPeriodsFlag = true;

#ifdef HAS_MIDI
	midi.enable = true;
#endif

	editor.programRunning = true;
}

static void cleanUpAndExit(void) // never call this inside the main loop!
{
#ifdef HAS_MIDI
#ifdef __APPLE__
	// on Mac we used a thread to init MIDI (as it could take several seconds)
	if (midi.initMidiThread != NULL)
	{
		SDL_WaitThread(midi.initMidiThread, NULL);
		midi.initMidiThread = NULL;
	}
#endif
	midi.enable = false; // stop MIDI callback from doing things
	while (midi.callbackBusy) SDL_Delay(1); // wait for MIDI callback to finish

	closeMidiInDevice();
	freeMidiIn();
	freeMidiInputDeviceList();

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	if (editor.midiConfigFileLocationU != NULL)
	{
		free(editor.midiConfigFileLocationU);
		editor.midiConfigFileLocationU = NULL;
	}
#endif

	closeAudio();
	closeReplayer();
	closeVideo();
	freeSprites();
	freeDiskOp();
	clearCopyBuffer();
	clearSampleUndo();
	freeAudioDeviceSelectorBuffers();
	windUpFTHelp();
	freeTextBoxes();
	freeMouseCursors();
	freeBMPs();
	freeScopeIntrpLUT();

	if (editor.audioDevConfigFileLocationU != NULL)
	{
		free(editor.audioDevConfigFileLocationU);
		editor.audioDevConfigFileLocationU = NULL;
	}

	if (editor.configFileLocationU != NULL)
	{
		free(editor.configFileLocationU);
		editor.configFileLocationU = NULL;
	}

	if (editor.binaryPathU != NULL)
	{
		free(editor.binaryPathU);
		editor.binaryPathU = NULL;
	}

#ifdef _WIN32
	closeSingleInstancing();
#endif

	SDL_Quit();
}

#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv)
{
	/* OS X/macOS: hackish way of setting the current working directory to the place where we double clicked
	** on the icon (for FT2.CFG loading)
	*/

	// if we launched from the terminal, argv[0][0] would be '.'
	if (argv[0] != NULL && argv[0][0] == DIR_DELIMITER) // don't do the hack if we launched from the terminal
	{
		char *tmpPath = strdup(argv[0]);
		if (tmpPath != NULL)
		{
			// cut off program filename
			int32_t tmpPathLen = strlen(tmpPath);
			for (int32_t i = tmpPathLen - 1; i >= 0; i--)
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

#ifdef _WIN32
static void disableWasapi(void)
{
	// disable problematic WASAPI SDL2 audio driver on Windows (causes clicks/pops sometimes...)

	const int32_t numAudioDrivers = SDL_GetNumAudioDrivers();
	if (numAudioDrivers <= 1)
		return;

	// look for directsound and enable it if found
	for (int32_t i = 0; i < numAudioDrivers; i++)
	{
		const char *audioDriver = SDL_GetAudioDriver(i);
		if (audioDriver != NULL && strcmp("directsound", audioDriver) == 0)
		{
			SDL_setenv("SDL_AUDIODRIVER", "directsound", true);
			audio.rescanAudioDevicesSupported = false;
			return;
		}
	}

	// directsound is not available, try winmm
	for (int32_t i = 0; i < numAudioDrivers; i++)
	{
		const char *audioDriver = SDL_GetAudioDriver(i);
		if (audioDriver != NULL && strcmp("winmm", audioDriver) == 0)
		{
			SDL_setenv("SDL_AUDIODRIVER", "winmm", true);
			audio.rescanAudioDevicesSupported = false;
			return;
		}
	}

	// we didn't find directsound or winmm, let's use wasapi after all...
}
#endif
