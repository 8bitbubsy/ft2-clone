// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
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
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

pattMark_t pattMark; // globalized

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
static int32_t last_TimeH, last_TimeM, last_TimeS;

bool allocatePattern(uint16_t nr) // for tracker use only, not in loader!
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (patt[nr] == NULL)
	{
		/* Original FT2 allocates only the amount of rows needed, but we don't
		** do that to avoid out of bondary row look-up between out-of-sync replayer
		** state and tracker state (yes it used to happen, rarely). We're not wasting
		** too much RAM for a modern computer anyway. Worst case: 256 allocated
		** patterns would be ~10MB.
		**/

		patt[nr] = (tonTyp *)calloc((MAX_PATT_LEN * TRACK_WIDTH) + 16, 1);
		if (patt[nr] == NULL)
		{
			if (audioWasntLocked)
				unlockAudio();

			return false;
		}

		// XXX: Do we really need this? Sounds redundant.
		song.pattLen = pattLens[nr];
	}

	if (audioWasntLocked)
		unlockAudio();

	return true;
}

void killPatternIfUnused(uint16_t nr) // for tracker use only, not in loader!
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (patternEmpty(nr))
	{
		if (patt[nr] != NULL)
		{
			free(patt[nr]);
			patt[nr] = NULL;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
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
	if (ui.numChannelsShown > ui.maxVisibleChannels)
		ui.numChannelsShown = ui.maxVisibleChannels;

	assert(ui.numChannelsShown >= 2 && ui.numChannelsShown <= 12);

	ui.patternChannelWidth = chanWidths[(ui.numChannelsShown / 2) - 1] + 3;
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
	textOutShadow(114, 122, PAL_FORGRND, PAL_DSKTOP2, "Instrument number");
	textOutShadow(114, 135, PAL_FORGRND, PAL_DSKTOP2, "Volume column");
	textOutShadow(114, 148, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 1");
	textOutShadow(114, 161, PAL_FORGRND, PAL_DSKTOP2, "Effect digit 2,3");

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
	ui.advEditShown = false;

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

	ui.scopesShown = true;
	drawScopeFramework();
}

void showAdvEdit(void)
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.advEditShown = true;
	ui.scopesShown  = false;
	drawAdvEdit();
}

void toggleAdvEdit(void)
{
	if (ui.advEditShown)
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
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.transposeShown = true;
	ui.scopesShown = false;
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

	ui.transposeShown = false;
	ui.scopesShown = true;
	drawScopeFramework();
}

void toggleTranspose(void)
{
	if (ui.transposeShown)
		hideTranspose();
	else
		showTranspose();
}

// ----- PATTERN CURSOR FUNCTIONS -----

void cursorChannelLeft(void)
{
	cursor.object = CURSOR_EFX2;

	if (cursor.ch == 0)
	{
		cursor.ch = (uint8_t)(song.antChn - 1);
		if (ui.pattChanScrollShown)
			setScrollBarPos(SB_CHAN_SCROLL, song.antChn, true);
	}
	else
	{
		cursor.ch--;
		if (ui.pattChanScrollShown)
		{
			if (cursor.ch < ui.channelOffset)
				scrollBarScrollUp(SB_CHAN_SCROLL, 1);
		}
	}
}

void cursorChannelRight(void)
{
	cursor.object = CURSOR_NOTE;

	if (cursor.ch >= song.antChn-1)
	{
		cursor.ch = 0;
		if (ui.pattChanScrollShown)
			setScrollBarPos(SB_CHAN_SCROLL, 0, true);
	}
	else
	{
		cursor.ch++;
		if (ui.pattChanScrollShown && cursor.ch >= ui.channelOffset+ui.numChannelsShown)
			scrollBarScrollDown(SB_CHAN_SCROLL, 1);
	}
}

void cursorTabLeft(void)
{
	if (cursor.object == CURSOR_NOTE)
		cursorChannelLeft();

	cursor.object = CURSOR_NOTE;
	ui.updatePatternEditor = true;
}

void cursorTabRight(void)
{
	cursorChannelRight();
	cursor.object = CURSOR_NOTE;
	ui.updatePatternEditor = true;
}

void chanLeft(void)
{
	cursorChannelLeft();
	cursor.object = CURSOR_NOTE;
	ui.updatePatternEditor = true;
}

void chanRight(void)
{
	cursorChannelRight();
	cursor.object = CURSOR_NOTE;
	ui.updatePatternEditor = true;
}

void cursorLeft(void)
{
	cursor.object--;

	if (!config.ptnS3M)
	{
		while (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
			cursor.object--;
	}

	if (cursor.object == -1)
	{
		cursor.object = CURSOR_EFX2;
		cursorChannelLeft();
	}

	ui.updatePatternEditor = true;
}

void cursorRight(void)
{
	cursor.object++;

	if (!config.ptnS3M)
	{
		while (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
			cursor.object++;
	}

	if (cursor.object == 8)
	{
		cursor.object = CURSOR_NOTE;
		cursorChannelRight();
	}

	ui.updatePatternEditor = true;
}

void showPatternEditor(void)
{
	ui.patternEditorShown = true;
	updateChanNums();
	drawPatternBorders();
	ui.updatePatternEditor = true;
}

void hidePatternEditor(void)
{
	hideScrollBar(SB_CHAN_SCROLL);
	hidePushButton(PB_CHAN_SCROLL_LEFT);
	hidePushButton(PB_CHAN_SCROLL_RIGHT);

	ui.patternEditorShown = false;
}

static void updatePatternEditorGUI(void)
{
	uint8_t i;
	pushButton_t *p;
	textBox_t *t;

	if (ui.extended)
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
		pushButtons[PB_SWAP_BANK].caption = "Swap B.";
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
		pushButtons[PB_SWAP_BANK].caption2 = "Bank";
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

	// force update even if new values were to be the same as the old ones
	scrollBars[SB_POS_ED].oldEnd = 0xFFFFFFFF;
	scrollBars[SB_POS_ED].oldPage = 0xFFFFFFFF;
	scrollBars[SB_POS_ED].oldPos = 0xFFFFFFFF;
}

void patternEditorExtended(void)
{
	// backup old screen flags
	ui._aboutScreenShown = ui.aboutScreenShown;
	ui._helpScreenShown = ui.helpScreenShown;
	ui._configScreenShown = ui.configScreenShown;
	ui._diskOpShown = ui.diskOpShown;
	ui._nibblesShown = ui.nibblesShown;
	ui._transposeShown = ui.transposeShown;
	ui._instEditorShown = ui.instEditorShown;
	ui._instEditorExtShown = ui.instEditorExtShown;
	ui._sampleEditorExtShown = ui.sampleEditorExtShown;
	ui._patternEditorShown = ui.patternEditorShown;
	ui._sampleEditorShown = ui.sampleEditorShown;
	ui._advEditShown= ui.advEditShown;
	ui._wavRendererShown = ui.wavRendererShown;
	ui._trimScreenShown = ui.trimScreenShown;

	hideTopScreen();
	hideSampleEditor();
	hideInstEditor();

	ui.extended = true;
	ui.patternEditorShown = true;
	updatePatternEditorGUI(); // change pattern editor layout (based on ui.extended flag)
	ui.updatePatternEditor = true; // redraw pattern editor

	drawFramework(0,    0, 112, 53, FRAMEWORK_TYPE1);
	drawFramework(112,  0, 106, 33, FRAMEWORK_TYPE1);
	drawFramework(112, 33, 106, 20, FRAMEWORK_TYPE1);
	drawFramework(218,  0, 168, 53, FRAMEWORK_TYPE1);

	// pos ed. stuff

	drawFramework(2,  2, 51, 20, FRAMEWORK_TYPE2);
	drawFramework(2, 31, 51, 20, FRAMEWORK_TYPE2);

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
	textOutShadow(222, 39, PAL_FORGRND, PAL_DSKTOP2, "Ptn.");
	textOutShadow(305, 39, PAL_FORGRND, PAL_DSKTOP2, "Ln.");

	ui.instrSwitcherShown = true;
	showInstrumentSwitcher();

	drawSongLength();
	drawSongRepS();
	drawEditPattern(editor.editPattern);
	drawPatternLength(editor.editPattern);
	drawPosEdNums(editor.songPos);
	ui.updatePosSections = true;

	// kludge to fix scrollbar thumb when the scrollbar height changes during playback
	if (songPlaying)
		setScrollBarPos(SB_POS_ED, editor.songPos, false);
}

void exitPatternEditorExtended(void)
{
	ui.extended = false;
	updatePatternEditorGUI();
	hidePushButton(PB_EXIT_EXT_PATT);

	// set back top screen button maps

	// set back old screen flags
	ui.aboutScreenShown = ui._aboutScreenShown;
	ui.helpScreenShown = ui._helpScreenShown;
	ui.configScreenShown = ui._configScreenShown;
	ui.diskOpShown = ui._diskOpShown;
	ui.nibblesShown = ui._nibblesShown;
	ui.transposeShown = ui._transposeShown;
	ui.instEditorShown = ui._instEditorShown;
	ui.instEditorExtShown = ui._instEditorExtShown;
	ui.sampleEditorExtShown = ui._sampleEditorExtShown;
	ui.patternEditorShown = ui._patternEditorShown;
	ui.sampleEditorShown = ui._sampleEditorShown;
	ui.advEditShown = ui._advEditShown;
	ui.wavRendererShown = ui._wavRendererShown;
	ui.trimScreenShown = ui.trimScreenShown;

	showTopScreen(true);
	showBottomScreen();

	// kludge to fix scrollbar thumb when the scrollbar height changes during playback
	if (songPlaying)
		setScrollBarPos(SB_POS_ED, editor.songPos, false);
}

void togglePatternEditorExtended(void)
{
	if (ui.extended)
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

	limit = (uint16_t)(song.antChn - 1);
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

	assert(ui.patternChannelWidth > 0);
	if (ui.patternChannelWidth == 0)
		return 0;

	mouseX = mouse.x - 29;
	mouseX = CLAMP(mouseX, 0, 573);

	chEnd = (ui.channelOffset + ui.numChannelsShown) - 1;

	ch = ui.channelOffset + (int8_t)(mouseX / ui.patternChannelWidth);
	ch = CLAMP(ch, 0, chEnd);

	// in some setups there can be non-used channels to the right, do clamping
	if (ch >= song.antChn)
		ch = (int8_t)(song.antChn - 1);

	return ch;
}

static int16_t mouseYToRow(void) // used to get row num from mouse y (for pattern marking)
{
	uint8_t charHeight, mode;
	int16_t row, patternLen, my, maxY, maxRow;
	const pattCoordsMouse_t *pattCoordsMouse;

	pattCoordsMouse = &pattCoordMouseTable[config.ptnUnpressed][ui.pattChanScrollShown][ui.extended];

	// clamp mouse y to boundaries
	maxY = ui.pattChanScrollShown ? 382 : 396;
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
		mode = (ui.extended * 4) + (config.ptnUnpressed * 2) + ui.pattChanScrollShown;

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
		ui.updatePatternEditor = true;
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

		ui.updatePatternEditor = true;
		return;
	}

	// we're holding down the mouse button inside the pattern data area

	forceMarking = songPlaying;

	// scroll left/right with mouse
	if (ui.pattChanScrollShown)
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
			ui.updatePatternEditor = true;

			lastMarkX1 = pattMark.markX1;
			lastMarkX2 = pattMark.markX2;
		}
	}

	// scroll down/up with mouse (if song is not playing)
	if (!songPlaying)
	{
		y1 = ui.extended ? 56 : 176;
		y2 = ui.pattChanScrollShown ? 382 : 396;

		if (mouse.y < y1)
		{
			pattLen = pattLens[editor.editPattern];
			if (editor.pattPos > 0)
				setPos(-1, editor.pattPos - 1, true);

			forceMarking = true;
			ui.updatePatternEditor = true;
		}
		else if (mouse.y > y2)
		{
			pattLen = pattLens[editor.editPattern];
			if (editor.pattPos < (pattLen - 1))
				setPos(-1, editor.pattPos + 1, true);

			forceMarking = true;
			ui.updatePatternEditor = true;
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
			ui.updatePatternEditor = true;

			lastMarkY1 = pattMark.markY1;
			lastMarkY2 = pattMark.markY2;
		}
	}
}

void rowOneUpWrap(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos = (song.pattPos - 1 + song.pattLen) % song.pattLen;
	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowOneDownWrap(void)
{
	const bool audioWasntLocked = !audio.locked;
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
		ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowUp(uint16_t amount)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos -= amount;
	if (song.pattPos < 0)
		song.pattPos = 0;

	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void rowDown(uint16_t amount)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.pattPos += amount;
	if (song.pattPos >= song.pattLen)
		song.pattPos = song.pattLen - 1;

	if (!songPlaying)
	{
		editor.pattPos = (uint8_t)song.pattPos;
		ui.updatePatternEditor = true;
	}

	if (audioWasntLocked)
		unlockAudio();
}

void keybPattMarkUp(void)
{
	int8_t xPos;
	int16_t pattPos;

	xPos = cursor.ch;
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

	xPos = cursor.ch;
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

	xPos = cursor.ch;
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

	xPos = cursor.ch;
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
		pattPtr = &patt[nr][(i * MAX_VOICES) + cursor.ch];
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

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;

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
		saveBuff[i] = pattPtr[(i * MAX_VOICES) + cursor.ch];

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

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;

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
	if (!ui.pattChanScrollShown)
	{
		ui.channelOffset = 0;
		return;
	}

	if (ui.channelOffset == (uint8_t)pos)
		return;

	ui.channelOffset = (uint8_t)pos;

	assert(song.antChn > ui.numChannelsShown);
	if (ui.channelOffset >= song.antChn-ui.numChannelsShown)
		ui.channelOffset = (uint8_t)(song.antChn-ui.numChannelsShown);

	if (cursor.ch >= ui.channelOffset+ui.numChannelsShown)
	{
		cursor.object = CURSOR_NOTE;
		cursor.ch = (ui.channelOffset + ui.numChannelsShown) - 1;
	}
	else if (cursor.ch < ui.channelOffset)
	{
		cursor.object = CURSOR_NOTE;
		cursor.ch = ui.channelOffset;
	}

	ui.updatePatternEditor = true;
}

void jumpToChannel(uint8_t channel) // for ALT+q..i ALT+a..k
{
	if (ui.sampleEditorShown || ui.instEditorShown)
		return;

	channel %= song.antChn;
	if (cursor.ch == channel)
		return;

	if (ui.pattChanScrollShown)
	{
		assert(song.antChn > ui.numChannelsShown);

		if (channel >= ui.channelOffset+ui.numChannelsShown)
			scrollBarScrollDown(SB_CHAN_SCROLL, (channel - (ui.channelOffset + ui.numChannelsShown)) + 1);
		else if (channel < ui.channelOffset)
			scrollBarScrollUp(SB_CHAN_SCROLL, ui.channelOffset - channel);
	}

	cursor.ch = channel; // set it here since scrollBarScrollX() changes it...
	ui.updatePatternEditor = true;
}

void sbPosEdPos(uint32_t pos)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos != (int16_t)pos)
		setPos((int16_t)pos, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdPosUp(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.songPos < song.len-1)
		setPos(song.songPos + 1, 0, true);

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdPosDown(void)
{
	const bool audioWasntLocked = !audio.locked;
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

	ui.updatePosSections = true;
	ui.updatePosEdScrollBar = true;
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

	ui.updatePosSections = true;
	ui.updatePosEdScrollBar = true;
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

		ui.updatePatternEditor = true;
		ui.updatePosSections = true;
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

		ui.updatePatternEditor = true;
		ui.updatePosSections = true;
		setSongModifiedFlag();
	}
	unlockMixerCallback();
}

void pbPosEdLenUp(void)
{
	if (song.len >= 255)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	song.len++;

	ui.updatePosSections = true;
	ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdLenDown(void)
{
	if (song.len <= 1)
		return;

	const bool audioWasntLocked = !audio.locked;
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

	ui.updatePosSections = true;
	ui.updatePosEdScrollBar = true;
	setSongModifiedFlag();

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdRepSUp(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.repS < song.len-1)
	{
		song.repS++;
		ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbPosEdRepSDown(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (song.repS > 0)
	{
		song.repS--;
		ui.updatePosSections = true;
		setSongModifiedFlag();
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbBPMUp(void)
{
	if (song.speed == 255)
		return;

	const bool audioWasntLocked = !audio.locked;
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

	const bool audioWasntLocked = !audio.locked;
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

	const bool audioWasntLocked = !audio.locked;
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

	const bool audioWasntLocked = !audio.locked;
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

	if (ui.patternEditorShown)
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

	if (ui.patternEditorShown)
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
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPlaying)
	{
		if (song.pattNr < 255)
		{
			song.pattNr++;
			updatePtnLen();

			ui.updatePatternEditor = true;
			ui.updatePosSections = true;
		}
	}
	else
	{
		if (editor.editPattern < 255)
		{
			editor.editPattern++;

			song.pattNr = editor.editPattern;
			updatePtnLen();

			ui.updatePatternEditor = true;
			ui.updatePosSections = true;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbEditPattDown(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (songPlaying)
	{
		if (song.pattNr > 0)
		{
			song.pattNr--;
			updatePtnLen();

			ui.updatePatternEditor = true;
			ui.updatePosSections = true;
		}
	}
	else
	{
		if (editor.editPattern > 0)
		{
			editor.editPattern--;

			song.pattNr = editor.editPattern;
			updatePtnLen();

			ui.updatePatternEditor = true;
			ui.updatePosSections = true;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
}

void pbPattLenUp(void)
{
	const uint16_t pattLen = pattLens[editor.editPattern];
	if (pattLen >= 256)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	
	setPatternLen(editor.editPattern, pattLen + 1);
	checkMarkLimits();

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;
	setSongModifiedFlag();

	if (audioWasntLocked)
		unlockAudio();
}

void pbPattLenDown(void)
{
	const uint16_t pattLen = pattLens[editor.editPattern];
	if (pattLen <= 1)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	
	setPatternLen(editor.editPattern, pattLen - 1);
	checkMarkLimits();

	ui.updatePatternEditor = true;
	ui.updatePosSections = true;
	setSongModifiedFlag();

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
	if (ui.extended)
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

		if (ui.extended)
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
	if (ui.extended)
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

		if (ui.extended)
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

	if (ui.extended)
	{
		x = 165;
		y = 5;
	}
	else
	{
		x = 59;
		y = 52;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)song.len, 2);
}

void drawSongRepS(void)
{
	int16_t x, y;

	if (ui.extended)
	{
		x = 165;
		y = 19;
	}
	else
	{
		x = 59;
		y = 64;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, (uint8_t)song.repS, 2);
}

void drawSongBPM(uint16_t val)
{
	char str[4];
	const char *strOut;
	
	if (ui.extended)
		return;

	if (val <= 255)
	{
		strOut = dec3StrTab[val];
	}
	else
	{
		if (val > 999)
			val = 999;

		str[0] = '0' + (char)(val / 100);
		str[1] = '0' + ((val / 10) % 10);
		str[2] = '0' + (val % 10);
		str[3] = 0;

		strOut = str;
	}

	textOutFixed(145, 36, PAL_FORGRND, PAL_DESKTOP, strOut);
}

void drawSongSpeed(uint16_t val)
{
	if (ui.extended)
		return;

	if (val > 99)
		val = 99;

	textOutFixed(152, 50, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[val]);
}

void drawEditPattern(uint16_t editPattern)
{
	int16_t x, y;

	if (ui.extended)
	{
		x = 252;
		y = 39;
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

	if (ui.extended)
	{
		x = 326;
		y = 39;
	}
	else
	{
		x = 230;
		y = 50;
	}

	hexOutBg(x, y, PAL_FORGRND, PAL_DESKTOP, pattLens[editPattern], 3);
}

void drawGlobalVol(uint16_t val)
{
	if (ui.extended)
		return;

	assert(val <= 64);
	textOutFixed(87, 80, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[val]);
}

void drawIDAdd(void)
{
	assert(editor.ID_Add <= 16);
	textOutFixed(152, 64, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[editor.ID_Add]);
}

void resetPlaybackTime(void)
{
	song.musicTime64 = 0;
	last_TimeH = 0;
	last_TimeM = 0;
	last_TimeS = 0;
}

void drawPlaybackTime(void)
{
	if (songPlaying)
	{
		uint32_t seconds = song.musicTime64 >> 32;

		last_TimeH = seconds / 3600; seconds -= last_TimeH * 3600;
		last_TimeM = seconds / 60;   seconds -= last_TimeM * 60;
		last_TimeS = seconds;
	}

	textOutFixed(235, 80, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[last_TimeH]);
	textOutFixed(255, 80, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[last_TimeM]);
	textOutFixed(275, 80, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[last_TimeS]);
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
		pushButtons[PB_LOGO].bitmapUnpressed = &bmp.ft2LogoBadges[(154 * 32) * 0];
		pushButtons[PB_LOGO].bitmapPressed = &bmp.ft2LogoBadges[(154 * 32) * 1];
	}
	else
	{
		pushButtons[PB_LOGO].bitmapUnpressed = &bmp.ft2LogoBadges[(154 * 32) * 2];
		pushButtons[PB_LOGO].bitmapPressed = &bmp.ft2LogoBadges[(154 * 32) * 3];
	}

	drawPushButton(PB_LOGO);
}

void changeBadgeType(uint8_t badgeType)
{
	pushButtons[PB_BADGE].bitmapFlag = true;

	if (badgeType == 0)
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &bmp.ft2ByBadges[(25 * 32) * 0];
		pushButtons[PB_BADGE].bitmapPressed = &bmp.ft2ByBadges[(25 * 32) * 1];
	}
	else
	{
		pushButtons[PB_BADGE].bitmapUnpressed = &bmp.ft2ByBadges[(25 * 32) * 2];
		pushButtons[PB_BADGE].bitmapPressed = &bmp.ft2ByBadges[(25 * 32) * 3];
	}

	drawPushButton(PB_BADGE);
}

void updateInstrumentSwitcher(void)
{
	int8_t i;
	int16_t y;

	if (ui.extended) // extended pattern editor
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

	if (!ui.instrSwitcherShown)
		return;

	for (i = 0; i < 8; i++)
		showTextBox(TB_INST1 + i);

	if (ui.extended)
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

	if (ui.instrSwitcherShown)
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
	song.repS = 0; // Silly: FT2 doesn't do this!
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

	resetPlaybackTime();

	if (!audio.linearFreqTable)
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
		memset(song.instrName[i], 0, 22 + 1);
	}

	updateNewInstrument();

	editor.currVolEnvPoint = 0;
	editor.currPanEnvPoint = 0;

	updateSampleEditorSample();

	if (ui.sampleEditorShown)
		updateSampleEditor();
	else if (ui.instEditorShown || ui.instEditorExtShown)
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
	ui.pattChanScrollShown = song.antChn > getMaxVisibleChannels();
	cursor.object = CURSOR_NOTE;
	cursor.ch = 0;
	setScrollBarPos(SB_CHAN_SCROLL, 0, true);
	ui.channelOffset = 0;
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

		ui.updatePatternEditor = true;
		ui.updatePosSections = true;

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

		ui.updatePatternEditor = true;
		ui.updatePosSections = true;

		unlockMixerCallback();
		setSongModifiedFlag();
	}
}
