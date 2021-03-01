#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_unicode.h"

enum
{
	STEREO_SAMPLE_READ_LEFT = 1,
	STEREO_SAMPLE_READ_RIGHT = 2,
	STEREO_SAMPLE_CONVERT = 3,
};

void normalizeSigned32Bit(int32_t *sampleData, uint32_t sampleLength);
void normalize32BitFloatToSigned16Bit(float *fSampleData, uint32_t sampleLength);
void normalize64BitFloatToSigned16Bit(double *dSampleData, uint32_t sampleLength);

bool loadSample(UNICHAR *filenameU, uint8_t sampleSlot, bool loadAsInstrFlag);
void removeSampleIsLoadingFlag(void);

// globals for sample loaders
extern bool loadAsInstrFlag, smpFilenameSet;
extern char *smpFilename;
extern uint8_t sampleSlot;
extern sampleTyp tmpSmp;
// --------------------------

// file extensions accepted by Disk Op. in sample mode
extern char *supportedSmpExtensions[];
