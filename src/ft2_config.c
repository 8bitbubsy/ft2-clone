// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#define _WIN32_IE 0x0500
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "ft2_header.h"
#include "ft2_video.h"
#include "ft2_audio.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_mouse.h"
#include "ft2_wav_renderer.h"
#include "ft2_sampling.h"
#include "ft2_audioselector.h"
#include "ft2_midi.h"
#include "ft2_palette.h"
#include "ft2_pattern_draw.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

config_t config; // globalized

#ifdef _MSC_VER // hide POSIX warnings
#pragma warning(disable: 4996)
#endif

static uint8_t configBuffer[CONFIG_FILE_SIZE];

static void xorConfigBuffer(uint8_t *ptr8)
{
	for (int32_t i = 0; i < CONFIG_FILE_SIZE; i++)
		ptr8[i] ^= i*7;
}

static int32_t calcChecksum(const uint8_t *p, uint16_t len) // for Nibbles highscore data
{
	if (len == 0)
		return 0;

	uint16_t data = 0;
	uint32_t checksum = 0;

	for (uint16_t i = len; i > 0; i--)
	{
		data = ((data | *p++) + i) ^ i;
		checksum += data;
		data <<= 8;
	}

	return checksum;
}

static void loadConfigFromBuffer(void)
{
	lockMixerCallback();

	memcpy(&config, configBuffer, CONFIG_FILE_SIZE);

	// if Nibbles highscore checksum is incorrect, load default highscores instead
	const int32_t newChecksum = calcChecksum((uint8_t *)&config.NI_HighScore, sizeof (config.NI_HighScore));
	if (newChecksum != config.NI_HighScoreChecksum)
		memcpy(&config.NI_HighScore, &defConfigData[636], sizeof (config.NI_HighScore));

	// sanitize Nibbles highscore names
	for (int32_t i = 0; i < 10; i++)
	{
		config.NI_HighScore[i].name[21] = '\0';
		if (config.NI_HighScore[i].nameLen > 21)
			config.NI_HighScore[i].nameLen = 21;
	}

	// clamp user palette values
	for (int32_t i = 0; i < 16; i++)
	{
		config.userPal->r = palMax(config.userPal->r);
		config.userPal->g = palMax(config.userPal->g);
		config.userPal->b = palMax(config.userPal->b);
	}

	// copy over user palette
	memcpy(palTable[11], config.userPal, sizeof (pal16) * 16);

	// sanitize certain values

	config.modulesPath[80-1] = '\0';
	config.instrPath[80-1] = '\0';
	config.samplesPath[80-1] = '\0';
	config.patternsPath[80-1] = '\0';
	config.tracksPath[80-1] = '\0';

	// clear data after the actual Pascal string length. FT2 can save garbage in that area.

	if (config.modulesPathLen < 80)
		memset(&config.modulesPath[config.modulesPathLen], 0, 80-config.modulesPathLen);

	if (config.instrPathLen < 80)
		memset(&config.instrPath[config.instrPathLen], 0, 80-config.instrPathLen);

	if (config.samplesPathLen < 80)
		memset(&config.samplesPath[config.samplesPathLen], 0, 80-config.samplesPathLen);

	if (config.patternsPathLen < 80)
		memset(&config.patternsPath[config.patternsPathLen], 0, 80-config.patternsPathLen);

	if (config.tracksPathLen < 80)
		memset(&config.tracksPath[config.tracksPathLen], 0, 80-config.tracksPathLen);

	config.boostLevel = CLAMP(config.boostLevel, 1, 32);
	config.masterVol = CLAMP(config.masterVol, 0, 256);
	config.ptnMaxChannels = CLAMP(config.ptnMaxChannels, 0, 3);
	config.ptnFont = CLAMP(config.ptnFont, 0, 3);
	config.mouseType = CLAMP(config.mouseType, 0, 3);
	config.cfg_StdPalNr = CLAMP(config.cfg_StdPalNr, 0, 11);
	config.cfg_SortPriority = CLAMP(config.cfg_SortPriority, 0, 1);
	config.NI_AntPlayers = CLAMP(config.NI_AntPlayers, 0, 1);
	config.NI_Speed = CLAMP(config.NI_Speed, 0, 3);
	config.recMIDIVolSens = CLAMP(config.recMIDIVolSens, 0, 200);
	config.recMIDIChn  = CLAMP(config.recMIDIChn, 1, 16);

	config.interpolation &= 3; // one extra bit used in FT2 clone (off, sinc, linear)

	if (config.recTrueInsert > 1)
		config.recTrueInsert = 1;

	if (config.mouseAnimType != 0 && config.mouseAnimType != 2)
		config.mouseAnimType = 0;

	if (config.recQuantRes != 1 && config.recQuantRes != 2 && config.recQuantRes != 4 &&
		config.recQuantRes != 8 && config.recQuantRes != 16)
	{
		config.recQuantRes = 16;
	}

	if (config.audioFreq != 44100 && config.audioFreq != 48000 && config.audioFreq != 96000 && config.audioFreq != 192000)
		config.audioFreq = 48000;

	if (config.audioInputFreq <= 1) // default value from FT2 (this was cdr_Sync) - set defaults
		config.audioInputFreq = INPUT_FREQ_48KHZ;

	if (config.specialFlags == 64) // default value from FT2 (this was ptnDefaultLen byte #1) - set defaults
		config.specialFlags = BUFFSIZE_1024 | BITDEPTH_16;

	if (config.specialFlags & 64) // deprecated BUFFSIZE_4096 ("Very large") setting
	{
		// set to current highest setting ("Large" aka. 2048)
		config.specialFlags &= ~(BUFFSIZE_1024 + 64);
		config.specialFlags |= BUFFSIZE_2048;
	}

	if (config.windowFlags == 0) // default value from FT2 (this was ptnDefaultLen byte #2) - set defaults
		config.windowFlags = WINSIZE_AUTO;

	// audio bit depth - remove 32-bit flag if both are enabled
	if ((config.specialFlags & BITDEPTH_16) && (config.specialFlags & BITDEPTH_32))
		config.specialFlags &= ~BITDEPTH_32;

	if (audio.dev != 0)
		setNewAudioSettings();

	audioSetInterpolationType(config.interpolation);
	audioSetVolRamp((config.specialFlags & NO_VOLRAMP_FLAG) ? false : true);
	setAudioAmp(config.boostLevel, config.masterVol, config.specialFlags & BITDEPTH_32);
	setMouseShape(config.mouseType);
	changeLogoType(config.id_FastLogo);
	changeBadgeType(config.id_TritonProd);
	ui.maxVisibleChannels = (uint8_t)(2 + ((config.ptnMaxChannels + 1) * 2));
	setPal16(palTable[config.cfg_StdPalNr], true);
	updatePattFontPtrs();

	unlockMixerCallback();
}

static void configDrawAmp(void)
{
	char str[8];
	sprintf(str, "%02d", config.boostLevel);
	textOutFixed(607, 120, PAL_FORGRND, PAL_DESKTOP, str);
}

static void setDefaultConfigSettings(void)
{
	memcpy(configBuffer, defConfigData, CONFIG_FILE_SIZE);
	loadConfigFromBuffer();
}

void resetConfig(void)
{
	if (okBox(2, "System request", "Are you sure you want to reset your FT2 configuration?") != 1)
		return;

	const uint8_t oldWindowFlags = config.windowFlags;

	setDefaultConfigSettings();
	setToDefaultAudioOutputDevice();
	setToDefaultAudioInputDevice();

	saveConfig(false);

	// redraw new changes
	showTopScreen(false);
	showBottomScreen();

	setWindowSizeFromConfig(true);

	// handle pixel filter change
	if ((oldWindowFlags & PIXEL_FILTER) != (config.windowFlags & PIXEL_FILTER))
	{
		recreateTexture();
		if (video.fullscreen)
		{
			leaveFullScreen();
			enterFullscreen();
		}
	}

	if (config.specialFlags2 & HARDWARE_MOUSE)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);
}

bool loadConfig(bool showErrorFlag)
{
	// this routine can be called at any time, so make sure we free these first...

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

	// now we can get the audio devices from audiodev.ini

	audio.currOutputDevice = getAudioOutputDeviceFromConfig();
	audio.currInputDevice = getAudioInputDeviceFromConfig();

#ifdef HAS_MIDI
	if (midi.initThreadDone)
	{
		setMidiInputDeviceFromConfig();
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
			drawMidiInputList();
	}
#endif

	if (editor.configFileLocationU == NULL)
	{
		if (showErrorFlag)
			okBox(0, "System message", "Error opening config file for reading!");

		return false;
	}

	FILE *f = UNICHAR_FOPEN(editor.configFileLocationU, "rb");
	if (f == NULL)
	{
		if (showErrorFlag)
			okBox(0, "System message", "Error opening config file for reading!");

		return false;
	}

	fseek(f, 0, SEEK_END);
	const size_t fileSize = ftell(f);
	rewind(f);

	// check if it's a valid FT2 config file (FT2.CFG filesize varies depending on version)
	if (fileSize < 1732 || fileSize > CONFIG_FILE_SIZE)
	{
		fclose(f);
		if (showErrorFlag)
			okBox(0, "System message", "Error loading config: the config file is not valid!");

		return false;
	}

	if (fileSize < CONFIG_FILE_SIZE) // old version, make sure unloaded entries are zeroed out
		memset(configBuffer, 0, CONFIG_FILE_SIZE);

	// read to config buffer and close file handle
	if (fread(configBuffer, fileSize, 1, f) != 1)
	{
		fclose(f);
		if (showErrorFlag)
			okBox(0, "System message", "Error opening config file for reading!");

		return false;
	}

	fclose(f);

	xorConfigBuffer(configBuffer); // decrypt config buffer

	if (memcmp(&configBuffer[0], CFG_ID_STR, 35) != 0)
	{
		if (showErrorFlag)
			okBox(0, "System message", "Error loading config: the config file is not valid!");

		return false;
	}

	loadConfigFromBuffer();
	return true;
}

void loadConfig2(void) // called by "Load config" button
{
	const uint8_t oldWindowFlags = config.windowFlags;

	loadConfig(CONFIG_SHOW_ERRORS);

	// redraw new changes
	showTopScreen(false);
	showBottomScreen();

	// handle pixel filter change
	if ((oldWindowFlags & PIXEL_FILTER) != (config.windowFlags & PIXEL_FILTER))
	{
		recreateTexture();
		if (video.fullscreen)
		{
			leaveFullScreen();
			enterFullscreen();
		}
	}

	if (config.specialFlags2 & HARDWARE_MOUSE)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);
}

bool saveConfig(bool showErrorFlag)
{
	if (editor.configFileLocationU == NULL)
	{
		if (showErrorFlag)
			okBox(0, "System message", "General I/O error during saving! Is the file in use?");

		return false;
	}

	saveAudioDevicesToConfig(audio.currOutputDevice, audio.currInputDevice);
#ifdef HAS_MIDI
	saveMidiInputDeviceToConfig();
#endif

	FILE *f = UNICHAR_FOPEN(editor.configFileLocationU, "wb");
	if (f == NULL)
	{
		if (showErrorFlag)
			okBox(0, "System message", "General I/O error during config saving! Is the file in use?");

		return false;
	}

	config.NI_HighScoreChecksum = calcChecksum((uint8_t *)config.NI_HighScore, sizeof (config.NI_HighScore));

	// set default path lengths (Pascal strings)
	config.modulesPathLen = (uint8_t)strlen(config.modulesPath);
	config.instrPathLen = (uint8_t)strlen(config.instrPath);
	config.samplesPathLen = (uint8_t)strlen(config.samplesPath);
	config.patternsPathLen = (uint8_t)strlen(config.patternsPath);
	config.tracksPathLen = (uint8_t)strlen(config.tracksPath);

	// copy over user palette
	memcpy(config.userPal, palTable[11], sizeof (pal16) * 16);

	// copy config to buffer and encrypt it
	memcpy(configBuffer, &config, CONFIG_FILE_SIZE);
	xorConfigBuffer(configBuffer);

	if (fwrite(configBuffer, 1, CONFIG_FILE_SIZE, f) != CONFIG_FILE_SIZE)
	{
		fclose(f);

		if (showErrorFlag)
			okBox(0, "System message", "General I/O error during config saving! Is the file in use?");

		return false;
	}

	fclose(f);
	return true;
}

void saveConfig2(void) // called by "Save config" button
{
	saveConfig(CONFIG_SHOW_ERRORS);
}

static UNICHAR *getFullAudDevConfigPathU(void) // kinda hackish
{
	int32_t audiodevDotIniStrLen, ft2DotCfgStrLen;

	if (editor.configFileLocationU == NULL)
		return NULL;

	const int32_t ft2ConfPathLen = (int32_t)UNICHAR_STRLEN(editor.configFileLocationU);

#ifdef _WIN32
	audiodevDotIniStrLen = (int32_t)UNICHAR_STRLEN(L"audiodev.ini");
	ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN(L"FT2.CFG");
#else
	audiodevDotIniStrLen = (int32_t)UNICHAR_STRLEN("audiodev.ini");
	ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN("FT2.CFG");
#endif

	UNICHAR *filePathU = (UNICHAR *)malloc((ft2ConfPathLen + audiodevDotIniStrLen + 1) * sizeof (UNICHAR));
	filePathU[0] = 0;

	UNICHAR_STRCPY(filePathU, editor.configFileLocationU);
	filePathU[ft2ConfPathLen-ft2DotCfgStrLen] = 0;

#ifdef _WIN32
	UNICHAR_STRCAT(filePathU, L"audiodev.ini");
#else
	UNICHAR_STRCAT(filePathU, "audiodev.ini");
#endif

	return filePathU;
}

static UNICHAR *getFullMidiDevConfigPathU(void) // kinda hackish
{
	int32_t mididevDotIniStrLen, ft2DotCfgStrLen;

	if (editor.configFileLocationU == NULL)
		return NULL;

	const int32_t ft2ConfPathLen = (int32_t)UNICHAR_STRLEN(editor.configFileLocationU);

#ifdef _WIN32
	mididevDotIniStrLen = (int32_t)UNICHAR_STRLEN(L"mididev.ini");
	ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN(L"FT2.CFG");
#else
	mididevDotIniStrLen = (int32_t)UNICHAR_STRLEN("mididev.ini");
	ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN("FT2.CFG");
#endif

	UNICHAR *filePathU = (UNICHAR *)malloc((ft2ConfPathLen + mididevDotIniStrLen + 1) * sizeof (UNICHAR));
	filePathU[0] = 0;

	UNICHAR_STRCPY(filePathU, editor.configFileLocationU);
	filePathU[ft2ConfPathLen-ft2DotCfgStrLen] = 0;

#ifdef _WIN32
	UNICHAR_STRCAT(filePathU, L"mididev.ini");
#else
	UNICHAR_STRCAT(filePathU, "mididev.ini");
#endif

	return filePathU;
}

static void setConfigFileLocation(void) // kinda hackish
{
	// Windows
#ifdef _WIN32
	int32_t ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN(L"FT2.CFG");

	UNICHAR *oldPathU = (UNICHAR *)malloc((PATH_MAX + 8 + 1) * sizeof (UNICHAR));
	UNICHAR *tmpPathU = (UNICHAR *)malloc((PATH_MAX + 8 + 1) * sizeof (UNICHAR));
	editor.configFileLocationU = (UNICHAR *)malloc((PATH_MAX + ft2DotCfgStrLen + 1) * sizeof (UNICHAR));

	if (oldPathU == NULL || tmpPathU == NULL || editor.configFileLocationU == NULL)
	{
		if (oldPathU != NULL) free(oldPathU);
		if (tmpPathU != NULL) free(tmpPathU);
		if (editor.configFileLocationU != NULL) free(editor.configFileLocationU);

		editor.configFileLocationU = NULL;
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	oldPathU[0] = 0;
	tmpPathU[0] = 0;

	if (GetCurrentDirectoryW(PATH_MAX - ft2DotCfgStrLen - 1, oldPathU) == 0)
	{
		free(oldPathU);
		free(tmpPathU);
		free(editor.configFileLocationU);

		editor.configFileLocationU = NULL;
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	UNICHAR_STRCPY(editor.configFileLocationU, oldPathU);

	FILE *f = fopen("FT2.CFG", "rb");
	if (f == NULL) // FT2.CFG not found in current dir, try default config dir
	{
		int32_t result = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, tmpPathU);
		if (result == S_OK)
		{
			if (SetCurrentDirectoryW(tmpPathU) != 0)
			{
				result = chdir("FT2 clone");
				if (result != 0)
				{
					_mkdir("FT2 clone");
					result = chdir("FT2 clone");
				}

				if (result == 0)
					GetCurrentDirectoryW(PATH_MAX - ft2DotCfgStrLen - 1, editor.configFileLocationU); // we can, set it
			}
		}
	}
	else
	{
		fclose(f);
	}

	free(tmpPathU);
	SetCurrentDirectoryW(oldPathU);
	free(oldPathU);

	UNICHAR_STRCAT(editor.configFileLocationU, L"\\FT2.CFG");

	// OS X / macOS
#elif defined __APPLE__
	int32_t ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN("FT2.CFG");

	editor.configFileLocationU = (UNICHAR *)malloc((PATH_MAX + ft2DotCfgStrLen + 1) * sizeof (UNICHAR));
	if (editor.configFileLocationU == NULL)
	{
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	editor.configFileLocationU[0] = 0;

	if (getcwd(editor.configFileLocationU, PATH_MAX - ft2DotCfgStrLen - 1) == NULL)
	{
		free(editor.configFileLocationU);
		editor.configFileLocationU = NULL;
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	FILE *f = fopen("FT2.CFG", "rb");
	if (f == NULL) // FT2.CFG not found in current dir, try default config dir
	{
		if (chdir(getenv("HOME")) == 0)
		{
			int32_t result = chdir("Library/Application Support");
			if (result == 0)
			{
				result = chdir("FT2 clone");
				if (result != 0)
				{
					mkdir("FT2 clone", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
					result = chdir("FT2 clone");
				}

				if (result == 0)
					getcwd(editor.configFileLocationU, PATH_MAX - ft2DotCfgStrLen - 1);
			}
		}
	}
	else
	{
		fclose(f);
	}

	strcat(editor.configFileLocationU, "/FT2.CFG");

	// Linux etc
#else
	int32_t ft2DotCfgStrLen = (int32_t)UNICHAR_STRLEN("FT2.CFG");

	editor.configFileLocationU = (UNICHAR *)malloc((PATH_MAX + ft2DotCfgStrLen + 1) * sizeof (UNICHAR));
	if (editor.configFileLocationU == NULL)
	{
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	editor.configFileLocationU[0] = 0;

	if (getcwd(editor.configFileLocationU, PATH_MAX - ft2DotCfgStrLen - 1) == NULL)
	{
		free(editor.configFileLocationU);
		editor.configFileLocationU = NULL;
		showErrorMsgBox("Error: Couldn't set config file location. You can't load/save the config!");
		return;
	}

	FILE *f = fopen("FT2.CFG", "rb");
	if (f == NULL) // FT2.CFG not found in current dir, try default config dir
	{
		int32_t result = -1;

		// try to use $XDG_CONFIG_HOME first. If not set, use $HOME
		const char *xdgConfigHome = getenv("XDG_CONFIG_HOME");
		const char *home = getenv("HOME");

		if (xdgConfigHome != NULL)
			result = chdir(xdgConfigHome);
		else if (home != NULL && chdir(home) == 0)
			result = chdir(".config");

		if (result == 0)
		{
			result = chdir("FT2 clone");
			if (result != 0)
			{
				mkdir("FT2 clone", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
				result = chdir("FT2 clone");
			}

			if (result == 0)
				getcwd(editor.configFileLocationU, PATH_MAX - ft2DotCfgStrLen - 1);
		}
	}
	else
	{
		fclose(f);
	}

	strcat(editor.configFileLocationU, "/FT2.CFG");
#endif

	editor.midiConfigFileLocationU = getFullMidiDevConfigPathU();
	editor.audioDevConfigFileLocationU = getFullAudDevConfigPathU();
}

void loadConfigOrSetDefaults(void)
{
	setConfigFileLocation();
	if (editor.configFileLocationU == NULL)
	{
		setDefaultConfigSettings();
		return;
	}

	FILE *f = UNICHAR_FOPEN(editor.configFileLocationU, "rb");
	if (f == NULL)
	{
		setDefaultConfigSettings();
		return;
	}

	fseek(f, 0, SEEK_END);
	const size_t fileSize = ftell(f);
	rewind(f);

	// not a valid FT2 config file (FT2.CFG filesize varies depending on version)
	if (fileSize < 1732 || fileSize > CONFIG_FILE_SIZE)
	{
		fclose(f);
		setDefaultConfigSettings();
		showErrorMsgBox("The configuration file (FT2.CFG) was corrupt, default settings were loaded.");
		return;
	}

	if (fileSize < CONFIG_FILE_SIZE)
		memset(configBuffer, 0, CONFIG_FILE_SIZE);

	if (fread(configBuffer, fileSize, 1, f) != 1)
	{
		fclose(f);
		setDefaultConfigSettings();
		showErrorMsgBox("I/O error while reading FT2.CFG, default settings were loaded.");
		return;
	}

	fclose(f);

	// decrypt config buffer
	xorConfigBuffer(configBuffer);

	if (memcmp(&configBuffer[0], CFG_ID_STR, 35) != 0)
	{
		setDefaultConfigSettings();
		showErrorMsgBox("The configuration file (FT2.CFG) was corrupt, default settings were loaded.");
		return;
	}

	loadConfigFromBuffer();
}

// GUI-related code

static void drawQuantValue(void)
{
	char str[8];
	sprintf(str, "%02d", config.recQuantRes);
	textOutFixed(354, 123, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawMIDIChanValue(void)
{
	char str[8];
	sprintf(str, "%02d", config.recMIDIChn);
	textOutFixed(578, 109, PAL_FORGRND, PAL_DESKTOP, str);
}

static void drawMIDITransp(void)
{
	fillRect(571, 123, 20, 8, PAL_DESKTOP);

	const char sign = (config.recMIDITranspVal < 0) ? '-' : '+';

	const int8_t val = (int8_t)(ABS(config.recMIDITranspVal));
	if (val >= 10)
	{
		charOut(571, 123, PAL_FORGRND, sign);
		charOut(578, 123, PAL_FORGRND, '0' + ((val / 10) % 10));
		charOut(585, 123, PAL_FORGRND, '0' + (val % 10));
	}
	else
	{
		if (val > 0)
			charOut(578, 123, PAL_FORGRND, sign);

		charOut(585, 123, PAL_FORGRND, '0' + (val % 10));
	}
}

static void drawMIDISens(void)
{
	char str[8];
	sprintf(str, "%03d", config.recMIDIVolSens);
	textOutFixed(525, 160, PAL_FORGRND, PAL_DESKTOP, str);
}

static void setConfigRadioButtonStates(void)
{
	uint16_t tmpID;

	uncheckRadioButtonGroup(RB_GROUP_CONFIG_SELECT);
	switch (editor.currConfigScreen)
	{
		default:
		case CONFIG_SCREEN_IO_DEVICES:    tmpID = RB_CONFIG_IO_DEVICES;    break;
		case CONFIG_SCREEN_LAYOUT:        tmpID = RB_CONFIG_LAYOUT;        break;
		case CONFIG_SCREEN_MISCELLANEOUS: tmpID = RB_CONFIG_MISCELLANEOUS; break;
#ifdef HAS_MIDI
		case CONFIG_SCREEN_MIDI_INPUT:    tmpID = RB_CONFIG_MIDI_INPUT;    break;
#endif
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(RB_GROUP_CONFIG_SELECT);
}

void setConfigIORadioButtonStates(void) // accessed by other .c files
{
	uint16_t tmpID;

	// AUDIO BUFFER SIZE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_SOUND_BUFF_SIZE);

	tmpID = RB_CONFIG_SBS_1024;
	if (config.specialFlags & BUFFSIZE_512)
		tmpID = RB_CONFIG_SBS_512;
	else if (config.specialFlags & BUFFSIZE_2048)
		tmpID = RB_CONFIG_SBS_2048;

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// AUDIO BIT DEPTH
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_BIT_DEPTH);

	tmpID = RB_CONFIG_AUDIO_16BIT;
	if (config.specialFlags & BITDEPTH_32)
		tmpID = RB_CONFIG_AUDIO_24BIT;

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// AUDIO INTERPOLATION
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INTERPOLATION);

	if (config.interpolation == INTERPOLATION_NONE)
		tmpID = RB_CONFIG_AUDIO_INTRP_NONE;
	else if (config.interpolation == INTERPOLATION_LINEAR)
		tmpID = RB_CONFIG_AUDIO_INTRP_LINEAR;
	else
		tmpID = RB_CONFIG_AUDIO_INTRP_SINC;

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// AUDIO FREQUENCY
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_FREQ);
	switch (config.audioFreq)
	{
		         case 44100:  tmpID = RB_CONFIG_AUDIO_44KHZ;  break;
		default: case 48000:  tmpID = RB_CONFIG_AUDIO_48KHZ;  break;
		         case 96000:  tmpID = RB_CONFIG_AUDIO_96KHZ;  break;
		         case 192000: tmpID = RB_CONFIG_AUDIO_192KHZ; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// AUDIO INPUT FREQUENCY
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INPUT_FREQ);
	switch (config.audioInputFreq)
	{
		         case INPUT_FREQ_44KHZ: tmpID = RB_CONFIG_AUDIO_INPUT_44KHZ; break;
		default: case INPUT_FREQ_48KHZ: tmpID = RB_CONFIG_AUDIO_INPUT_48KHZ; break;
		         case INPUT_FREQ_96KHZ: tmpID = RB_CONFIG_AUDIO_INPUT_96KHZ; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// FREQUENCY TABLE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_FREQ_TABLE);
	tmpID = audio.linearPeriodsFlag ? RB_CONFIG_FREQ_LINEAR : RB_CONFIG_FREQ_AMIGA;
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// show result

	showRadioButtonGroup(RB_GROUP_CONFIG_SOUND_BUFF_SIZE);
	showRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_BIT_DEPTH);
	showRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	showRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_FREQ);
	showRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INPUT_FREQ);
	showRadioButtonGroup(RB_GROUP_CONFIG_FREQ_TABLE);
}

static void setConfigIOCheckButtonStates(void)
{
	checkBoxes[CB_CONF_VOL_RAMP].checked = (config.specialFlags & NO_VOLRAMP_FLAG) ? false : true;
	showCheckBox(CB_CONF_VOL_RAMP);
}

static void setConfigLayoutCheckButtonStates(void)
{
	checkBoxes[CB_CONF_PATTSTRETCH].checked = config.ptnUnpressed;
	checkBoxes[CB_CONF_HEXCOUNT].checked = config.ptnHex;
	checkBoxes[CB_CONF_ACCIDENTAL].checked = config.ptnAcc ? true : false;
	checkBoxes[CB_CONF_SHOWZEROES].checked = config.ptnInstrZero;
	checkBoxes[CB_CONF_FRAMEWORK].checked = config.ptnFrmWrk;
	checkBoxes[CB_CONF_LINECOLORS].checked = config.ptnLineLight;
	checkBoxes[CB_CONF_CHANNUMS].checked = config.ptnChnNumbers;
	checkBoxes[CB_CONF_SHOW_VOLCOL].checked = config.ptnS3M;
	checkBoxes[CB_CONF_SOFTWARE_MOUSE].checked = (config.specialFlags2 & HARDWARE_MOUSE) ? false : true;

	showCheckBox(CB_CONF_PATTSTRETCH);
	showCheckBox(CB_CONF_HEXCOUNT);
	showCheckBox(CB_CONF_ACCIDENTAL);
	showCheckBox(CB_CONF_SHOWZEROES);
	showCheckBox(CB_CONF_FRAMEWORK);
	showCheckBox(CB_CONF_LINECOLORS);
	showCheckBox(CB_CONF_CHANNUMS);
	showCheckBox(CB_CONF_SHOW_VOLCOL);
	showCheckBox(CB_CONF_SOFTWARE_MOUSE);
}

static void setConfigLayoutRadioButtonStates(void)
{
	uint16_t tmpID;

	// MOUSE SHAPE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_MOUSE);
	switch (config.mouseType)
	{
		default:
		case MOUSE_IDLE_SHAPE_NICE:   tmpID = RB_CONFIG_MOUSE_NICE;    break;
		case MOUSE_IDLE_SHAPE_UGLY:   tmpID = RB_CONFIG_MOUSE_UGLY;    break;
		case MOUSE_IDLE_SHAPE_AWFUL:  tmpID = RB_CONFIG_MOUSE_AWFUL;   break;
		case MOUSE_IDLE_SHAPE_USABLE: tmpID = RB_CONFIG_MOUSE_USABLE;  break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// MOUSE BUSY SHAPE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_MOUSE_BUSY);
	switch (config.mouseAnimType)
	{
		default:
		case MOUSE_BUSY_SHAPE_CLOCK: tmpID = RB_CONFIG_MOUSE_BUSY_CLOCK; break;
		case MOUSE_BUSY_SHAPE_GLASS: tmpID = RB_CONFIG_MOUSE_BUSY_GLASS; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// SCOPE STYLE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_SCOPE);
	tmpID = RB_CONFIG_SCOPE_NORMAL;
	if (config.specialFlags & LINED_SCOPES) tmpID = RB_CONFIG_SCOPE_LINED;
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	switch (config.mouseType)
	{
		default:
		case MOUSE_IDLE_SHAPE_NICE:    tmpID = RB_CONFIG_MOUSE_NICE;    break;
		case MOUSE_IDLE_SHAPE_UGLY:    tmpID = RB_CONFIG_MOUSE_UGLY;    break;
		case MOUSE_IDLE_SHAPE_AWFUL:   tmpID = RB_CONFIG_MOUSE_AWFUL;   break;
		case MOUSE_IDLE_SHAPE_USABLE:  tmpID = RB_CONFIG_MOUSE_USABLE;  break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// MAX VISIBLE CHANNELS
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_PATTERN_CHANS);
	switch (config.ptnMaxChannels)
	{
		default:
		case MAX_CHANS_SHOWN_4:  tmpID = RB_CONFIG_MAXCHAN_4;  break;
		case MAX_CHANS_SHOWN_6:  tmpID = RB_CONFIG_MAXCHAN_6;  break;
		case MAX_CHANS_SHOWN_8:  tmpID = RB_CONFIG_MAXCHAN_8;  break;
		case MAX_CHANS_SHOWN_12: tmpID = RB_CONFIG_MAXCHAN_12; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// PATTERN FONT
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_FONT);
	switch (config.ptnFont)
	{
		default:
		case PATT_FONT_CAPITALS:  tmpID = RB_CONFIG_FONT_CAPITALS;  break;
		case PATT_FONT_LOWERCASE: tmpID = RB_CONFIG_FONT_LOWERCASE; break;
		case PATT_FONT_FUTURE:    tmpID = RB_CONFIG_FONT_FUTURE;    break;
		case PATT_FONT_BOLD:      tmpID = RB_CONFIG_FONT_BOLD;      break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// PALETTE ENTRIES
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_PAL_ENTRIES);
	radioButtons[RB_CONFIG_PAL_PATTERNTEXT + cfg_ColorNr].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_CONFIG_PAL_ENTRIES);

	// PALETTE PRESET
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_PAL_PRESET);
	switch (config.cfg_StdPalNr)
	{
		default:
		case PAL_ARCTIC:          tmpID = RB_CONFIG_PAL_ARCTIC;          break;
		case PAL_AURORA_BOREALIS: tmpID = RB_CONFIG_PAL_AURORA_BOREALIS; break;
		case PAL_BLUES:           tmpID = RB_CONFIG_PAL_BLUES;           break;
		case PAL_GOLD:            tmpID = RB_CONFIG_PAL_GOLD;            break;
		case PAL_HEAVY_METAL:     tmpID = RB_CONFIG_PAL_HEAVY_METAL;     break;
		case PAL_JUNGLE:          tmpID = RB_CONFIG_PAL_JUNGLE;          break;
		case PAL_LITHE_DARK:      tmpID = RB_CONFIG_PAL_LITHE_DARK;      break;
		case PAL_ROSE:            tmpID = RB_CONFIG_PAL_ROSE;            break;
		case PAL_DARK_MODE:       tmpID = RB_CONFIG_PAL_DARK_MODE;       break;
		case PAL_VIOLENT:         tmpID = RB_CONFIG_PAL_VIOLENT;         break;
		case PAL_WHY_COLORS:      tmpID = RB_CONFIG_PAL_WHY_COLORS;      break;
		case PAL_USER_DEFINED:    tmpID = RB_CONFIG_PAL_USER_DEFINED;    break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// show result

	showRadioButtonGroup(RB_GROUP_CONFIG_MOUSE);
	showRadioButtonGroup(RB_GROUP_CONFIG_MOUSE_BUSY);
	showRadioButtonGroup(RB_GROUP_CONFIG_SCOPE);
	showRadioButtonGroup(RB_GROUP_CONFIG_PATTERN_CHANS);
	showRadioButtonGroup(RB_GROUP_CONFIG_FONT);
	showRadioButtonGroup(RB_GROUP_CONFIG_PAL_PRESET);
}

static void setConfigMiscCheckButtonStates(void)
{
	checkBoxes[CB_CONF_SAMP_CUT_TO_BUF].checked = config.smpCutToBuffer;
	checkBoxes[CB_CONF_PATT_CUT_TO_BUF].checked = config.ptnCutToBuffer;
	checkBoxes[CB_CONF_KILL_NOTES_AT_STOP].checked = config.killNotesOnStopPlay;
	checkBoxes[CB_CONF_FILE_OVERWRITE_WARN].checked = config.cfg_OverwriteWarning;
	checkBoxes[CB_CONF_MULTICHAN_REC].checked = config.multiRec;
	checkBoxes[CB_CONF_MULTICHAN_JAZZ].checked = config.multiKeyJazz;
	checkBoxes[CB_CONF_MULTICHAN_EDIT].checked = config.multiEdit;
	checkBoxes[CB_CONF_REC_KEYOFF].checked = config.recRelease;
	checkBoxes[CB_CONF_QUANTIZATION].checked = config.recQuant;
	checkBoxes[CB_CONF_CHANGE_PATTLEN_INS_DEL].checked = config.recTrueInsert;
	checkBoxes[CB_CONF_MIDI_ALLOW_PC].checked = config.recMIDIAllowPC;
#ifdef HAS_MIDI
	checkBoxes[CB_CONF_MIDI_ENABLE].checked = midi.enable;
#else
	checkBoxes[CB_CONF_MIDI_ENABLE].checked = false;
#endif
	checkBoxes[CB_CONF_MIDI_REC_ALL].checked = config.recMIDIAllChn;
	checkBoxes[CB_CONF_MIDI_REC_TRANS].checked = config.recMIDITransp;
	checkBoxes[CB_CONF_MIDI_REC_VELOC].checked = config.recMIDIVelocity;
	checkBoxes[CB_CONF_MIDI_REC_AFTERTOUCH].checked = config.recMIDIAftert;
	checkBoxes[CB_CONF_FORCE_VSYNC_OFF].checked = (config.windowFlags & FORCE_VSYNC_OFF) ? true : false;
	checkBoxes[CB_CONF_START_IN_FULLSCREEN].checked = (config.windowFlags & START_IN_FULLSCR) ? true : false;
	checkBoxes[CB_CONF_PIXEL_FILTER].checked = (config.windowFlags & PIXEL_FILTER) ? true : false;
	checkBoxes[CB_CONF_STRETCH_IMAGE].checked = (config.specialFlags2 & STRETCH_IMAGE) ? true : false;

	showCheckBox(CB_CONF_SAMP_CUT_TO_BUF);
	showCheckBox(CB_CONF_PATT_CUT_TO_BUF);
	showCheckBox(CB_CONF_KILL_NOTES_AT_STOP);
	showCheckBox(CB_CONF_FILE_OVERWRITE_WARN);
	showCheckBox(CB_CONF_MULTICHAN_REC);
	showCheckBox(CB_CONF_MULTICHAN_JAZZ);
	showCheckBox(CB_CONF_MULTICHAN_EDIT);
	showCheckBox(CB_CONF_REC_KEYOFF);
	showCheckBox(CB_CONF_QUANTIZATION);
	showCheckBox(CB_CONF_CHANGE_PATTLEN_INS_DEL);
	showCheckBox(CB_CONF_MIDI_ALLOW_PC);
	showCheckBox(CB_CONF_MIDI_ENABLE);
	showCheckBox(CB_CONF_MIDI_REC_ALL);
	showCheckBox(CB_CONF_MIDI_REC_TRANS);
	showCheckBox(CB_CONF_MIDI_REC_VELOC);
	showCheckBox(CB_CONF_MIDI_REC_AFTERTOUCH);
	showCheckBox(CB_CONF_FORCE_VSYNC_OFF);
	showCheckBox(CB_CONF_START_IN_FULLSCREEN);
	showCheckBox(CB_CONF_PIXEL_FILTER);
	showCheckBox(CB_CONF_STRETCH_IMAGE);
}

static void setConfigMiscRadioButtonStates(void)
{
	uint16_t tmpID;

	// FILE SORTING
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_FILESORT);
	switch (config.cfg_SortPriority)
	{
		default:
		case FILESORT_EXT:  tmpID = RB_CONFIG_FILESORT_EXT;  break;
		case FILESORT_NAME: tmpID = RB_CONFIG_FILESORT_NAME; break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// WINDOW SIZE
	uncheckRadioButtonGroup(RB_GROUP_CONFIG_WIN_SIZE);

	     if (config.windowFlags & WINSIZE_AUTO) tmpID = RB_CONFIG_WIN_SIZE_AUTO;
	else if (config.windowFlags & WINSIZE_1X) tmpID = RB_CONFIG_WIN_SIZE_1X;
	else if (config.windowFlags & WINSIZE_2X) tmpID = RB_CONFIG_WIN_SIZE_2X;
	else if (config.windowFlags & WINSIZE_3X) tmpID = RB_CONFIG_WIN_SIZE_3X;
	else if (config.windowFlags & WINSIZE_4X) tmpID = RB_CONFIG_WIN_SIZE_4X;

	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	// show result

	showRadioButtonGroup(RB_GROUP_CONFIG_FILESORT);
	showRadioButtonGroup(RB_GROUP_CONFIG_WIN_SIZE);
}

void showConfigScreen(void)
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	ui.configScreenShown = true;

	drawFramework(0, 0, 110, 173, FRAMEWORK_TYPE1);

	setConfigRadioButtonStates();

	checkBoxes[CB_CONF_AUTOSAVE].checked = config.cfg_AutoSave;
	showCheckBox(CB_CONF_AUTOSAVE);

	showPushButton(PB_CONFIG_RESET);
	showPushButton(PB_CONFIG_LOAD);
	showPushButton(PB_CONFIG_SAVE);
	showPushButton(PB_CONFIG_EXIT);

	textOutShadow(4,   4, PAL_FORGRND, PAL_DSKTOP2, "Configuration:");
	textOutShadow(21, 19, PAL_FORGRND, PAL_DSKTOP2, "I/O devices");
	textOutShadow(21, 35, PAL_FORGRND, PAL_DSKTOP2, "Layout");
	textOutShadow(21, 51, PAL_FORGRND, PAL_DSKTOP2, "Miscellaneous");
#ifdef HAS_MIDI
	textOutShadow(21, 67, PAL_FORGRND, PAL_DSKTOP2, "MIDI input");
#endif
	textOutShadow(20, 93, PAL_FORGRND, PAL_DSKTOP2, "Auto save");

	switch (editor.currConfigScreen)
	{
		default:
		case CONFIG_SCREEN_IO_DEVICES:
		{
			drawFramework(110,   0, 276, 87, FRAMEWORK_TYPE1);
			drawFramework(110,  87, 276, 86, FRAMEWORK_TYPE1);

			drawFramework(386,   0, 119,  58, FRAMEWORK_TYPE1);
			drawFramework(386,  58, 119,  44, FRAMEWORK_TYPE1);
			drawFramework(386, 102, 119,  71, FRAMEWORK_TYPE1);

			drawFramework(505,   0, 127,  73, FRAMEWORK_TYPE1);
			drawFramework(505, 117, 127,  56, FRAMEWORK_TYPE1);
			drawFramework(505,  73, 127,  44, FRAMEWORK_TYPE1);

			drawFramework(112,  16, AUDIO_SELECTORS_BOX_WIDTH+4, 69, FRAMEWORK_TYPE2);
			drawFramework(112, 103, AUDIO_SELECTORS_BOX_WIDTH+4, 47, FRAMEWORK_TYPE2);

			drawAudioOutputList();
			drawAudioInputList();

			if (audio.rescanAudioDevicesSupported)
				showPushButton(PB_CONFIG_AUDIO_RESCAN);

			showPushButton(PB_CONFIG_AUDIO_OUTPUT_DOWN);
			showPushButton(PB_CONFIG_AUDIO_OUTPUT_UP);
			showPushButton(PB_CONFIG_AUDIO_INPUT_DOWN);
			showPushButton(PB_CONFIG_AUDIO_INPUT_UP);
			showPushButton(PB_CONFIG_AMP_DOWN);
			showPushButton(PB_CONFIG_AMP_UP);
			showPushButton(PB_CONFIG_MASTVOL_DOWN);
			showPushButton(PB_CONFIG_MASTVOL_UP);

			textOutShadow(114,   4, PAL_FORGRND, PAL_DSKTOP2, "Audio output devices:");
			textOutShadow(114,  91, PAL_FORGRND, PAL_DSKTOP2, "Audio input devices (sampling):");

			textOutShadow(114, 157, PAL_FORGRND, PAL_DSKTOP2, "Input rate:");
			textOutShadow(194, 157, PAL_FORGRND, PAL_DSKTOP2, "44.1kHz");
			textOutShadow(265, 157, PAL_FORGRND, PAL_DSKTOP2, "48.0kHz");
			textOutShadow(336, 157, PAL_FORGRND, PAL_DSKTOP2, "96.0kHz");

			textOutShadow(390,   3, PAL_FORGRND, PAL_DSKTOP2, "Audio buffer size:");
			textOutShadow(406,  17, PAL_FORGRND, PAL_DSKTOP2, "Small");
			textOutShadow(406,  31, PAL_FORGRND, PAL_DSKTOP2, "Medium (default)");
			textOutShadow(406,  45, PAL_FORGRND, PAL_DSKTOP2, "Large");

			textOutShadow(390,  61, PAL_FORGRND, PAL_DSKTOP2, "Audio bit depth:");
			textOutShadow(406,  75, PAL_FORGRND, PAL_DSKTOP2, "16-bit (default)");
			textOutShadow(406,  89, PAL_FORGRND, PAL_DSKTOP2, "32-bit float");

			textOutShadow(390, 105, PAL_FORGRND, PAL_DSKTOP2, "Interpolation:");
			textOutShadow(406, 118, PAL_FORGRND, PAL_DSKTOP2, "None");
			textOutShadow(406, 132, PAL_FORGRND, PAL_DSKTOP2, "Linear (FT2)");
			textOutShadow(406, 146, PAL_FORGRND, PAL_DSKTOP2, "Windowed-sinc");
			textOutShadow(406, 161, PAL_FORGRND, PAL_DSKTOP2, "Volume ramping");

			textOutShadow(509,   3, PAL_FORGRND, PAL_DSKTOP2, "Mixing frequency:");
			textOutShadow(525,  17, PAL_FORGRND, PAL_DSKTOP2, "44100Hz");
			textOutShadow(525,  31, PAL_FORGRND, PAL_DSKTOP2, "48000Hz (default)");
			textOutShadow(525,  45, PAL_FORGRND, PAL_DSKTOP2, "96000Hz");
			textOutShadow(525,  59, PAL_FORGRND, PAL_DSKTOP2, "192000Hz");

			textOutShadow(509,  76, PAL_FORGRND, PAL_DSKTOP2, "Frequency table:");
			textOutShadow(525,  90, PAL_FORGRND, PAL_DSKTOP2, "Amiga freq. table");
			textOutShadow(525, 104, PAL_FORGRND, PAL_DSKTOP2, "Linear freq. table");

			textOutShadow(509, 120, PAL_FORGRND, PAL_DSKTOP2, "Amplification:");
			charOutShadow(621, 120, PAL_FORGRND, PAL_DSKTOP2, 'X');
			textOutShadow(509, 148, PAL_FORGRND, PAL_DSKTOP2, "Master volume:");

			setConfigIORadioButtonStates();
			setConfigIOCheckButtonStates();

			configDrawAmp();

			setScrollBarPos(SB_AMP_SCROLL,       config.boostLevel - 1, false);
			setScrollBarPos(SB_MASTERVOL_SCROLL, config.masterVol,      false);

			showScrollBar(SB_AUDIO_INPUT_SCROLL);
			showScrollBar(SB_AUDIO_OUTPUT_SCROLL);
			showScrollBar(SB_AMP_SCROLL);
			showScrollBar(SB_MASTERVOL_SCROLL);
		}
		break;

		case CONFIG_SCREEN_LAYOUT:
		{
			drawFramework(110,   0, 142, 106, FRAMEWORK_TYPE1);
			drawFramework(252,   0, 142,  98, FRAMEWORK_TYPE1);
			drawFramework(394,   0, 238,  86, FRAMEWORK_TYPE1);
			drawFramework(110, 106, 142,  67, FRAMEWORK_TYPE1);
			drawFramework(252,  98, 142,  45, FRAMEWORK_TYPE1);
			drawFramework(394,  86, 238, 87, FRAMEWORK_TYPE1);

			drawFramework(252, 143, 142,  30, FRAMEWORK_TYPE1);

			textOutShadow(114, 109, PAL_FORGRND, PAL_DSKTOP2, "Mouse shape:");
			textOutShadow(130, 121, PAL_FORGRND, PAL_DSKTOP2, "Nice");
			textOutShadow(194, 121, PAL_FORGRND, PAL_DSKTOP2, "Ugly");
			textOutShadow(130, 135, PAL_FORGRND, PAL_DSKTOP2, "Awful");
			textOutShadow(194, 135, PAL_FORGRND, PAL_DSKTOP2, "Usable");
			textOutShadow(114, 148, PAL_FORGRND, PAL_DSKTOP2, "Mouse busy shape:");
			textOutShadow(130, 160, PAL_FORGRND, PAL_DSKTOP2, "Vogue");
			textOutShadow(194, 160, PAL_FORGRND, PAL_DSKTOP2, "Mr. H");

			textOutShadow(114,   3, PAL_FORGRND, PAL_DSKTOP2, "Pattern layout:");
			textOutShadow(130,  16, PAL_FORGRND, PAL_DSKTOP2, "Pattern stretch");
			textOutShadow(130,  29, PAL_FORGRND, PAL_DSKTOP2, "Hex line numbers");
			textOutShadow(130,  42, PAL_FORGRND, PAL_DSKTOP2, "Accidential");
			textOutShadow(130,  55, PAL_FORGRND, PAL_DSKTOP2, "Show zeroes");
			textOutShadow(130,  68, PAL_FORGRND, PAL_DSKTOP2, "Framework");
			textOutShadow(130,  81, PAL_FORGRND, PAL_DSKTOP2, "Line number colors");
			textOutShadow(130,  94, PAL_FORGRND, PAL_DSKTOP2, "Channel numbering");

			textOutShadow(256,   3, PAL_FORGRND, PAL_DSKTOP2, "Pattern modes:");
			textOutShadow(271,  16, PAL_FORGRND, PAL_DSKTOP2, "Show volume column");
			textOutShadow(256,  30, PAL_FORGRND, PAL_DSKTOP2, "Maximum visible chn.:");
			textOutShadow(272,  43, PAL_FORGRND, PAL_DSKTOP2, "4 channels");
			textOutShadow(272,  57, PAL_FORGRND, PAL_DSKTOP2, "6 channels");
			textOutShadow(272,  71, PAL_FORGRND, PAL_DSKTOP2, "8 channels");
			textOutShadow(272,  85, PAL_FORGRND, PAL_DSKTOP2, "12 channels");

			textOutShadow(257, 101, PAL_FORGRND, PAL_DSKTOP2, "Pattern font:");
			textOutShadow(272, 115, PAL_FORGRND, PAL_DSKTOP2, "Capitals");
			textOutShadow(338, 114, PAL_FORGRND, PAL_DSKTOP2, "Lower-c.");
			textOutShadow(272, 130, PAL_FORGRND, PAL_DSKTOP2, "Future");
			textOutShadow(338, 129, PAL_FORGRND, PAL_DSKTOP2, "Bold");

			textOutShadow(256, 146, PAL_FORGRND, PAL_DSKTOP2, "Scopes:");
			textOutShadow(319, 146, PAL_FORGRND, PAL_DSKTOP2, "Std.");
			textOutShadow(360, 146, PAL_FORGRND, PAL_DSKTOP2, "Lined");

			textOutShadow(272, 160, PAL_FORGRND, PAL_DSKTOP2, "Software mouse");

			textOutShadow(414,   3, PAL_FORGRND, PAL_DSKTOP2, "Pattern text");
			textOutShadow(414,  17, PAL_FORGRND, PAL_DSKTOP2, "Block mark");
			textOutShadow(414,  31, PAL_FORGRND, PAL_DSKTOP2, "Text on block");
			textOutShadow(414,  45, PAL_FORGRND, PAL_DSKTOP2, "Mouse");
			textOutShadow(414,  59, PAL_FORGRND, PAL_DSKTOP2, "Desktop");
			textOutShadow(414,  73, PAL_FORGRND, PAL_DSKTOP2, "Buttons");

			textOutShadow(414,  90, PAL_FORGRND, PAL_DSKTOP2, "Arctic");
			textOutShadow(528,  90, PAL_FORGRND, PAL_DSKTOP2, "LiTHe dark");
			textOutShadow(414, 104, PAL_FORGRND, PAL_DSKTOP2, "Aurora Borealis");
			textOutShadow(528, 104, PAL_FORGRND, PAL_DSKTOP2, "Rose");
			textOutShadow(414, 118, PAL_FORGRND, PAL_DSKTOP2, "Blues");
			textOutShadow(528, 118, PAL_FORGRND, PAL_DSKTOP2, "Dark mode");
			textOutShadow(414, 132, PAL_FORGRND, PAL_DSKTOP2, "Gold");
			textOutShadow(528, 132, PAL_FORGRND, PAL_DSKTOP2, "Violent");
			textOutShadow(414, 146, PAL_FORGRND, PAL_DSKTOP2, "Heavy Metal");
			textOutShadow(528, 146, PAL_FORGRND, PAL_DSKTOP2, "Why colors?");
			textOutShadow(414, 160, PAL_FORGRND, PAL_DSKTOP2, "Jungle");
			textOutShadow(528, 160, PAL_FORGRND, PAL_DSKTOP2, "User defined");

			showPaletteEditor();

			setConfigLayoutCheckButtonStates();
			setConfigLayoutRadioButtonStates();
		}
		break;

		case CONFIG_SCREEN_MISCELLANEOUS:
		{
			drawFramework(110,   0,  99,  43, FRAMEWORK_TYPE1);
			drawFramework(209,   0, 199,  55, FRAMEWORK_TYPE1);
			drawFramework(408,   0, 224,  91, FRAMEWORK_TYPE1);

			drawFramework(110,  43,  99,  57, FRAMEWORK_TYPE1);
			drawFramework(209,  55, 199, 118, FRAMEWORK_TYPE1);
			drawFramework(408,  91, 224,  82, FRAMEWORK_TYPE1);

			drawFramework(110, 100,  99,  73, FRAMEWORK_TYPE1);

			// text boxes
			drawFramework(485,  15, 145,  14, FRAMEWORK_TYPE2);
			drawFramework(485,  30, 145,  14, FRAMEWORK_TYPE2);
			drawFramework(485,  45, 145,  14, FRAMEWORK_TYPE2);
			drawFramework(485,  60, 145,  14, FRAMEWORK_TYPE2);
			drawFramework(485,  75, 145,  14, FRAMEWORK_TYPE2);

			textOutShadow(114,   3, PAL_FORGRND, PAL_DSKTOP2, "Dir. sorting pri.:");
			textOutShadow(130,  16, PAL_FORGRND, PAL_DSKTOP2, "Ext.");
			textOutShadow(130,  30, PAL_FORGRND, PAL_DSKTOP2, "Name");

			textOutShadow(228,   4, PAL_FORGRND, PAL_DSKTOP2, "Sample \"cut to buffer\"");
			textOutShadow(228,  17, PAL_FORGRND, PAL_DSKTOP2, "Pattern \"cut to buffer\"");
			textOutShadow(228,  30, PAL_FORGRND, PAL_DSKTOP2, "Kill voices at music stop");
			textOutShadow(228,  43, PAL_FORGRND, PAL_DSKTOP2, "File-overwrite warning");

			textOutShadow(464,   3, PAL_FORGRND, PAL_DSKTOP2, "Default directories:");
			textOutShadow(413,  17, PAL_FORGRND, PAL_DSKTOP2, "Modules");
			textOutShadow(413,  32, PAL_FORGRND, PAL_DSKTOP2, "Instruments");
			textOutShadow(413,  47, PAL_FORGRND, PAL_DSKTOP2, "Samples");
			textOutShadow(413,  62, PAL_FORGRND, PAL_DSKTOP2, "Patterns");
			textOutShadow(413,  77, PAL_FORGRND, PAL_DSKTOP2, "Tracks");

			textOutShadow(114,  46, PAL_FORGRND, PAL_DSKTOP2, "Window size:");
			textOutShadow(130,  59, PAL_FORGRND, PAL_DSKTOP2, "Auto fit");
			textOutShadow(130,  73, PAL_FORGRND, PAL_DSKTOP2, "1x");
			textOutShadow(172,  73, PAL_FORGRND, PAL_DSKTOP2, "3x");
			textOutShadow(130,  87, PAL_FORGRND, PAL_DSKTOP2, "2x");
			textOutShadow(172,  87, PAL_FORGRND, PAL_DSKTOP2, "4x");
			textOutShadow(114, 103, PAL_FORGRND, PAL_DSKTOP2, "Video settings:");
			textOutShadow(130, 117, PAL_FORGRND, PAL_DSKTOP2, "VSync off");
			textOutShadow(130, 130, PAL_FORGRND, PAL_DSKTOP2, "Fullscreen");
			textOutShadow(130, 143, PAL_FORGRND, PAL_DSKTOP2, "Stretched");
			textOutShadow(130, 156, PAL_FORGRND, PAL_DSKTOP2, "Pixel filter");

			textOutShadow(213,  58, PAL_FORGRND, PAL_DSKTOP2, "Rec./Edit/Play:");
			textOutShadow(228,  71, PAL_FORGRND, PAL_DSKTOP2, "Multichannel record");
			textOutShadow(228,  84, PAL_FORGRND, PAL_DSKTOP2, "Multichannel \"key jazz\"");
			textOutShadow(228,  97, PAL_FORGRND, PAL_DSKTOP2, "Multichannel edit");
			textOutShadow(228, 110, PAL_FORGRND, PAL_DSKTOP2, "Record key-off notes");
			textOutShadow(228, 123, PAL_FORGRND, PAL_DSKTOP2, "Quantization");
			textOutShadow(338, 123, PAL_FORGRND, PAL_DSKTOP2, "1/");
			textOutShadow(228, 136, PAL_FORGRND, PAL_DSKTOP2, "Change pattern length when");
			textOutShadow(228, 147, PAL_FORGRND, PAL_DSKTOP2, "inserting/deleting line.");
			textOutShadow(228, 161, PAL_FORGRND, PAL_DSKTOP2, "Allow MIDI-in program change");

			textOutShadow(428,  95, PAL_FORGRND, PAL_DSKTOP2, "Enable MIDI");
			textOutShadow(412, 108, PAL_FORGRND, PAL_DSKTOP2, "Record MIDI chn.");
			charOutShadow(523, 108, PAL_FORGRND, PAL_DSKTOP2, '(');
			textOutShadow(546, 108, PAL_FORGRND, PAL_DSKTOP2, "all )");
			textOutShadow(428, 121, PAL_FORGRND, PAL_DSKTOP2, "Record transpose");
			textOutShadow(428, 134, PAL_FORGRND, PAL_DSKTOP2, "Record velocity");
			textOutShadow(428, 147, PAL_FORGRND, PAL_DSKTOP2, "Record aftertouch");
			textOutShadow(412, 160, PAL_FORGRND, PAL_DSKTOP2, "Vel./A.t. senstvty.");
			charOutShadow(547, 160, PAL_FORGRND, PAL_DSKTOP2, '%');

			setConfigMiscCheckButtonStates();
			setConfigMiscRadioButtonStates();

			drawQuantValue();
			drawMIDIChanValue();
			drawMIDITransp();
			drawMIDISens();

			showPushButton(PB_CONFIG_QUANTIZE_UP);
			showPushButton(PB_CONFIG_QUANTIZE_DOWN);
			showPushButton(PB_CONFIG_MIDICHN_UP);
			showPushButton(PB_CONFIG_MIDICHN_DOWN);
			showPushButton(PB_CONFIG_MIDITRANS_UP);
			showPushButton(PB_CONFIG_MIDITRANS_DOWN);
			showPushButton(PB_CONFIG_MIDISENS_DOWN);
			showPushButton(PB_CONFIG_MIDISENS_UP);

			showTextBox(TB_CONF_DEF_MODS_DIR);
			showTextBox(TB_CONF_DEF_INSTRS_DIR);
			showTextBox(TB_CONF_DEF_SAMPS_DIR);
			showTextBox(TB_CONF_DEF_PATTS_DIR);
			showTextBox(TB_CONF_DEF_TRACKS_DIR);
			drawTextBox(TB_CONF_DEF_MODS_DIR);
			drawTextBox(TB_CONF_DEF_INSTRS_DIR);
			drawTextBox(TB_CONF_DEF_SAMPS_DIR);
			drawTextBox(TB_CONF_DEF_PATTS_DIR);
			drawTextBox(TB_CONF_DEF_TRACKS_DIR);

			setScrollBarPos(SB_MIDI_SENS, config.recMIDIVolSens, false);
			showScrollBar(SB_MIDI_SENS);
		}
		break;

		case CONFIG_SCREEN_MIDI_INPUT:
		{
			drawFramework(110, 0, 394, 173, FRAMEWORK_TYPE1);
			drawFramework(112, 2, 369, 169, FRAMEWORK_TYPE2);
			drawFramework(504, 0, 128, 173, FRAMEWORK_TYPE1);

			textOutShadow(528, 112, PAL_FORGRND, PAL_DSKTOP2, "Input Devices");

			blitFast(517, 51, bmp.midiLogo, 103, 55);

#ifdef HAS_MIDI
			showPushButton(PB_CONFIG_MIDI_INPUT_DOWN);
			showPushButton(PB_CONFIG_MIDI_INPUT_UP);
			rescanMidiInputDevices();
			drawMidiInputList();
			showScrollBar(SB_MIDI_INPUT_SCROLL);
#endif
		}
		break;
	}
}

void hideConfigScreen(void)
{
	// CONFIG LEFT SIDE
	hideRadioButtonGroup(RB_GROUP_CONFIG_SELECT);
	hideCheckBox(CB_CONF_AUTOSAVE);
	hidePushButton(PB_CONFIG_RESET);
	hidePushButton(PB_CONFIG_LOAD);
	hidePushButton(PB_CONFIG_SAVE);
	hidePushButton(PB_CONFIG_EXIT);

	// CONFIG AUDIO
	hideRadioButtonGroup(RB_GROUP_CONFIG_SOUND_BUFF_SIZE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_BIT_DEPTH);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INTERPOLATION);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_FREQ);
	hideRadioButtonGroup(RB_GROUP_CONFIG_AUDIO_INPUT_FREQ);
	hideRadioButtonGroup(RB_GROUP_CONFIG_FREQ_TABLE);
	hideCheckBox(CB_CONF_VOL_RAMP);
	hidePushButton(PB_CONFIG_AUDIO_RESCAN);
	hidePushButton(PB_CONFIG_AUDIO_OUTPUT_DOWN);
	hidePushButton(PB_CONFIG_AUDIO_OUTPUT_UP);
	hidePushButton(PB_CONFIG_AUDIO_INPUT_DOWN);
	hidePushButton(PB_CONFIG_AUDIO_INPUT_UP);
	hidePushButton(PB_CONFIG_AMP_DOWN);
	hidePushButton(PB_CONFIG_AMP_UP);
	hidePushButton(PB_CONFIG_MASTVOL_DOWN);
	hidePushButton(PB_CONFIG_MASTVOL_UP);
	hideScrollBar(SB_AUDIO_INPUT_SCROLL);
	hideScrollBar(SB_AUDIO_OUTPUT_SCROLL);
	hideScrollBar(SB_AMP_SCROLL);
	hideScrollBar(SB_MASTERVOL_SCROLL);

	// CONFIG LAYOUT
	hideRadioButtonGroup(RB_GROUP_CONFIG_MOUSE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_MOUSE_BUSY);
	hideRadioButtonGroup(RB_GROUP_CONFIG_SCOPE);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PATTERN_CHANS);
	hideRadioButtonGroup(RB_GROUP_CONFIG_FONT);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PAL_ENTRIES);
	hideRadioButtonGroup(RB_GROUP_CONFIG_PAL_PRESET);
	hideCheckBox(CB_CONF_PATTSTRETCH);
	hideCheckBox(CB_CONF_HEXCOUNT);
	hideCheckBox(CB_CONF_ACCIDENTAL);
	hideCheckBox(CB_CONF_SHOWZEROES);
	hideCheckBox(CB_CONF_FRAMEWORK);
	hideCheckBox(CB_CONF_LINECOLORS);
	hideCheckBox(CB_CONF_CHANNUMS);
	hideCheckBox(CB_CONF_SHOW_VOLCOL);
	hideCheckBox(CB_CONF_SOFTWARE_MOUSE);
	hidePushButton(PB_CONFIG_PAL_R_DOWN);
	hidePushButton(PB_CONFIG_PAL_R_UP);
	hidePushButton(PB_CONFIG_PAL_G_DOWN);
	hidePushButton(PB_CONFIG_PAL_G_UP);
	hidePushButton(PB_CONFIG_PAL_B_DOWN);
	hidePushButton(PB_CONFIG_PAL_B_UP);
	hidePushButton(PB_CONFIG_PAL_CONT_DOWN);
	hidePushButton(PB_CONFIG_PAL_CONT_UP);
	hideScrollBar(SB_PAL_R);
	hideScrollBar(SB_PAL_G);
	hideScrollBar(SB_PAL_B);
	hideScrollBar(SB_PAL_CONTRAST);

	// CONFIG MISCELLANEOUS
	hideRadioButtonGroup(RB_GROUP_CONFIG_FILESORT);
	hideRadioButtonGroup(RB_GROUP_CONFIG_WIN_SIZE);
	hidePushButton(PB_CONFIG_QUANTIZE_UP);
	hidePushButton(PB_CONFIG_QUANTIZE_DOWN);
	hidePushButton(PB_CONFIG_MIDICHN_UP);
	hidePushButton(PB_CONFIG_MIDICHN_DOWN);
	hidePushButton(PB_CONFIG_MIDITRANS_UP);
	hidePushButton(PB_CONFIG_MIDITRANS_DOWN);
	hidePushButton(PB_CONFIG_MIDISENS_DOWN);
	hidePushButton(PB_CONFIG_MIDISENS_UP);
	hideCheckBox(CB_CONF_FORCE_VSYNC_OFF);
	hideCheckBox(CB_CONF_START_IN_FULLSCREEN);
	hideCheckBox(CB_CONF_PIXEL_FILTER);
	hideCheckBox(CB_CONF_STRETCH_IMAGE);
	hideCheckBox(CB_CONF_SAMP_CUT_TO_BUF);
	hideCheckBox(CB_CONF_PATT_CUT_TO_BUF);
	hideCheckBox(CB_CONF_KILL_NOTES_AT_STOP);
	hideCheckBox(CB_CONF_FILE_OVERWRITE_WARN);
	hideCheckBox(CB_CONF_MULTICHAN_REC);
	hideCheckBox(CB_CONF_MULTICHAN_JAZZ);
	hideCheckBox(CB_CONF_MULTICHAN_EDIT);
	hideCheckBox(CB_CONF_REC_KEYOFF);
	hideCheckBox(CB_CONF_QUANTIZATION);
	hideCheckBox(CB_CONF_CHANGE_PATTLEN_INS_DEL);
	hideCheckBox(CB_CONF_MIDI_ALLOW_PC);
	hideCheckBox(CB_CONF_MIDI_ENABLE);
	hideCheckBox(CB_CONF_MIDI_REC_ALL);
	hideCheckBox(CB_CONF_MIDI_REC_TRANS);
	hideCheckBox(CB_CONF_MIDI_REC_VELOC);
	hideCheckBox(CB_CONF_MIDI_REC_AFTERTOUCH);
	hideTextBox(TB_CONF_DEF_MODS_DIR);
	hideTextBox(TB_CONF_DEF_INSTRS_DIR);
	hideTextBox(TB_CONF_DEF_SAMPS_DIR);
	hideTextBox(TB_CONF_DEF_PATTS_DIR);
	hideTextBox(TB_CONF_DEF_TRACKS_DIR);
	hideScrollBar(SB_MIDI_SENS);

#ifdef HAS_MIDI
	// CONFIG MIDI
	hidePushButton(PB_CONFIG_MIDI_INPUT_DOWN);
	hidePushButton(PB_CONFIG_MIDI_INPUT_UP);
	hideScrollBar(SB_MIDI_INPUT_SCROLL);
#endif

	ui.configScreenShown = false;
}

void exitConfigScreen(void)
{
	hideConfigScreen();
	showTopScreen(true);
}

// CONFIG AUDIO

void configToggleImportWarning(void)
{
	config.dontShowAgainFlags ^= DONT_SHOW_IMPORT_WARNING_FLAG;
}

void configToggleNotYetAppliedWarning(void)
{
	config.dontShowAgainFlags ^= DONT_SHOW_NOT_YET_APPLIED_WARNING_FLAG;
}

void rbConfigIODevices(void)
{
	checkRadioButton(RB_CONFIG_IO_DEVICES);
	editor.currConfigScreen = CONFIG_SCREEN_IO_DEVICES;

	hideConfigScreen();
	showConfigScreen();
}

void rbConfigLayout(void)
{
	checkRadioButton(RB_CONFIG_LAYOUT);
	editor.currConfigScreen = CONFIG_SCREEN_LAYOUT;

	hideConfigScreen();
	showConfigScreen();
}

void rbConfigMiscellaneous(void)
{
	checkRadioButton(RB_CONFIG_MISCELLANEOUS);
	editor.currConfigScreen = CONFIG_SCREEN_MISCELLANEOUS;

	hideConfigScreen();
	showConfigScreen();
}

#ifdef HAS_MIDI
void rbConfigMidiInput(void)
{
	checkRadioButton(RB_CONFIG_MIDI_INPUT);
	editor.currConfigScreen = CONFIG_SCREEN_MIDI_INPUT;

	hideConfigScreen();
	showConfigScreen();
}
#endif

void rbConfigSbs512(void)
{
	config.specialFlags &= ~(BUFFSIZE_1024 + BUFFSIZE_2048);
	config.specialFlags |= BUFFSIZE_512;

	setNewAudioSettings();
}

void rbConfigSbs1024(void)
{
	config.specialFlags &= ~(BUFFSIZE_512 + BUFFSIZE_2048);
	config.specialFlags |= BUFFSIZE_1024;

	setNewAudioSettings();
}

void rbConfigSbs2048(void)
{
	config.specialFlags &= ~(BUFFSIZE_512 + BUFFSIZE_1024);
	config.specialFlags |= BUFFSIZE_2048;

	setNewAudioSettings();
}

void rbConfigAudio16bit(void)
{
	config.specialFlags &= ~BITDEPTH_32;
	config.specialFlags |=  BITDEPTH_16;

	setNewAudioSettings();
}

void rbConfigAudio24bit(void)
{
	config.specialFlags &= ~BITDEPTH_16;
	config.specialFlags |=  BITDEPTH_32;

	setNewAudioSettings();
}

void rbConfigAudioIntrpNone(void)
{
	config.interpolation = INTERPOLATION_NONE;
	audioSetInterpolationType(config.interpolation);
	checkRadioButton(RB_CONFIG_AUDIO_INTRP_NONE);
}

void rbConfigAudioIntrpLinear(void)
{
	config.interpolation = INTERPOLATION_LINEAR;
	audioSetInterpolationType(config.interpolation);
	checkRadioButton(RB_CONFIG_AUDIO_INTRP_LINEAR);
}

void rbConfigAudioIntrpSinc(void)
{
	config.interpolation = INTERPOLATION_SINC;
	audioSetInterpolationType(config.interpolation);
	checkRadioButton(RB_CONFIG_AUDIO_INTRP_SINC);
}

void rbConfigAudio44kHz(void)
{
	config.audioFreq = 44100;
	setNewAudioSettings();
}

void rbConfigAudio48kHz(void)
{
	config.audioFreq = 48000;
	setNewAudioSettings();
}

void rbConfigAudio96kHz(void)
{
	config.audioFreq = 96000;
	setNewAudioSettings();
}

void rbConfigAudio192kHz(void)
{
	config.audioFreq = 192000;
	setNewAudioSettings();
}

void rbConfigAudioInput44kHz(void)
{
	config.audioInputFreq = INPUT_FREQ_44KHZ;
	checkRadioButton(RB_CONFIG_AUDIO_INPUT_44KHZ);
}

void rbConfigAudioInput48kHz(void)
{
	config.audioInputFreq = INPUT_FREQ_48KHZ;
	checkRadioButton(RB_CONFIG_AUDIO_INPUT_48KHZ);
}

void rbConfigAudioInput96kHz(void)
{
	config.audioInputFreq = INPUT_FREQ_96KHZ;
	checkRadioButton(RB_CONFIG_AUDIO_INPUT_96KHZ);
}

void rbConfigFreqTableAmiga(void)
{
	lockMixerCallback();
	setFrqTab(false);
	unlockMixerCallback();
}

void rbConfigFreqTableLinear(void)
{
	lockMixerCallback();
	setFrqTab(true);
	unlockMixerCallback();
}

void cbToggleAutoSaveConfig(void)
{
	config.cfg_AutoSave ^= 1;
}
void cbConfigVolRamp(void)
{
	config.specialFlags ^= NO_VOLRAMP_FLAG;
	audioSetVolRamp((config.specialFlags & NO_VOLRAMP_FLAG) ? false : true);
}

// CONFIG LAYOUT

static void redrawPatternEditor(void) // called after changing some pattern editor settings in config
{
	// if the cursor was on the volume column while we turned volume column off, move it to effect type slot
	if (!config.ptnS3M && (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2))
		cursor.object = CURSOR_EFX0;

	updateChanNums();
	ui.updatePatternEditor = true;
}

void cbConfigPattStretch(void)
{
	config.ptnUnpressed ^= 1;
	redrawPatternEditor();
}

void cbConfigHexCount(void)
{
	config.ptnHex ^= 1;
	redrawPatternEditor();
}

void cbConfigAccidential(void)
{
	config.ptnAcc ^= 1;
	showCheckBox(CB_CONF_ACCIDENTAL);
	redrawPatternEditor();
}

void cbConfigShowZeroes(void)
{
	config.ptnInstrZero ^= 1;
	redrawPatternEditor();
}

void cbConfigFramework(void)
{
	config.ptnFrmWrk ^= 1;
	redrawPatternEditor();
}

void cbConfigLineColors(void)
{
	config.ptnLineLight ^= 1;
	redrawPatternEditor();
}

void cbConfigChanNums(void)
{
	config.ptnChnNumbers ^= 1;
	redrawPatternEditor();
}

void cbConfigShowVolCol(void)
{
	config.ptnS3M ^= 1;
	redrawPatternEditor();
}

void cbSoftwareMouse(void)
{
	config.specialFlags2 ^= HARDWARE_MOUSE;
	if (!createMouseCursors())
		okBox(0, "System message", "Error: Couldn't create/show mouse cursor!");

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		checkBoxes[CB_CONF_SOFTWARE_MOUSE].checked = false;
		drawCheckBox(CB_CONF_SOFTWARE_MOUSE);
		SDL_ShowCursor(SDL_TRUE);
	}
	else
	{
		checkBoxes[CB_CONF_SOFTWARE_MOUSE].checked = true;
		drawCheckBox(CB_CONF_SOFTWARE_MOUSE);
		SDL_ShowCursor(SDL_FALSE);
	}
}

void rbConfigMouseNice(void)
{
	config.mouseType = MOUSE_IDLE_SHAPE_NICE;
	checkRadioButton(RB_CONFIG_MOUSE_NICE);
	createMouseCursors();
	setMouseShape(config.mouseType);
}

void rbConfigMouseUgly(void)
{
	config.mouseType = MOUSE_IDLE_SHAPE_UGLY;
	checkRadioButton(RB_CONFIG_MOUSE_UGLY);
	createMouseCursors();
	setMouseShape(config.mouseType);
}

void rbConfigMouseAwful(void)
{
	config.mouseType = MOUSE_IDLE_SHAPE_AWFUL;
	checkRadioButton(RB_CONFIG_MOUSE_AWFUL);
	createMouseCursors();
	setMouseShape(config.mouseType);
}

void rbConfigMouseUsable(void)
{
	config.mouseType = MOUSE_IDLE_SHAPE_USABLE;
	checkRadioButton(RB_CONFIG_MOUSE_USABLE);
	createMouseCursors();
	setMouseShape(config.mouseType);
}

void rbConfigMouseBusyVogue(void)
{
	config.mouseAnimType = MOUSE_BUSY_SHAPE_GLASS;
	checkRadioButton(RB_CONFIG_MOUSE_BUSY_GLASS);
	resetMouseBusyAnimation();
}

void rbConfigMouseBusyMrH(void)
{
	config.mouseAnimType = MOUSE_BUSY_SHAPE_CLOCK;
	checkRadioButton(RB_CONFIG_MOUSE_BUSY_CLOCK);
	resetMouseBusyAnimation();
}

void rbConfigScopeStandard(void)
{
	config.specialFlags &= ~LINED_SCOPES;
	checkRadioButton(RB_CONFIG_SCOPE_NORMAL);
}

void rbConfigScopeLined(void)
{
	config.specialFlags |= LINED_SCOPES;
	checkRadioButton(RB_CONFIG_SCOPE_LINED);
}

void rbConfigPatt4Chans(void)
{
	config.ptnMaxChannels = MAX_CHANS_SHOWN_4;
	checkRadioButton(RB_CONFIG_MAXCHAN_4);
	ui.maxVisibleChannels = 2 + (((uint8_t)config.ptnMaxChannels + 1) * 2);
	redrawPatternEditor();
}

void rbConfigPatt6Chans(void)
{
	config.ptnMaxChannels = MAX_CHANS_SHOWN_6;
	checkRadioButton(RB_CONFIG_MAXCHAN_6);
	ui.maxVisibleChannels = 2 + (((uint8_t)config.ptnMaxChannels + 1) * 2);
	redrawPatternEditor();
}

void rbConfigPatt8Chans(void)
{
	config.ptnMaxChannels = MAX_CHANS_SHOWN_8;
	checkRadioButton(RB_CONFIG_MAXCHAN_8);
	ui.maxVisibleChannels = 2 + (((uint8_t)config.ptnMaxChannels + 1) * 2);
	redrawPatternEditor();
}

void rbConfigPatt12Chans(void)
{
	config.ptnMaxChannels = MAX_CHANS_SHOWN_12;
	checkRadioButton(RB_CONFIG_MAXCHAN_12);
	ui.maxVisibleChannels = 2 + (((uint8_t)config.ptnMaxChannels + 1) * 2);
	redrawPatternEditor();
}

void rbConfigFontCapitals(void)
{
	config.ptnFont = PATT_FONT_CAPITALS;
	checkRadioButton(RB_CONFIG_FONT_CAPITALS);
	updatePattFontPtrs();
	redrawPatternEditor();
}

void rbConfigFontLowerCase(void)
{
	config.ptnFont = PATT_FONT_LOWERCASE;
	checkRadioButton(RB_CONFIG_FONT_LOWERCASE);
	updatePattFontPtrs();
	redrawPatternEditor();
}

void rbConfigFontFuture(void)
{
	config.ptnFont = PATT_FONT_FUTURE;
	checkRadioButton(RB_CONFIG_FONT_FUTURE);
	updatePattFontPtrs();
	redrawPatternEditor();
}

void rbConfigFontBold(void)
{
	config.ptnFont = PATT_FONT_BOLD;
	checkRadioButton(RB_CONFIG_FONT_BOLD);
	updatePattFontPtrs();
	redrawPatternEditor();
}

void rbFileSortExt(void)
{
	config.cfg_SortPriority = FILESORT_EXT;
	checkRadioButton(RB_CONFIG_FILESORT_EXT);
	editor.diskOpReadOnOpen = true;
}

void rbFileSortName(void)
{
	config.cfg_SortPriority = FILESORT_NAME;
	checkRadioButton(RB_CONFIG_FILESORT_NAME);
	editor.diskOpReadOnOpen = true;
}

void rbWinSizeAuto(void)
{
	if (video.fullscreen)
	{
		okBox(0, "System message", "You can't change the window size while in fullscreen mode!");
		return;
	}

	config.windowFlags &= ~(WINSIZE_1X + WINSIZE_2X + WINSIZE_3X + WINSIZE_4X);
	config.windowFlags |= WINSIZE_AUTO;
	setWindowSizeFromConfig(true);
	checkRadioButton(RB_CONFIG_WIN_SIZE_AUTO);
}

void rbWinSize1x(void)
{
	if (video.fullscreen)
	{
		okBox(0, "System message", "You can't change the window size while in fullscreen mode!");
		return;
	}

	config.windowFlags &= ~(WINSIZE_AUTO + WINSIZE_2X + WINSIZE_3X + WINSIZE_4X);
	config.windowFlags |= WINSIZE_1X;
	setWindowSizeFromConfig(true);
	checkRadioButton(RB_CONFIG_WIN_SIZE_1X);
}

void rbWinSize2x(void)
{
	if (video.fullscreen)
	{
		okBox(0, "System message", "You can't change the window size while in fullscreen mode!");
		return;
	}

	config.windowFlags &= ~(WINSIZE_AUTO + WINSIZE_1X + WINSIZE_3X + WINSIZE_4X);
	config.windowFlags |= WINSIZE_2X;
	setWindowSizeFromConfig(true);
	checkRadioButton(RB_CONFIG_WIN_SIZE_2X);
}

void rbWinSize3x(void)
{
	if (video.fullscreen)
	{
		okBox(0, "System message", "You can't change the window size while in fullscreen mode!");
		return;
	}

#ifdef __arm__
	okBox(0, "System message", "3x video upscaling is not supported on ARM devices for performance reasons.");
#else
	config.windowFlags &= ~(WINSIZE_AUTO + WINSIZE_1X + WINSIZE_2X + WINSIZE_4X);
	config.windowFlags |= WINSIZE_3X;
	setWindowSizeFromConfig(true);
	checkRadioButton(RB_CONFIG_WIN_SIZE_3X);
#endif
}

void rbWinSize4x(void)
{
	if (video.fullscreen)
	{
		okBox(0, "System message", "You can't change the window size while in fullscreen mode!");
		return;
	}

#ifdef __arm__
	okBox(0, "System message", "4x video upscaling is not supported on ARM devices for performance reasons.");
#else
	config.windowFlags &= ~(WINSIZE_AUTO + WINSIZE_1X + WINSIZE_2X + WINSIZE_3X);
	config.windowFlags |= WINSIZE_4X;
	setWindowSizeFromConfig(true);
	checkRadioButton(RB_CONFIG_WIN_SIZE_4X);
#endif
}

void cbSampCutToBuff(void)
{
	config.smpCutToBuffer ^= 1;
}

void cbPattCutToBuff(void)
{
	config.ptnCutToBuffer ^= 1;
}

void cbKillNotesAtStop(void)
{
	config.killNotesOnStopPlay ^= 1;
}

void cbFileOverwriteWarn(void)
{
	config.cfg_OverwriteWarning ^= 1;
}

void cbMultiChanRec(void)
{
	config.multiRec ^= 1;
}

void cbMultiChanKeyJazz(void)
{
	config.multiKeyJazz ^= 1;
}

void cbMultiChanEdit(void)
{
	config.multiEdit ^= 1;
}

void cbRecKeyOff(void)
{
	config.recRelease ^= 1;
}

void cbQuantization(void)
{
	config.recQuant ^= 1;
}

void cbChangePattLenInsDel(void)
{
	config.recTrueInsert ^= 1;
}

void cbMIDIAllowPC(void)
{
	config.recMIDIAllowPC ^= 1;
}

void cbMIDIEnable(void)
{
#ifdef HAS_MIDI
	midi.enable ^= 1;
#else
	checkBoxes[CB_CONF_MIDI_ENABLE].checked = false;
	drawCheckBox(CB_CONF_MIDI_ENABLE);

	okBox(0, "System message", "This program was not compiled with MIDI functionality!");
#endif
}

void cbMIDIRecTransp(void)
{
	config.recMIDITransp ^= 1;
}

void cbMIDIRecAllChn(void)
{
	config.recMIDIAllChn ^= 1;
}

void cbMIDIRecVelocity(void)
{
	config.recMIDIVelocity ^= 1;
}

void cbMIDIRecAftert(void)
{
	config.recMIDIAftert ^= 1;
}

void cbVsyncOff(void)
{
	config.windowFlags ^= FORCE_VSYNC_OFF;

	if (!(config.dontShowAgainFlags & DONT_SHOW_NOT_YET_APPLIED_WARNING_FLAG))
		okBox(7, "System message", "This setting is not applied until you close and reopen the program.");
}

void cbFullScreen(void)
{
	config.windowFlags ^= START_IN_FULLSCR;

	if (!(config.dontShowAgainFlags & DONT_SHOW_NOT_YET_APPLIED_WARNING_FLAG))
		okBox(7, "System message", "This setting is not applied until you close and reopen the program.");
}

void cbPixelFilter(void)
{
	config.windowFlags ^= PIXEL_FILTER;

	recreateTexture();
	if (video.fullscreen)
	{
		leaveFullScreen();
		enterFullscreen();
	}
}

void cbStretchImage(void)
{
	config.specialFlags2 ^= STRETCH_IMAGE;

	if (video.fullscreen)
	{
		leaveFullScreen();
		enterFullscreen();
	}
}

void configQuantizeUp(void)
{
	if (config.recQuantRes <= 8)
	{
		config.recQuantRes *= 2;
		drawQuantValue();
	}
}

void configQuantizeDown(void)
{
	if (config.recQuantRes > 1)
	{
		config.recQuantRes /= 2;
		drawQuantValue();
	}
}

void configMIDIChnUp(void)
{
	config.recMIDIChn++;
	config.recMIDIChn = ((config.recMIDIChn - 1) & 15) + 1;

	drawMIDIChanValue();
}

void configMIDIChnDown(void)
{
	config.recMIDIChn--;
	config.recMIDIChn = (((uint16_t)(config.recMIDIChn - 1)) & 15) + 1;

	drawMIDIChanValue();
}

void configMIDITransUp(void)
{
	if (config.recMIDITranspVal < 72)
	{
		config.recMIDITranspVal++;
		drawMIDITransp();
	}
}

void configMIDITransDown(void)
{
	if (config.recMIDITranspVal > -72)
	{
		config.recMIDITranspVal--;
		drawMIDITransp();
	}
}

void configMIDISensDown(void)
{
	scrollBarScrollLeft(SB_MIDI_SENS, 1);
}

void configMIDISensUp(void)
{
	scrollBarScrollRight(SB_MIDI_SENS, 1);
}

void sbMIDISens(uint32_t pos)
{
	if (config.recMIDIVolSens != (int16_t)pos)
	{
		config.recMIDIVolSens = (int16_t)pos;
		drawMIDISens();
	}
}

void sbAmp(uint32_t pos)
{
	if (config.boostLevel != (int8_t)pos + 1)
	{
		config.boostLevel = (int8_t)pos + 1;
		setAudioAmp(config.boostLevel, config.masterVol, config.specialFlags & BITDEPTH_32);
		configDrawAmp();
		updateWavRendererSettings();
	}
}

void configAmpDown(void)
{
	scrollBarScrollLeft(SB_AMP_SCROLL, 1);
}

void configAmpUp(void)
{
	scrollBarScrollRight(SB_AMP_SCROLL, 1);
}

void sbMasterVol(uint32_t pos)
{
	if (config.masterVol != (int16_t)pos)
	{
		config.masterVol = (int16_t)pos;
		setAudioAmp(config.boostLevel, config.masterVol, config.specialFlags & BITDEPTH_32);
	}
}

void configMasterVolDown(void)
{
	scrollBarScrollLeft(SB_MASTERVOL_SCROLL, 1);
}

void configMasterVolUp(void)
{
	scrollBarScrollRight(SB_MASTERVOL_SCROLL, 1);
}
