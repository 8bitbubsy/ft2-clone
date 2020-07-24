#pragma once

#include <stdint.h>
#include "../ft2_audio.h"

void centerMix8bNoLoop(voice_t *v, uint32_t numSamples);
void centerMix8bLoop(voice_t *v, uint32_t numSamples);
void centerMix8bBidiLoop(voice_t *v, uint32_t numSamples);
void centerMix8bNoLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix8bLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix8bBidiLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bNoLoop(voice_t *v, uint32_t numSamples);
void centerMix16bLoop(voice_t *v, uint32_t numSamples);
void centerMix16bBidiLoop(voice_t *v, uint32_t numSamples);
void centerMix16bNoLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bBidiLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix8bRampNoLoop(voice_t *v, uint32_t numSamples);
void centerMix8bRampLoop(voice_t *v, uint32_t numSamples);
void centerMix8bRampBidiLoop(voice_t *v, uint32_t numSamples);
void centerMix8bRampNoLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix8bRampLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix8bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bRampNoLoop(voice_t *v, uint32_t numSamples);
void centerMix16bRampLoop(voice_t *v, uint32_t numSamples);
void centerMix16bRampBidiLoop(voice_t *v, uint32_t numSamples);
void centerMix16bRampNoLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bRampLoopIntrp(voice_t *v, uint32_t numSamples);
void centerMix16bRampBidiLoopIntrp(voice_t *v, uint32_t numSamples);
