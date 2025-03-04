#pragma once

#include <stdint.h>
#include "ft2_header.h"

void clearSampleUndo(void);
void cbSfxNormalization(void);
void pbSfxCyclesUp(void);
void pbSfxCyclesDown(void);
void pbSfxTriangle(void);
void pbSfxSaw(void);
void pbSfxSine(void);
void pbSfxSquare(void);
void pbSfxResoUp(void);
void pbSfxResoDown(void);
void pbSfxLowPass(void);
void pbSfxHighPass(void);
void sfxPreviewFilter(uint32_t cutof);
void pbSfxSubBass(void);
void pbSfxSubTreble(void);
void pbSfxAddBass(void);
void pbSfxAddTreble(void);
void pbSfxSetAmp(void);
void pbSfxUndo(void);
void hideSampleEffectsScreen(void);
void pbEffects(void);
