#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_unicode.h"

bool tmpPatternEmpty(int32_t pattNum);
void clearUnusedChannels(note_t *p, int16_t numRows, int32_t numChannels);
bool allocateTmpInstr(int32_t insNum);
bool allocateTmpPatt(int32_t pattNum, uint16_t numRows);
void loadMusic(UNICHAR *filenameU);
bool loadMusicUnthreaded(UNICHAR *filenameU, bool autoPlay);
bool handleModuleLoadFromArg(int argc, char **argv);
void loadDroppedFile(char *fullPathUTF8, bool songModifiedCheck);
void handleLoadMusicEvents(void);

// file extensions accepted by Disk Op. in module mode
extern char *supportedModExtensions[];

extern volatile bool tmpLinearPeriodsFlag;
extern uint8_t tmpBuffer[65536];
extern int16_t patternNumRowsTmp[MAX_PATTERNS];
extern note_t *patternTmp[MAX_PATTERNS];
extern instr_t *instrTmp[1+256];
extern song_t songTmp;
