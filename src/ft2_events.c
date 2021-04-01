// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#else
#include <limits.h>
#include <signal.h>
#include <unistd.h> // chdir()
#endif
#include <stdio.h>
#include <sys/stat.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_diskop.h"
#include "ft2_module_loader.h"
#include "ft2_module_saver.h"
#include "ft2_sample_loader.h"
#include "ft2_mouse.h"
#include "ft2_midi.h"
#include "ft2_video.h"
#include "ft2_trim.h"
#include "ft2_inst_ed.h"
#include "ft2_sampling.h"
#include "ft2_textboxes.h"
#include "ft2_sysreqs.h"
#include "ft2_keyboard.h"
#include "ft2_sample_ed.h"
#include "ft2_sample_ed_features.h"
#include "ft2_structs.h"

#define CRASH_TEXT "Oh no!\nThe Fasttracker II clone has crashed...\n\nA backup .xm was hopefully " \
                   "saved to the current module directory.\n\nPlease report this bug if you can.\n" \
                   "Try to mention what you did before the crash happened.\n" \
                   "My email can be found at the bottom of 16-bits.org."

static bool backupMadeAfterCrash;

#ifdef _WIN32
#define SYSMSG_FILE_ARG (WM_USER+1)
#define ARGV_SHARED_MEM_MAX_LEN ((PATH_MAX+1) * sizeof (WCHAR))
#define SHARED_HWND_NAME TEXT("Local\\FT2CloneHwnd")
#define SHARED_FILENAME TEXT("Local\\FT2CloneFilename")
static HWND hWnd;
static HANDLE oneInstHandle, hMapFile;
static LPCTSTR sharedMemBuf;

// used for Windows usleep() implementation
static NTSTATUS (__stdcall *NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval);
#endif

static void handleInput(void);

// usleep() implementation for Windows (Warning: This might not be future-safe!)
#ifdef _WIN32
void usleep(uint32_t usec)
{
	LARGE_INTEGER lpDueTime;

	if (NtDelayExecution == NULL)
	{
		// NtDelayExecution() is not available (shouldn't happen), use regular sleep()
		Sleep(usec / 1000);
	}
	else
	{
		// this prevents a 64-bit MUL (will not overflow with the ranges we use anyway)
		lpDueTime.HighPart = UINT32_MAX;
		lpDueTime.LowPart = (DWORD)(-10 * (int32_t)usec);

		NtDelayExecution(false, &lpDueTime);
	}
}

void setupWin32Usleep(void)
{
	NtDelayExecution = (NTSTATUS (__stdcall *)(BOOL, PLARGE_INTEGER))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
	timeBeginPeriod(0); // enter highest timer resolution
}

void freeWin32Usleep(void)
{
	timeEndPeriod(0); // exit highest timer resolution
}
#endif

void readInput(void)
{
	readMouseXY();
	readKeyModifiers();
	setSyncedReplayerVars();
	handleInput();
}

void handleThreadEvents(void)
{
	if (okBoxData.active)
	{
		okBoxData.returnData = okBox(okBoxData.typ, okBoxData.headline, okBoxData.text);
		okBoxData.active = false;
	}
}

void handleEvents(void)
{
#ifdef HAS_MIDI
	// called after MIDI has been initialized
	if (midi.rescanDevicesFlag)
	{
		midi.rescanDevicesFlag = false;

		rescanMidiInputDevices();
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
			drawMidiInputList();
	}
#endif

	if (editor.trimThreadWasDone)
	{
		editor.trimThreadWasDone = false;
		trimThreadDone();
	}

	if (editor.updateCurSmp)
	{
		editor.updateCurSmp = false;

		updateNewInstrument();
		updateNewSample();

		diskOpSetFilename(DISKOP_ITEM_SAMPLE, editor.tmpFilenameU);

		removeSampleIsLoadingFlag();
		setMouseBusy(false);
	}

	if (editor.updateCurInstr)
	{
		editor.updateCurInstr = false;

		updateNewInstrument();
		updateNewSample();

		diskOpSetFilename(DISKOP_ITEM_INSTR, editor.tmpInstrFilenameU);
		setMouseBusy(false);
	}

	// some Disk Op. stuff

	if (editor.diskOpReadDir)
	{
		editor.diskOpReadDir = false;
		diskOp_StartDirReadThread();
	}

	if (editor.diskOpReadDone)
	{
		editor.diskOpReadDone = false;
		if (ui.diskOpShown)
			diskOp_DrawDirectory();
	}

	handleLoadMusicEvents();

	if (editor.samplingAudioFlag) handleSamplingUpdates();
	if (ui.setMouseBusy) mouseAnimOn();
	if (ui.setMouseIdle) mouseAnimOff();

	if (editor.updateWindowTitle)
	{
		editor.updateWindowTitle = false;
		updateWindowTitle(false);
	}
}

// Windows specific routines
#ifdef _WIN32
static bool instanceAlreadyOpen(void)
{
	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_HWND_NAME);
	if (hMapFile != NULL)
		return true; // another instance is already open

	// no instance is open, let's created a shared memory file with hWnd in it
	oneInstHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (HWND), SHARED_HWND_NAME);
	if (oneInstHandle != NULL)
	{
		sharedMemBuf = (LPTSTR)MapViewOfFile(oneInstHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (HWND));
		if (sharedMemBuf != NULL)
		{
			CopyMemory((PVOID)sharedMemBuf, &video.hWnd, sizeof (HWND));
			UnmapViewOfFile(sharedMemBuf);
			sharedMemBuf = NULL;
		}
	}

	return false;
}

bool handleSingleInstancing(int32_t argc, char **argv)
{
	SDL_SysWMinfo wmInfo;

	SDL_VERSION(&wmInfo.version);
	if (!SDL_GetWindowWMInfo(video.window, &wmInfo))
		return false;

	video.hWnd = wmInfo.info.win.window;
	if (instanceAlreadyOpen() && argc >= 2 && argv[1][0] != '\0')
	{
		sharedMemBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (HWND));
		if (sharedMemBuf != NULL)
		{
			memcpy(&hWnd, sharedMemBuf, sizeof (HWND));

			UnmapViewOfFile(sharedMemBuf);
			sharedMemBuf = NULL;
			CloseHandle(hMapFile);
			hMapFile = NULL;

			hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, ARGV_SHARED_MEM_MAX_LEN, SHARED_FILENAME);
			if (hMapFile != NULL)
			{
				sharedMemBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, ARGV_SHARED_MEM_MAX_LEN);
				if (sharedMemBuf != NULL)
				{
					strcpy((char *)sharedMemBuf, argv[1]);

					UnmapViewOfFile(sharedMemBuf);
					sharedMemBuf = NULL;

					SendMessage(hWnd, SYSMSG_FILE_ARG, 0, 0);
					Sleep(80); // wait a bit to make sure first instance received msg

					CloseHandle(hMapFile);
					hMapFile = NULL;

					return true; // quit instance now
				}
			}

			return true;
		}

		CloseHandle(hMapFile);
		hMapFile = NULL;
	}

	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	return false;
}

static void handleSysMsg(SDL_Event inputEvent)
{
	if (inputEvent.type != SDL_SYSWMEVENT)
		return;

	SDL_SysWMmsg *wmMsg = inputEvent.syswm.msg;
	if (wmMsg->subsystem == SDL_SYSWM_WINDOWS && wmMsg->msg.win.msg == SYSMSG_FILE_ARG)
	{
		hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, SHARED_FILENAME);
		if (hMapFile != NULL)
		{
			sharedMemBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, ARGV_SHARED_MEM_MAX_LEN);
			if (sharedMemBuf != NULL)
			{
				editor.autoPlayOnDrop = true;
				loadDroppedFile((char *)sharedMemBuf, true);

				UnmapViewOfFile(sharedMemBuf);
				sharedMemBuf = NULL;
			}

			CloseHandle(hMapFile);
			hMapFile = NULL;
		}
	}
}

void closeSingleInstancing(void)
{
	if (oneInstHandle != NULL)
	{
		CloseHandle(oneInstHandle);
		oneInstHandle = NULL;
	}
}

static LONG WINAPI exceptionHandler(EXCEPTION_POINTERS *ptr)
{
#define BACKUP_FILES_TO_TRY 1000
	char fileName[32];
	struct stat statBuffer;

	if (oneInstHandle != NULL)
		CloseHandle(oneInstHandle);

	if (!backupMadeAfterCrash)
	{
		if (getDiskOpModPath() != NULL && UNICHAR_CHDIR(getDiskOpModPath()) == 0)
		{
			// find a free filename
			int32_t i;
			for (i = 1; i < BACKUP_FILES_TO_TRY; i++)
			{
				sprintf(fileName, "backup%03d.xm", (int32_t)i);
				if (stat(fileName, &statBuffer) != 0)
					break; // filename OK
			}

			if (i != BACKUP_FILES_TO_TRY)
			{
				UNICHAR *fileNameU = cp437ToUnichar(fileName);
				if (fileNameU != NULL)
				{
					saveXM(fileNameU);
					free(fileNameU);
				}
			}
		}

		backupMadeAfterCrash = true; // set this flag to prevent multiple backups from being saved at once
		showErrorMsgBox(CRASH_TEXT);
	}

	return EXCEPTION_CONTINUE_SEARCH;

	(void)ptr;
}
#else
static void exceptionHandler(int32_t signal)
{
#define BACKUP_FILES_TO_TRY 1000
	char fileName[32];
	struct stat statBuffer;

	if (signal == 15)
		return;

	if (!backupMadeAfterCrash)
	{
		if (getDiskOpModPath() != NULL && UNICHAR_CHDIR(getDiskOpModPath()) == 0)
		{
			// find a free filename
			int32_t i;
			for (i = 1; i < BACKUP_FILES_TO_TRY; i++)
			{
				sprintf(fileName, "backup%03d.xm", i);
				if (stat(fileName, &statBuffer) != 0)
					break; // filename OK
			}

			if (i != BACKUP_FILES_TO_TRY)
			{
				UNICHAR *fileNameU = cp437ToUnichar(fileName);
				if (fileNameU != NULL)
				{
					saveXM(fileNameU);
					free(fileNameU);
				}
			}
		}

		backupMadeAfterCrash = true; // set this flag to prevent multiple backups from being saved at once
		showErrorMsgBox(CRASH_TEXT);
	}
}
#endif

void setupCrashHandler(void)
{
#ifndef _DEBUG
#ifdef _WIN32
	SetUnhandledExceptionFilter(exceptionHandler);
#else
	struct sigaction act;
	struct sigaction oldAct;

	memset(&act, 0, sizeof (act));
	act.sa_handler = exceptionHandler;
	act.sa_flags = SA_RESETHAND;

	sigaction(SIGILL | SIGABRT | SIGFPE | SIGSEGV, &act, &oldAct);
	sigaction(SIGILL, &act, &oldAct);
	sigaction(SIGABRT, &act, &oldAct);
	sigaction(SIGFPE, &act, &oldAct);
	sigaction(SIGSEGV, &act, &oldAct);
#endif
#endif
}

void handleWaitVblQuirk(SDL_Event *event)
{
	if (event->type == SDL_WINDOWEVENT)
	{
		if (event->window.event == SDL_WINDOWEVENT_HIDDEN)
			video.windowHidden = true;
		else if (event->window.event == SDL_WINDOWEVENT_SHOWN)
			video.windowHidden = false;

		if (video.vsync60HzPresent)
		{
			/* If we minimize the window and vsync is present, vsync is temporarily turned off.
			** recalc waitVBL() vars so that it can sleep properly in said mode.
			*/
			if (event->window.event == SDL_WINDOWEVENT_MINIMIZED ||
				event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
			{
				setupWaitVBL();
			}
		}
	}
}

static void handleInput(void)
{
	SDL_Event event;

	if (!editor.busy)
		handleLastGUIObjectDown(); // this should be handled before main input poll (on next frame)

	while (SDL_PollEvent(&event))
	{
		handleWaitVblQuirk(&event);

		if (editor.busy)
		{
			const uint32_t eventType = event.type;
			const SDL_Scancode key = event.key.keysym.scancode;

			/* The Echo tool in Smp. Ed. can take forever if abused, let
			** mouse buttons/ESC/SIGTERM force-stop it.
			*/
			if (eventType == SDL_MOUSEBUTTONDOWN || eventType == SDL_QUIT ||
				(eventType == SDL_KEYUP && key == SDL_SCANCODE_ESCAPE))
			{
				handleEchoToolPanic();
			}

			// let certain mouse buttons or keyboard keys stop certain events
			if (eventType == SDL_MOUSEBUTTONDOWN ||
				(eventType == SDL_KEYDOWN && key != SDL_SCANCODE_MUTE &&
				 key != SDL_SCANCODE_AUDIOMUTE && key != SDL_SCANCODE_VOLUMEDOWN &&
				 key != SDL_SCANCODE_VOLUMEUP))
			{
				// only let keyboard keys interrupt audio sampling
				if (editor.samplingAudioFlag && eventType != SDL_MOUSEBUTTONDOWN)
					stopSampling();

				editor.wavIsRendering = false;
			}

			continue; // another thread is busy with something, drop input
		}

#ifdef _WIN32
		handleSysMsg(event);
#endif
		// text input when editing texts
		if (event.type == SDL_TEXTINPUT)
		{
			if (editor.editTextFlag)
			{
				if (keyb.ignoreTextEditKey)
				{
					keyb.ignoreTextEditKey = false;
					continue;
				}

				char *inputText = utf8ToCp437(event.text.text, false);
				if (inputText != NULL)
				{
					if (inputText[0] != '\0')
						handleTextEditInputChar(inputText[0]);

					free(inputText);
				}
			}
		}
		else if (event.type == SDL_MOUSEWHEEL)
		{
			if (event.wheel.y > 0)
				mouseWheelHandler(MOUSE_WHEEL_UP);
			else if (event.wheel.y < 0)
				mouseWheelHandler(MOUSE_WHEEL_DOWN);
		}
		else if (event.type == SDL_DROPFILE)
		{
			editor.autoPlayOnDrop = false;
			loadDroppedFile(event.drop.file, true);
			SDL_free(event.drop.file);
		}
		else if (event.type == SDL_QUIT)
		{
			if (ui.sysReqShown)
				continue;

			if (editor.editTextFlag)
				exitTextEditing();

			if (!song.isModified)
			{
				editor.throwExit = true;
			}
			else
			{
				if (!video.fullscreen)
				{
					// de-minimize window and set focus so that the user sees the message box
					SDL_RestoreWindow(video.window);
					SDL_RaiseWindow(video.window);
				}

				if (quitBox(true) == 1)
					editor.throwExit = true;
			}
		}
		else if (event.type == SDL_KEYUP)
		{
			keyUpHandler(event.key.keysym.scancode, event.key.keysym.sym);
		}
		else if (event.type == SDL_KEYDOWN)
		{
			keyDownHandler(event.key.keysym.scancode, event.key.keysym.sym, event.key.repeat);
		}
		else if (event.type == SDL_MOUSEBUTTONUP)
		{
			mouseButtonUpHandler(event.button.button);
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN)
		{
			mouseButtonDownHandler(event.button.button);
		}

		if (editor.throwExit)
			editor.programRunning = false;
	}

#ifdef HAS_MIDI
	// MIDI vibrato
	const uint8_t vibDepth = (midi.currMIDIVibDepth >> 9) & 0x0F;
	if (vibDepth > 0)
		recordMIDIEffect(0x04, 0xA0 | vibDepth);
#endif
}
