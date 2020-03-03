#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ft2_replayer.h"

enum
{
	FREQ_TABLE_LINEAR = 0,
	FREQ_TABLE_AMIGA  = 1,
};

// for audio/video sync queue. (2^n-1 - don't change this! Queue buffer is already ~2.7MB in size)
#define SYNC_QUEUE_LEN 4095

#define MAX_AUDIO_DEVICES 99

#define MIN_AUDIO_FREQ 44100
#define MAX_AUDIO_FREQ 96000

#define CUBIC_TABLE_LEN (8192+1)

struct audio_t
{
	char *currInputDevice, *currOutputDevice, *lastWorkingAudioDeviceName;
	char *inputDeviceNames[MAX_AUDIO_DEVICES], *outputDeviceNames[MAX_AUDIO_DEVICES];
	volatile bool locked, resetSyncTickTimeFlag, volumeRampingFlag, interpolationFlag;
	bool linearFreqTable, rescanAudioDevicesSupported;
	int32_t inputDeviceNum, outputDeviceNum, lastWorkingAudioFreq, lastWorkingAudioBits;
	int32_t quickVolSizeVal, *mixBufferL, *mixBufferR, *mixBufferLUnaligned, *mixBufferRUnaligned;
	uint32_t freq;
	uint32_t audLatencyPerfValInt, audLatencyPerfValFrac;
	uint64_t tickTime64, tickTime64Frac;
	double dAudioLatencyMs, dSpeedValMul, dScopeFreqMul;
	SDL_AudioDeviceID dev;
	uint32_t wantFreq, haveFreq, wantSamples, haveSamples, wantChannels, haveChannels;
} audio;

typedef struct
{
	const int8_t *SBase8, *SRevBase8;
	const int16_t *SBase16, *SRevBase16;
	bool backwards, isFadeOutVoice;
	uint8_t SPan;
	uint16_t SVol;
	int32_t SLVol1, SRVol1, SLVol2, SRVol2, SLVolIP, SRVolIP, SVolIPLen, SPos, SLen, SRepS, SRepL;
	uint32_t SPosDec, SFrq, SFrqRev;
	void (*mixRoutine)(void *, int32_t); // function pointer to mix routine
} voice_t;

typedef struct pattSyncData_t
{
	uint8_t pattern, globalVol, songPos, timer, speed, tempo, patternPos;
	uint64_t timestamp;
} pattSyncData_t;

struct pattSync
{
	volatile int32_t readPos, writePos;
	pattSyncData_t data[SYNC_QUEUE_LEN + 1];
} pattSync;

typedef struct chSyncData_t
{
	syncedChannel_t channels[MAX_VOICES];
	uint64_t timestamp;
} chSyncData_t;

struct chSync
{
	volatile int32_t readPos, writePos;
	chSyncData_t data[SYNC_QUEUE_LEN + 1];
} chSync;

extern pattSyncData_t *pattSyncEntry;
extern chSyncData_t *chSyncEntry;

extern volatile bool pattQueueReading, pattQueueClearing, chQueueReading, chQueueClearing;

void resetOldRevFreqs(void);
int32_t pattQueueReadSize(void);
int32_t pattQueueWriteSize(void);
bool pattQueuePush(pattSyncData_t t);
bool pattQueuePop(void);
pattSyncData_t *pattQueuePeek(void);
uint64_t getPattQueueTimestamp(void);
int32_t chQueueReadSize(void);
int32_t chQueueWriteSize(void);
bool chQueuePush(chSyncData_t t);
bool chQueuePop(void);
chSyncData_t *chQueuePeek(void);
uint64_t getChQueueTimestamp(void);
void setAudioAmp(int16_t ampFactor, int16_t master, bool bitDepth32Flag);
void setNewAudioFreq(uint32_t freq);
void setBackOldAudioFreq(void);
void setSpeed(uint16_t bpm);
void audioSetVolRamp(bool volRamp);
void audioSetInterpolation(bool interpolation);
void stopVoice(uint8_t i);
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
void mix_SaveIPVolumes(void);
void mix_UpdateChannelVolPanFrq(void);
uint32_t mixReplayerTickToBuffer(uint8_t *stream, uint8_t bitDepth);
