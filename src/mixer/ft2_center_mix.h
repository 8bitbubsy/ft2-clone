#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

// no volume ramping

// 8-bit
void centerMix8bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bBidiLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// 16-bit
void centerMix16bNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bBidiLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// volume ramping

// 8-bit
void centerMix8bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix8bRampBidiLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);

// 16bit
void centerMix16bRampNoLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoop(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampNoLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoopS8Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampNoLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoopLIntrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampNoLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
void centerMix16bRampBidiLoopS16Intrp(voice_t *v, uint32_t bufferPos, uint32_t numSamples);
