#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ft2_replayer.h"

enum
{
	FREQ_TABLE_LINEAR = 0,
	FREQ_TABLE_AMIGA = 1,
};

#define MIN_AUDIO_FREQ 44100
#define MAX_AUDIO_FREQ 192000

#define MIXER_FRAC_BITS 32
#define MIXER_FRAC_SCALE (1ULL << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

#define MAX_AUDIO_DEVICES 99

// for audio/video sync queue. (2^n-1 - don't change this! Queue buffer is already BIG in size)
#define SYNC_QUEUE_LEN 4095

typedef struct audio_t
{
	char *currInputDevice, *currOutputDevice, *lastWorkingAudioDeviceName;
	char *inputDeviceNames[MAX_AUDIO_DEVICES], *outputDeviceNames[MAX_AUDIO_DEVICES];
	volatile bool locked, resetSyncTickTimeFlag, volumeRampingFlag;
	bool linearPeriodsFlag, rescanAudioDevicesSupported;
	volatile uint8_t interpolationType;
	int32_t quickVolRampSamples, inputDeviceNum, outputDeviceNum, lastWorkingAudioFreq, lastWorkingAudioBits;
	uint32_t freq, audLatencyPerfValInt, audLatencyPerfValFrac, samplesPerTick, musicTimeSpeedVal;
	uint64_t tickTime64, tickTime64Frac, tickTimeTab[MAX_BPM+1];
	double dRampQuickVolMul, dRampTickMul, dRampTickMulTab[MAX_BPM+1];
	double *dMixBufferL, *dMixBufferR, *dMixBufferLUnaligned, *dMixBufferRUnaligned, dHz2MixDeltaMul;
	double dAudioLatencyMs, dSamplesPerTick, dTickSampleCounter, dSamplesPerTickTab[MAX_BPM+1];

	SDL_AudioDeviceID dev;
	uint32_t wantFreq, haveFreq, wantSamples, haveSamples, wantChannels, haveChannels;
} audio_t;

typedef struct
{
	const int8_t *base8, *revBase8;
	const int16_t *base16, *revBase16;
	bool active, backwards, isFadeOutVoice, hasLooped;
	uint8_t mixFuncOffset, pan, loopType;
	int32_t pos, end, loopStart, loopLength, oldPeriod;
	uint32_t volRampSamples;
	uint64_t posFrac, delta, oldDelta;

	// if (loopEnabled && hasLooped && samplingPos <= loopStart+SINC_LEFT_TAPS) readFixedTapsFromThisPointer();
	const int8_t *leftEdgeTaps8;
	const int16_t *leftEdgeTaps16;

	const double *dSincLUT;
	double dOldHz, dHz, dVol, dDestVolL, dDestVolR, dVolL, dVolR, dVolDeltaL, dVolDeltaR;
} voice_t;

typedef struct pattSyncData_t
{
	uint8_t pattern, globalVol, songPos, timer, tempo, patternPos;
	uint16_t speed;
	uint64_t timestamp;
} pattSyncData_t;

typedef struct pattSync_t
{
	volatile int32_t readPos, writePos;
	pattSyncData_t data[SYNC_QUEUE_LEN+1];
} pattSync_t;

typedef struct chSyncData_t
{
	syncedChannel_t channels[MAX_VOICES];
	uint64_t timestamp;
} chSyncData_t;

typedef struct chSync_t
{
	volatile int32_t readPos, writePos;
	chSyncData_t data[SYNC_QUEUE_LEN+1];
} chSync_t;

void resetCachedMixerVars(void);
int32_t pattQueueReadSize(void);
int32_t pattQueueWriteSize(void);
bool pattQueuePush(pattSyncData_t t);
bool pattQueuePop(void);
bool pattQueuePop(void);
pattSyncData_t *pattQueuePeek(void);
uint64_t getPattQueueTimestamp(void);
int32_t chQueueReadSize(void);
int32_t chQueueWriteSize(void);
bool chQueuePush(chSyncData_t t);
bool chQueuePop(void);
chSyncData_t *chQueuePeek(void);
uint64_t getChQueueTimestamp(void);
void resetSyncQueues(void);

void calcPanningTable(void);
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag);
void setNewAudioFreq(uint32_t freq);
void setBackOldAudioFreq(void);
void P_SetSpeed(uint16_t bpm);
void audioSetVolRamp(bool volRamp);
void audioSetInterpolationType(uint8_t interpolationType);
void stopVoice(int32_t i);
bool setupAudio(bool showErrorMsg);
void closeAudio(void);
void pauseAudio(void);
void resumeAudio(void);
bool setNewAudioSettings(void);
void resetAudioDither(void);
void lockAudio(void);
void unlockAudio(void);
void lockMixerCallback(void);
void unlockMixerCallback(void);
void updateSendAudSamplesRoutine(bool lockMixer);
void resetRampVolumes(void);
void updateVoices(void);
void mixReplayerTickToBuffer(uint32_t samplesToMix, uint8_t *stream, uint8_t bitDepth);

// in ft2_audio.c
extern audio_t audio;
extern pattSyncData_t *pattSyncEntry;
extern chSyncData_t *chSyncEntry;
extern chSync_t chSync;
extern pattSync_t pattSync;

extern volatile bool pattQueueClearing, chQueueClearing;
