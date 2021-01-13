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

/* Prevent the scrollbar thumbs from being so small that
** it's difficult to use them. In units of pixels.
** Shouldn't be higher than 9!
*/
#define MIN_THUMB_LENGTH 5

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
	**  style     = scrollbar style (flat/noflat)
	** funcOnDown = function to call when pressed
	*/

	// ------ POSITION EDITOR SCROLLBARS ------
	//x,  y,  w,  h,  type,               style                 funcOnDown
	{ 55, 15, 18, 21, SCROLLBAR_VERTICAL, SCROLLBAR_THUMB_FLAT, sbPosEdPos },

	// ------ INSTRUMENT SWITCHER SCROLLBARS ------
	//x,   y,   w,  h,  type,               style                 funcOnDown
	{ 566, 112, 18, 28, SCROLLBAR_VERTICAL, SCROLLBAR_THUMB_FLAT, sbSmpBankPos },

	// ------ PATTERN VIEWER SCROLLBARS ------
	//x,  y,   w,   h,  type,                 style                 funcOnDown
	{ 28, 385, 576, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_FLAT, setChannelScrollPos },

	// ------ HELP SCREEN SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                 funcOnDown
	{ 611, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_THUMB_FLAT, helpScrollSetPos },

	// ------ SAMPLE EDITOR SCROLLBARS ------
	//x,  y,   w,   h,  type,                 style                 funcOnDown
	{ 26, 331, 580, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_FLAT, scrollSampleData },

	// ------ INSTRUMENT EDITOR SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                   funcOnDown
	{ 544, 175, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setVolumeScroll },
	{ 544, 189, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setPanningScroll },
	{ 544, 203, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setFinetuneScroll },
	{ 544, 220, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setFadeoutScroll },
	{ 544, 234, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setVibSpeedScroll },
	{ 544, 248, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setVibDepthScroll },
	{ 544, 262, 62, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, setVibSweepScroll },

	// ------ INSTRUMENT EDITOR EXTENSION SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                   funcOnDown
	{ 195, 130, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbMidiChPos },
	{ 195, 144, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbMidiPrgPos },
	{ 195, 158, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbMidiBendPos },

	// ------ CONFIG AUDIO SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                   funcOnDown
	{ 365,  29, 18, 43, SCROLLBAR_VERTICAL,   SCROLLBAR_THUMB_FLAT,   sbAudOutputSetPos },
	{ 365, 116, 18, 21, SCROLLBAR_VERTICAL,   SCROLLBAR_THUMB_FLAT,   sbAudInputSetPos },
	{ 529, 132, 79, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbAmp },
	{ 529, 158, 79, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbMasterVol },

	// ------ CONFIG LAYOUT SCROLLBARS ------
	//x,   y,  w,  h,  type,                 style                   funcOnDown
	{ 536, 15, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbPalRPos },
	{ 536, 29, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbPalGPos },
	{ 536, 43, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbPalBPos },
	{ 536, 71, 70, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbPalContrastPos },

	// ------ CONFIG MISCELLANEOUS SCROLLBARS ------
	//x,   y,   w,  h,  type,                 style                   funcOnDown
	{ 578, 158, 29, 13, SCROLLBAR_HORIZONTAL, SCROLLBAR_THUMB_NOFLAT, sbMIDISens },

#ifdef HAS_MIDI
	// ------ CONFIG MIDI SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                 funcOnDown
	{ 483, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_THUMB_FLAT, sbMidiInputSetPos },
#endif

	// ------ DISK OP. SCROLLBARS ------
	//x,   y,  w,  h,   type,               style                 funcOnDown
	{ 335, 15, 18, 143, SCROLLBAR_VERTICAL, SCROLLBAR_THUMB_FLAT, sbDiskOpSetPos }
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
	if (scrollBar->thumbType == SCROLLBAR_THUMB_FLAT)
	{
		// flat
		fillRect(thumbX, thumbY, thumbW, thumbH, PAL_PATTEXT);
	}
	else
	{
		// 3D
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
	int16_t thumbX, thumbY, thumbW, thumbH, scrollEnd, realThumbLength;
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

		if (scrollBar->thumbType == SCROLLBAR_THUMB_NOFLAT)
		{
			realThumbLength = 15;

			thumbW = realThumbLength;
			if (thumbW < MIN_THUMB_LENGTH)
				thumbW = MIN_THUMB_LENGTH;

			if (scrollBar->end > 0)
			{
				length = scrollBar->w - realThumbLength;
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
				realThumbLength = (int16_t)CLAMP(tmp32, 1, scrollBar->w);
			}
			else
			{
				realThumbLength = 1;
			}

			thumbW = realThumbLength;
			if (thumbW < MIN_THUMB_LENGTH)
				thumbW = MIN_THUMB_LENGTH;

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
			realThumbLength = (int16_t)CLAMP(tmp32, 1, scrollBar->h);
		}
		else
		{
			realThumbLength = 1;
		}

		thumbH = realThumbLength;
		if (thumbH < MIN_THUMB_LENGTH)
			thumbH = MIN_THUMB_LENGTH;

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
	scrollBar->realThumbLength = realThumbLength;
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
	if (scrollBar->thumbType == SCROLLBAR_THUMB_FLAT)
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
	if (scrollBar->thumbType == SCROLLBAR_THUMB_FLAT)
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

	const int32_t mx = mouse.x;
	const int32_t my = mouse.y;

	scrollBar_t *scrollBar = &scrollBars[start];
	for (uint16_t i = start; i < end; i++, scrollBar++)
	{
		if (!scrollBar->visible)
			continue;

		if (mx >= scrollBar->x && mx < scrollBar->x+scrollBar->w &&
		    my >= scrollBar->y && my < scrollBar->y+scrollBar->h)
		{
			mouse.lastUsedObjectID = i;
			mouse.lastUsedObjectType = OBJECT_SCROLLBAR;

			// kludge for when a system request is about to open
			scrollBar->state = SCROLLBAR_PRESSED;
			if (scrollBar->thumbType == SCROLLBAR_THUMB_NOFLAT)
				drawScrollBar(mouse.lastUsedObjectType);

			if (scrollBar->type == SCROLLBAR_HORIZONTAL)
			{
				mouse.lastScrollXTmp = mouse.lastScrollX = mx;

				if (mx >= scrollBar->thumbX && mx < scrollBar->thumbX+scrollBar->thumbW)
				{
					mouse.saveMouseX = mouse.lastScrollX - scrollBar->thumbX;
				}
				else
				{
					mouse.saveMouseX = scrollBar->thumbW >> 1;

					scrollPos = mouse.lastScrollX - scrollBar->x - mouse.saveMouseX;
					if (scrollBar->thumbType == SCROLLBAR_THUMB_NOFLAT)
					{
						dTmp = scrollPos * (scrollBar->w / (double)(scrollBar->w - scrollBar->thumbW));
						scrollPos = (int32_t)(dTmp + 0.5);
					}

					assert(scrollBar->w > 0);
					scrollPos = CLAMP(scrollPos, 0, scrollBar->w);

					length = scrollBar->w + (scrollBar->realThumbLength - scrollBar->thumbW);
					if (length < 1)
						length = 1;

					dTmp = ((double)scrollPos * scrollBar->end) / length;
					scrollPos = (int32_t)(dTmp + 0.5);

					setScrollBarPos(mouse.lastUsedObjectID, scrollPos, true);
				}
			}
			else
			{
				mouse.lastScrollY = my;
				if (my >= scrollBar->thumbY && my < scrollBar->thumbY+scrollBar->thumbH)
				{
					mouse.saveMouseY = mouse.lastScrollY - scrollBar->thumbY;
				}
				else
				{
					mouse.saveMouseY = scrollBar->thumbH >> 1;

					scrollPos = mouse.lastScrollY - scrollBar->y - mouse.saveMouseY;

					assert(scrollBar->h > 0);
					scrollPos = CLAMP(scrollPos, 0, scrollBar->h);

					length = scrollBar->h + (scrollBar->realThumbLength - scrollBar->thumbH);
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
			if (scrollBar->thumbType == SCROLLBAR_THUMB_NOFLAT)
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
	int32_t scrollX, scrollY, length;
	double dTmp;

	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_SCROLLBARS);
	scrollBar_t *scrollBar = &scrollBars[mouse.lastUsedObjectID];
	if (!scrollBar->visible)
		return;

	if (scrollBar->type == SCROLLBAR_HORIZONTAL)
	{
		if (mouse.x != mouse.lastScrollX)
		{
			mouse.lastScrollX = mouse.x;
			scrollX = mouse.lastScrollX - mouse.saveMouseX - scrollBar->x;

			if (scrollBar->thumbType == SCROLLBAR_THUMB_NOFLAT)
			{
				assert(scrollBar->w >= 16);
				dTmp = scrollX * (scrollBar->w / (double)(scrollBar->w - scrollBar->thumbW));
				scrollX = (int32_t)(dTmp + 0.5);
			}

			assert(scrollBar->w > 0);
			scrollX = CLAMP(scrollX, 0, scrollBar->w);

			length = scrollBar->w + (scrollBar->realThumbLength - scrollBar->thumbW);
			if (length < 1)
				length = 1;

			dTmp = ((double)scrollX * scrollBar->end) / length;
			scrollX = (int32_t)(dTmp + 0.5);

			setScrollBarPos(mouse.lastUsedObjectID, scrollX, true);

			if (mouse.lastUsedObjectID != OBJECT_ID_NONE) // this can change in the callback in setScrollBarPos()
				drawScrollBar(mouse.lastUsedObjectID);
		}
	}
	else
	{
		if (mouse.y != mouse.lastScrollY)
		{
			mouse.lastScrollY = mouse.y;

			scrollY = mouse.lastScrollY - mouse.saveMouseY - scrollBar->y;

			assert(scrollBar->h > 0);
			scrollY = CLAMP(scrollY, 0, scrollBar->h);

			length = scrollBar->h + (scrollBar->realThumbLength - scrollBar->thumbH);
			if (length < 1)
				length = 1;

			dTmp = ((double)scrollY * scrollBar->end) / length;
			scrollY = (int32_t)(dTmp + 0.5);

			setScrollBarPos(mouse.lastUsedObjectID, scrollY, true);

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
