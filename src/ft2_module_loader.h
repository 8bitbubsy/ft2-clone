#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

void loadMusic(UNICHAR *filenameU);
bool loadMusicUnthreaded(UNICHAR *filenameU, bool autoPlay);
bool handleModuleLoadFromArg(int argc, char **argv);
void loadDroppedFile(char *fullPathUTF8, bool songModifiedCheck);
void handleLoadMusicEvents(void);
void clearUnusedChannels(tonTyp *p, int16_t pattLen, int32_t antChn);
void checkSampleRepeat(sampleTyp *s);
