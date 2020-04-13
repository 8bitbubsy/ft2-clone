#pragma once

#include <stdint.h>
#include "ft2_header.h"

#define MIN_WAV_RENDER_FREQ 44100

#if defined __amd64__ || defined _WIN64

#define MAX_WAV_RENDER_FREQ 192000

#else

#define MAX_WAV_RENDER_FREQ 48000

#endif

#define MAX_WAV_RENDER_SAMPLES_PER_TICK (((MAX_WAV_RENDER_FREQ * 5) / 2) / MIN_BPM)

void updateWavRendererSettings(void);
void drawWavRenderer(void);
void showWavRenderer(void);
void hideWavRenderer(void);
void exitWavRenderer(void);
uint32_t dump_RenderTick(uint8_t *buffer);
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
