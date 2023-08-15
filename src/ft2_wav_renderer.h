#pragma once

#include <stdint.h>
#include "ft2_header.h"
#include "ft2_cpu.h"

#define MIN_WAV_RENDER_FREQ 44100

#if CPU_64BIT
#define MAX_WAV_RENDER_FREQ 384000
#else
#define MAX_WAV_RENDER_FREQ 48000
#endif

void cbToggleWavRenderBPMMode(void);
void setWavRenderFrequency(int32_t freq);
void setWavRenderBitDepth(uint8_t bitDepth);
void updateWavRendererSettings(void);
void drawWavRenderer(void);
void showWavRenderer(void);
void hideWavRenderer(void);
void exitWavRenderer(void);
void pbWavRender(void);
void pbWavExit(void);
void pbWavFreqUp(void);
void pbWavFreqDown(void);
void pbWavAmpUp(void);
void pbWavAmpDown(void);
void pbWavSongStartUp(void);
void pbWavSongStartDown(void);
void pbWavSongEndUp(void);
void pbWavSongEndDown(void);
void resetWavRenderer(void);
void rbWavRenderBitDepth16(void);
void rbWavRenderBitDepth32(void);
