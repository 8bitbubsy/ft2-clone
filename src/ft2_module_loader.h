#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_unicode.h"

bool tmpPatternEmpty(uint16_t nr);
void clearUnusedChannels(tonTyp *p, int16_t pattLen, int32_t antChn);
bool allocateTmpInstr(int16_t nr);
bool allocateTmpPatt(int32_t nr, uint16_t rows);
void loadMusic(UNICHAR *filenameU);
bool loadMusicUnthreaded(UNICHAR *filenameU, bool autoPlay);
bool handleModuleLoadFromArg(int argc, char **argv);
void loadDroppedFile(char *fullPathUTF8, bool songModifiedCheck);
void handleLoadMusicEvents(void);

// file extensions accepted by Disk Op. in module mode
extern char *supportedModExtensions[];

extern volatile bool tmpLinearPeriodsFlag;
extern int16_t pattLensTmp[MAX_PATTERNS];
extern tonTyp *pattTmp[MAX_PATTERNS];
extern instrTyp *instrTmp[1+256];
extern songTyp songTmp;
