#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

bool loadSample(UNICHAR *filenameU, uint8_t sampleSlot, bool loadAsInstrFlag);
bool fileIsInstrument(char *fullPath);
bool fileIsSample(char *fullPath);
void removeSampleIsLoadingFlag(void);
