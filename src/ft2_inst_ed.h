#pragma once

#include <stdint.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_audio.h"
#include "ft2_unicode.h"

void drawC4Rate(void);

bool fileIsInstr(UNICHAR *filenameU);

void saveInstr(UNICHAR *filenameU, int16_t nr);
void loadInstr(UNICHAR *filenameU);
void copyInstr(void); // dstInstr = srcInstr
void xchgInstr(void); // dstInstr <-> srcInstr
void updateNewSample(void);
void updateNewInstrument(void);
void handleInstEditorRedrawing(void);
void hideInstEditor(void);
void exitInstEditor(void);
void updateInstEditor(void);
void showInstEditor(void);
void toggleInstEditor(void);
void midiChDown(void);
void midiChUp(void);
void midiPrgDown(void);
void midiPrgUp(void);
void midiBendDown(void);
void midiBendUp(void);
void sbMidiChPos(uint32_t pos);
void sbMidiPrgPos(uint32_t pos);
void sbMidiBendPos(uint32_t pos);
void volPreDef1(void);
void volPreDef2(void);
void volPreDef3(void);
void volPreDef4(void);
void volPreDef5(void);
void volPreDef6(void);
void panPreDef1(void);
void panPreDef2(void);
void panPreDef3(void);
void panPreDef4(void);
void panPreDef5(void);
void panPreDef6(void);
void relToneOctUp(void);
void relToneOctDown(void);
void relToneUp(void);
void relToneDown(void);
void volEnvAdd(void);
void volEnvDel(void);
void volEnvSusUp(void);
void volEnvSusDown(void);
void volEnvRepSUp(void);
void volEnvRepSDown(void);
void volEnvRepEUp(void);
void volEnvRepEDown(void);
void panEnvAdd(void);
void panEnvDel(void);
void panEnvSusUp(void);
void panEnvSusDown(void);
void panEnvRepSUp(void);
void panEnvRepSDown(void);
void panEnvRepEUp(void);
void panEnvRepEDown(void);
void volDown(void);
void volUp(void);
void panDown(void);
void panUp(void);
void ftuneDown(void);
void ftuneUp(void);
void fadeoutDown(void);
void fadeoutUp(void);
void vibSpeedDown(void);
void vibSpeedUp(void);
void vibDepthDown(void);
void vibDepthUp(void);
void vibSweepDown(void);
void vibSweepUp(void);
void setVolumeScroll(uint32_t pos);
void setPanningScroll(uint32_t pos);
void setFinetuneScroll(uint32_t pos);
void setFadeoutScroll(uint32_t pos);
void setVibSpeedScroll(uint32_t pos);
void setVibDepthScroll(uint32_t pos);
void setVibSweepScroll(uint32_t pos);
void rbVibWaveSine(void);
void rbVibWaveSquare(void);
void rbVibWaveRampDown(void);
void rbVibWaveRampUp(void);
void cbVEnv(void);
void cbVEnvSus(void);
void cbVEnvLoop(void);
void cbPEnv(void);
void cbPEnvSus(void);
void cbPEnvLoop(void);
void drawPiano(chSyncData_t *chSyncData);
bool testInstrVolEnvMouseDown(bool mouseButtonDown);
bool testInstrPanEnvMouseDown(bool mouseButtonDown);
bool testPianoKeysMouseDown(bool mouseButtonDown);
bool testInstrSwitcherMouseDown(void);
void cbInstMidiEnable(void);
void cbInstMuteComputer(void);
void drawInstEditorExt(void);
void showInstEditorExt(void);
void hideInstEditorExt(void);
void toggleInstEditorExt(void);
