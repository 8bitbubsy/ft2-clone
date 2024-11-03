// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_pattern_ed.h"
#include "ft2_audioselector.h"
#include "ft2_midi.h"
#include "ft2_mouse.h"
#include "ft2_video.h"
#include "ft2_palette.h"
#include "ft2_structs.h"

#define FIXED_THUMB_SIZE 15

static int32_t lastMouseX, lastMouseY, scrollBias;

/* Prevent the scrollbar thumbs from being so small that
** it's difficult to use them. In units of pixels.
** Shouldn't be higher than 9!
*/
#define MIN_THUMB_SIZE 5

scrollBar_t scrollBars[NUM_SCROLLBARS] =
{
	// ------ RESERVED SCROLLBARS ------
	{ 0 }, { 0 }, { 0 },

	/*
	** -- STRUCT INFO: --
	**  x         = x position
	**  y         = y position
	**  w         = width
	**  h         = height
	**  type      = scrollbar type (vertical/horizontal)
	**  style     = scrollbar style (dynamic or fixed thumb size)
	** funcOnDown = function to call when pressed
	*/

	// ------ POSITION EDITOR SCROLLBARS ------
	//x,  y,  w,  h,  type,               style                         funcOnDown
	{ 55, 15, 18, 21, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, sbPosEdPos },

	// ------ INSTRUMENT SWITCHER SCROLLBARS ------
	//x,   y,   w,  h,  type,               style                         funcOnDown
	{ 566, 112, 18, 28, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, sbSmpBankPos },

	// ------ PATTERN VIEWER SCROLLBARS ------
	//x,  y,   w,   h,  type,                 style                         funcOnDown
	{ 28, 385, 576, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, setChannelScrollPos },

	// ------ HELP SCREEN SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                         funcOnDown
	{ 611, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, helpScrollSetPos },

	// ------ SAMPLE EDITOR SCROLLBARS ------
	//x,  y,   w,   h,  type,                 style                         funcOnDown
	{ 26, 331, 580, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, scrollSampleData },

	// ------ INSTRUMENT EDITOR SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                       funcOnDown
	{ 544, 175, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setVolumeScroll },
	{ 544, 189, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setPanningScroll },
	{ 544, 203, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setFinetuneScroll },
	{ 544, 220, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setFadeoutScroll },
	{ 544, 234, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setVibSpeedScroll },
	{ 544, 248, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setVibDepthScroll },
	{ 544, 262, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, setVibSweepScroll },

	// ------ INSTRUMENT EDITOR EXTENSION SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                       funcOnDown
	{ 195, 130, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbMidiChPos },
	{ 195, 144, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbMidiPrgPos },
	{ 195, 158, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbMidiBendPos },

	// ------ CONFIG AUDIO SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                         funcOnDown
	{ 365,  29, 18, 43, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE, sbAudOutputSetPos },
	{ 365, 116, 18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_DYNAMIC_THUMB_SIZE, sbAudInputSetPos },
	{ 533, 117, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE,   sbAmp },
	{ 533, 143, 75, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE,   sbMasterVol },

	// ------ CONFIG LAYOUT SCROLLBARS ------
	//x,   y,  w,  h,  type,                 style                       funcOnDown
	{ 536, 15, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbPalRPos },
	{ 536, 29, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbPalGPos },
	{ 536, 43, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbPalBPos },
	{ 536, 71, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbPalContrastPos },

	// ------ CONFIG MISCELLANEOUS SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                       funcOnDown
	{ 578, 158, 29, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_FIXED_THUMB_SIZE, sbMIDISens },

#ifdef HAS_MIDI
	// ------ CONFIG MIDI SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                         funcOnDown
	{ 483, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, sbMidiInputSetPos },
#endif

	// ------ DISK OP. SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                         funcOnDown
	{ 335, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_DYNAMIC_THUMB_SIZE, sbDiskOpSetPos }
};

void drawScrollBar(uint16_t scrollBarID)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];
	if (!scrollBar->visible)
		return;

	assert(scrollBar->x < SCREEN_W && scrollBar->y < SCREEN_H && scrollBar->w >= 3 && scrollBar->h >= 3);

	int16_t thumbX = scrollBar->thumbX;
	int16_t thumbY = scrollBar->thumbY;
	int16_t thumbW = scrollBar->thumbW;
	int16_t thumbH = scrollBar->thumbH;

	// clear scrollbar background (lazy, but sometimes even faster than filling bg gaps)
	clearRect(scrollBar->x, scrollBar->y, scrollBar->w, scrollBar->h);

	// draw thumb

	if (scrollBar->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		fillRect(thumbX, thumbY, thumbW, thumbH, PAL_PATTEXT);
	}
	else
	{
		// fixed thumb size

		fillRect(thumbX, thumbY, thumbW, thumbH, PAL_BUTTONS);

		if (scrollBar->state == SCROLLBAR_UNPRESSED)
		{
			// top left corner inner border
			hLine(thumbX, thumbY,     thumbW - 1, PAL_BUTTON1);
			vLine(thumbX, thumbY + 1, thumbH - 2, PAL_BUTTON1);

			// bottom right corner inner border
			hLine(thumbX,              thumbY + thumbH - 1, thumbW - 1, PAL_BUTTON2);
			vLine(thumbX + thumbW - 1, thumbY,              thumbH,     PAL_BUTTON2);
		}
		else
		{
			// top left corner inner border
			hLine(thumbX, thumbY,     thumbW,     PAL_BUTTON2);
			vLine(thumbX, thumbY + 1, thumbH - 1, PAL_BUTTON2);
		}
	}
}

void showScrollBar(uint16_t scrollBarID)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBars[scrollBarID].visible = true;
	drawScrollBar(scrollBarID);
}

void hideScrollBar(uint16_t scrollBarID)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBars[scrollBarID].state = 0;
	scrollBars[scrollBarID].visible = false;
}

static void setScrollBarThumbCoords(uint16_t scrollBarID)
{
	int16_t thumbX, thumbY, thumbW, thumbH, scrollEnd, originalThumbSize;
	int32_t tmp32, length, end;
	double dTmp;

	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	assert(scrollBar->page > 0);

	// uninitialized scrollbar, set full thumb length/height
	if (scrollBar->end == 0)
	{
		scrollBar->thumbX = scrollBar->x + 1;
		scrollBar->thumbY = scrollBar->y + 1;
		scrollBar->thumbW = scrollBar->w - 2;
		scrollBar->thumbH = scrollBar->h - 2;
		return;
	}

	if (scrollBar->type == SCROLLBAR_HORIZONTAL)
	{
		// horizontal scrollbar

		thumbY = scrollBar->y + 1;
		thumbH = scrollBar->h - 2;
		scrollEnd = scrollBar->x + scrollBar->w;

		if (scrollBar->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
		{
			thumbW = originalThumbSize = FIXED_THUMB_SIZE;

			if (scrollBar->end > 0)
			{
				length = scrollBar->w - originalThumbSize;
				dTmp = (length / (double)scrollBar->end) * scrollBar->pos;
				tmp32 = (int32_t)(dTmp + 0.5);
				thumbX = (int16_t)(scrollBar->x + tmp32);
			}
			else
			{
				thumbX = scrollBar->x;
			}
		}
		else
		{
			if (scrollBar->end > 0)
			{
				dTmp = (scrollBar->w / (double)scrollBar->end) * scrollBar->page;
				tmp32 = (int32_t)(dTmp + 0.5);
				originalThumbSize = (int16_t)CLAMP(tmp32, 1, scrollBar->w);
			}
			else
			{
				originalThumbSize = 1;
			}

			thumbW = originalThumbSize;
			if (thumbW < MIN_THUMB_SIZE)
				thumbW = MIN_THUMB_SIZE;

			if (scrollBar->end > scrollBar->page)
			{
				length = scrollBar->w - thumbW;
				end = scrollBar->end - scrollBar->page;

				dTmp = (length / (double)end) * scrollBar->pos;
				tmp32 = (int32_t)(dTmp + 0.5);
				thumbX = (int16_t)(scrollBar->x + tmp32);
			}
			else
			{
				thumbX = scrollBar->x;
			}
		}

		// prevent scrollbar thumb coords from being outside of the scrollbar area
		thumbX = CLAMP(thumbX, scrollBar->x, scrollEnd-1);
		if (thumbX+thumbW > scrollEnd)
			thumbW = scrollEnd - thumbX;
	}
	else
	{
		// vertical scrollbar

		thumbX = scrollBar->x + 1;
		thumbW = scrollBar->w - 2;
		scrollEnd = scrollBar->y + scrollBar->h;

		if (scrollBar->end > 0)
		{
			dTmp = (scrollBar->h / (double)scrollBar->end) * scrollBar->page;
			tmp32 = (int32_t)(dTmp + 0.5);
			originalThumbSize = (int16_t)CLAMP(tmp32, 1, scrollBar->h);
		}
		else
		{
			originalThumbSize = 1;
		}

		thumbH = originalThumbSize;
		if (thumbH < MIN_THUMB_SIZE)
			thumbH = MIN_THUMB_SIZE;

		if (scrollBar->end > scrollBar->page)
		{
			length = scrollBar->h - thumbH;
			end = scrollBar->end - scrollBar->page;

			dTmp = (length / (double)end) * scrollBar->pos;
			tmp32 = (int32_t)(dTmp + 0.5);
			thumbY = (int16_t)(scrollBar->y + tmp32);
		}
		else
		{
			thumbY = scrollBar->y;
		}

		// prevent scrollbar thumb coords from being outside of the scrollbar area
		thumbY = CLAMP(thumbY, scrollBar->y, scrollEnd - 1);
		if (thumbY+thumbH > scrollEnd)
			thumbH = scrollEnd - thumbY;
	}

	// set values now
	scrollBar->originalThumbSize = originalThumbSize;
	scrollBar->thumbX = thumbX;
	scrollBar->thumbY = thumbY;
	scrollBar->thumbW = thumbW;
	scrollBar->thumbH = thumbH;
}

void scrollBarScrollUp(uint16_t scrollBarID, uint32_t amount)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	assert(scrollBar->page > 0 && scrollBar->end > 0);

	if (scrollBar->end < scrollBar->page || scrollBar->pos == 0)
		return;

	if (scrollBar->pos >= amount)
		scrollBar->pos -= amount;
	else
		scrollBar->pos = 0;

	setScrollBarThumbCoords(scrollBarID);
	drawScrollBar(scrollBarID);

	if (scrollBar->callbackFunc != NULL)
		scrollBar->callbackFunc(scrollBar->pos);
}

void scrollBarScrollDown(uint16_t scrollBarID, uint32_t amount)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	assert(scrollBar->page > 0 && scrollBar->end > 0);

	if (scrollBar->end < scrollBar->page)
		return;

	uint32_t endPos = scrollBar->end;
	if (scrollBar->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= scrollBar->page)
			endPos -= scrollBar->page;
		else
			endPos = 0;
	}

	// check if we're already at the end
	if (scrollBar->pos == endPos)
		return;

	scrollBar->pos += amount;
	if (scrollBar->pos > endPos)
		scrollBar->pos = endPos;

	setScrollBarThumbCoords(scrollBarID);
	drawScrollBar(scrollBarID);

	if (scrollBar->callbackFunc != NULL)
		scrollBar->callbackFunc(scrollBar->pos);
}

void scrollBarScrollLeft(uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollUp(scrollBarID, amount);
}

void scrollBarScrollRight(uint16_t scrollBarID, uint32_t amount)
{
	scrollBarScrollDown(scrollBarID, amount);
}

void setScrollBarPos(uint16_t scrollBarID, uint32_t pos, bool triggerCallBack)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	if (scrollBar->page == 0)
	{
		scrollBar->pos = 0;
		return;
	}

	if (scrollBar->end < scrollBar->page || scrollBar->pos == pos)
	{
		setScrollBarThumbCoords(scrollBarID);
		drawScrollBar(scrollBarID);
		return;
	}

	uint32_t endPos = scrollBar->end;
	if (scrollBar->thumbType == SCROLLBAR_DYNAMIC_THUMB_SIZE)
	{
		if (endPos >= scrollBar->page)
			endPos -= scrollBar->page;
		else
			endPos = 0;
	}

	scrollBar->pos = pos;
	if (scrollBar->pos > endPos)
		scrollBar->pos = endPos;

	setScrollBarThumbCoords(scrollBarID);
	drawScrollBar(scrollBarID);

	if (triggerCallBack && scrollBar->callbackFunc != NULL)
		scrollBar->callbackFunc(scrollBar->pos);
}

uint32_t getScrollBarPos(uint16_t scrollBarID)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	return scrollBars[scrollBarID].pos;
}

void setScrollBarEnd(uint16_t scrollBarID, uint32_t end)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	if (end < 1)
		end = 1;

	scrollBar->end = end;
	
	bool setPos = false;
	if (scrollBar->pos >= end)
	{
		scrollBar->pos = end - 1;
		setPos = true;
	}

	if (scrollBar->page > 0)
	{
		if (setPos)
		{
			setScrollBarPos(scrollBarID, scrollBar->pos, false);
			// this will also call setScrollBarThumbCoords() and drawScrollBar()
		}
		else
		{
			setScrollBarThumbCoords(scrollBarID);
			drawScrollBar(scrollBarID);
		}
	}
}

void setScrollBarPageLength(uint16_t scrollBarID, uint32_t pageLength)
{
	assert(scrollBarID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[scrollBarID];

	if (pageLength < 1)
		pageLength = 1;

	scrollBar->page = pageLength;
	if (scrollBar->end > 0)
	{
		setScrollBarPos(scrollBarID, scrollBar->pos, false);
		setScrollBarThumbCoords(scrollBarID);
		drawScrollBar(scrollBarID);
	}
}

bool testScrollBarMouseDown(void)
{
	uint16_t start, end;
	int32_t scrollPos, length;
	double dTmp;

	if (ui.sysReqShown)
	{
		// if a system request is open, only test the first three scrollbars (reserved)
		start = 0;
		end = 3;
	}
	else
	{
		start = 3;
		end = NUM_SCROLLBARS;
	}

	scrollBar_t *scrollBar = &scrollBars[start];
	for (uint16_t i = start; i < end; i++, scrollBar++)
	{
		if (!scrollBar->visible)
			continue;

		if (mouse.x >= scrollBar->x && mouse.x < scrollBar->x+scrollBar->w &&
		    mouse.y >= scrollBar->y && mouse.y < scrollBar->y+scrollBar->h)
		{
			mouse.lastUsedObjectID = i;
			mouse.lastUsedObjectType = OBJECT_SCROLLBAR;

			// kludge for when a system request is about to open
			scrollBar->state = SCROLLBAR_PRESSED;
			if (scrollBar->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
				drawScrollBar(mouse.lastUsedObjectType);

			if (scrollBar->type == SCROLLBAR_HORIZONTAL)
			{
				lastMouseX = mouse.x;

				if (mouse.x >= scrollBar->thumbX && mouse.x < scrollBar->thumbX+scrollBar->thumbW)
				{
					// clicked on thumb

					scrollBias = mouse.x - scrollBar->thumbX;
				}
				else
				{
					// clicked outside of thumb

					scrollBias = scrollBar->thumbW >> 1;

					scrollPos = mouse.x - scrollBias - scrollBar->x;
					assert(scrollBar->w > 0);
					scrollPos = CLAMP(scrollPos, 0, scrollBar->w);

					if (scrollBar->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
						length = scrollBar->w - scrollBar->thumbW;
					else
						length = scrollBar->w + (scrollBar->originalThumbSize - scrollBar->thumbW);

					if (length < 1)
						length = 1;

					dTmp = ((double)scrollPos * scrollBar->end) / length;
					scrollPos = (int32_t)(dTmp + 0.5);

					setScrollBarPos(mouse.lastUsedObjectID, scrollPos, true);
				}
			}
			else
			{
				// vertical scroll bar

				lastMouseY = mouse.y;

				if (mouse.y >= scrollBar->thumbY && mouse.y < scrollBar->thumbY+scrollBar->thumbH)
				{
					// clicked on thumb

					scrollBias = mouse.y - scrollBar->thumbY;
				}
				else
				{
					// clicked outside of thumb

					scrollBias = scrollBar->thumbH >> 1;

					scrollPos = mouse.y - scrollBias - scrollBar->y;

					assert(scrollBar->h > 0);
					scrollPos = CLAMP(scrollPos, 0, scrollBar->h);

					length = scrollBar->h + (scrollBar->originalThumbSize - scrollBar->thumbH);
					if (length < 1)
						length = 1;

					dTmp = ((double)scrollPos * scrollBar->end) / length;
					scrollPos = (int32_t)(dTmp + 0.5);

					setScrollBarPos(mouse.lastUsedObjectID, scrollPos, true);
				}
			}

			// objectID can be set to none in scrollbar's callback during setScrollBarPos()
			if (mouse.lastUsedObjectID == OBJECT_ID_NONE)
				return true;

			scrollBar->state = SCROLLBAR_PRESSED;
			if (scrollBar->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
				drawScrollBar(mouse.lastUsedObjectID);

			return true;
		}
	}

	return false;
}

void testScrollBarMouseRelease(void)
{
	if (mouse.lastUsedObjectType != OBJECT_SCROLLBAR || mouse.lastUsedObjectID == OBJECT_ID_NONE)
		return;

	assert(mouse.lastUsedObjectID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[mouse.lastUsedObjectID];

	if (scrollBar->visible)
	{
		scrollBar->state = SCROLLBAR_UNPRESSED;
		drawScrollBar(mouse.lastUsedObjectID);
	}
}

void handleScrollBarsWhileMouseDown(void)
{
	int32_t scrollPos, length;
	double dTmp;

	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[mouse.lastUsedObjectID];
	if (!scrollBar->visible)
		return;

	if (scrollBar->type == SCROLLBAR_HORIZONTAL)
	{
		if (mouse.x != lastMouseX)
		{
			lastMouseX = mouse.x;

			scrollPos = mouse.x - scrollBias - scrollBar->x;
			assert(scrollBar->w > 0);
			scrollPos = CLAMP(scrollPos, 0, scrollBar->w);

			if (scrollBar->thumbType == SCROLLBAR_FIXED_THUMB_SIZE)
				length = scrollBar->w - scrollBar->thumbW;
			else
				length = scrollBar->w + (scrollBar->originalThumbSize - scrollBar->thumbW);

			if (length < 1)
				length = 1;

			dTmp = ((double)scrollPos * scrollBar->end) / length;
			scrollPos = (int32_t)(dTmp + 0.5);

			setScrollBarPos(mouse.lastUsedObjectID, scrollPos, true);

			if (mouse.lastUsedObjectID != OBJECT_ID_NONE) // this can change in the callback in setScrollBarPos()
				drawScrollBar(mouse.lastUsedObjectID);
		}
	}
	else
	{
		// vertical scroll bar

		if (mouse.y != lastMouseY)
		{
			lastMouseY = mouse.y;

			scrollPos = mouse.y - scrollBias - scrollBar->y;

			assert(scrollBar->h > 0);
			scrollPos = CLAMP(scrollPos, 0, scrollBar->h);

			length = scrollBar->h + (scrollBar->originalThumbSize - scrollBar->thumbH);
			if (length < 1)
				length = 1;

			dTmp = ((double)scrollPos * scrollBar->end) / length;
			scrollPos = (int32_t)(dTmp + 0.5);

			setScrollBarPos(mouse.lastUsedObjectID, scrollPos, true);

			if (mouse.lastUsedObjectID != OBJECT_ID_NONE) // this can change in the callback in setScrollBarPos()
				drawScrollBar(mouse.lastUsedObjectID);
		}
	}
}

void initializeScrollBars(void)
{
	// pattern editor
	setScrollBarPageLength(SB_CHAN_SCROLL, 8);
	setScrollBarEnd(SB_CHAN_SCROLL, 8);

	// position editor
	setScrollBarPageLength(SB_POS_ED, 5);
	setScrollBarEnd(SB_POS_ED, 5);

	// instrument switcher
	setScrollBarPageLength(SB_SAMPLE_LIST, 5);
	setScrollBarEnd(SB_SAMPLE_LIST, 16);

	// help screen
	setScrollBarPageLength(SB_HELP_SCROLL, 15);
	setScrollBarEnd(SB_HELP_SCROLL, 1);

	// config screen
	setScrollBarPageLength(SB_AMP_SCROLL, 1);
	setScrollBarEnd(SB_AMP_SCROLL, 31);
	setScrollBarPageLength(SB_MASTERVOL_SCROLL, 1);
	setScrollBarEnd(SB_MASTERVOL_SCROLL, 256);
	setScrollBarPageLength(SB_PAL_R, 1);
	setScrollBarEnd(SB_PAL_R, 63);
	setScrollBarPageLength(SB_PAL_G, 1);
	setScrollBarEnd(SB_PAL_G, 63);
	setScrollBarPageLength(SB_PAL_B, 1);
	setScrollBarEnd(SB_PAL_B, 63);
	setScrollBarPageLength(SB_PAL_CONTRAST, 1);
	setScrollBarEnd(SB_PAL_CONTRAST, 100);
	setScrollBarPageLength(SB_MIDI_SENS, 1);
	setScrollBarEnd(SB_MIDI_SENS, 200);
	setScrollBarPageLength(SB_AUDIO_OUTPUT_SCROLL, 6);
	setScrollBarEnd(SB_AUDIO_OUTPUT_SCROLL, 1);
	setScrollBarPageLength(SB_AUDIO_INPUT_SCROLL, 4);
	setScrollBarEnd(SB_AUDIO_INPUT_SCROLL, 1);

#ifdef HAS_MIDI
	setScrollBarPageLength(SB_MIDI_INPUT_SCROLL, 15);
	setScrollBarEnd(SB_MIDI_INPUT_SCROLL, 1);
#endif

	// disk op.
	setScrollBarPageLength(SB_DISKOP_LIST, DISKOP_ENTRY_NUM);
	setScrollBarEnd(SB_DISKOP_LIST, 1);

	// instrument editor
	setScrollBarPageLength(SB_INST_VOL, 1);
	setScrollBarEnd(SB_INST_VOL, 64);
	setScrollBarPageLength(SB_INST_PAN, 1);
	setScrollBarEnd(SB_INST_PAN, 255);
	setScrollBarPageLength(SB_INST_FTUNE, 1);
	setScrollBarEnd(SB_INST_FTUNE, 255);
	setScrollBarPageLength(SB_INST_FADEOUT, 1);
	setScrollBarEnd(SB_INST_FADEOUT, 0xFFF);
	setScrollBarPageLength(SB_INST_VIBSPEED, 1);
	setScrollBarEnd(SB_INST_VIBSPEED, 0x3F);
	setScrollBarPageLength(SB_INST_VIBDEPTH, 1);
	setScrollBarEnd(SB_INST_VIBDEPTH, 0xF);
	setScrollBarPageLength(SB_INST_VIBSWEEP, 1);
	setScrollBarEnd(SB_INST_VIBSWEEP, 0xFF);

	// instrument editor extension
	setScrollBarPageLength(SB_INST_EXT_MIDI_CH, 1);
	setScrollBarEnd(SB_INST_EXT_MIDI_CH, 15);
	setScrollBarPageLength(SB_INST_EXT_MIDI_PRG, 1);
	setScrollBarEnd(SB_INST_EXT_MIDI_PRG, 127);
	setScrollBarPageLength(SB_INST_EXT_MIDI_BEND, 1);
	setScrollBarEnd(SB_INST_EXT_MIDI_BEND, 36);
}
