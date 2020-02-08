// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gfxdata.h"
#include "ft2_config.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_sample_ed.h"
#include "ft2_pattern_draw.h"
#include "ft2_inst_ed.h"
#include "ft2_scopes.h"
#include "ft2_diskop.h"
#include "ft2_audio.h"
#include "ft2_wav_renderer.h"
#include "ft2_mouse.h"
#include "ft2_video.h"

#include "ft2_module_loader.h"

// for pattern marking w/ keyboard
static int8_t lastChMark;
static int16_t lastRowMark;

// for pattern marking w/ mouse
static int32_t lastMarkX1 = -1, lastMarkX2 = -1, lastMarkY1 = -1, lastMarkY2 = -1;

static const uint8_t ptnAntLine[8] = { 27, 25, 20, 19, 42, 40, 31, 30 };
static const uint8_t ptnLineSub[8] = { 13, 12,  9,  9, 20, 19, 15, 14 };
static const uint8_t iSwitchExtW[4] = { 40, 40, 40, 39 };
static const uint8_t iSwitchExtY[8] = { 2, 2, 2, 2, 19, 19, 19, 19 };
static const uint8_t iSwitchY[8] = { 2, 19, 36, 53, 73, 90, 107, 124 };
static const uint16_t iSwitchExtX[4] = { 221, 262, 303, 344 };

static int32_t lastMouseX, lastMouseY;

// defined at the bottom of this file
extern const pattCoordsMouse_t pattCoordMouseTable[2][2][2];

// globalized
const uint16_t chanWidths[6] = { 141, 141, 93, 69, 45, 45 };

bool allocatePattern(uint16_t nr) // for tracker use only, not in loader!
{
	bool audioWasntLocked;

	if (patt[nr] == NULL)
	{
		audioWasntLocked = !audio.locked;
		if (audioWasntLocked)
			lockAudio();

		/* Original FT2 allocates only the amount of rows needed, but we don't
		 * do that to avoid out of bondary row look-up between out-of-sync replayer
		 * state and tracker state (yes it used to happen, rarely). We're not wasting
		 * too much RAM for a modern computer anyway. Worst case: 256 allocated
		 * patterns would be ~10MB.
		 */

		patt[nr] = (tonTyp *)calloc((MAX_PATT_LEN * TRACK_WIDTH) + 16, 1);
		if (patt[nr] == NULL)
		{
			if (audioWasntLocked)
				unlockAudio();

			return false;
		}

		// XXX: Do we really need this? Sounds redundant.
		song.pattLen = pattLens[nr];

		if (audioWasntLocked)
			unlockAudio();
	}

	return true;
}

void killPatternIfUnused(uint16_t nr) // for tracker use only, not in loader!
{
	bool audioWasntLocked;

	if (patternEmpty(nr))
	{
		audioWasntLocked = !audio.locked;
		if (audioWasntLocked)
			lockAudio();

		if (patt[nr] != NULL)
		{
			free(patt[nr]);
			patt[nr] = NULL;
		}

		if (audioWasntLocked)
			unlockAudio();
	}
}

uint8_t getMaxVisibleChannels(void)
{
	if (config.ptnS3M)
	{
		     if (config.ptnMaxChannels == 0) return 4;
		else if (config.ptnMaxChannels == 1) return 6;
		else if (config.ptnMaxChannels == 2) return 8;
		else if (config.ptnMaxChannels == 3) return 8;

	}
	else
	{
		     if (config.ptnMaxChannels == 0) return 4;
		else if (config.ptnMaxChannels == 1) return 6;
		else if (config.ptnMaxChannels == 2) return 8;
		else if (config.ptnMaxChannels == 3) return 12;
	}

	return 8;
}

void updatePatternWidth(void)
{
	if (editor.ui.numChannelsShown > editor.ui.maxVisibleChannels)
		editor.ui.numChannelsShown = editor.ui.maxVisibleChannels;

	assert(editor.ui.numChannelsShown >= 2 && editor.ui.numChannelsShown <= 12);

	editor.ui.patternChannelWidth = chanWidths[(editor.ui.numChannelsShown / 2) - 1] + 3;
}

void updateAdvEdit(void)
{
	hexOutBg(92, 113, PAL_FORGRND, PAL_DESKTOP, editor.srcInstr, 2);
	hexOutBg(92, 126, PAL_FORGRND, PAL_DESKTOP, editor.curInstr, 2);
}

void setAdvEditCheckBoxes(void)
{
	checkBoxes[CB_ENABLE_MASKING].checked = editor.copyMaskEnable;
	checkBoxes[CB_COPY_MASK_0].checked = editor.copyMask[0];
	checkBoxes[CB_COPY_MASK_1].checked = editor.copyMask[1];
	checkBoxes[CB_COPY_MASK_2].checked = editor.copyMask[2];
	checkBoxes[CB_COPY_MASK_3].checked = editor.copyMask[3];
	checkBoxes[CB_COPY_MASK_4].checked = editor.copyMask[4];
	checkBoxes[CB_PASTE_MASK_0].checked = editor.pasteMask[0];
	checkBoxes[CB_PASTE_MASK_1].checked = editor.pasteMask[1];
	checkBoxes[CB_PASTE_MASK_2].checked = editor.pasteMask[2];
	checkBoxes[CB_PASTE_MASK_3].checked = editor.pasteMask[3];
	checkBoxes[CB_PASTE_MASK_4].checked = editor.pasteMask[4];
	checkBoxes[CB_TRANSP_MASK_0].checked = editor.transpMask[0];
	checkBoxes[CB_TRANSP_MASK_1].checked = editor.transpMask[1];
	checkBoxes[CB_TRANSP_MASK_2].checked = editor.transpMask[2];
	checkBoxes[CB_TRANSP_MASK_3].checked = editor.transpMask[3];
	checkBoxes[CB_TRANSP_MASK_4].checked = editor.transpMask[4];

	showCheckBox(CB_ENABLE_MASKING);
	showCheckBox(CB_COPY_MASK_0);
	showCheckBox(CB_COPY_MASK_1);
	showCheckBox(CB_COPY_MASK_2);
	showCheckBox(CB_COPY_MASK_3);
	showCheckBox(CB_COPY_MASK_4);
	showCheckBox(CB_PASTE_MASK_0);
	showCheckBox(CB_PASTE_MASK_1);
	showCheckBox(CB_PASTE_MASK_2);
	showCheckBox(CB_PASTE_MASK_3);
	showCheckBox(CB_PASTE_MASK_4);
	showCheckBox(CB_TRANSP_MASK_0);
	showCheckBox(CB_TRANSP_MASK_1);
	showCheckBox(CB_TRANSP_MASK_2);
	showCheckBox(CB_TRANSP_MASK_3);
	showCheckBox(CB_TRANSP_MASK_4);
}

void drawAdvEdit(void)
{
	drawFramework(  0,  92, 110,  17, FRAMEWORK_TYPE1);
	drawFramework(  0, 109, 110,  64, FRAMEWORK_TYPE1);
	drawFramework(110,  92, 124,  81, FRAMEWORK_TYPE1);
	drawFramework(234,  92,  19,  81, FRAMEWORK_TYPE1);
	drawFramework(253,  92,  19,  81, FRAMEWORK_TYPE1);
	drawFramework(272,  92,  19,  81, FRAMEWORK_TYPE1);

	textOutShadow(  4,  96, PAL_FORGRND, PAL_DSKTOP2, "Instr. remap:");
	textOutShadow(  4, 113, PAL_FORGRND, PAL_DSKTOP2, "Old number");
	textOutShadow(  4, 126, PAL_FORGRND, PAL_DSKTOP2, "New number");
	textOutShadow(129,  96, PAL_FORGRND, PAL_DSKTOP2, "Masking enable");
	textOutShadow(114, 109, PAL_FORGRND, PAL_DSKTOP2, "Note");
	textOutShadow(114, 121, PAL_FORGRND, PAL_DSKTOP2, "Instrument number");
	textOutShadow(114, 134, PAL_FORGRND, PAL_DSKTOP2, "Volume column");
	textOutShadow(114, 147, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 1");
	textOutShadow(114, 160, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 2,3");

	charOutShadow(239, 95, PAL_FORGRND, PAL_DSKTOP2, 'C');
	charOutShadow(258, 95, PAL_FORGRND, PAL_DSKTOP2, 'P');
	charOutShadow(277, 95, PAL_FORGRND, PAL_DSKTOP2, 'T');

	showPushButton(PB_REMAP_TRACK);
	showPushButton(PB_REMAP_PATTERN);
	showPushButton(PB_REMAP_SONG);
	showPushButton(PB_REMAP_BLOCK);

	setAdvEditCheckBoxes();

	updateAdvEdit();
}

void hideAdvEdit(void)
{
	editor.ui.advEditShown = false;

	hidePushButton(PB_REMAP_TRACK);
	hidePushButton(PB_REMAP_PATTERN);
	hidePushButton(PB_REMAP_SONG);
	hidePushButton(PB_REMAP_BLOCK);

	hideCheckBox(CB_ENABLE_MASKING);
	hideCheckBox(CB_COPY_MASK_0);
	hideCheckBox(CB_COPY_MASK_1);
	hideCheckBox(CB_COPY_MASK_2);
	hideCheckBox(CB_COPY_MASK_3);
	hideCheckBox(CB_COPY_MASK_4);
	hideCheckBox(CB_PASTE_MASK_0);
	hideCheckBox(CB_PASTE_MASK_1);
	hideCheckBox(CB_PASTE_MASK_2);
	hideCheckBox(CB_PASTE_MASK_3);
	hideCheckBox(CB_PASTE_MASK_4);
	hideCheckBox(CB_TRANSP_MASK_0);
	hideCheckBox(CB_TRANSP_MASK_1);
	hideCheckBox(CB_TRANSP_MASK_2);
	hideCheckBox(CB_TRANSP_MASK_3);
	hideCheckBox(CB_TRANSP_MASK_4);

	editor.ui.scopesShown = true;
	drawScopeFramework();
}

void showAdvEdit(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	editor.ui.advEditShown = true;
	editor.ui.scopesShown  = false;
	drawAdvEdit();
}

void toggleAdvEdit(void)
{
	if (editor.ui.advEditShown)
		hideAdvEdit();
	else
		showAdvEdit();
}

void drawTranspose(void)
{
	drawFramework(0,    92,  53,  16, FRAMEWORK_TYPE1);
	drawFramework(53,   92, 119,  16, FRAMEWORK_TYPE1);
	drawFramework(172,  92, 119,  16, FRAMEWORK_TYPE1);
	drawFramework(0,   108,  53,  65, FRAMEWORK_TYPE1);
	drawFramework(53,  108, 119,  65, FRAMEWORK_TYPE1);
	drawFramework(172, 108, 119,  65, FRAMEWORK_TYPE1);

	textOutShadow(4,    95, PAL_FORGRND, PAL_DSKTOP2, "Transp.");
	textOutShadow(58,   95, PAL_FORGRND, PAL_DSKTOP2, "Current instrument");
	textOutShadow(188,  95, PAL_FORGRND, PAL_DSKTOP2, "All instruments");
	textOutShadow(4,   114, PAL_FORGRND, PAL_DSKTOP2, "Track");
	textOutShadow(4,   129, PAL_FORGRND, PAL_DSKTOP2, "Pattern");
	textOutShadow(4,   144, PAL_FORGRND, PAL_DSKTOP2, "Song");
	textOutShadow(4,   159, PAL_FORGRND, PAL_DSKTOP2, "Block");

	showPushButton(PB_TRANSP_CUR_INS_TRK_UP);
	showPushButton(PB_TRANSP_CUR_INS_TRK_DN);
	showPushButton(PB_TRANSP_CUR_INS_TRK_12UP);
	showPushButton(PB_TRANSP_CUR_INS_TRK_12DN);
	showPushButton(PB_TRANSP_ALL_INS_TRK_UP);
	showPushButton(PB_TRANSP_ALL_INS_TRK_DN);
	showPushButton(PB_TRANSP_ALL_INS_TRK_12UP);
	showPushButton(PB_TRANSP_ALL_INS_TRK_12DN);
	showPushButton(PB_TRANSP_CUR_INS_PAT_UP);
	showPushButton(PB_TRANSP_CUR_INS_PAT_DN);
	showPushButton(PB_TRANSP_CUR_INS_PAT_12UP);
	showPushButton(PB_TRANSP_CUR_INS_PAT_12DN);
	showPushButton(PB_TRANSP_ALL_INS_PAT_UP);
	showPushButton(PB_TRANSP_ALL_INS_PAT_DN);
	showPushButton(PB_TRANSP_ALL_INS_PAT_12UP);
	showPushButton(PB_TRANSP_ALL_INS_PAT_12DN);
	showPushButton(PB_TRANSP_CUR_INS_SNG_UP);
	showPushButton(PB_TRANSP_CUR_INS_SNG_DN);
	showPushButton(PB_TRANSP_CUR_INS_SNG_12UP);
	showPushButton(PB_TRANSP_CUR_INS_SNG_12DN);
	showPushButton(PB_TRANSP_ALL_INS_SNG_UP);
	showPushButton(PB_TRANSP_ALL_INS_SNG_DN);
	showPushButton(PB_TRANSP_ALL_INS_SNG_12UP);
	showPushButton(PB_TRANSP_ALL_INS_SNG_12DN);
	showPushButton(PB_TRANSP_CUR_INS_BLK_UP);
	showPushButton(PB_TRANSP_CUR_INS_BLK_DN);
	showPushButton(PB_TRANSP_CUR_INS_BLK_12UP);
	showPushButton(PB_TRANSP_CUR_INS_BLK_12DN);
	showPushButton(PB_TRANSP_ALL_INS_BLK_UP);
	showPushButton(PB_TRANSP_ALL_INS_BLK_DN);
	showPushButton(PB_TRANSP_ALL_INS_BLK_12UP);
	showPushButton(PB_TRANSP_ALL_INS_BLK_12DN);
}

void showTranspose(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	editor.ui.transposeShown = true;
	editor.ui.scopesShown = false;
	drawTranspose();
}

void hideTranspose(void)
{
	hidePushButton(PB_TRANSP_CUR_INS_TRK_UP);
	hidePushButton(PB_TRANSP_CUR_INS_TRK_DN);
	hidePushButton(PB_TRANSP_CUR_INS_TRK_12UP);
	hidePushButton(PB_TRANSP_CUR_INS_TRK_12DN);
	hidePushButton(PB_TRANSP_ALL_INS_TRK_UP);
	hidePushButton(PB_TRANSP_ALL_INS_TRK_DN);
	hidePushButton(PB_TRANSP_ALL_INS_TRK_12UP);
	hidePushButton(PB_TRANSP_ALL_INS_TRK_12DN);
	hidePushButton(PB_TRANSP_CUR_INS_PAT_UP);
	hidePushButton(PB_TRANSP_CUR_INS_PAT_DN);
	hidePushButton(PB_TRANSP_CUR_INS_PAT_12UP);
	hidePushButton(PB_TRANSP_CUR_INS_PAT_12DN);
	hidePushButton(PB_TRANSP_ALL_INS_PAT_UP);
	hidePushButton(PB_TRANSP_ALL_INS_PAT_DN);
	hidePushButton(PB_TRANSP_ALL_INS_PAT_12UP);
	hidePushButton(PB_TRANSP_ALL_INS_PAT_12DN);
	hidePushButton(PB_TRANSP_CUR_INS_SNG_UP);
	hidePushButton(PB_TRANSP_CUR_INS_SNG_DN);
	hidePushButton(PB_TRANSP_CUR_INS_SNG_12UP);
	hidePushButton(PB_TRANSP_CUR_INS_SNG_12DN);
	hidePushButton(PB_TRANSP_ALL_INS_SNG_UP);
	hidePushButton(PB_TRANSP_ALL_INS_SNG_DN);
	hidePushButton(PB_TRANSP_ALL_INS_SNG_12UP);
	hidePushButton(PB_TRANSP_ALL_INS_SNG_12DN);
	hidePushButton(PB_TRANSP_CUR_INS_BLK_UP);
	hidePushButton(PB_TRANSP_CUR_INS_BLK_DN);
	hidePushButton(PB_TRANSP_CUR_INS_BLK_12UP);
	hidePushButton(PB_TRANSP_CUR_INS_BLK_12DN);
	hidePushButton(PB_TRANSP_ALL_INS_BLK_UP);
	hidePushButton(PB_TRANSP_ALL_INS_BLK_DN);
	hidePushButton(PB_TRANSP_ALL_INS_BLK_12UP);
	hidePushButton(PB_TRANSP_ALL_INS_BLK_12DN);

	editor.ui.transposeShown = false;
	editor.ui.scopesShown = true;
	drawScopeFramework();
}

void toggleTranspose(void)
{
	if (editor.ui.transposeShown)
		hideTranspose();
	else
		showTranspose();
}

// ----- PATTERN CURSOR FUNCTIONS -----

void cursorChannelLeft(void)
{
	editor.cursor.object = CURSOR_EFX2;

	if (editor.cursor.ch == 0)
	{
		editor.cursor.ch = song.antChn - 1;
		if (editor.ui.pattChanScrollShown)
			setScrollBarPos(SB_CHAN_SCROLL, song.antChn, true);
	}
	else
	{
		editor.cursor.ch--;
		if (editor.ui.pattChanScrollShown)
		{
			if (editor.cursor.ch < editor.ui.channelOffset)
				scrollBarScrollUp(SB_CHAN_SCROLL, 1);
		}
	}
}

void cursorChannelRight(void)
{
	editor.cursor.object = CURSOR_NOTE;

	if (editor.cursor.ch >= song.antChn-1)
	{
		editor.cursor.ch = 0;
		if (editor.ui.pattChanScrollShown)
			setScrollBarPos(SB_CHAN_SCROLL, 0, true);
	}
	else
	{
		editor.cursor.ch++;
		if (editor.ui.pattChanScrollShown && editor.cursor.ch >= editor.ui.channelOffset+editor.ui.numChannelsShown)
			scrollBarScrollDown(SB_CHAN_SCROLL, 1);
	}
}

void cursorTabLeft(void)
{
	if (editor.cursor.object == CURSOR_NOTE)
		cursorChannelLeft();

	editor.cursor.object = CURSOR_NOTE;
	editor.ui.updatePatternEditor = true;
}

void cursorTabRight(void)
{
	cursorChannelRight();
	editor.cursor.object = CURSOR_NOTE;
	editor.ui.updatePatternEditor = true;
}

void chanLeft(void)
{
	cursorChannelLeft();
	editor.cursor.object = CURSOR_NOTE;
	editor.ui.updatePatternEditor = true;
}

void chanRight(void)
{
	cursorChannelRight();
	editor.cursor.object = CURSOR_NOTE;
	editor.ui.updatePatternEditor = true;
}

void cursorLeft(void)
{
	editor.cursor.object--;

	if (!config.ptnS3M)
	{
		while (editor.cursor.object == CURSOR_VOL1 || editor.cursor.object == CURSOR_VOL2)
			editor.cursor.object--;
	}

	if (editor.cursor.object == -1)
	{
		editor.cursor.object = CURSOR_EFX2;
		cursorChannelLeft();
	}

	editor.ui.updatePatternEditor = true;
}

void cursorRight(void)
{
	editor.cursor.object++;

	if (!config.ptnS3M)
	{
		while (editor.cursor.object == CURSOR_VOL1 || editor.cursor.object == CURSOR_VOL2)
			editor.cursor.object++;
	}

	if (editor.cursor.object == 8)
	{
		editor.cursor.object = CURSOR_NOTE;
		cursorChannelRight();
	}

	editor.ui.updatePatternEditor = true;
}

void showPatternEditor(void)
{
	editor.ui.patternEditorShown = true;
	updateChanNums();
	drawPatternBorders();
	editor.ui.updatePatternEditor = true;
}

void hidePatternEditor(void)
{
	hideScrollBar(SB_CHAN_SCROLL);
	hidePushButton(PB_CHAN_SCROLL_LEFT);
	hidePushButton(PB_CHAN_SCROLL_RIGHT);

	editor.ui.patternEditorShown = false;
}

static void updatePatternEditorGUI(void)
{
	uint8_t i;
	pushButton_t *p;
	textBox_t *t;

	if (editor.ui.extended)
	{
		// extended pattern editor

		// instrument names
		for (i = 0; i < 8; i++)
		{
			t = &textBoxes[TB_INST1+i];

			if (i < 4)
			{
				t->x = 406;
				t->y = 5 + (i * 11);
			}
			else
			{
				t->x = 529;
				t->y = 5 + ((i - 4) * 11);
			}

			t->w = 99;
			t->renderW = t->w - (t->tx * 2);
		}

		scrollBars[SB_POS_ED].h = 23;

		pushButtons[PB_POSED_POS_DOWN].y = 38;
		pushButtons[PB_POSED_PATT_UP].y = 20;
		pushButtons[PB_POSED_PATT_DOWN].y = 20;
		pushButtons[PB_POSED_DEL].y = 35;
		pushButtons[PB_SWAP_BANK].caption = "Swap b.";
		pushButtons[PB_SWAP_BANK].caption2 = NULL;
		pushButtons[PB_SWAP_BANK].x = 162;
		pushButtons[PB_SWAP_BANK].y = 35;
		pushButtons[PB_SWAP_BANK].w = 53;
		pushButtons[PB_SWAP_BANK].h = 16;
		pushButtons[PB_POSED_LEN_UP].x = 180;
		pushButtons[PB_POSED_LEN_UP].y = 3;
		pushButtons[PB_POSED_LEN_DOWN].x = 197;
		pushButtons[PB_POSED_LEN_DOWN].y = 3;
		pushButtons[PB_POSED_REP_UP].x = 180;
		pushButtons[PB_POSED_REP_UP].y = 17;
		pushButtons[PB_POSED_REP_DOWN].x = 197;
		pushButtons[PB_POSED_REP_DOWN].y = 17;
		pushButtons[PB_PATT_UP].x = 267;
		pushButtons[PB_PATT_UP].y = 37;
		pushButtons[PB_PATT_DOWN].x = 284;
		pushButtons[PB_PATT_DOWN].y = 37;
		pushButtons[PB_PATTLEN_UP].x = 348;
		pushButtons[PB_PATTLEN_UP].y = 37;
		pushButtons[PB_PATTLEN_DOWN].x = 365;
		pushButtons[PB_PATTLEN_DOWN].y = 37;

		// instrument switcher
		for (i = 0; i < 16; i++)
		{
			p = &pushButtons[PB_RANGE1+i];

			p->w = iSwitchExtW[i & 3];
			p->x = iSwitchExtX[i & 3];
			p->y = iSwitchExtY[i & 7];
		}
	}
	else
	{
		// instrument names
		for (i = 0; i < 8; i++)
		{
			t = &textBoxes[TB_INST1+i];

			t->y = 5 + (i * 11);
			t->x = 446;
			t->w = 140;
			t->renderW = t->w - (t->tx * 2);
		}

		// normal pattern editor

		scrollBars[SB_POS_ED].h = 21;

		pushButtons[PB_POSED_POS_DOWN].y = 36;
		pushButtons[PB_POSED_PATT_UP].y = 19;
		pushButtons[PB_POSED_PATT_DOWN].y = 19;
		pushButtons[PB_POSED_DEL].y = 33;
		pushButtons[PB_SWAP_BANK].caption = "Swap";
		pushButtons[PB_SWAP_BANK].caption2 = "bank";
		pushButtons[PB_SWAP_BANK].x = 590;
		pushButtons[PB_SWAP_BANK].y = 144;
		pushButtons[PB_SWAP_BANK].w = 39;
		pushButtons[PB_SWAP_BANK].h = 27;
		pushButtons[PB_POSED_LEN_UP].x = 74;
		pushButtons[PB_POSED_LEN_UP].y = 50;
		pushButtons[PB_POSED_LEN_DOWN].x = 91;
		pushButtons[PB_POSED_LEN_DOWN].y = 50;
		pushButtons[PB_POSED_REP_UP].x = 74;
		pushButtons[PB_POSED_REP_UP].y = 62;
		pushButtons[PB_POSED_REP_DOWN].x = 91;
		pushButtons[PB_POSED_REP_DOWN].y = 62;
		pushButtons[PB_PATT_UP].x = 253;
		pushButtons[PB_PATT_UP].y = 34;
		pushButtons[PB_PATT_DOWN].x = 270;
		pushButtons[PB_PATT_DOWN].y = 34;
		pushButtons[PB_PATTLEN_UP].x = 253;
		pushButtons[PB_PATTLEN_UP].y = 48;
		pushButtons[PB_PATTLEN_DOWN].x = 270;
		pushButtons[PB_PATTLEN_DOWN].y = 48;

		// instrument switcher
		for (i = 0; i < 16; i++)
		{
			p = &pushButtons[PB_RANGE1+i];

			p->w = 39;
			p->x = 590;
			p->y = iSwitchY[i & 7];
		}
	}
}

void patternEditorExtended(void)
{
	// backup old screen flags
	editor.ui._aboutScreenShown = editor.ui.aboutScreenShown;
	editor.ui._helpScreenShown = editor.ui.helpScreenShown;
	editor.ui._configScreenShown = editor.ui.configScreenShown;
	editor.ui._diskOpShown = editor.ui.diskOpShown;
	editor.ui._nibblesShown = editor.ui.nibblesShown;
	editor.ui._transposeShown = editor.ui.transposeShown;
	editor.ui._instEditorShown = editor.ui.instEditorShown;
	editor.ui._instEditorExtShown = editor.ui.instEditorExtShown;
	editor.ui._sampleEditorExtShown = editor.ui.sampleEditorExtShown;
	editor.ui._patternEditorShown = editor.ui.patternEditorShown;
	editor.ui._sampleEditorShown = editor.ui.sampleEditorShown;
	editor.ui._advEditShown= editor.ui.advEditShown;
	editor.ui._wavRendererShown = editor.ui.wavRendererShown;
	editor.ui._trimScreenShown = editor.ui.trimScreenShown;

	hideTopScreen();
	hideSampleEditor();
	hideInstEditor();

	editor.ui.extended = true;
	editor.ui.patternEditorShown = true;
	updatePatternEditorGUI(); // change pattern editor layout (based on ui.extended flag)
	editor.ui.updatePatternEditor = true; // redraw pattern editor

	drawFramework(0,    0, 112, 53, FRAMEWORK_TYPE1);
	drawFramework(112,  0, 106, 33, FRAMEWORK_TYPE1);
	drawFramework(112, 33, 106, 20, FRAMEWORK_TYPE1);
	drawFramework(218,  0, 168, 53, FRAMEWORK_TYPE1);

	// pos ed. stuff

	drawFramework(2,  2, 51, 20, FRAMEWORK_TYPE2);
	drawFramework(2, 31, 51, 20, FRAMEWORK_TYPE2);

	// force updating of end/page/length when showing scrollbar
	scrollBars[SB_POS_ED].oldEnd = 0xFFFFFFFF;
	scrollBars[SB_POS_ED].oldPage = 0xFFFFFFFF;
	scrollBars[SB_POS_ED].oldPos = 0xFFFFFFFF;

	showScrollBar(SB_POS_ED);

	showPushButton(PB_POSED_POS_UP);
	showPushButton(PB_POSED_POS_DOWN);
	showPushButton(PB_POSED_INS);
	showPushButton(PB_POSED_PATT_UP);
	showPushButton(PB_POSED_PATT_DOWN);
	showPushButton(PB_POSED_DEL);
	showPushButton(PB_POSED_LEN_UP);
	showPushButton(PB_POSED_LEN_DOWN);
	showPushButton(PB_POSED_REP_UP);
	showPushButton(PB_POSED_REP_DOWN);
	showPushButton(PB_SWAP_BANK);
	showPushButton(PB_PATT_UP);
	showPushButton(PB_PATT_DOWN);
	showPushButton(PB_PATTLEN_UP);
	showPushButton(PB_PATTLEN_DOWN);

	showPushButton(PB_EXIT_EXT_PATT);

	textOutShadow(116,  5, PAL_FORGRND, PAL_DSKTOP2, "Sng.len.");
	textOutShadow(116, 19, PAL_FORGRND, PAL_DSKTOP2, "Repst.");
	textOutShadow(222, 40, PAL_FORGRND, PAL_DSKTOP2, "Ptn.");
	textOutShadow(305, 40, PAL_FORGRND, PAL_DSKTOP2, "Ln.");

	editor.ui.instrSwitcherShown = true;
	showInstrumentSwitcher();

	drawSongLength();
	drawSongRepS();
	drawEditPattern(editor.editPattern);
	drawPatternLength(editor.editPattern);
	drawPosEdNums(editor.songPos);
	editor.ui.updatePosSections = true;

	// kludge to fix scrollbar thumb when the scrollbar height changes during playback
	if (songPlaying)
		setScrollBarPos(SB_POS_ED, editor.songPos, false);
}

void exitPatternEditorExtended(void)
{
	editor.ui.extended = false;
	updatePatternEditorGUI();
	hidePushButton(PB_EXIT_EXT_PATT);

	// set back top screen button maps

	// set back old screen flags
	editor.ui.aboutScreenShown = editor.ui._aboutScreenShown;
	editor.ui.helpScreenShown = editor.ui._helpScreenShown;
	editor.ui.configScreenShown = editor.ui._configScreenShown;
	editor.ui.diskOpShown = editor.ui._diskOpShown;
	editor.ui.nibblesShown = editor.ui._nibblesShown;
	editor.ui.transposeShown = editor.ui._transposeShown;
	editor.ui.instEditorShown = editor.ui._instEditorShown;
	editor.ui.instEditorExtShown = editor.ui._instEditorExtShown;
	editor.ui.sampleEditorExtShown = editor.ui._sampleEditorExtShown;
	editor.ui.patternEditorShown = editor.ui._patternEditorShown;
	editor.ui.sampleEditorShown = editor.ui._sampleEditorShown;
	editor.ui.advEditShown = editor.ui._advEditShown;
	editor.ui.wavRendererShown = editor.ui._wavRendererShown;
	editor.ui.trimScreenShown = editor.ui.trimScreenShown;

	showTopScreen(true);
	showBottomScreen();

	// kludge to fix scrollbar thumb when the scrollbar height changes during playback
	if (songPlaying)
		setScrollBarPos(SB_POS_ED, editor.songPos, false);
}

void togglePatternEditorExtended(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();
	else
		patternEditorExtended();
}

void clearPattMark(void)
{
	memset(&pattMark, 0, sizeof (pattMark));

	lastMarkX1 = -1;
	lastMarkX2 = -1;
	lastMarkY1 = -1;
	lastMarkY2 = -1;
}

void checkMarkLimits(void)
{
	uint16_t limit;

	limit = pattLens[editor.editPattern];
	pattMark.markY1 = CLAMP(pattMark.markY1, 0, limit);
	pattMark.markY2 = CLAMP(pattMark.markY2, 0, limit);

	limit = song.antChn - 1;
	pattMark.markX1 = CLAMP(pattMark.markX1, 0, limit);
	pattMark.markX2 = CLAMP(pattMark.markX2, 0, limit);

	// will probably never happen? FT2 has this in CheckMarkLimits() though...
	if (pattMark.markX1 > pattMark.markX2)
		pattMark.markX1 = pattMark.markX2;
}

static int8_t mouseXToCh(void) // used to get channel num from mouse x (for pattern marking)
{
	int8_t ch, chEnd;
	int32_t mouseX;

	assert(editor.ui.patternChannelWidth > 0);
	if (editor.ui.patternChannelWidth == 0)
		return 0;

	mouseX = mouse.x - 29;
	mouseX = CLAMP(mouseX, 0, 573);

	chEnd = (editor.ui.channelOffset + editor.ui.numChannelsShown) - 1;

	ch = editor.ui.channelOffset + (int8_t)(mouseX / editor.ui.patternChannelWidth);
	ch = CLAMP(ch, 0, chEnd);

	// in some setups there can be non-used channels to the right, do clamping
	if (ch >= song.antChn)
		ch  = song.antChn - 1;

	return ch;
}

static int16_t mouseYToRow(void) // used to get row num from mouse y (for pattern marking)
{
	uint8_t charHeight, mode;
	int16_t row, patternLen, my, maxY, maxRow;
	const pattCoordsMouse_t *pattCoordsMouse;

	pattCoordsMouse = &pattCoordMouseTable[config.ptnUnpressed][editor.ui.pattChanScrollShown][editor.ui.extended];

	// clamp mouse y to boundaries
	maxY = editor.ui.pattChanScrollShown ? 382 : 396;
	my   = (int16_t)(CLAMP(mouse.y, pattCoordsMouse->upperRowsY, maxY));

	charHeight = config.ptnUnpressed ? 11 : 8;

	// test top/middle/bottom rows
	if (my < pattCoordsMouse->midRowY)
	{
		// top rows
		row = editor.pattPos - (pattCoordsMouse->numUpperRows - ((my - pattCoordsMouse->upperRowsY) / charHeight));
		if (row < 0)
			row = 0;

		return row;
	}
	else if (my >= pattCoordsMouse->midRowY && my <= pattCoordsMouse->midRowY+10)
	{
		// current row (middle)
		return editor.pattPos;
	}
	else
	{
		// bottom rows
		row = (editor.pattPos + 1) + ((my - pattCoordsMouse->lowerRowsY) / charHeight);

		// prevent being able to mark the next unseen row on the bottom (in some configurations)
		mode = (editor.ui.extended * 4) + (config.ptnUnpressed * 2) + editor.ui.pattChanScrollShown;

		maxRow = (ptnAntLine[mode] + (editor.pattPos - ptnLineSub[mode])) - 1;
		if (row > maxRow)
			row = maxRow;

		// clamp to pattern length
		patternLen = pattLens[editor.editPattern];
		if (row >= patternLen)
			row = patternLen - 1;

		return row;
	}
}

void handlePatternDataMouseDown(bool mouseButtonHeld)
{
	bool forceMarking;
	int8_t chTmp;
	int16_t y1, y2, rowTmp, pattLen;

	// non-FT2 feature: Use right mouse button to remove pattern marking
	if (mouse.rightButtonPressed)
	{
		clearPattMark();
		editor.ui.updatePatternEditor = true;
		return;
	}

	if (!mouseButtonHeld)
	{
		// we clicked inside the pattern data area for the first time, set initial vars

		mouse.lastUsedObjectType = OBJECT_PATTERNMARK;

		lastMouseX = mouse.x;
		lastMouseY = mouse.y;

		lastChMark = mouseXToCh();
		lastRowMark = mouseYToRow();

		pattMark.markX1 = lastChMark;
		pattMark.markX2 = lastChMark;
		pattMark.markY1 = lastRowMark;
		pattMark.markY2 = lastRowMark + 1;

		checkMarkLimits();

		editor.ui.updatePatternEditor = true;
		return;
	}

	// we're holding down the mouse button inside the pattern data area

	forceMarking = songPlaying;

	// scroll left/right with mouse
	if (editor.ui.pattChanScrollShown)
	{
		if (mouse.x < 29)
		{
			scrollBarScrollUp(SB_CHAN_SCROLL, 1);
			forceMarking = true;
		}
		else if (mouse.x > 604)
		{
			scrollBarScrollDown(SB_CHAN_SCROLL, 1);
			forceMarking = true;
		}
	}

	// mark channels
	if (forceMarking || lastMouseX != mouse.x)
	{
		lastMouseX = mouse.x;

		chTmp = mouseXToCh();
		if (chTmp < lastChMark)
		{
			pattMark.markX1 = chTmp;
			pattMark.markX2 = lastChMark;
		}
		else
		{
			pattMark.markX2 = chTmp;
			pattMark.markX1 = lastChMark;
		}

		if (lastMarkX1 != pattMark.markX1 || lastMarkX2 != pattMark.markX2)
		{
			checkMarkLimits();
			editor.ui.updatePatternEditor = true;

			lastMarkX1 = pattMark.markX1;
			lastMarkX2 = pattMark.markX2;
		}
	}

	// scroll down/up with mouse (if song is not playing)
	if (!songPlaying)
	{
		y1 = editor.ui.extended ? 56 : 176;
		y2 = editor.ui.pattChanScrollShown ? 382 : 396;

		if (mouse.y < y1)
		{
			pattLen = pattLens[editor.editPattern];
			if (editor.pattPos > 0)
				setPos(-1, editor.pattPos - 1, true);

			forceMarking = true;
			editor.ui.updatePatternEditor = true;
		}
		else if (mouse.y > y2)
		{
			pattLen = pattLens[editor.editPattern];
			if (editor.pattPos < (pattLen - 1))
				setPos(-1, editor.pattPos + 1, true);

			forceMarking = true;
			editor.ui.updatePatternEditor = true;
		}
	}

	// mark rows
	if (forceMarking || lastMouseY != mouse.y)
	{
		lastMouseY = mouse.y;

		rowTmp = mouseYToRow();
		if (rowTmp < lastRowMark)
		{
			pattMark.markY1 = rowTmp;
			pattMark.markY2 = lastRowMark + 1;
		}
		else
		{
			pattMark.markY2 = rowTmp + 1;
			pattMark.markY1 = lastRowMark;
		}

		if (lastMarkY1 != pattMark.markY1 || lastMarkY2 != pattMark.markY2)
		{
			checkMarkLimits();
			editor.ui.updatePatternEditor = true;

			lastMarkY1 = pattMark.markY1;
			lastMarkY2 = pattMark.markY2;
		}
	}
}

void rowOneUpWrap(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos = (song.pattPos - 1 + song.pattLen) % song.pattLen;
	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		editor.ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowOneDownWrap(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPlaying)
	{
		song.timer = 2;
	}
	else
	{
		song.pattPos = (song.pattPos + 1 + song.pattLen) % song.pattLen;
		editor.pattPos = (uint8_t)song.pattPos;
		editor.ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowUp(uint16_t amount)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos -= amount;
	if (song.pattPos < 0)
		song.pattPos = 0;

	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		editor.ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowDown(uint16_t amount)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos += amount;
	if (song.pattPos >= song.pattLen)
		song.pattPos = song.pattLen - 1;

	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		editor.ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void keybPattMarkUp(void)
{
	int8_t xPos;
	int16_t pattPos;

	xPos = editor.cursor.ch;
	pattPos = editor.pattPos;

	if (xPos != pattMark.markX1 && xPos != pattMark.markX2)
	{
		pattMark.markX1 = xPos;
		pattMark.markX2 = xPos;
		pattMark.markY1 = pattPos;
		pattMark.markY2 = pattPos + 1;
	}

	if (pattPos == pattMark.markY1-1)
	{
		pattMark.markY1 = pattPos;
	}
	else if (pattPos == pattMark.markY2)
	{
		pattMark.markY2 = pattPos - 1;
	}
	else if (pattPos != pattMark.markY1 && pattPos != pattMark.markY2)
	{
		pattMark.markX1 = xPos;
		pattMark.markX2 = xPos;
		pattMark.markY1 = pattPos;
		pattMark.markY2 = pattPos + 1;

	}

	checkMarkLimits();
	rowOneUpWrap();
}

void keybPattMarkDown(void)
{
	int8_t xPos;
	int16_t pattPos;

	xPos = editor.cursor.ch;
	pattPos = editor.pattPos;

	if (xPos != pattMark.markX1 && xPos != pattMark.markX2)
	{
		pattMark.markX1 = xPos;
		pattMark.markX2 = xPos;
		pattMark.markY1 = pattPos;
		pattMark.markY2 = pattPos + 1;
	}

	if (pattPos == pattMark.markY2)
	{
		pattMark.markY2 = pattPos + 1;
	}
	else if (pattPos == pattMark.markY1-1)
	{
		pattMark.markY1 = pattPos + 2;
	}
	else if (pattPos != pattMark.markY1 && pattPos != pattMark.markY2)
	{
		pattMark.markX1 = xPos;
		pattMark.markX2 = xPos;
		pattMark.markY1 = pattPos;
		pattMark.markY2 = pattPos + 1;
	}

	checkMarkLimits();
	rowOneDownWrap();
}

void keybPattMarkLeft(void)
{
	int8_t xPos;
	int16_t pattPos;

	xPos = editor.cursor.ch;
	pattPos = editor.pattPos;

	if (pattPos != pattMark.markY1-1 && pattPos != pattMark.markY2)
	{
		pattMark.markY1 = pattPos - 1;
		pattMark.markY2 = pattPos;
	}

	if (xPos == pattMark.markX1)
	{
		pattMark.markX1 = xPos - 1;
	}
	else if (xPos == pattMark.markX2)
	{
		pattMark.markX2 = xPos - 1;
	}
	else if (xPos != pattMark.markX1 && xPos != pattMark.markX2)
	{
		pattMark.markX1 = xPos - 1;
		pattMark.markX2 = xPos;
		pattMark.markY1 = pattPos - 1;
		pattMark.markY2 = pattPos;
	}

	checkMarkLimits();
	chanLeft();
}

void keybPattMarkRight(void)
{
	int8_t xPos;
	int16_t pattPos;

	xPos = editor.cursor.ch;
	pattPos = editor.pattPos;

	if (pattPos != pattMark.markY1-1 && pattPos != pattMark.markY2)
	{
		pattMark.markY1 = pattPos - 1;
		pattMark.markY2 = pattPos;
	}

	if (xPos == pattMark.markX2)
	{
		pattMark.markX2 = xPos + 1;
	}
	else if (xPos == pattMark.markX1)
	{
		pattMark.markX1 = xPos + 1;
	}
	else if (xPos != pattMark.markX1 && xPos != pattMark.markX2)
	{
		pattMark.markX1 = xPos;
		pattMark.markX2 = xPos + 1;
		pattMark.markY1 = pattPos - 1;
		pattMark.markY2 = pattPos;
	}

	checkMarkLimits();
	chanRight();
}

bool loadTrack(UNICHAR *filenameU)
{
	FILE *f;
	uint16_t nr, pattLen;
	tonTyp *pattPtr, loadBuff[MAX_PATT_LEN];
	trackHeaderType th;

	f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
	{
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	nr = editor.editPattern;
	pattLen = pattLens[nr];

	if (fread(&th, 1, sizeof (th), f) != sizeof (th))
	{
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		goto trackLoadError;
	}

	if (th.ver != 1)
	{
		okBox(0, "System message", "Incompatible format version!");
		goto trackLoadError;
	}

	if (th.len > MAX_PATT_LEN)
		th.len = MAX_PATT_LEN;

	if (pattLen > th.len)
		pattLen = th.len;

	if (fread(loadBuff, pattLen * sizeof (tonTyp), 1, f) != 1)
	{
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		goto trackLoadError;
	}

	if (!allocatePattern(nr))
	{
		okBox(0, "System message", "Not enough memory!");
		goto trackLoadError;
	}

	pattPtr = patt[nr];

	lockMixerCallback();
	for (uint16_t i = 0; i < pattLen; i++)
	{
		pattPtr = &patt[nr][(i * MAX_VOICES) + editor.cursor.ch];
		*pattPtr = loadBuff[i];

		// non-FT2 security fix: remove overflown (illegal) stuff
		if (pattPtr->ton > 97)
			pattPtr->ton = 0;

		if (pattPtr->effTyp > 35)
		{
			pattPtr->effTyp = 0;
			pattPtr->eff = 0;
		}
	}

	unlockMixerCallback();

	fclose(f);

	editor.ui.updatePatternEditor = true;
	editor.ui.updatePosSections = true;

	diskOpSetFilename(DISKOP_ITEM_TRACK, filenameU);
	setSongModifiedFlag();

	return true;

trackLoadError:
	fclose(f);
	return false;
}

bool saveTrack(UNICHAR *filenameU)
{
	FILE *f;
	uint16_t nr, pattLen, i;
	tonTyp *pattPtr, saveBuff[MAX_PATT_LEN];
	trackHeaderType th;

	nr = editor.editPattern;
	pattPtr = patt[nr];

	if (pattPtr == NULL)
	{
		okBox(0, "System message", "The current pattern is empty!");
		return false;
	}

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	pattLen = pattLens[nr];
	for (i = 0; i < pattLen; i++)
		saveBuff[i] = pattPtr[(i * MAX_VOICES) + editor.cursor.ch];

	th.len = pattLen;
	th.ver = 1;

	if (fwrite(&th, sizeof (th), 1, f) !=  1)
	{
		fclose(f);
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	if (fwrite(saveBuff, pattLen * sizeof (tonTyp), 1, f) != 1)
	{
		fclose(f);
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	fclose(f);
	return true;
}

bool loadPattern(UNICHAR *filenameU)
{
	FILE *f;
	uint16_t nr, pattLen;
	tonTyp *pattPtr;
	patternHeaderType th;

	f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
	{
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	nr = editor.editPattern;

	if (!allocatePattern(nr))
	{
		okBox(0, "System message", "Not enough memory!");
		goto loadPattError;
	}

	pattPtr = patt[nr];
	pattLen = pattLens[nr];

	if (fread(&th, 1, sizeof (th), f) != sizeof (th))
	{
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		goto loadPattError;
	}

	if (th.ver != 1)
	{
		okBox(0, "System message", "Incompatible format version!");
		goto loadPattError;
	}

	if (th.len > MAX_PATT_LEN)
		th.len = MAX_PATT_LEN;

	if (pattLen > th.len)
		pattLen = th.len;

	lockMixerCallback();

	if (fread(pattPtr, pattLen * TRACK_WIDTH, 1, f) != 1)
	{
		unlockMixerCallback();
		okBox(0, "System message", "General I/O error during loading! Is the file in use?");
		goto loadPattError;
	}

	// non-FT2 security fix: remove overflown (illegal) stuff
	for (uint16_t i = 0; i < pattLen; i++)
	{
		for (uint16_t j = 0; j < MAX_VOICES; j++)
		{
			pattPtr = &patt[nr][(i * MAX_VOICES) + j];
			if (pattPtr->ton > 97)
				pattPtr->ton = 0;

			if (pattPtr->effTyp > 35)
			{
				pattPtr->effTyp = 0;
				pattPtr->eff = 0;
			}
		}
	}

	unlockMixerCallback();

	fclose(f);

	editor.ui.updatePatternEditor = true;
	editor.ui.updatePosSections = true;

	diskOpSetFilename(DISKOP_ITEM_PATTERN, filenameU);
	setSongModifiedFlag();

	return true;

loadPattError:
	fclose(f);
	return false;
}

bool savePattern(UNICHAR *filenameU)
{
	FILE *f;
	uint16_t nr, pattLen;
	tonTyp *pattPtr;
	patternHeaderType th;

	nr = editor.editPattern;
	pattPtr = patt[nr];

	if (pattPtr == NULL)
	{
		okBox(0, "System message", "The current pattern is empty!");
		return false;
	}

	f = UNICHAR_FOPEN(filenameU, "wb");
	if (f == NULL)
	{
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	pattLen = pattLens[nr];

	th.len = pattLen;
	th.ver = 1;

	if (fwrite(&th, 1, sizeof (th), f) != sizeof (th))
	{
		fclose(f);
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	if (fwrite(pattPtr, pattLen * TRACK_WIDTH, 1, f) != 1)
	{
		fclose(f);
		okBox(0, "System message", "General I/O error during saving! Is the file in use?");
		return false;
	}

	fclose(f);
	return true;
}

void scrollChannelLeft(void)
{
	scrollBarScrollLeft(SB_CHAN_SCROLL, 1);
}

void scrollChannelRight(void)
{
	scrollBarScrollRight(SB_CHAN_SCROLL, 1);
}

void setChannelScrollPos(uint32_t pos)
{
	if (!editor.ui.pattChanScrollShown)
	{
		editor.ui.channelOffset = 0;
		return;
	}

	if (editor.ui.channelOffset == (uint8_t)pos)
		return;

	editor.ui.channelOffset = (uint8_t)pos;

	assert(song.antChn > editor.ui.numChannelsShown);
	if (editor.ui.channelOffset >= song.antChn-editor.ui.numChannelsShown)
		editor.ui.channelOffset = song.antChn-editor.ui.numChannelsShown;

	if (editor.cursor.ch >= editor.ui.channelOffset+editor.ui.numChannelsShown)
	{
		editor.cursor.object = CURSOR_NOTE;
		editor.cursor.ch = (editor.ui.channelOffset + editor.ui.numChannelsShown) - 1;
	}
	else if (editor.cursor.ch < editor.ui.channelOffset)
	{
		editor.cursor.object = CURSOR_NOTE;
		editor.cursor.ch = editor.ui.channelOffset;
	}

	editor.ui.updatePatternEditor = true;
}

void jumpToChannel(uint8_t channel) // for ALT+q..i ALT+a..k
{
	if (editor.ui.sampleEditorShown || editor.ui.instEditorShown)
		return;

	channel %= song.antChn;
	if (editor.cursor.ch == channel)
		return;

	if (editor.ui.pattChanScrollShown)
	{
		assert(song.antChn > editor.ui.numChannelsShown);

		if (channel >= editor.ui.channelOffset+editor.ui.numChannelsShown)
			scrollBarScrollDown(SB_CHAN_SCROLL, (channel - (editor.ui.channelOffset + editor.ui.numChannelsShown)) + 1);
		else if (channel < editor.ui.channelOffset)
			scrollBarScrollUp(SB_CHAN_SCROLL, editor.ui.channelOffset - channel);
	}

	editor.cursor.ch = channel; // set it here since scrollBarScrollX() changes it...
	editor.ui.updatePatternEditor = true;
}

void sbPosEdPos(uint32_t pos)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos != (int16_t)pos)
		setPos((int16_t)pos, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdPosUp(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos < song.len-1)
		setPos(song.songPos + 1, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdPosDown(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos > 0)
		setPos(song.songPos - 1, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdIns(void)
{
	uint8_t oldPatt;

	if (song.len >= 255)
		return;

	lockMixerCallback();

	oldPatt = song.songTab[song.songPos];
	for (uint16_t i = 0; i < 255-song.songPos; i++)
		song.songTab[255-i] = song.songTab[254-i];
	song.songTab[song.songPos] = oldPatt;

	song.len++;

	editor.ui.updatePosSections = true;
	editor.ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	unlockMixerCallback();
}

void pbPosEdDel(void)
{
	if (song.len <= 1)
		return;

	lockMixerCallback();

	if (song.songPos < 254)
	{
		for (uint16_t i = 0; i < 254-song.songPos; i++)
			song.songTab[song.songPos+i] = song.songTab[song.songPos+1+i];
	}

	song.len--;
	if (song.repS >= song.len)
		song.repS = song.len - 1;

	if (song.songPos > song.len-1)
	{
		editor.songPos = song.songPos = song.len-1;
		setPos(song.songPos, -1, false);
	}

	editor.ui.updatePosSections = true;
	editor.ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	unlockMixerCallback();
}

void pbPosEdPattUp(void)
{
	if (song.songTab[song.songPos] == 255)
		return;

	lockMixerCallback();
	if (song.songTab[song.songPos] < 255)
	{
		song.songTab[song.songPos]++;
		song.pattNr = song.songTab[song.songPos];
		editor.editPattern = (uint8_t)song.pattNr;
		song.pattLen = pattLens[editor.editPattern];

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}
	unlockMixerCallback();
}

void pbPosEdPattDown(void)
{
	if (song.songTab[song.songPos] == 0)
		return;

	lockMixerCallback();
	if (song.songTab[song.songPos] > 0)
	{
		song.songTab[song.songPos]--;
		song.pattNr = song.songTab[song.songPos];
		editor.editPattern = (uint8_t)song.pattNr;
		song.pattLen = pattLens[editor.editPattern];

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}
	unlockMixerCallback();
}

void pbPosEdLenUp(void)
{
	bool audioWasntLocked;

	if (song.len >= 255)
		return;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.len++;

	editor.ui.updatePosSections = true;
	editor.ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdLenDown(void)
{
	bool audioWasntLocked;

	if (song.len == 0)
		return;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.len--;

	if (song.repS >= song.len)
		song.repS = song.len - 1;

	if (song.songPos >= song.len)
	{
		song.songPos = song.len - 1;
		setPos(song.songPos, -1, false);
	}

	editor.ui.updatePosSections = true;
	editor.ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdRepSUp(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.repS < song.len-1)
	{
		song.repS++;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdRepSDown(void)
{
	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.repS > 0)
	{
		song.repS--;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbBPMUp(void)
{
	if (song.speed == 255)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.speed < 255)
	{
		song.speed++;
		setSpeed(song.speed);

		// if song is playing, the update is handled in the audio/video sync queue
		if (!songPlaying)
		{
			editor.speed = song.speed;
			drawSongBPM(song.speed);
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbBPMDown(void)
{
	if (song.speed == 32)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.speed > 32)
	{
		song.speed--;
		setSpeed(song.speed);

		// if song is playing, the update is handled in the audio/video sync queue
		if (!songPlaying)
		{
			editor.speed = song.speed;
			drawSongBPM(editor.speed);
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbSpeedUp(void)
{
	if (song.tempo == 31)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.tempo < 31)
	{
		song.tempo++;

		// if song is playing, the update is handled in the audio/video sync queue
		if (!songPlaying)
		{
			editor.tempo = song.tempo;
			drawSongSpeed(editor.tempo);
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbSpeedDown(void)
{
	if (song.tempo == 0)
		return;

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.tempo > 0)
	{
		song.tempo--;

		// if song is playing, the update is handled in the audio/video sync queue
		if (!songPlaying)
		{
			editor.tempo = song.tempo;
			drawSongSpeed(editor.tempo);
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbIncAdd(void)
{
	if (editor.ID_Add == 16)
		editor.ID_Add = 0;
	else
		editor.ID_Add++;

	drawIDAdd();
}

void pbDecAdd(void)
{
	if (editor.ID_Add == 0)
		editor.ID_Add = 16;
	else
		editor.ID_Add--;

	drawIDAdd();
}

void pbAddChan(void)
{
	if (song.antChn > 30)
		return;

	lockMixerCallback();
	song.antChn += 2;

	hideTopScreen();
	showTopLeftMainScreen(true);
	showTopRightMainScreen();

	if (editor.ui.patternEditorShown)
		showPatternEditor();

	setSongModifiedFlag();
	unlockMixerCallback();
}

void pbSubChan(void)
{
	if (song.antChn < 4)
		return;

	lockMixerCallback();

	song.antChn -= 2;

	checkMarkLimits();

	hideTopScreen();
	showTopLeftMainScreen(true);
	showTopRightMainScreen();

	if (editor.ui.patternEditorShown)
		showPatternEditor();

	setSongModifiedFlag();
	unlockMixerCallback();
}

static void updatePtnLen(void)
{
	uint16_t len = pattLens[editor.editPattern];

	song.pattLen = len;
	if (song.pattPos >= len)
	{
		song.pattPos = len - 1;
		editor.pattPos = song.pattPos;
	}

	checkMarkLimits();
}

void pbEditPattUp(void)
{
	if (songPlaying)
	{
		if (song.pattNr == 255)
			return;
	}
	else
	{
		if (editor.editPattern == 255)
			return;
	}

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPlaying)
	{
		if (song.pattNr < 255)
		{
			song.pattNr++;
			updatePtnLen();

			editor.ui.updatePatternEditor = true;
			editor.ui.updatePosSections = true;
		}
	}
	else
	{
		if (editor.editPattern < 255)
		{
			editor.editPattern++;

			song.pattNr = editor.editPattern;
			updatePtnLen();

			editor.ui.updatePatternEditor = true;
			editor.ui.updatePosSections = true;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbEditPattDown(void)
{
	if (songPlaying)
	{
		if (song.pattNr == 0)
			return;
	}
	else
	{
		if (editor.editPattern == 0)
			return;
	}

	bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPlaying)
	{
		if (song.pattNr > 0)
		{
			song.pattNr--;
			updatePtnLen();

			editor.ui.updatePatternEditor = true;
			editor.ui.updatePosSections = true;
		}
	}
	else
	{
		if (editor.editPattern > 0)
		{
			editor.editPattern--;

			song.pattNr = editor.editPattern;
			updatePtnLen();

			editor.ui.updatePatternEditor = true;
			editor.ui.updatePosSections = true;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbPattLenUp(void)
{
	bool audioWasntLocked;
	uint16_t pattLen;

	if (pattLens[editor.editPattern] >= 256)
		return;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	pattLen = pattLens[editor.editPattern];
	if (pattLen < 256)
	{
		setPatternLen(editor.editPattern, pattLen + 1);
		checkMarkLimits();

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbPattLenDown(void)
{
	bool audioWasntLocked;
	uint16_t pattLen;

	if (pattLens[editor.editPattern] <= 1)
		return;

	audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	pattLen = pattLens[editor.editPattern];
	if (pattLen > 1)
	{
		setPatternLen(editor.editPattern, pattLen - 1);
		checkMarkLimits();

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void drawPosEdNums(int16_t songPos)
{
	uint8_t y;
	int16_t entry;
	uint32_t color1, color2;

	color1 = video.palette[PAL_PATTEXT];
	color2 = video.palette[PAL_FORGRND];

	if (songPos >= song.len)
		songPos = song.len - 1;

	// clear
	if (editor.ui.extended)
	{
		clearRect(8,  4, 39, 16);
		fillRect(8, 23, 39, 7, PAL_DESKTOP);
		clearRect(8, 33, 39, 16);
	}
	else
	{
		clearRect(8,  4, 39, 15);
		fillRect(8, 22, 39, 7, PAL_DESKTOP);
		clearRect(8, 32, 39, 15);
	}

	// top two
	for (y = 0; y < 2; y++)
	{
		entry = songPos - (2 - y);
		if (entry < 0)
			continue;

		assert(entry < 256);

		if (editor.ui.extended)
		{
			pattTwoHexOut(8,  4 + (y * 9), (uint8_t)entry, color1);
			pattTwoHexOut(32, 4 + (y * 9), song.songTab[entry], color1);
		}
		else
		{
			pattTwoHexOut(8,  4 + (y * 8), (uint8_t)entry, color1);
			pattTwoHexOut(32, 4 + (y * 8), song.songTab[entry], color1);
		}
	}

	assert(songPos < 256);

	// middle
	if (editor.ui.extended)
	{
		pattTwoHexOut(8,  23, (uint8_t)songPos, color2);
		pattTwoHexOut(32, 23, song.songTab[songPos], color2);
	}
	else
	{
		pattTwoHexOut(8,  22, (uint8_t)songPos, color2);
		pattTwoHexOut(32, 22, song.songTab[songPos], color2);
	}

	// bottom two
	for (y = 0; y < 2; y++)
	{
		entry = songPos + (1 + y);
		if (entry >= song.len)
			break;

		if (editor.ui.extended)
		{
			pattTwoHexOut(8,  33 + (y * 9), (uint8_t)entry, color1);
			pattTwoHexOut(32, 33 + (y * 9), song.songTab[entry], color1);
		}
		else
		{
			pattTwoHexOut(8,  32 + (y * 8), (uint8_t)entry, color1);
			pattTwoHexOut(32, 32 + (y * 8), song.songTab[entry], color1);
		}
	}
}

void drawSongLength(void)
{
	int16_t x, y;

	if (editor.ui.extended)
	{
		x = 165;
		y = 6;
	}
	else
	{
		x = 59;
		y = 53;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)song.len, 2);
}

void drawSongRepS(void)
{
	int16_t x, y;

	if (editor.ui.extended)
	{
		x = 165;
		y = 20;
	}
	else
	{
		x = 59;
		y = 65;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)song.repS, 2);
}

void drawSongBPM(uint16_t val)
{
	char str[8];

	if (editor.ui.extended)
		return;

	sprintf(str, "%03d", val);
	textOutFixed(145, 36, PAL_FORGRND, PAL_DESKTOP, str);
}

void drawSongSpeed(uint16_t val)
{
	char str[8];

	if (editor.ui.extended)
		return;

	sprintf(str, "%02d", val);
	textOutFixed(152, 50, PAL_FORGRND, PAL_DESKTOP, str);
}

void drawEditPattern(uint16_t editPattern)
{
	int16_t x, y;

	if (editor.ui.extended)
	{
		x = 251;
		y = 40;
	}
	else
	{
		x = 237;
		y = 36;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, editPattern, 2);
}

void drawPatternLength(uint16_t editPattern)
{
	int16_t x, y;

	if (editor.ui.extended)
	{
		x = 326;
		y = 40;
	}
	else
	{
		x = 230;
		y = 50;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, pattLens[editPattern], 3);
}

void drawGlobalVol(int16_t globalVol)
{
	char str[8];

	if (editor.ui.extended)
		return;

	assert(globalVol >= 0 && globalVol <= 64);
	sprintf(str, "%02d", globalVol);
	textOutFixed(87, 80, PAL_FORGRND, PAL_DESKTOP, str);
}

void drawIDAdd(void)
{
	char str[8];

	assert(editor.ID_Add <= 16);
	sprintf(str, "%02d", editor.ID_Add);
	textOutFixed(152, 64, PAL_FORGRND, PAL_DESKTOP, str);
}

void drawPlaybackTime(void)
{
	char str[2 + 1];
	uint32_t a, MI_TimeH, MI_TimeM, MI_TimeS;

	a = ((song.musicTime >> 8) * 5) >> 9;
	MI_TimeH = a / 3600;
	a -= (MI_TimeH * 3600);
	MI_TimeM = a / 60;
	MI_TimeS = a - (MI_TimeM * 60);

	// hours
	str[0] = '0' + (char)(MI_TimeH / 10);
	str[1] = '0' + (char)(MI_TimeH % 10);
	str[2] = '\0';
	textOutFixed(235, 80, PAL_FORGRND, PAL_DESKTOP, str);

	// minutes
	str[0] = '0' + (char)(MI_TimeM / 10);
	str[1] = '0' + (char)(MI_TimeM % 10);
	str[2] = '\0';
	textOutFixed(255, 80, PAL_FORGRND, PAL_DESKTOP, str);

	// seconds
	str[0] = '0' + (char)(MI_TimeS / 10);
	str[1] = '0' + (char)(MI_TimeS % 10);
	str[2] = '\0';
	textOutFixed(275, 80, PAL_FORGRND, PAL_DESKTOP, str);
}

void drawSongName(void)
{
	drawFramework(421, 155, 166, 18, FRAMEWORK_TYPE1);
	drawFramework(423, 157, 162, 14, FRAMEWORK_TYPE2);
	drawTextBox(TB_SONG_NAME);
}

void changeLogoType(uint8_t logoType)
{
	pushButtons[PB_LOGO].bitmapFlag = true;

	if (logoType == 0)
	{
		pushButtons[PB_LOGO].bitmapUnpressed = &ft2LogoBadges[(154 * 32) * 0];
		pushButtons[PB_LOGO].bitmapPressed = &ft2LogoBadges[(154 * 32) * 1];
	}
	else
	{
		pushButtons[PB_LOGO].bitmapUnpressed = &ft2LogoBadges[(154 * 32) * 2];
		pushButtons[PB_LOGO].bitmapPressed = &ft2LogoBadges[(154 * 32) * 3];
	}

	drawPushButton(PB_LOGO);
}

void changeBadgeType(uint8_t badgeType)
{
	pushButtons[PB_BADGE].bitmapFlag = true;

	if (badgeType == 0)
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &ft2InfoBadges[(25 * 32) * 0];
		pushButtons[PB_BADGE].bitmapPressed = &ft2InfoBadges[(25 * 32) * 1];
	}
	else
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &ft2InfoBadges[(25 * 32) * 2];
		pushButtons[PB_BADGE].bitmapPressed = &ft2InfoBadges[(25 * 32) * 3];
	}

	drawPushButton(PB_BADGE);
}

void updateInstrumentSwitcher(void)
{
	int8_t i;
	int16_t y;

	if (editor.ui.extended) // extended pattern editor
	{
		//INSTRUMENTS

		clearRect(388, 5, 116, 43); // left box
		clearRect(511, 5, 116, 43); // right box

		// draw source instrument selection
		if (editor.srcInstr >= editor.instrBankOffset && editor.srcInstr <= editor.instrBankOffset+8)
		{
			y = 5 + ((editor.srcInstr - editor.instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
			{
				if (y <= 47)
					fillRect(388, y, 15, 10, PAL_BUTTONS); // left box
				else
					fillRect(511, y - 44, 15, 10, PAL_BUTTONS); // right box
			}
		}

		// draw destination instrument selection
		if (editor.curInstr >= editor.instrBankOffset && editor.curInstr <= editor.instrBankOffset+8)
		{
			y = 5 + ((editor.curInstr - editor.instrBankOffset - 1) * 11);
			y = 5 + ((editor.curInstr - editor.instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
			{
				if (y <= 47)
					fillRect(406, y, 98, 10, PAL_BUTTONS); // left box
				else
					fillRect(529, y - 44, 98, 10, PAL_BUTTONS); // right box
			}
		}

		// draw numbers and texts
		for (i = 0; i < 4; i++)
		{
			hexOut(388, 5 + (i * 11), PAL_FORGRND, 1 + editor.instrBankOffset + i, 2);
			hexOut(511, 5 + (i * 11), PAL_FORGRND, 5 + editor.instrBankOffset + i, 2);
			drawTextBox(TB_INST1 + i);
			drawTextBox(TB_INST5 + i);
		}
	}
	else // normal pattern editor
	{
		// INSTRUMENTS

		clearRect(424, 5,  15, 87); // src instrument
		clearRect(446, 5, 139, 87); // main instrument

		// draw source instrument selection
		if (editor.srcInstr >= editor.instrBankOffset && editor.srcInstr <= editor.instrBankOffset+8)
		{
			y = 5 + ((editor.srcInstr - editor.instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
				fillRect(424, y, 15, 10, PAL_BUTTONS);
		}

		// draw destination instrument selection
		if (editor.curInstr >= editor.instrBankOffset && editor.curInstr <= editor.instrBankOffset+8)
		{
			y = 5 + ((editor.curInstr - editor.instrBankOffset - 1) * 11);
			if (y >= 5 && y <= 82)
				fillRect(446, y, 139, 10, PAL_BUTTONS);
		}

		// draw numbers and texts
		for (i = 0; i < 8; i++)
		{
			hexOut(424, 5 + (i * 11), PAL_FORGRND, 1 + editor.instrBankOffset + i, 2);
			drawTextBox(TB_INST1 + i);
		}

		// SAMPLES

		clearRect(424, 99,  15, 54); // src sample
		clearRect(446, 99, 115, 54); // main sample

		// draw source sample selection
		if (editor.srcSmp >= editor.sampleBankOffset && editor.srcSmp <= editor.sampleBankOffset+4)
		{
			y = 99 + ((editor.srcSmp - editor.sampleBankOffset) * 11);
			if (y >= 36 && y <= 143)
				fillRect(424, y, 15, 10, PAL_BUTTONS);
		}

		// draw destination sample selection
		if (editor.curSmp >= editor.sampleBankOffset && editor.curSmp <= editor.sampleBankOffset+4)
		{
			y = 99 + ((editor.curSmp - editor.sampleBankOffset) * 11);
			if (y >= 36 && y <= 143)
				fillRect(446, y, 115, 10, PAL_BUTTONS);
		}

		// draw numbers and texts
		for (i = 0; i < 5; i++)
		{
			hexOut(424, 99 + (i * 11), PAL_FORGRND, editor.sampleBankOffset + i, 2);
			drawTextBox(TB_SAMP1 + i);
		}
	}
}

void showInstrumentSwitcher(void)
{
	uint16_t i;

	if (!editor.ui.instrSwitcherShown)
		return;

	for (i = 0; i < 8; i++)
		showTextBox(TB_INST1 + i);

	if (editor.ui.extended)
	{
		hidePushButton(PB_SAMPLE_LIST_UP);
		hidePushButton(PB_SAMPLE_LIST_DOWN);
		hideScrollBar(SB_SAMPLE_LIST);

		drawFramework(386,  0, 246,   3, FRAMEWORK_TYPE1);
		drawFramework(506,  3,   3,  47, FRAMEWORK_TYPE1);
		drawFramework(386, 50, 246,   3, FRAMEWORK_TYPE1);
		drawFramework(629,  3,   3,  47, FRAMEWORK_TYPE1);

		clearRect(386, 3, 120, 47);
		clearRect(509, 3, 120, 47);
	}
	else
	{
		drawFramework(421,   0, 166,   3, FRAMEWORK_TYPE1);
		drawFramework(442,   3,   3,  91, FRAMEWORK_TYPE1);
		drawFramework(421,  94, 166,   3, FRAMEWORK_TYPE1);
		drawFramework(442,  97,   3,  58, FRAMEWORK_TYPE1);
		drawFramework(563,  97,  24,  58, FRAMEWORK_TYPE1);
		drawFramework(587,   0,  45,  71, FRAMEWORK_TYPE1);
		drawFramework(587,  71,  45,  71, FRAMEWORK_TYPE1);
		drawFramework(587, 142,  45,  31, FRAMEWORK_TYPE1);

		fillRect(421,  3,  21, 91, PAL_BCKGRND);
		fillRect(445,  3, 142, 91, PAL_BCKGRND);
		fillRect(421, 97,  21, 58, PAL_BCKGRND);
		fillRect(445, 97, 118, 58, PAL_BCKGRND);

		showPushButton(PB_SAMPLE_LIST_UP);
		showPushButton(PB_SAMPLE_LIST_DOWN);
		showScrollBar(SB_SAMPLE_LIST);

		for (i = 0; i < 5; i++)
			showTextBox(TB_SAMP1 + i);
	}

	updateInstrumentSwitcher();

	for (i = 0; i < 8; i++)
		showPushButton(PB_RANGE1 + i + (editor.instrBankSwapped * 8));

	showPushButton(PB_SWAP_BANK);
}

void hideInstrumentSwitcher(void)
{
	uint8_t i;

	for (i = 0; i < 16; i++)
		hidePushButton(PB_RANGE1 + i);

	hidePushButton(PB_SWAP_BANK);
	hidePushButton(PB_SAMPLE_LIST_UP);
	hidePushButton(PB_SAMPLE_LIST_DOWN);
	hideScrollBar(SB_SAMPLE_LIST);

	for (i = 0; i < 8; i++)
		hideTextBox(TB_INST1 + i);

	for (i = 0; i < 5; i++)
		hideTextBox(TB_SAMP1 + i);
}

void pbSwapInstrBank(void)
{
	editor.instrBankSwapped ^= 1;

	if (editor.instrBankSwapped)
		editor.instrBankOffset += (8 * 8);
	else
		editor.instrBankOffset -= (8 * 8);

	updateTextBoxPointers();

	if (editor.ui.instrSwitcherShown)
	{
		updateInstrumentSwitcher();
		for (uint16_t i = 0; i < 8; i++)
		{
			hidePushButton(PB_RANGE1 + i + (!editor.instrBankSwapped * 8));
			showPushButton(PB_RANGE1 + i + ( editor.instrBankSwapped * 8));
		}
	}
}

void pbSetInstrBank1(void)
{
	editor.instrBankOffset = 0 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank2(void)
{
	editor.instrBankOffset = 1 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank3(void)
{
	editor.instrBankOffset = 2 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank4(void)
{
	editor.instrBankOffset = 3 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank5(void)
{
	editor.instrBankOffset = 4 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank6(void)
{
	editor.instrBankOffset = 5 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank7(void)
{
	editor.instrBankOffset = 6 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank8(void)
{
	editor.instrBankOffset = 7 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank9(void)
{
	editor.instrBankOffset = 8 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank10(void)
{
	editor.instrBankOffset = 9 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank11(void)
{
	editor.instrBankOffset = 10 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank12(void)
{
	editor.instrBankOffset = 11 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank13(void)
{
	editor.instrBankOffset = 12 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank14(void)
{
	editor.instrBankOffset = 13 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank15(void)
{
	editor.instrBankOffset = 14 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void pbSetInstrBank16(void)
{
	editor.instrBankOffset = 15 * 8;

	updateTextBoxPointers();
	updateInstrumentSwitcher();
}

void setNewInstr(int16_t ins)
{ 
	if (ins <= MAX_INST)
	{
		editor.curInstr = (uint8_t)ins;
		updateTextBoxPointers();
		updateInstrumentSwitcher();
		updateNewInstrument();
	}
}

void sampleListScrollUp(void)
{
	scrollBarScrollUp(SB_SAMPLE_LIST, 1);
}

void sampleListScrollDown(void)
{
	scrollBarScrollDown(SB_SAMPLE_LIST, 1);
}

static void zapSong(void)
{
	lockMixerCallback();

	song.len = 1;
	song.repS = 0; // FT2 doesn't do this...
	song.speed = 125;
	song.tempo = 6;
	song.songPos = 0;
	song.globVol = 64;

	memset(song.name, 0, sizeof (song.name));
	memset(song.songTab, 0, sizeof (song.songTab));

	// zero all pattern data and reset pattern lengths

	freeAllPatterns();
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
		pattLens[i] = 64;
	song.pattLen = pattLens[song.pattNr];

	resetMusic();
	setSpeed(song.speed);

	editor.songPos = song.songPos;
	editor.editPattern = song.pattNr;
	editor.speed = song.speed;
	editor.tempo = song.tempo;
	editor.globalVol = song.globVol;
	editor.timer = 1;

	if (songPlaying)
		song.musicTime = 0;

	setFrqTab(true);

	clearPattMark();
	resetWavRenderer();
	resetChannels();
	unlockMixerCallback();

	setScrollBarPos(SB_POS_ED, 0, false);
	setScrollBarEnd(SB_POS_ED, (song.len - 1) + 5);

	updateWindowTitle(true);
}

static void zapInstrs(void)
{
	lockMixerCallback();

	for (int16_t i = 1; i <= MAX_INST; i++)
	{
		freeInstr(i);
		memset(song.instrName[i], 0, 23);
	}

	updateNewInstrument();

	editor.currVolEnvPoint = 0;
	editor.currPanEnvPoint = 0;

	updateSampleEditorSample();

	if (editor.ui.sampleEditorShown)
		updateSampleEditor();
	else if (editor.ui.instEditorShown || editor.ui.instEditorExtShown)
		updateInstEditor();

	unlockMixerCallback();
}

void pbZap(void)
{
	int16_t choice = okBox(4, "System request", "Total devastation of the...");

	if (choice == 1) // zap all
	{
		zapSong();
		zapInstrs();
	}
	else if (choice == 2) // zap song
	{
		zapSong();
	}
	else if (choice == 3) // zap instruments
	{
		zapInstrs();
	}

	if (choice >= 1 && choice <= 3)
	{
		// redraw top screens
		hideTopScreen();
		showTopScreen(true);

		setSongModifiedFlag();
	}
}

void sbSmpBankPos(uint32_t pos)
{
	if (editor.sampleBankOffset != pos)
	{
		editor.sampleBankOffset = (uint8_t)pos;

		updateTextBoxPointers();
		updateInstrumentSwitcher();
	}
}

void pbToggleLogo(void)
{
	config.id_FastLogo ^= 1;
	changeLogoType(config.id_FastLogo);
}

void pbToggleBadge(void)
{
	config.id_TritonProd ^= 1;
	changeBadgeType(config.id_TritonProd);
}

void resetChannelOffset(void)
{
	editor.ui.pattChanScrollShown = song.antChn > getMaxVisibleChannels();
	editor.cursor.object = CURSOR_NOTE;
	editor.cursor.ch = 0;
	setScrollBarPos(SB_CHAN_SCROLL, 0, true);
	editor.ui.channelOffset = 0;
}

void shrinkPattern(void)
{
	uint16_t nr, pattLen;
	tonTyp *pattPtr;

	if (okBox(2, "System request", "Shrink pattern?") != 1)
		return;

	nr = editor.editPattern;

	pattLen = pattLens[nr];
	if (pattLen > 1)
	{
		lockMixerCallback();

		pattPtr = patt[nr];
		if (pattPtr != NULL)
		{
			for (uint16_t i = 0; i < pattLen/2; i++)
			{
				for (uint16_t j = 0; j < MAX_VOICES; j++)
					pattPtr[(i * MAX_VOICES) + j] = pattPtr[((i * 2) * MAX_VOICES) + j];
			}
		}

		pattLens[nr] /= 2;

		if (song.pattNr == nr)
			song.pattLen = pattLens[nr];

		song.pattPos /= 2;
		if (song.pattPos >= pattLens[nr])
			song.pattPos = pattLens[nr] - 1;

		editor.pattPos = song.pattPos;

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;

		unlockMixerCallback();
		setSongModifiedFlag();
	}
}

void expandPattern(void)
{
	uint16_t nr, pattLen;
	tonTyp *tmpPtn;

	nr = editor.editPattern;

	pattLen = pattLens[nr];
	if (pattLen > 128)
	{
		okBox(0, "System message", "Pattern is too long to be expanded.");
	}
	else
	{
		lockMixerCallback();

		if (patt[nr] != NULL)
		{
			tmpPtn = (tonTyp *)malloc((pattLen * 2) * TRACK_WIDTH);
			if (tmpPtn == NULL)
			{
				unlockMixerCallback();
				okBox(0, "System message", "Not enough memory!");
				return;
			}

			for (uint16_t i = 0; i < pattLen; i++)
			{
				for (uint16_t j = 0; j < MAX_VOICES; j++)
					tmpPtn[((i * 2) * MAX_VOICES) + j] = patt[nr][(i * MAX_VOICES) + j];

				memset(&tmpPtn[((i * 2) + 1) * MAX_VOICES], 0, TRACK_WIDTH);
			}

			free(patt[nr]);
			patt[nr] = tmpPtn;
		}

		pattLens[nr] *= 2;

		if (song.pattNr == nr)
			song.pattLen = pattLens[nr];

		song.pattPos *= 2;
		if (song.pattPos >= pattLens[nr])
			song.pattPos = pattLens[nr] - 1;

		editor.pattPos = song.pattPos;

		editor.ui.updatePatternEditor = true;
		editor.ui.updatePosSections = true;

		unlockMixerCallback();
		setSongModifiedFlag();
	}
}

const pattCoordsMouse_t pattCoordMouseTable[2][2][2] =
{
	/*
		uint16_t upperRowsY, midRowY, lowerRowsY;
		uint16_t numUpperRows;
	*/

	// no pattern stretch
	{
		// no pattern channel scroll
		{
			{ 177, 281, 293, 13 }, //   normal pattern editor
			{  57, 217, 229, 20 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 177, 274, 286, 12 }, //   normal pattern editor
			{  57, 210, 222, 19 }, // extended pattern editor
		}
	},

	// pattern stretch
	{
		// no pattern channel scroll
		{
			{ 176, 275, 286,  9 }, //   normal pattern editor
			{  56, 221, 232, 15 }, // extended pattern editor
		},

		// pattern channel scroll
		{
			{ 175, 274, 284,  9 }, //   normal pattern editor
			{  55, 209, 219, 14 }, // extended pattern editor
		},
	}
};
