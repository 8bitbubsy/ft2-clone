// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_mouse.h"
#include "ft2_wav_renderer.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

radioButton_t radioButtons[NUM_RADIOBUTTONS] =
{
	/*
	** -- STRUCT INFO: --
	**  x        = x position
	**  y        = y position
	**  w        = clickable width in pixels, relative to x
	**  group    = what group the radiobutton belongs to
	**  funcOnUp = function to call when released
	*/

	// ------ HELP SCREEN RADIOBUTTONS ------
	//x, y,   w,  group,         funcOnUp
	{ 5, 18,  69, RB_GROUP_HELP, rbHelpFeatures },
	{ 5, 34,  60, RB_GROUP_HELP, rbHelpEffects },
	{ 5, 50,  86, RB_GROUP_HELP, rbHelpKeybindings },
	{ 5, 66, 109, RB_GROUP_HELP, rbHelpHowToUseFT2 },
	{ 5, 82, 101, RB_GROUP_HELP, rbHelpFAQ },
	{ 5, 98,  86, RB_GROUP_HELP, rbHelpKnownBugs },

	// ------ NIBBLES SCREEN RADIOBUTTONS ------
	//x, y,   w,   group,                       funcOnUp
	{  4, 105, 61, RB_GROUP_NIBBLES_PLAYERS,    nibblesSet1Player },
	{  4, 119, 68, RB_GROUP_NIBBLES_PLAYERS,    nibblesSet2Players },
	{ 79, 117, 55, RB_GROUP_NIBBLES_DIFFICULTY, nibblesSetNovice },
	{ 79, 131, 63, RB_GROUP_NIBBLES_DIFFICULTY, nibblesSetAverage },
	{ 79, 145, 34, RB_GROUP_NIBBLES_DIFFICULTY, nibblesSetPro },
	{ 79, 159, 50, RB_GROUP_NIBBLES_DIFFICULTY, nibblesSetTriton },

	// ------ SAMPLER SCREEN RADIOBUTTONS ------
	//x,   y,   w,  group,                 funcOnUp
	{ 357, 351, 58, RB_GROUP_SAMPLE_LOOP,  rbSampleNoLoop },
	{ 357, 368, 62, RB_GROUP_SAMPLE_LOOP,  rbSampleForwardLoop },
	{ 357, 385, 67, RB_GROUP_SAMPLE_LOOP,  rbSamplePingpongLoop },
	{ 431, 368, 44, RB_GROUP_SAMPLE_DEPTH, rbSample8bit },
	{ 431, 383, 50, RB_GROUP_SAMPLE_DEPTH, rbSample16bit },

	// ------ INSTRUMENT EDITOR SCREEN RADIOBUTTONS ------
	//x,   y,   w,  group,                  funcOnUp
	{ 442, 279, 25, RB_GROUP_INST_WAVEFORM, rbVibWaveSine },
	{ 472, 279, 25, RB_GROUP_INST_WAVEFORM, rbVibWaveSquare },
	{ 502, 279, 25, RB_GROUP_INST_WAVEFORM, rbVibWaveRampDown },
	{ 532, 279, 25, RB_GROUP_INST_WAVEFORM, rbVibWaveRampUp },

	// ------ CONFIG SCREEN LEFT RADIOBUTTONS ------
	//x, y,  w,  group,                  funcOnUp
	{ 5, 18, 48, RB_GROUP_CONFIG_SELECT, rbConfigAudio },
	{ 5, 34, 57, RB_GROUP_CONFIG_SELECT, rbConfigLayout },
	{ 5, 50, 97, RB_GROUP_CONFIG_SELECT, rbConfigMiscellaneous },
#ifdef HAS_MIDI
	{ 5, 66, 72, RB_GROUP_CONFIG_SELECT, rbConfigMidiInput },
#endif

	// ------ CONFIG AUDIO ------

	// audio buffer size
	//x,   y,  w,   group,                           funcOnUp
	{ 390, 16,  45, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, rbConfigAudioBuffSmall  },
	{ 390, 30, 112, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, rbConfigAudioBuffMedium },
	{ 390, 44,  49, RB_GROUP_CONFIG_SOUND_BUFF_SIZE, rbConfigAudioBuffLarge },

	// audio bit depth
	//x,   y,  w,  group,                           funcOnUp
	{ 390, 73, 51, RB_GROUP_CONFIG_AUDIO_BIT_DEPTH, rbConfigAudio16Bit },
	{ 453, 73, 51, RB_GROUP_CONFIG_AUDIO_BIT_DEPTH, rbConfigAudio32BitFloat },

	// audio interpolation
	//x,   y,   w,   group,                               funcOnUp
	{ 390,  90, 108, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpDisabled },
	{ 390, 104,  90, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpLinear },
	{ 390, 118, 109, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpQuadratic },
	{ 390, 132,  85, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpCubic },
	{ 390, 146,  94, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpSinc8 },
	{ 390, 160, 101, RB_GROUP_CONFIG_AUDIO_INTERPOLATION, rbConfigAudioIntrpSinc16 },

	// audio output frequency
	//x,   y,  w,  group,                      funcOnUp
	{ 513, 16, 65, RB_GROUP_CONFIG_AUDIO_FREQ, rbConfigAudio44kHz },
	{ 513, 30, 65, RB_GROUP_CONFIG_AUDIO_FREQ, rbConfigAudio48kHz },
	{ 513, 44, 65, RB_GROUP_CONFIG_AUDIO_FREQ, rbConfigAudio96kHz },

	// audio input frequency
	//x,   y,   w,  group,                            funcOnUp
	{ 180, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, rbConfigAudioInput44kHz },
	{ 251, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, rbConfigAudioInput48kHz },
	{ 322, 156, 60, RB_GROUP_CONFIG_AUDIO_INPUT_FREQ, rbConfigAudioInput96kHz },

	// frequency slides
	//x,   y,  w,   group,                       funcOnUp
	{ 513, 74,  49, RB_GROUP_CONFIG_FREQ_SLIDES, rbConfigFreqSlidesAmiga  },
	{ 513, 88, 107, RB_GROUP_CONFIG_FREQ_SLIDES, rbConfigFreqSlidesLinear },

	// ------ CONFIG LAYOUT ------

	// mouse shape
	//x,   y,   w,  group,                 funcOnUp
	{ 115, 120, 41, RB_GROUP_CONFIG_MOUSE, rbConfigMouseNice },
	{ 178, 120, 41, RB_GROUP_CONFIG_MOUSE, rbConfigMouseUgly },
	{ 115, 134, 47, RB_GROUP_CONFIG_MOUSE, rbConfigMouseAwful },
	{ 178, 134, 55, RB_GROUP_CONFIG_MOUSE, rbConfigMouseUsable },

	// mouse busy shape
	//x,   y,   w,  group,                      funcOnUp
	{ 115, 159, 51, RB_GROUP_CONFIG_MOUSE_BUSY, rbConfigMouseBusyVogue },
	{ 178, 159, 45, RB_GROUP_CONFIG_MOUSE_BUSY, rbConfigMouseBusyMrH },

	// scope style
	//x,   y,   w,  group,                 funcOnUp
	{ 305, 145, 38, RB_GROUP_CONFIG_SCOPE, rbConfigScopeStandard },
	{ 346, 145, 46, RB_GROUP_CONFIG_SCOPE, rbConfigScopeLined },

	// visible pattern channels
	//x,   y,  w,  group,                        funcOnUp
	{ 257, 42, 78, RB_GROUP_CONFIG_PATTERN_CHANS, rbConfigPatt4Chans },
	{ 257, 56, 78, RB_GROUP_CONFIG_PATTERN_CHANS, rbConfigPatt6Chans },
	{ 257, 70, 78, RB_GROUP_CONFIG_PATTERN_CHANS, rbConfigPatt8Chans },
	{ 257, 84, 85, RB_GROUP_CONFIG_PATTERN_CHANS, rbConfigPatt12Chans },

	// pattern font
	//x,   y,   w,  group,                funcOnUp
	{ 257, 114, 62, RB_GROUP_CONFIG_FONT, rbConfigFontCapitals },
	{ 323, 114, 68, RB_GROUP_CONFIG_FONT, rbConfigFontLowerCase },
	{ 257, 129, 54, RB_GROUP_CONFIG_FONT, rbConfigFontFuture },
	{ 323, 129, 40, RB_GROUP_CONFIG_FONT, rbConfigFontBold },

	// palette entries
	//x,   y,  w,  group,                       funcOnUp
	{ 399, 2,  88, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalPatternText },
	{ 399, 16, 79, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalBlockMark },
	{ 399, 30, 97, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalTextOnBlock },
	{ 399, 44, 52, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalMouse },
	{ 399, 58, 63, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalDesktop },
	{ 399, 72, 61, RB_GROUP_CONFIG_PAL_ENTRIES, rbConfigPalButttons },

	// palette presets
	//x,   y,   w,   group,                      funcOnUp
	{ 399,  89,  50, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalArctic },
	{ 512,  89,  81, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalLitheDark },
	{ 399, 103, 105, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalAuroraBorealis },
	{ 512, 103,  45, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalRose },
	{ 399, 117,  47, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalBlues },
	{ 512, 117,  77, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalDarkMode },
	{ 399, 131,  40, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalGold },
	{ 512, 131,  56, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalViolent },
	{ 399, 145,  87, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalHeavyMetal },
	{ 512, 145,  87, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalWhyColors },
	{ 399, 159,  54, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalJungle },
	{ 512, 159,  90, RB_GROUP_CONFIG_PAL_PRESET, rbConfigPalUserDefined },

	// ------ CONFIG MISCELLANEOUS ------

	// FILENAME SORTING
	//x,   y,  w,  group,                    funcOnUp
	{ 114, 15, 40, RB_GROUP_CONFIG_FILESORT, rbFileSortExt },
	{ 114, 29, 48, RB_GROUP_CONFIG_FILESORT, rbFileSortName },

	// WINDOW SIZE
	//x,   y,  w,  group,                    funcOnUp
	{ 114, 58, 60, RB_GROUP_CONFIG_WIN_SIZE, rbWinSizeAuto },
	{ 114, 72, 31, RB_GROUP_CONFIG_WIN_SIZE, rbWinSize1x },
	{ 156, 72, 31, RB_GROUP_CONFIG_WIN_SIZE, rbWinSize3x },
	{ 114, 86, 31, RB_GROUP_CONFIG_WIN_SIZE, rbWinSize2x },
	{ 156, 86, 31, RB_GROUP_CONFIG_WIN_SIZE, rbWinSize4x },

	// ------ DISK OP. ------

	// FILENAME SORTING
	//x, y,  w,  group,                funcOnUp
	{ 4, 16, 55, RB_GROUP_DISKOP_ITEM, rbDiskOpModule },
	{ 4, 30, 45, RB_GROUP_DISKOP_ITEM, rbDiskOpInstr },
	{ 4, 44, 56, RB_GROUP_DISKOP_ITEM, rbDiskOpSample },
	{ 4, 58, 59, RB_GROUP_DISKOP_ITEM, rbDiskOpPattern },
	{ 4, 72, 50, RB_GROUP_DISKOP_ITEM, rbDiskOpTrack },

	// MODULE SAVE AS FORMATS
	//x, y,  w,   group,                      funcOnUp
	{ 4, 100, 40, RB_GROUP_DISKOP_MOD_SAVEAS, rbDiskOpModSaveMod },
	{ 4, 114, 33, RB_GROUP_DISKOP_MOD_SAVEAS, rbDiskOpModSaveXm },
	{ 4, 128, 40, RB_GROUP_DISKOP_MOD_SAVEAS, rbDiskOpModSaveWav },

	// INSTRUMENT SAVE AS FORMATS
	//x, y,   w,  group,                      funcOnUp
	{ 4, 100, 29, RB_GROUP_DISKOP_INS_SAVEAS, NULL },

	// SAMPLE SAVE AS FORMATS
	//x, y,   w,  group,                      funcOnUp
	{ 4, 100, 40, RB_GROUP_DISKOP_SMP_SAVEAS, rbDiskOpSmpSaveRaw },
	{ 4, 114, 34, RB_GROUP_DISKOP_SMP_SAVEAS, rbDiskOpSmpSaveIff },
	{ 4, 128, 40, RB_GROUP_DISKOP_SMP_SAVEAS, rbDiskOpSmpSaveWav },

	// PATTERN SAVE AS FORMATS
	//x, y,   w,  group,                      funcOnUp
	{ 4, 100, 33, RB_GROUP_DISKOP_PAT_SAVEAS, NULL },

	// TRACK SAVE AS FORMATS
	//x, y,   w,  group,                      funcOnUp
	{ 4, 100, 31, RB_GROUP_DISKOP_TRK_SAVEAS, NULL },

	// WAV RENDERER BITDEPTH
	//x,   y,  w,  group,                        funcOnUp
	{ 130, 95, 52, RB_GROUP_WAV_RENDER_BITDEPTH, rbWavRenderBitDepth16 },
	{ 195, 95, 93, RB_GROUP_WAV_RENDER_BITDEPTH, rbWavRenderBitDepth32 }
};

void drawRadioButton(uint16_t radioButtonID)
{
	assert(radioButtonID < NUM_RADIOBUTTONS);
	radioButton_t *radioButton = &radioButtons[radioButtonID];
	if (!radioButton->visible)
		return;

	assert(radioButton->x < SCREEN_W && radioButton->y < SCREEN_H);

	const uint8_t *gfxPtr = &bmp.radiobuttonGfx[radioButton->state*(RADIOBUTTON_W*RADIOBUTTON_H)];
	blitFast(radioButton->x, radioButton->y, gfxPtr, RADIOBUTTON_W, RADIOBUTTON_H);
}

void showRadioButton(uint16_t radioButtonID)
{
	assert(radioButtonID < NUM_RADIOBUTTONS);
	radioButtons[radioButtonID].visible = true;
	drawRadioButton(radioButtonID);
}

void hideRadioButton(uint16_t radioButtonID)
{
	assert(radioButtonID < NUM_RADIOBUTTONS);
	radioButtons[radioButtonID].state = 0;
	radioButtons[radioButtonID].visible = false;
}

void checkRadioButton(uint16_t radioButtonID)
{
	assert(radioButtonID < NUM_RADIOBUTTONS);
	const uint16_t testGroup = radioButtons[radioButtonID].group;

	radioButton_t *radioButton = radioButtons;
	for (uint16_t i = 0; i < NUM_RADIOBUTTONS; i++, radioButton++)
	{
		if (radioButton->group == testGroup && radioButton->state == RADIOBUTTON_CHECKED)
		{
			radioButton->state = RADIOBUTTON_UNCHECKED;
			drawRadioButton(i);
			break;
		}
	}

	radioButtons[radioButtonID].state = RADIOBUTTON_CHECKED;
	drawRadioButton(radioButtonID);
}

void uncheckRadioButtonGroup(uint16_t radioButtonGroup)
{
	radioButton_t *radioButton = radioButtons;
	for (uint16_t i = 0; i < NUM_RADIOBUTTONS; i++, radioButton++)
	{
		if (radioButton->group == radioButtonGroup)
			radioButton->state = RADIOBUTTON_UNCHECKED;
	}
}

void showRadioButtonGroup(uint16_t radioButtonGroup)
{
	radioButton_t *radioButton = radioButtons;
	for (uint16_t i = 0; i < NUM_RADIOBUTTONS; i++, radioButton++)
	{
		if (radioButton->group == radioButtonGroup)
			showRadioButton(i);
	}
}

void hideRadioButtonGroup(uint16_t radioButtonGroup)
{
	radioButton_t *radioButton = radioButtons;
	for (uint16_t i = 0; i < NUM_RADIOBUTTONS; i++, radioButton++)
	{
		if (radioButton->group == radioButtonGroup)
			hideRadioButton(i);
	}
}

void handleRadioButtonsWhileMouseDown(void)
{
	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_RADIOBUTTONS);
	radioButton_t *radioButton = &radioButtons[mouse.lastUsedObjectID];
	if (!radioButton->visible || radioButton->state == RADIOBUTTON_CHECKED)
		return;

	radioButton->state = RADIOBUTTON_UNCHECKED;
	if (mouse.x >= radioButton->x && mouse.x < radioButton->x+radioButton->clickAreaWidth &&
	    mouse.y >= radioButton->y && mouse.y < radioButton->y+(RADIOBUTTON_H+1))
	{
		radioButton->state = RADIOBUTTON_PRESSED;
	}

	if (mouse.lastX != mouse.x || mouse.lastY != mouse.y)
	{
		mouse.lastX = mouse.x;
		mouse.lastY = mouse.y;

		drawRadioButton(mouse.lastUsedObjectID);
	}
}

bool testRadioButtonMouseDown(void)
{
	if (ui.sysReqShown)
		return false;

	const int32_t mx = mouse.x;
	const int32_t my = mouse.y;

	radioButton_t *radioButton = radioButtons;
	for (uint16_t i = 0; i < NUM_RADIOBUTTONS; i++, radioButton++)
	{
		if (!radioButton->visible || radioButton->state == RADIOBUTTON_CHECKED)
			continue;

		if (mx >= radioButton->x && mx < radioButton->x+radioButton->clickAreaWidth &&
		    my >= radioButton->y && my < radioButton->y+(RADIOBUTTON_H+1))
		{
			mouse.lastUsedObjectID = i;
			mouse.lastUsedObjectType = OBJECT_RADIOBUTTON;
			return true;
		}
	}

	return false;
}

void testRadioButtonMouseRelease(void)
{
	if (mouse.lastUsedObjectType != OBJECT_RADIOBUTTON || mouse.lastUsedObjectID == OBJECT_ID_NONE)
		return;

	assert(mouse.lastUsedObjectID < NUM_RADIOBUTTONS);
	radioButton_t *radioButton = &radioButtons[mouse.lastUsedObjectID];
	if (!radioButton->visible || radioButton->state == RADIOBUTTON_CHECKED)
		return;

	if (mouse.x >= radioButton->x && mouse.x < radioButton->x+radioButton->clickAreaWidth && 
	    mouse.y >= radioButton->y && mouse.y < radioButton->y+(RADIOBUTTON_H+1))
	{
		radioButton->state = RADIOBUTTON_UNCHECKED;
		drawRadioButton(mouse.lastUsedObjectID);

		if (radioButton->callbackFunc != NULL)
			radioButton->callbackFunc();
	}
}
