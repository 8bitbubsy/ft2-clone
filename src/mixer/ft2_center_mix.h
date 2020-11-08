#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

// no volume ramping

// 8-bit
void centerMix8bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// 16-bit
void centerMix16bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// volume ramping

// 8-bit
void centerMix8bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// 16bit
void centerMix16bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampNoLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoopSIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
