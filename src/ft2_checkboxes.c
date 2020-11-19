// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_sample_ed.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_pattern_ed.h"
#include "ft2_trim.h"
#include "ft2_mouse.h"
#include "ft2_edit.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

checkBox_t checkBoxes[NUM_CHECKBOXES] =
{
	// ------ RESERVED CHECKBOX ------
	{ 0 },

	/*
	** -- STRUCT INFO: --
	**  x        = x position
	**  y        = y position
	**  w        = clickable width space, relative to x
	**  h        = clickable height space, relative to y
	**  funcOnUp = function to call when released
	*/

	// ------ NIBBLES CHECKBOXES ------
	//x, y,   w,  h,  funcOnUp
	{ 3, 133, 70, 12, nibblesToggleSurround },
	{ 3, 146, 40, 12, nibblesToggleGrid },
	{ 3, 159, 45, 12, nibblesToggleWrap },

	// ------ ADVANCED EDIT CHECKBOXES ------
	//x,   y,   w,   h,  funcOnUp
	{ 113,  94, 105, 12, toggleCopyMaskEnable },
	{ 237, 107,  13, 12, toggleCopyMask0 },
	{ 237, 120,  13, 12, toggleCopyMask1 },
	{ 237, 133,  13, 12, toggleCopyMask2 },
	{ 237, 146,  13, 12, toggleCopyMask3 },
	{ 237, 159,  13, 12, toggleCopyMask4 },
	{ 256, 107,  13, 12, togglePasteMask0 },
	{ 256, 120,  13, 12, togglePasteMask1 },
	{ 256, 133,  13, 12, togglePasteMask2 },
	{ 256, 146,  13, 12, togglePasteMask3 },
	{ 256, 159,  13, 12, togglePasteMask4 },
	{ 275, 107,  13, 12, toggleTranspMask0 },
	{ 275, 120,  13, 12, toggleTranspMask1 },
	{ 275, 133,  13, 12, toggleTranspMask2 },
	{ 275, 146,  13, 12, toggleTranspMask3 },
	{ 275, 159,  13, 12, toggleTranspMask4 },

	// ------ INSTRUMENT EDITOR CHECKBOXES ------
	//x,   y,   w,   h,  funcOnUp
	{   3, 175, 118, 12, cbVEnv },
	{ 341, 192,  64, 12, cbVEnvSus },
	{ 341, 217,  70, 12, cbVEnvLoop },
	{   3, 262, 123, 12, cbPEnv },
	{ 341, 279,  64, 12, cbPEnvSus },
	{ 341, 304,  70, 12, cbPEnvLoop },

	// ------ INSTRUMENT EDITOR EXTENSION CHECKBOXES ------
	//x,   y,   w,   h,  funcOnUp
	{   3, 112, 148, 12, cbInstMidiEnable },
	{ 172, 112, 103, 12, cbInstMuteComputer },

	// ------ TRIM SCREEN CHECKBOXES ------
	//x,   y,   w,   h,  funcOnUp
	{   3, 107, 113, 12, cbTrimUnusedPatt },
	{   3, 120, 132, 12, cbTrimUnusedInst },
	{   3, 133, 110, 12, cbTrimUnusedSamp },
	{   3, 146, 115, 12, cbTrimUnusedChans },
	{   3, 159, 130, 12, cbTrimUnusedSmpData },
	{ 139,  94, 149, 12, cbTrimSmpsTo8Bit },

	// ------ CONFIG CHECKBOXES ------
	//x,   y,   w,   h,  funcOnUp
	{   3,  91,  77, 12, cbToggleAutoSaveConfig },
	{ 389, 159, 107, 12, cbConfigVolRamp },
	{ 113,  14, 108, 12, cbConfigPattStretch },
	{ 113,  27, 117, 12, cbConfigHexCount },
	{ 113,  40,  81, 12, cbConfigAccidential },
	{ 113,  53,  92, 12, cbConfigShowZeroes },
	{ 113,  66,  81, 12, cbConfigFramework },
	{ 113,  79, 128, 12, cbConfigLineColors },
	{ 113,  92, 126, 12, cbConfigChanNums },
	{ 255,  14, 136, 12, cbConfigShowVolCol },
	{ 255, 158, 111, 12, cbSoftwareMouse },
	// ---------------------------------
	{ 212,   2, 150, 12, cbSampCutToBuff },
	{ 212,  15, 153, 12, cbPattCutToBuff },
	{ 212,  28, 159, 12, cbKillNotesAtStop },
	{ 212,  41, 149, 12, cbFileOverwriteWarn },
	{ 212,  69, 130, 12, cbMultiChanRec },
	{ 212,  82, 157, 12, cbMultiChanKeyJazz },
	{ 212,  95, 114, 12, cbMultiChanEdit },
	{ 212, 108, 143, 12, cbRecKeyOff },
	{ 212, 121,  89, 12, cbQuantization },
	{ 212, 134, 180, 25, cbChangePattLenInsDel },
	{ 212, 159, 187, 12, cbMIDIAllowPC },
	{ 411,  93,  83, 12, cbMIDIEnable },
	{ 530, 106,  29, 12, cbMIDIRecAllChn },
	{ 411, 119, 121, 12, cbMIDIRecTransp },
	{ 411, 132, 109, 12, cbMIDIRecVelocity },
	{ 411, 145, 124, 12, cbMIDIRecAftert },
	{ 113, 115,  74, 12, cbVsyncOff },
	{ 113, 128,  78, 12, cbFullScreen },
	{ 113, 141,  75, 12, cbStretchImage },
	{ 113, 154,  78, 12, cbPixelFilter }
};

void drawCheckBox(uint16_t checkBoxID)
{
	const uint8_t *gfxPtr;

	assert(checkBoxID < NUM_CHECKBOXES);
	checkBox_t *checkBox = &checkBoxes[checkBoxID];
	if (!checkBox->visible)
		return;

	if (checkBoxID == CB_CONF_ACCIDENTAL)
		gfxPtr = &bmp.checkboxGfx[4*(CHECKBOX_W*CHECKBOX_H)]; // for the special "Accidental" check button in Config Layout
	else
		gfxPtr = &bmp.checkboxGfx[0*(CHECKBOX_W*CHECKBOX_H)];

	if (checkBox->checked)
		gfxPtr += 2*(CHECKBOX_W*CHECKBOX_H);

	if (checkBox->state == CHECKBOX_PRESSED)
		gfxPtr += 1*(CHECKBOX_W*CHECKBOX_H);

	blitFast(checkBox->x, checkBox->y, gfxPtr, CHECKBOX_W, CHECKBOX_H);
}

void showCheckBox(uint16_t checkBoxID)
{
	assert(checkBoxID < NUM_CHECKBOXES);
	checkBoxes[checkBoxID].visible = true;
	drawCheckBox(checkBoxID);
}

void hideCheckBox(uint16_t checkBoxID)
{
	assert(checkBoxID < NUM_CHECKBOXES);
	checkBoxes[checkBoxID].state = 0;
	checkBoxes[checkBoxID].visible = false;
}

void handleCheckBoxesWhileMouseDown(void)
{
	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_CHECKBOXES);
	checkBox_t *checkBox = &checkBoxes[mouse.lastUsedObjectID];
	if (!checkBox->visible)
		return;

	checkBox->state = CHECKBOX_UNPRESSED;
	if (mouse.x >= checkBox->x && mouse.x < checkBox->x+checkBox->clickAreaWidth &&
	    mouse.y >= checkBox->y && mouse.y < checkBox->y+checkBox->clickAreaHeight)
	{
		checkBox->state = CHECKBOX_PRESSED;
	}

	if (mouse.lastX != mouse.x || mouse.lastY != mouse.y)
	{
		mouse.lastX = mouse.x;
		mouse.lastY = mouse.y;

		drawCheckBox(mouse.lastUsedObjectID);
	}
}

bool testCheckBoxMouseDown(void)
{
	uint16_t start, end;

	if (ui.sysReqShown)
	{
		// if a system request is open, only test the first three checkboxes (reserved)
		start = 0;
		end = 1;
	}
	else
	{
		start = 1;
		end = NUM_CHECKBOXES;
	}

	checkBox_t *checkBox = &checkBoxes[start];
	for (uint16_t i = start; i < end; i++, checkBox++)
	{
		if (!checkBox->visible)
			continue;

		if (mouse.x >= checkBox->x && mouse.x < checkBox->x+checkBox->clickAreaWidth &&
		    mouse.y >= checkBox->y && mouse.y < checkBox->y+checkBox->clickAreaHeight)
		{
			mouse.lastUsedObjectID = i;
			mouse.lastUsedObjectType = OBJECT_CHECKBOX;
			checkBox->state = CHECKBOX_PRESSED;
			drawCheckBox(mouse.lastUsedObjectID);
			return true;
		}
	}

	return false;
}

void testCheckBoxMouseRelease(void)
{
	if (mouse.lastUsedObjectType != OBJECT_CHECKBOX || mouse.lastUsedObjectID == OBJECT_ID_NONE)
		return;

	assert(mouse.lastUsedObjectID < NUM_CHECKBOXES);
	checkBox_t *checkBox = &checkBoxes[mouse.lastUsedObjectID];
	if (!checkBox->visible)
		return;

	if (mouse.x >= checkBox->x && mouse.x < checkBox->x+checkBox->clickAreaWidth &&
	    mouse.y >= checkBox->y && mouse.y < checkBox->y+checkBox->clickAreaHeight)
	{
		checkBox->checked ^= 1;

		checkBox->state = CHECKBOX_UNPRESSED;
		drawCheckBox(mouse.lastUsedObjectID);

		if (checkBox->callbackFunc != NULL)
			checkBox->callbackFunc();
	}
}
