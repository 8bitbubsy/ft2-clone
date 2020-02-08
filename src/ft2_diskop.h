#pragma once

#include <stdint.h>
#include "ft2_unicode.h"

#define DISKOP_ENTRY_NUM 15

enum
{
	DISKOP_ITEM_MODULE = 0,
	DISKOP_ITEM_INSTR = 1,
	DISKOP_ITEM_SAMPLE = 2,
	DISKOP_ITEM_PATTERN = 3,
	DISKOP_ITEM_TRACK = 4,

	MOD_SAVE_MODE_MOD = 0,
	MOD_SAVE_MODE_XM = 1,
	MOD_SAVE_MODE_WAV = 2,
	SMP_SAVE_MODE_RAW = 0,
	SMP_SAVE_MODE_IFF = 1,
	SMP_SAVE_MODE_WAV = 2
};

int32_t getFileSize(UNICHAR *fileNameU);
uint8_t getDiskOpItem(void);
void updateCurrSongFilename(void); // for window title
char *getCurrSongFilename(void); // for window title
char *getDiskOpFilename(void);
const UNICHAR *getDiskOpCurPath(void);
const UNICHAR *getDiskOpModPath(void);
const UNICHAR *getDiskOpSmpPath(void);
void changeFilenameExt(char *name, char *ext, int32_t nameMaxLen);
void diskOpChangeFilenameExt(char *ext);
void freeDiskOp(void);
bool setupDiskOp(void);
void diskOpSetFilename(uint8_t type, UNICHAR *pathU);
void sanitizeFilename(const char *src);
bool diskOpGoParent(void);
void pbDiskOpRoot(void);
int32_t getExtOffset(char *s, int32_t stringLen); // get byte offset of file extension (last '.')
bool testDiskOpMouseDown(bool mouseHeldDown);
void testDiskOpMouseRelease(void);
void startDiskOpFillThread(void);
void diskOp_DrawDirectory(void);
void showDiskOpScreen(void);
void hideDiskOpScreen(void);
void exitDiskOpScreen(void);
void toggleDiskOpScreen(void);
void sbDiskOpSetPos(uint32_t pos);
void pbDiskOpListUp(void);
void pbDiskOpListDown(void);
void pbDiskOpParent(void);
void pbDiskOpShowAll(void);
#ifdef _WIN32
void pbDiskOpDrive1(void);
void pbDiskOpDrive2(void);
void pbDiskOpDrive3(void);
void pbDiskOpDrive4(void);
void pbDiskOpDrive5(void);
void pbDiskOpDrive6(void);
void pbDiskOpDrive7(void);
void pbDiskOpDrive8(void);
#endif
void pbDiskOpSave(void);
void pbDiskOpDelete(void);
void pbDiskOpRename(void);
void pbDiskOpMakeDir(void);
void pbDiskOpRefresh(void);
void pbDiskOpSetPath(void);
void pbDiskOpExit(void);
void rbDiskOpModule(void);
void rbDiskOpInstr(void);
void rbDiskOpSample(void);
void rbDiskOpPattern(void);
void rbDiskOpTrack(void);
void rbDiskOpModSaveXm(void);
void rbDiskOpModSaveMod(void);
void rbDiskOpModSaveWav(void);
void rbDiskOpSmpSaveWav(void);
void rbDiskOpSmpSaveRaw(void);
void rbDiskOpSmpSaveIff(void);
void trimEntryName(char *name, bool isDir);
bool fileExistsAnsi(char *str);
