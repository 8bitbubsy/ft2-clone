#pragma once

#include <stdint.h>
#include "ft2_replayer.h"

#define SAMPLE_AREA_HEIGHT 154
#define SAMPLE_AREA_WIDTH 632
#define SAMPLE_AREA_Y_CENTER 250

// allocs sample with proper alignment and padding for branchless resampling interpolation
bool allocateTmpSmpData(sampleTyp *s, int32_t length);

// reallocs sample with proper alignment and padding for branchless resampling interpolation
bool reallocateTmpSmpData(sampleTyp *s, int32_t length);

sampleTyp *getCurSample(void);
void checkSampleRepeat(sampleTyp *s);
void fixSample(sampleTyp *s); // modifies samples before index 0, and after loop/end (for branchless mixer interpolation)
void restoreSample(sampleTyp *s); // restores samples after loop/end
void clearSample(void);
void clearCopyBuffer(void);
int32_t getSampleMiddleCRate(sampleTyp *s);
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
void sampMin(void);
void sampRepeatUp(void);
void sampRepeatDown(void);
void sampReplenUp(void);
void sampReplenDown(void);
int16_t getSampleValue(int8_t *ptr, uint8_t typ, int32_t pos);
void putSampleValue(int8_t *ptr, uint8_t typ, int32_t pos, int16_t val);
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
void sampleConv(void);
void sampleConvW(void);
void fixDC(void);
void smpEdStop(void);
void testSmpEdMouseUp(void);

extern int32_t smpEd_Rx1, smpEd_Rx2;
