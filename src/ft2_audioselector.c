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

char *getAudioOutputDeviceFromConfig(void)
{
#define MAX_DEV_STR_LEN 256

	const char *devStringTmp;
	char *devString;
	uint32_t devStringLen;
	FILE *f;

	devString = (char *)calloc(MAX_DEV_STR_LEN + 1, sizeof (char));
	if (devString == NULL)
		return NULL;

	f = UNICHAR_FOPEN(editor.audioDevConfigFileLocation, "r");
	if (f == NULL)
	{
		devStringTmp = SDL_GetAudioDeviceName(0, false);
		if (devStringTmp == NULL)
		{
			free(devString);
			return NULL;
		}

		devStringLen = (uint32_t)strlen(devStringTmp);
		if (devStringLen > 0)
			strncpy(devString, devStringTmp, MAX_DEV_STR_LEN);
		devString[devStringLen+1] = '\0'; // UTF-8 needs double null termination
	}
	else
	{
		if (fgets(devString, MAX_DEV_STR_LEN, f) == NULL)
		{
			free(devString);
			fclose(f);
			return NULL;
		}

		devStringLen = (uint32_t)strlen(devString);
		if (devString[devStringLen-1] == '\n')
			devString[devStringLen-1]  = '\0';
		devString[devStringLen+1] = '\0'; // UTF-8 needs double null termination

		fclose(f);
	}

	return devString;
}

char *getAudioInputDeviceFromConfig(void)
{
#define MAX_DEV_STR_LEN 256

	const char *devStringTmp;
	char *devString;
	uint32_t devStringLen;
	FILE *f;

	devString = (char *)calloc(MAX_DEV_STR_LEN + 1, sizeof (char));
	if (devString == NULL)
		return NULL;

	f = UNICHAR_FOPEN(editor.audioDevConfigFileLocation, "r");
	if (f == NULL)
	{
		devStringTmp = SDL_GetAudioDeviceName(0, true);
		if (devStringTmp == NULL)
		{
			free(devString);
			return NULL;
		}

		devStringLen = (uint32_t)strlen(devStringTmp);
		if (devStringLen > 0)
			strncpy(devString, devStringTmp, MAX_DEV_STR_LEN);
		devString[devStringLen+1] = '\0'; // UTF-8 needs double null termination
	}
	else
	{
		if (fgets(devString, MAX_DEV_STR_LEN, f) == NULL)
		{
			free(devString);
			fclose(f);
			return NULL;
		}

		// do it one more time (next line)
		if (fgets(devString, MAX_DEV_STR_LEN, f) == NULL)
		{
			free(devString);
			fclose(f);
			return NULL;
		}

		devStringLen = (uint32_t)strlen(devString);
		if (devString[devStringLen-1] == '\n')
			devString[devStringLen-1]  = '\0';
		devString[devStringLen+1] = '\0'; // UTF-8 needs double null termination

		fclose(f);
	}

	return devString;
}

bool saveAudioDevicesToConfig(const char *outputDevice, const char *inputDevice)
{
	FILE *f;

	f = UNICHAR_FOPEN(editor.audioDevConfigFileLocation, "w");
	if (f == NULL)
		return false;

	if (outputDevice != NULL)
		fputs(outputDevice, f);
	fputc('\n', f);
	if (inputDevice != NULL)
		fputs(inputDevice, f);

	fclose(f);
	return true;
}

void drawAudioOutputList(void)
{
	char *tmpString;
	uint16_t y;
	int32_t deviceEntry;

	clearRect(114, 18, AUDIO_SELECTORS_BOX_WIDTH, 66);

	if (audio.outputDeviceNum == 0)
	{
		textOut(114, 18, PAL_FORGRND, "No audio output devices found!");
		return;
	}

	for (int32_t i = 0; i < 6; i++)
	{
		deviceEntry = getScrollBarPos(SB_AUDIO_OUTPUT_SCROLL) + i;
		if (deviceEntry < audio.outputDeviceNum)
		{
			if (audio.outputDeviceNames[deviceEntry] == NULL)
				continue;

			y = 18 + (uint16_t)(i * 11);

			if (audio.currOutputDevice != NULL)
			{
				if (strcmp(audio.currOutputDevice, audio.outputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, AUDIO_SELECTORS_BOX_WIDTH, 10, PAL_BOXSLCT); // selection background color
			}

			tmpString = utf8ToCp437(audio.outputDeviceNames[deviceEntry], true);
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
	char *tmpString;
	uint16_t y;
	int32_t deviceEntry;

	clearRect(114, 105, AUDIO_SELECTORS_BOX_WIDTH, 44);

	if (audio.inputDeviceNum == 0)
	{
		textOut(114, 105, PAL_FORGRND, "No audio input devices found!");
		return;
	}

	for (int32_t i = 0; i < 4; i++)
	{
		deviceEntry = getScrollBarPos(SB_AUDIO_INPUT_SCROLL) + i;
		if (deviceEntry < audio.inputDeviceNum)
		{
			if (audio.inputDeviceNames[deviceEntry] == NULL)
				continue;

			y = 105 + (uint16_t)(i * 11);

			if (audio.currInputDevice != NULL)
			{
				if (strcmp(audio.currInputDevice, audio.inputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, AUDIO_SELECTORS_BOX_WIDTH, 10, PAL_BOXSLCT); // selection background color
			}

			tmpString = utf8ToCp437(audio.inputDeviceNames[deviceEntry], true);
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
	char *devString;
	int32_t mx, my, deviceNum;
	uint32_t devStringLen;

	if (!editor.ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_IO_DEVICES)
		return false;

	mx = mouse.x;
	my = mouse.y;

	if (my < 18 || my > 149 || mx < 114 || mx >= 114+AUDIO_SELECTORS_BOX_WIDTH)
		return false;

	if (my < 84)
	{
		// output device list

		deviceNum = (int32_t)scrollBars[SB_AUDIO_OUTPUT_SCROLL].pos + ((my - 18) / 11);
		if (audio.outputDeviceNum <= 0 || deviceNum >= audio.outputDeviceNum)
			return true;

		devString = audio.outputDeviceNames[deviceNum];
		if (devString != NULL && (audio.currOutputDevice == NULL || strcmp(audio.currOutputDevice, devString) != 0))
		{
			if (audio.currOutputDevice != NULL)
				free(audio.currOutputDevice);

			devStringLen = (uint32_t)strlen(devString);

			audio.currOutputDevice = (char *)malloc(devStringLen + 2);
			if (audio.currOutputDevice == NULL)
				return true;

			if (devStringLen > 0)
				strcpy(audio.currOutputDevice, devString);
			audio.currOutputDevice[devStringLen+1] = '\0'; // UTF-8 needs double null termination

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

		deviceNum = (int32_t)scrollBars[SB_AUDIO_INPUT_SCROLL].pos + ((my - 105) / 11);
		if (audio.inputDeviceNum <= 0 || deviceNum >= audio.inputDeviceNum)
			return true;

		devString = audio.inputDeviceNames[deviceNum];
		if (devString != NULL && (audio.currInputDevice == NULL || strcmp(audio.currInputDevice, devString) != 0))
		{
			if (audio.currInputDevice != NULL)
				free(audio.currInputDevice);

			devStringLen = (uint32_t)strlen(devString);

			audio.currInputDevice = (char *)malloc(devStringLen + 2);
			if (audio.currInputDevice == NULL)
				return true;

			if (devStringLen > 0)
				strcpy(audio.currInputDevice, devString);
			audio.currInputDevice[devStringLen+1] = '\0'; // UTF-8 needs double null termination

			drawAudioInputList();
		}

		return true;
	}

	return false;
}

void freeAudioDeviceLists(void)
{
	for (uint32_t i = 0; i < MAX_AUDIO_DEVICES; i++)
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
	if (editor.audioDevConfigFileLocation != NULL)
	{
		free(editor.audioDevConfigFileLocation);
		editor.audioDevConfigFileLocation = NULL;
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
	const char *devString;
	uint32_t stringLen;

	devString = SDL_GetAudioDeviceName(0, false);
	if (devString == NULL)
	{
		if (audio.currOutputDevice != NULL)
		{
			free(audio.currOutputDevice);
			audio.currOutputDevice = NULL;
		}

		return;
	}

	stringLen = (uint32_t)strlen(devString);

	if (audio.currOutputDevice != NULL)
	{
		free(audio.currOutputDevice);
		audio.currOutputDevice = NULL;
	}

	audio.currOutputDevice = (char *)malloc(stringLen + 2);
	if (audio.currOutputDevice == NULL)
		return;

	if (stringLen > 0)
		strcpy(audio.currOutputDevice, devString);

	audio.currOutputDevice[stringLen+1] = '\0'; // UTF-8 needs double null termination
}

void setToDefaultAudioInputDevice(void)
{
	const char *devString;
	uint32_t stringLen;

	devString = SDL_GetAudioDeviceName(0, true);
	if (devString == NULL)
	{
		if (audio.currInputDevice != NULL)
		{
			free(audio.currInputDevice);
			audio.currInputDevice = NULL;
		}

		return;
	}

	stringLen = (uint32_t)strlen(devString);

	if (audio.currInputDevice != NULL)
	{
		free(audio.currInputDevice);
		audio.currInputDevice = NULL;
	}

	audio.currInputDevice = (char *)malloc(stringLen + 2);
	if (audio.currInputDevice == NULL)
		return;

	if (stringLen > 0)
		strcpy(audio.currInputDevice, devString);

	audio.currInputDevice[stringLen+1] = '\0'; // UTF-8 needs double null termination
}

void rescanAudioDevices(void)
{
	bool listShown;
	const char *deviceName;
	uint32_t stringLen;

	listShown = (editor.ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES);

	freeAudioDeviceLists();

	// GET AUDIO OUTPUT DEVICES

	audio.outputDeviceNum = SDL_GetNumAudioDevices(false);
	if (audio.outputDeviceNum > MAX_AUDIO_DEVICES)
		audio.outputDeviceNum = MAX_AUDIO_DEVICES;

	for (int32_t i = 0; i < audio.outputDeviceNum; i++)
	{
		deviceName = SDL_GetAudioDeviceName(i, false);
		if (deviceName == NULL)
		{
			audio.outputDeviceNum--; // hide device
			continue;
		}

		stringLen = (uint32_t)strlen(deviceName);

		audio.outputDeviceNames[i] = (char *)malloc(stringLen + 2);
		if (audio.outputDeviceNames[i] == NULL)
			break;

		if (stringLen > 0)
			strcpy(audio.outputDeviceNames[i], deviceName);

		audio.outputDeviceNames[i][stringLen+1] = '\0'; // UTF-8 needs double null termination
	}

	// GET AUDIO INPUT DEVICES

	audio.inputDeviceNum = SDL_GetNumAudioDevices(true);
	if (audio.inputDeviceNum > MAX_AUDIO_DEVICES)
		audio.inputDeviceNum = MAX_AUDIO_DEVICES;

	for (int32_t i = 0; i < audio.inputDeviceNum; i++)
	{
		deviceName = SDL_GetAudioDeviceName(i, true);
		if (deviceName == NULL)
		{
			audio.inputDeviceNum--; // hide device
			continue;
		}

		stringLen = (uint32_t)strlen(deviceName);

		audio.inputDeviceNames[i] = (char *)malloc(stringLen + 2);
		if (audio.inputDeviceNames[i] == NULL)
			break;

		if (stringLen > 0)
			strcpy(audio.inputDeviceNames[i], deviceName);

		audio.inputDeviceNames[i][stringLen+1] = '\0'; // UTF-8 needs double null termination
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
	(void)pos;

	if (editor.ui.configScreenShown && (editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES))
		drawAudioOutputList();
}

void sbAudInputSetPos(uint32_t pos)
{
	(void)pos;

	if (editor.ui.configScreenShown && (editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES))
		drawAudioInputList();
}
