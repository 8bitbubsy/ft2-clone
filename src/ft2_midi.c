// this implements MIDI input only!

#ifdef HAS_MIDI

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_edit.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_pattern_ed.h"
#include "ft2_structs.h"
#include "rtmidi/rtmidi_c.h"

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

midi_t midi; // globalized

static volatile bool midiDeviceOpened;
static bool recMIDIValidChn = true;
static volatile RtMidiPtr midiInDev;

static inline void midiInSetChannel(uint8_t status)
{
	recMIDIValidChn = (config.recMIDIAllChn || (status & 0xF) == config.recMIDIChn-1);
}

static inline void midiInKeyAction(int8_t m, uint8_t mv)
{
	int16_t vol = (mv * 64 * config.recMIDIVolSens) / (127 * 100);
	if (vol > 64)
		vol = 64;

	// FT2 bugfix: If velocity>0, and sensitivity made vol=0, set vol to 1 (prevent key off)
	if (mv > 0 && vol == 0)
		vol = 1;

	if (mv > 0 && !config.recMIDIVelocity)
		vol = -1; // don't record volume (velocity)

	m -= 11;
	if (config.recMIDITransp)
		m += (int8_t)config.recMIDITranspVal;

	if ((mv == 0 || vol != 0) && m > 0 && m < 96 && recMIDIValidChn)
		recordNote(m, (int8_t)vol);
}

static inline void midiInControlChange(uint8_t data1, uint8_t data2)
{
	if (data1 != 1) // 1 = modulation wheel
		return;

	midi.currMIDIVibDepth = data2 << 6;

	if (recMIDIValidChn) // real FT2 forgot to check this here..
	{
		for (uint8_t i = 0; i < song.numChannels; i++)
		{
			if (channel[i].midiVibDepth != 0 || editor.keyOnTab[i] != 0)
				channel[i].midiVibDepth = midi.currMIDIVibDepth;
		}
	}

	const uint8_t vibDepth = (midi.currMIDIVibDepth >> 9) & 0x0F;
	if (vibDepth > 0 && recMIDIValidChn)
		recordMIDIEffect(0x04, 0xA0 | vibDepth);
}

static inline void midiInPitchBendChange(uint8_t data1, uint8_t data2)
{
	int16_t pitch = (int16_t)((data2 << 7) | data1) - 8192; // -8192..8191
	pitch >>= 6; // -128..127

	midi.currMIDIPitch = pitch;
	if (recMIDIValidChn)
	{
		channel_t *ch = channel;
		for (uint8_t i = 0; i < song.numChannels; i++, ch++)
		{
			if (ch->midiPitch != 0 || editor.keyOnTab[i] != 0)
				ch->midiPitch = midi.currMIDIPitch;
		}
	}
}

static void midiInCallback(double timeStamp, const unsigned char *message, size_t messageSize, void *userData)
{
	uint8_t byte[3];

	if (!midi.enable || messageSize < 2)
		return;

	midi.callbackBusy = true;

	byte[0] = message[0];
	if (byte[0] > 127 && byte[0] < 240)
	{
		byte[1] = message[1] & 0x7F;

		if (messageSize >= 3)
			byte[2] = message[2] & 0x7F;
		else
			byte[2] = 0;

		midiInSetChannel(byte[0]);

		     if (byte[0] >= 128 && byte[0] <= 128+15)       midiInKeyAction(byte[1], 0);
		else if (byte[0] >= 144 && byte[0] <= 144+15)       midiInKeyAction(byte[1], byte[2]);
		else if (byte[0] >= 176 && byte[0] <= 176+15)   midiInControlChange(byte[1], byte[2]);
		else if (byte[0] >= 224 && byte[0] <= 224+15) midiInPitchBendChange(byte[1], byte[2]);
	}

	midi.callbackBusy = false;

	(void)timeStamp;
	(void)userData;
}

static uint32_t getNumMidiInDevices(void)
{
	if (midiInDev == NULL)
		return 0;

	return rtmidi_get_port_count(midiInDev);
}

static char *getMidiInDeviceName(uint32_t deviceID)
{
	if (midiInDev == NULL)
		return NULL; // MIDI not initialized

	char *devStr = (char *)rtmidi_get_port_name(midiInDev, deviceID);
	if (devStr == NULL || !midiInDev->ok)
		return NULL;

	return devStr;
}

void closeMidiInDevice(void)
{
	if (midiDeviceOpened)
	{
		if (midiInDev != NULL)
		{
			rtmidi_in_cancel_callback(midiInDev);
			rtmidi_close_port(midiInDev);
		}

		midiDeviceOpened = false;
	}
}

void freeMidiIn(void)
{
	if (midiInDev != NULL)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
	}
}

bool initMidiIn(void)
{
	midiInDev = rtmidi_in_create_default();
	if (midiInDev == NULL)
		return false;

	if (!midiInDev->ok)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
		return false;
	}

	return true;
}

bool openMidiInDevice(uint32_t deviceID)
{
	if (midiDeviceOpened || midiInDev == NULL || midi.numInputDevices == 0)
		return false;

	rtmidi_open_port(midiInDev, deviceID, "FT2 Clone MIDI Port");
	if (!midiInDev->ok)
		return false;

	/*
	rtmidi_in_set_callback(midiInDev, midiInCallback, NULL);
	if (!midiInDev->ok)
	{
		rtmidi_close_port(midiInDev);
		return false;
	}

	rtmidi_in_ignore_types(midiInDev, true, true, true);
	*/

	midiDeviceOpened = true;
	return true;
}

void recordMIDIEffect(uint8_t efx, uint8_t efxData)
{
	// only handle this in record mode
	if (!midi.enable || (playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT))
		return;

	if (config.multiRec)
	{
		note_t *p = &pattern[editor.editPattern][editor.row * MAX_CHANNELS];
		for (int32_t i = 0; i < song.numChannels; i++, p++)
		{
			if (config.multiRecChn[i] && editor.chnMode[i])
			{
				if (!allocatePattern(editor.editPattern))
					return;

				if (p->efx == 0)
				{
					p->efx = efx;
					p->efxData = efxData;
					setSongModifiedFlag();
				}
			}
		}
	}
	else
	{
		if (!allocatePattern(editor.editPattern))
			return;

		note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
		if (p->efx != efx || p->efxData != efxData)
			setSongModifiedFlag();

		p->efx = efx;
		p->efxData = efxData;
	}
}

bool saveMidiInputDeviceToConfig(void)
{
	if (!midi.initThreadDone || midiInDev == NULL || !midiDeviceOpened)
		return false;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		return false;

	char *midiInStr = getMidiInDeviceName(midi.inputDevice);
	if (midiInStr == NULL)
		return false;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "w");
	if (f == NULL)
	{
		free(midiInStr);
		return false;
	}

	fputs(midiInStr, f);
	free(midiInStr);

	fclose(f);
	return true;
}

bool setMidiInputDeviceFromConfig(void)
{
	uint32_t i;

	if (midiInDev == NULL || editor.midiConfigFileLocationU == NULL)
		goto setDefMidiInputDev;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		goto setDefMidiInputDev;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "r");
	if (f == NULL)
		goto setDefMidiInputDev;

	char *devString = (char *)malloc(1024+2);
	if (devString == NULL)
	{
		fclose(f);
		goto setDefMidiInputDev;
	}
	devString[0] = '\0';

	if (fgets(devString, 1024, f) == NULL)
	{
		fclose(f);
		free(devString);
		goto setDefMidiInputDev;
	}

	fclose(f);

	// scan for device in list
	char *midiInStr = NULL;
	for (i = 0; i < numDevices; i++)
	{
		midiInStr = getMidiInDeviceName(i);
		if (midiInStr == NULL)
			continue;

		if (!_stricmp(devString, midiInStr))
			break; // device matched

		free(midiInStr);
		midiInStr = NULL;
	}

	free(devString);

	// device not found in list, set default
	if (i == numDevices)
		goto setDefMidiInputDev;

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = i;
	midi.inputDeviceName = midiInStr;
	midi.numInputDevices = numDevices;

	return true;

	// couldn't load device, set default
setDefMidiInputDev:
	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = 0;
	midi.inputDeviceName = strdup("Error configuring MIDI...");
	midi.numInputDevices = 1;

	return false;
}

void freeMidiInputDeviceList(void)
{
	for (int32_t i = 0; i < MAX_MIDI_DEVICES; i++)
	{
		if (midi.inputDeviceNames[i] != NULL)
		{
			free(midi.inputDeviceNames[i]);
			midi.inputDeviceNames[i] = NULL;
		}
	}

	midi.numInputDevices = 0;
}

void rescanMidiInputDevices(void)
{
	freeMidiInputDeviceList();

	midi.numInputDevices = getNumMidiInDevices();
	if (midi.numInputDevices > MAX_MIDI_DEVICES)
		midi.numInputDevices = MAX_MIDI_DEVICES;

	for (uint32_t i = 0; i < midi.numInputDevices; i++)
	{
		char *deviceName = getMidiInDeviceName(i);
		if (deviceName == NULL)
		{
			if (midi.numInputDevices > 0)
				midi.numInputDevices--; // hide device

			continue;
		}

		midi.inputDeviceNames[i] = deviceName;
	}

	setScrollBarEnd(SB_MIDI_INPUT_SCROLL, midi.numInputDevices);
	setScrollBarPos(SB_MIDI_INPUT_SCROLL, 0, false);
}

void drawMidiInputList(void)
{
	clearRect(114, 4, 365, 165);

	if (!midi.initThreadDone || midiInDev == NULL || midi.numInputDevices == 0)
	{
		textOut(114, 4 + (0 * 11), PAL_FORGRND, "No MIDI input devices found!");
		textOut(114, 4 + (1 * 11), PAL_FORGRND, "Either wait a few seconds for MIDI to initialize, or restart the");
		textOut(114, 4 + (2 * 11), PAL_FORGRND, "tracker if you recently plugged in a MIDI device.");
		return;
	}

	for (uint16_t i = 0; i < 15; i++)
	{
		uint32_t deviceEntry = getScrollBarPos(SB_MIDI_INPUT_SCROLL) + i;
		if (deviceEntry > MAX_MIDI_DEVICES)
			deviceEntry = MAX_MIDI_DEVICES;

		if (deviceEntry < midi.numInputDevices)
		{
			if (midi.inputDeviceNames[deviceEntry] == NULL)
				continue;

			const uint16_t y = 4 + (i * 11);

			if (midi.inputDeviceName != NULL)
			{
				if (_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, 365, 10, PAL_BOXSLCT); // selection background color
			}

			char *tmpString = utf8ToCp437(midi.inputDeviceNames[deviceEntry], true);
			if (tmpString != NULL)
			{
				textOutClipX(114, y, PAL_FORGRND, tmpString, 479);
				free(tmpString);
			}
		}
	}
}

void scrollMidiInputDevListUp(void)
{
	scrollBarScrollUp(SB_MIDI_INPUT_SCROLL, 1);
}

void scrollMidiInputDevListDown(void)
{
	scrollBarScrollDown(SB_MIDI_INPUT_SCROLL, 1);
}

void sbMidiInputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
		drawMidiInputList();

	(void)pos;
}

bool testMidiInputDeviceListMouseDown(void)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return false;

	if (mouse.x < 114 || mouse.x > 479 || mouse.y < 4 || mouse.y > 166)
		return false; // we didn't click inside the list area

	if (!midi.initThreadDone)
		return true;

	uint32_t deviceNum = (uint32_t)scrollBars[SB_MIDI_INPUT_SCROLL].pos + ((mouse.y - 4) / 11);
	if (deviceNum > MAX_MIDI_DEVICES)
		deviceNum = MAX_MIDI_DEVICES;

	if (midi.numInputDevices == 0 || deviceNum >= midi.numInputDevices)
		return true;

	if (midi.inputDeviceName != NULL)
	{
		if (!_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceNum]))
			return true; // we clicked the currently selected device, do nothing

		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDeviceName = strdup(midi.inputDeviceNames[deviceNum]);
	midi.inputDevice = deviceNum;

	closeMidiInDevice();
	freeMidiIn();
	initMidiIn();
	openMidiInDevice(midi.inputDevice);

	drawMidiInputList();
	return true;
}

int32_t initMidiFunc(void *ptr)
{
	initMidiIn();
	setMidiInputDeviceFromConfig();
	openMidiInDevice(midi.inputDevice);
	midi.rescanDevicesFlag = true;
	midi.initThreadDone = true;

	return true;
	(void)ptr;
}

#else
typedef int prevent_compiler_warning; // kludge: prevent warning about empty .c file if HAS_MIDI is not defined
#endif
