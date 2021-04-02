// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_audioselector.h"
#include "ft2_structs.h"

enum
{
	INPUT_DEVICE = 0,
	OUTPUT_DEVICE = 1
};

#define MAX_DEV_STR_LEN 256

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

static char *getReasonableAudioDevice(int32_t iscapture) // can and will return NULL
{
	int32_t numAudioDevs = SDL_GetNumAudioDevices(iscapture);
	if (numAudioDevs == 0 || numAudioDevs > 1)
		return NULL; // we don't know which audio output device is the default device

	const char *devName = SDL_GetAudioDeviceName(0, iscapture);
	if (devName == NULL)
		return NULL;

	return strdup(devName);
}

char *getAudioOutputDeviceFromConfig(void)
{
	bool audioDeviceRead = false;
	char *devString = NULL;

	FILE *f = UNICHAR_FOPEN(editor.audioDevConfigFileLocationU, "r");
	if (f != NULL)
	{
		devString = (char *)malloc(MAX_DEV_STR_LEN+1);
		if (devString == NULL)
		{
			fclose(f);
			return NULL; // out of memory
		}

		devString[0] = '\0';
		fgets(devString, MAX_DEV_STR_LEN, f);
		fclose(f);

		const int32_t devStringLen = (int32_t)strlen(devString);
		if (devStringLen > 0)
		{
			if (devString[devStringLen-1] == '\n')
				devString[devStringLen-1] = '\0';

			if (!(devStringLen == 1 && devString[0] == ' ')) // space only = no device
				audioDeviceRead = true;
		}
	}

	if (!audioDeviceRead)
	{
		if (devString != NULL)
			free(devString);

		devString = getReasonableAudioDevice(OUTPUT_DEVICE);
	}

	// SDL_OpenAudioDevice() doesn't seem to like an empty audio device string
	if (devString != NULL && devString[0] == '\0')
	{
		free(devString);
		return NULL;
	}

	return devString;
}

char *getAudioInputDeviceFromConfig(void)
{
	bool audioDeviceRead = false;
	char *devString = NULL;

	FILE *f = UNICHAR_FOPEN(editor.audioDevConfigFileLocationU, "r");
	if (f != NULL)
	{
		devString = (char *)malloc(MAX_DEV_STR_LEN+1);
		if (devString == NULL)
		{
			fclose(f);
			return NULL; // out of memory
		}

		devString[0] = '\0';
		fgets(devString, MAX_DEV_STR_LEN, f); // skip first line (we want the input device)
		fgets(devString, MAX_DEV_STR_LEN, f);
		fclose(f);

		const int32_t devStringLen = (int32_t)strlen(devString);
		if (devStringLen > 0)
		{
			if (devString[devStringLen-1] == '\n')
				devString[devStringLen-1] = '\0';

			if (!(devStringLen == 1 && devString[0] == ' ')) // space only = no device
				audioDeviceRead = true;
		}
	}

	if (!audioDeviceRead)
	{
		if (devString != NULL)
			free(devString);

		devString = getReasonableAudioDevice(INPUT_DEVICE);
	}

	// SDL_OpenAudioDevice() doesn't seem to like an empty audio device string
	if (devString != NULL && devString[0] == '\0')
	{
		free(devString);
		return NULL;
	}

	return devString;
}

bool saveAudioDevicesToConfig(const char *outputDevice, const char *inputDevice)
{
	FILE *f = UNICHAR_FOPEN(editor.audioDevConfigFileLocationU, "w");
	if (f == NULL)
		return false;

	if (outputDevice != NULL)
		fputs(outputDevice, f);
	else
		fputc(' ', f);

	fputc('\n', f);

	if (inputDevice != NULL)
		fputs(inputDevice, f);
	else
		fputc(' ', f);

	fclose(f);
	return true;
}

void drawAudioOutputList(void)
{
	clearRect(114, 18, AUDIO_SELECTORS_BOX_WIDTH, 66);

	if (audio.outputDeviceNum == 0)
	{
		textOut(114, 18, PAL_FORGRND, "No audio output devices found!");
		return;
	}

	for (int32_t i = 0; i < 6; i++)
	{
		const int32_t deviceEntry = getScrollBarPos(SB_AUDIO_OUTPUT_SCROLL) + i;
		if (deviceEntry < audio.outputDeviceNum)
		{
			if (audio.outputDeviceNames[deviceEntry] == NULL)
				continue;

			const uint16_t y = 18 + (uint16_t)(i * 11);

			if (audio.currOutputDevice != NULL)
			{
				if (strcmp(audio.currOutputDevice, audio.outputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, AUDIO_SELECTORS_BOX_WIDTH, 10, PAL_BOXSLCT); // selection background color
			}

			char *tmpString = utf8ToCp437(audio.outputDeviceNames[deviceEntry], true);
			if (tmpString != NULL)
			{
				textOutClipX(114, y, PAL_FORGRND, tmpString, 114 + AUDIO_SELECTORS_BOX_WIDTH);
				free(tmpString);
			}
		}
	}
}

void drawAudioInputList(void)
{
	clearRect(114, 105, AUDIO_SELECTORS_BOX_WIDTH, 44);

	if (audio.inputDeviceNum == 0)
	{
		textOut(114, 105, PAL_FORGRND, "No audio input devices found!");
		return;
	}

	for (int32_t i = 0; i < 4; i++)
	{
		const int32_t deviceEntry = getScrollBarPos(SB_AUDIO_INPUT_SCROLL) + i;
		if (deviceEntry < audio.inputDeviceNum)
		{
			if (audio.inputDeviceNames[deviceEntry] == NULL)
				continue;

			const uint16_t y = 105 + (uint16_t)(i * 11);

			if (audio.currInputDevice != NULL)
			{
				if (strcmp(audio.currInputDevice, audio.inputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, AUDIO_SELECTORS_BOX_WIDTH, 10, PAL_BOXSLCT); // selection background color
			}

			char *tmpString = utf8ToCp437(audio.inputDeviceNames[deviceEntry], true);
			if (tmpString != NULL)
			{
				textOutClipX(114, y, PAL_FORGRND, tmpString, 114 + AUDIO_SELECTORS_BOX_WIDTH);
				free(tmpString);
			}
		}
	}
}

bool testAudioDeviceListsMouseDown(void)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_IO_DEVICES)
		return false;

	const int32_t mx = mouse.x;
	const int32_t my = mouse.y;

	if (my < 18 || my > 149 || mx < 114 || mx >= 114+AUDIO_SELECTORS_BOX_WIDTH)
		return false;

	if (my < 84)
	{
		// output device list

		const int32_t deviceNum = (int32_t)scrollBars[SB_AUDIO_OUTPUT_SCROLL].pos + ((my - 18) / 11);
		if (audio.outputDeviceNum <= 0 || deviceNum >= audio.outputDeviceNum)
			return true;

		char *devString = audio.outputDeviceNames[deviceNum];
		if (devString != NULL && (audio.currOutputDevice == NULL || strcmp(audio.currOutputDevice, devString) != 0))
		{
			if (audio.currOutputDevice != NULL)
				free(audio.currOutputDevice);

			const uint32_t devStringLen = (uint32_t)strlen(devString);

			audio.currOutputDevice = (char *)malloc(devStringLen+1);
			if (audio.currOutputDevice == NULL)
				return true;

			audio.currOutputDevice[0] = '\0';

			if (devStringLen > 0)
				strcpy(audio.currOutputDevice, devString);

			if (!setNewAudioSettings())
				okBox(0, "System message", "Couldn't open audio input device!");
			else
				drawAudioOutputList();
		}

		return true;
	}
	else if (my >= 105)
	{
		// input device list

		const int32_t deviceNum = (int32_t)scrollBars[SB_AUDIO_INPUT_SCROLL].pos + ((my - 105) / 11);
		if (audio.inputDeviceNum <= 0 || deviceNum >= audio.inputDeviceNum)
			return true;

		char *devString = audio.inputDeviceNames[deviceNum];
		if (devString != NULL && (audio.currInputDevice == NULL || strcmp(audio.currInputDevice, devString) != 0))
		{
			if (audio.currInputDevice != NULL)
				free(audio.currInputDevice);

			const uint32_t devStringLen = (uint32_t)strlen(devString);

			audio.currInputDevice = (char *)malloc(devStringLen+1);
			if (audio.currInputDevice == NULL)
				return true;

			audio.currInputDevice[0] = '\0';

			if (devStringLen > 0)
				strcpy(audio.currInputDevice, devString);

			drawAudioInputList();
		}

		return true;
	}

	return false;
}

void freeAudioDeviceLists(void)
{
	for (int32_t i = 0; i < MAX_AUDIO_DEVICES; i++)
	{
		if (audio.outputDeviceNames[i] != NULL)
		{
			free(audio.outputDeviceNames[i]);
			audio.outputDeviceNames[i] = NULL;
		}

		if (audio.inputDeviceNames[i] != NULL)
		{
			free(audio.inputDeviceNames[i]);
			audio.inputDeviceNames[i] = NULL;
		}

		audio.outputDeviceNum = 0;
		audio.inputDeviceNum  = 0;
	}
}

void freeAudioDeviceSelectorBuffers(void)
{
	if (editor.audioDevConfigFileLocationU != NULL)
	{
		free(editor.audioDevConfigFileLocationU);
		editor.audioDevConfigFileLocationU = NULL;
	}

	if (audio.currOutputDevice != NULL)
	{
		free(audio.currOutputDevice);
		audio.currOutputDevice = NULL;
	}

	if (audio.currInputDevice != NULL)
	{
		free(audio.currInputDevice);
		audio.currInputDevice = NULL;
	}

	if (audio.lastWorkingAudioDeviceName != NULL)
	{
		free(audio.lastWorkingAudioDeviceName);
		audio.lastWorkingAudioDeviceName = NULL;
	}

	freeAudioDeviceLists();
}

void setToDefaultAudioOutputDevice(void)
{
	const char *devString = SDL_GetAudioDeviceName(0, false);
	if (devString == NULL)
	{
		if (audio.currOutputDevice != NULL)
		{
			free(audio.currOutputDevice);
			audio.currOutputDevice = NULL;
		}

		return;
	}

	const uint32_t stringLen = (uint32_t)strlen(devString);

	if (audio.currOutputDevice != NULL)
	{
		free(audio.currOutputDevice);
		audio.currOutputDevice = NULL;
	}

	audio.currOutputDevice = (char *)malloc(stringLen + 1);
	if (audio.currOutputDevice == NULL)
		return;

	if (stringLen > 0)
		strcpy(audio.currOutputDevice, devString);
}

void setToDefaultAudioInputDevice(void)
{
	const char *devString = SDL_GetAudioDeviceName(0, true);
	if (devString == NULL)
	{
		if (audio.currInputDevice != NULL)
		{
			free(audio.currInputDevice);
			audio.currInputDevice = NULL;
		}

		return;
	}

	const uint32_t stringLen = (uint32_t)strlen(devString);

	if (audio.currInputDevice != NULL)
	{
		free(audio.currInputDevice);
		audio.currInputDevice = NULL;
	}

	audio.currInputDevice = (char *)malloc(stringLen + 1);
	if (audio.currInputDevice == NULL)
		return;

	if (stringLen > 0)
		strcpy(audio.currInputDevice, devString);
}

void rescanAudioDevices(void)
{
	const bool listShown = (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES);

	freeAudioDeviceLists();

	// GET AUDIO OUTPUT DEVICES

	audio.outputDeviceNum = SDL_GetNumAudioDevices(false);
	if (audio.outputDeviceNum > MAX_AUDIO_DEVICES)
		audio.outputDeviceNum = MAX_AUDIO_DEVICES;

	for (int32_t i = 0; i < audio.outputDeviceNum; i++)
	{
		const char *deviceName = SDL_GetAudioDeviceName(i, false);
		if (deviceName == NULL)
		{
			audio.outputDeviceNum--; // hide device
			continue;
		}

		const uint32_t stringLen = (uint32_t)strlen(deviceName);

		audio.outputDeviceNames[i] = (char *)malloc(stringLen + 1);
		if (audio.outputDeviceNames[i] == NULL)
			break;

		if (stringLen > 0)
			strcpy(audio.outputDeviceNames[i], deviceName);
	}

	// GET AUDIO INPUT DEVICES

	audio.inputDeviceNum = SDL_GetNumAudioDevices(true);
	if (audio.inputDeviceNum > MAX_AUDIO_DEVICES)
		audio.inputDeviceNum = MAX_AUDIO_DEVICES;

	for (int32_t i = 0; i < audio.inputDeviceNum; i++)
	{
		const char *deviceName = SDL_GetAudioDeviceName(i, true);
		if (deviceName == NULL)
		{
			audio.inputDeviceNum--; // hide device
			continue;
		}

		const uint32_t stringLen = (uint32_t)strlen(deviceName);

		audio.inputDeviceNames[i] = (char *)malloc(stringLen + 1);
		if (audio.inputDeviceNames[i] == NULL)
			break;

		if (stringLen > 0)
			strcpy(audio.inputDeviceNames[i], deviceName);
	}

	setScrollBarEnd(SB_AUDIO_OUTPUT_SCROLL, audio.outputDeviceNum);
	setScrollBarPos(SB_AUDIO_OUTPUT_SCROLL, 0, false);
	setScrollBarEnd(SB_AUDIO_INPUT_SCROLL, audio.inputDeviceNum);
	setScrollBarPos(SB_AUDIO_INPUT_SCROLL, 0, false);

	// DRAW LISTS

	if (listShown)
	{
		drawAudioOutputList();
		drawAudioInputList();
		drawScrollBar(SB_AUDIO_OUTPUT_SCROLL);
		drawScrollBar(SB_AUDIO_INPUT_SCROLL);
	}
}

void scrollAudInputDevListUp(void)
{
	scrollBarScrollUp(SB_AUDIO_INPUT_SCROLL, 1);
}

void scrollAudInputDevListDown(void)
{
	scrollBarScrollDown(SB_AUDIO_INPUT_SCROLL, 1);
}

void scrollAudOutputDevListUp(void)
{
	scrollBarScrollUp(SB_AUDIO_OUTPUT_SCROLL, 1);
}

void scrollAudOutputDevListDown(void)
{
	scrollBarScrollDown(SB_AUDIO_OUTPUT_SCROLL, 1);
}

void sbAudOutputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && (editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES))
		drawAudioOutputList();

	(void)pos;
}

void sbAudInputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && (editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES))
		drawAudioInputList();

	(void)pos;
}
