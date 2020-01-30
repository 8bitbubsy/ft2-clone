#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

enum
{
	VOLUME_COLUMN_HIDDEN = 0,
	VOLUME_COLUMN_SHOWN = 1,

	TRANSP_ALL_INST = 0,
	TRANSP_CUR_INST = 1,

	TRANSP_TRACK = 0,
	TRANSP_PATT = 1,
	TRANSP_SONG = 2,
	TRANSP_BLOCK = 3
};

typedef struct trackHeaderType_t
{
	uint16_t ver, len;
} trackHeaderType;

typedef struct patternHeaderType_t
{
	uint16_t ver, len;
} patternHeaderType;

typedef struct pattCoord_t
{
	uint16_t upperRowsY, lowerRowsY;
	uint16_t upperRowsTextY, midRowTextY, lowerRowsTextY;
	uint16_t numUpperRows, numLowerRows;
} pattCoord_t;

typedef struct pattCoord2_t
{
	uint16_t upperRowsY, lowerRowsY;
	uint16_t upperRowsH, lowerRowsH;
} pattCoord2_t;

typedef struct pattCoordsMouse_t
{
	uint16_t upperRowsY, midRowY, lowerRowsY;
	uint16_t numUpperRows;
} pattCoordsMouse_t;

typedef struct markCoord_t
{
	uint16_t upperRowsY, midRowY, lowerRowsY;
} markCoord_t;

struct pattMark_t
{
	int16_t markX1, markX2, markY1, markY2;
} pattMark;

bool allocatePattern(uint16_t nr);
void killPatternIfUnused(uint16_t nr);
uint8_t getMaxVisibleChannels(void);
void updatePatternWidth(void);
void updateAdvEdit(void);
void drawAdvEdit(void);
void hideAdvEdit(void);
void showAdvEdit(void);
void toggleAdvEdit(void);
void cursorTabLeft(void);
void cursorTabRight(void);
void cursorChannelLeft(void);
void cursorChannelRight(void);
void cursorLeft(void);
void cursorRight(void);
void chanLeft(void);
void chanRight(void);
void showPatternEditor(void);
void updateInstrumentSwitcher(void);
void hidePatternEditor(void);
void patternEditorExtended(void);
void exitPatternEditorExtended(void);
void clearPattMark(void);
void checkMarkLimits(void);
void handlePatternDataMouseDown(bool mouseButtonHeld);
void togglePatternEditorExtended(void);
void rowOneUpWrap(void);
void rowOneDownWrap(void);
void rowUp(uint16_t amount);
void rowDown(uint16_t amount);
void keybPattMarkUp(void);
void keybPattMarkDown(void);
void keybPattMarkLeft(void);
void keybPattMarkRight(void);
void drawTranspose(void);
void showTranspose(void);
void hideTranspose(void);
void toggleTranspose(void);
bool loadTrack(UNICHAR *filenameU);
bool saveTrack(UNICHAR *filenameU);
bool loadPattern(UNICHAR *filenameU);
bool savePattern(UNICHAR *filenameU);
void scrollChannelLeft(void);
void scrollChannelRight(void);
void setChannelScrollPos(uint32_t pos);
void jumpToChannel(uint8_t channel); // for ALT+q..i ALT+a..k
void sbPosEdPos(uint32_t pos);
void pbPosEdPosUp(void);
void pbPosEdPosDown(void);
void pbPosEdIns(void);
void pbPosEdDel(void);
void pbPosEdPattUp(void);
void pbPosEdPattDown(void);
void pbPosEdLenUp(void);
void pbPosEdLenDown(void);
void pbPosEdRepSUp(void);
void pbPosEdRepSDown(void);
void pbBPMUp(void);
void pbBPMDown(void);
void pbSpeedUp(void);
void pbSpeedDown(void);
void pbIncAdd(void);
void pbDecAdd(void);
void pbAddChan(void);
void pbSubChan(void);
void pbEditPattUp(void);
void pbEditPattDown(void);
void pbPattLenUp(void);
void pbPattLenDown(void);
void drawPosEdNums(int16_t songPos);
void drawSongLength(void);
void drawSongRepS(void);
void drawSongBPM(uint16_t val);
void drawSongSpeed(uint16_t val);
void drawEditPattern(uint16_t editPattern);
void drawPatternLength(uint16_t editPattern);
void drawGlobalVol(int16_t globalVol);
void drawIDAdd(void);
void drawPlaybackTime(void);
void drawSongName(void);
void showInstrumentSwitcher(void);
void hideInstrumentSwitcher(void);
void changeLogoType(uint8_t logoType);
void changeBadgeType(uint8_t badgeType);
void resetChannelOffset(void);
void shrinkPattern(void);
void expandPattern(void);
void pbSwapInstrBank(void);
void pbSetInstrBank1(void);
void pbSetInstrBank2(void);
void pbSetInstrBank3(void);
void pbSetInstrBank4(void);
void pbSetInstrBank5(void);
void pbSetInstrBank6(void);
void pbSetInstrBank7(void);
void pbSetInstrBank8(void);
void pbSetInstrBank9(void);
void pbSetInstrBank10(void);
void pbSetInstrBank11(void);
void pbSetInstrBank12(void);
void pbSetInstrBank13(void);
void pbSetInstrBank14(void);
void pbSetInstrBank15(void);
void pbSetInstrBank16(void);
void setNewInstr(int16_t ins);
void sampleListScrollUp(void);
void sampleListScrollDown(void);
void pbZap(void);
void sbSmpBankPos(uint32_t pos);
void pbToggleLogo(void);
void pbToggleBadge(void);
