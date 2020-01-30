#pragma once

#include <stdint.h>
#include <stdbool.h>

enum // SCROLLBARS
{
	// reserved
	SB_RES_1,
	SB_RES_2,
	SB_RES_3,

	SB_POS_ED,
	SB_SAMPLE_LIST,
	SB_CHAN_SCROLL,
	SB_HELP_SCROLL,
	SB_SAMP_SCROLL,

	// Instrument Editor
	SB_INST_VOL,
	SB_INST_PAN,
	SB_INST_FTUNE,
	SB_INST_FADEOUT,
	SB_INST_VIBSPEED,
	SB_INST_VIBDEPTH,
	SB_INST_VIBSWEEP,

	// Instrument Editor Extension
	SB_INST_EXT_MIDI_CH,
	SB_INST_EXT_MIDI_PRG,
	SB_INST_EXT_MIDI_BEND,

	// Config I/O devices
	SB_AUDIO_OUTPUT_SCROLL,
	SB_AUDIO_INPUT_SCROLL,
	SB_AMP_SCROLL,
	SB_MASTERVOL_SCROLL,

	// Config Layout
	SB_PAL_R,
	SB_PAL_G,
	SB_PAL_B,
	SB_PAL_CONTRAST,

	// Config Miscellaneous
	SB_MIDI_SENS,

#ifdef HAS_MIDI
	// Config Midi
	SB_MIDI_INPUT_SCROLL,
#endif

	// Disk Op.
	SB_DISKOP_LIST,

	NUM_SCROLLBARS
};

enum
{
	SCROLLBAR_UNPRESSED = 0,
	SCROLLBAR_PRESSED = 1,
	SCROLLBAR_HORIZONTAL = 0,
	SCROLLBAR_VERTICAL = 1,
	SCROLLBAR_THUMB_NOFLAT = 0,
	SCROLLBAR_THUMB_FLAT = 1
};

typedef struct scrollBar_t // DO NOT TOUCH!
{
	uint16_t x, y, w, h;
	uint8_t type, thumbType;
	void (*callbackFunc)(uint32_t pos);

	bool visible;
	uint8_t state;
	uint32_t pos, page, end;
	uint32_t oldPos, oldPage, oldEnd;
	uint16_t thumbX, thumbY, thumbW, thumbH; 
} scrollBar_t;

void drawScrollBar(uint16_t scrollBarID);
void showScrollBar(uint16_t scrollBarID);
void hideScrollBar(uint16_t scrollBarID);
void scrollBarScrollUp(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollDown(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollLeft(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollRight(uint16_t scrollBarID, uint32_t amount);
void setScrollBarPos(uint16_t scrollBarID, uint32_t pos, bool triggerCallBack);
uint32_t getScrollBarPos(uint16_t scrollBarID);
void setScrollBarEnd(uint16_t scrollBarID, uint32_t end);
void setScrollBarPageLength(uint16_t scrollBarID, uint32_t pageLength);
bool testScrollBarMouseDown(void);
void testScrollBarMouseRelease(void);
void handleScrollBarsWhileMouseDown(void);
void initializeScrollBars(void);
