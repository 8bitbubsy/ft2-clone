#pragma once

#include <stdint.h>
#include "ft2_header.h"

#define SAMPLE_AREA_HEIGHT 154
#define SAMPLE_AREA_WIDTH 632
#define SAMPLE_AREA_Y_CENTER 250

// allocs sample with proper alignment and padding for branchless resampling interpolation
bool allocateSmpData(sample_t *s, int32_t length, bool sample16Bit);
bool allocateSmpDataPtr(smpPtr_t *sp, int32_t length, bool sample16Bit);

// reallocs sample with proper alignment and padding for branchless resampling interpolation
bool reallocateSmpData(sample_t *s, int32_t length, bool sample16Bit);
bool reallocateSmpDataPtr(smpPtr_t *sp, int32_t length, bool sample16Bit);

void setSmpDataPtr(sample_t *s, smpPtr_t *sp);
void freeSmpDataPtr(smpPtr_t *sp);
void freeSmpData(sample_t *s);
bool cloneSample(sample_t *src, sample_t *dst);
sample_t *getCurSample(void);
void sanitizeSample(sample_t *s);
void fixSample(sample_t *s); // modifies samples before index 0, and after loop/end (for branchless mixer interpolation)
void unfixSample(sample_t *s); // restores samples after loop/end
void clearSample(void);
void clearCopyBuffer(void);
int32_t getSampleMiddleCRate(sample_t *s);
int32_t getSampleRangeStart(void);
int32_t getSampleRangeEnd(void);
int32_t getSampleRangeLength(void);
void copySmp(void); // dstSmp = srcSmp
void xchgSmp(void); // dstSmp <-> srcSmp
void scrollSampleDataLeft(void);
void scrollSampleDataRight(void);
void scrollSampleData(uint32_t pos);
void sampPlayNoteUp(void);
void sampPlayNoteDown(void);
void sampPlayWave(void);
void sampPlayRange(void);
void sampPlayDisplay(void);
void showRange(void);
void rangeAll(void);
void mouseZoomSampleDataIn(void);
void mouseZoomSampleDataOut(void);
void zoomOut(void);
void showAll(void);
void saveRange(void);
void sampCut(void);
void sampCopy(void);
void sampPaste(void);
void sampCrop(void);
void sampXFade(void);
void rbSampleNoLoop(void);
void rbSampleForwardLoop(void);
void rbSamplePingpongLoop(void);
void rbSample8bit(void);
void rbSample16bit(void);
void sampMinimize(void);
void sampRepeatUp(void);
void sampRepeatDown(void);
void sampReplenUp(void);
void sampReplenDown(void);
double getSampleValue(int8_t *smpData, int32_t position, bool sample16Bit);
void putSampleValue(int8_t *smpData, int32_t position, double dSample, bool sample16Bit);
void writeSample(bool forceSmpRedraw);
void handleSampleDataMouseDown(bool mouseButtonHeld);
void updateSampleEditorSample(void);
void updateSampleEditor(void);
void hideSampleEditor(void);
void exitSampleEditor(void);
void showSampleEditor(void);
void handleSamplerRedrawing(void);
void toggleSampleEditor(void);
void toggleSampleEditorExt(void);
void showSampleEditorExt(void);
void hideSampleEditorExt(void);
void drawSampleEditorExt(void);
void handleSampleEditorExtRedrawing(void);
void sampleBackwards(void);
void sampleChangeSign(void);
void sampleByteSwap(void);
void fixDC(void);
void smpEdStop(void);
void testSmpEdMouseUp(void);

void sampleLine(int32_t x1, int32_t x2, int32_t y1, int32_t y2);

extern int32_t smpEd_Rx1, smpEd_Rx2;
