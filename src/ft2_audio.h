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

/*  Use 16 on non-x86_64 platforms so that we can avoid a
** 64-bit division in the outside mixer loop. x86_64 users
** are lucky and will get double the fractional delta precision.
** This is beneficial in 96kHz/192kHz mode, where deltas are
** lower in value.
*/
#if defined _WIN64 || defined __amd64__

#define MIN_AUDIO_FREQ 44100
#define MAX_AUDIO_FREQ 192000

#define MIXER_FRAC_BITS 32
#define MIXER_FRAC_SCALE (1ULL << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

#else

#define MIN_AUDIO_FREQ 44100
#define MAX_AUDIO_FREQ 48000

#define MIXER_FRAC_BITS 16
#define MIXER_FRAC_SCALE (1UL << MIXER_FRAC_BITS)
#define MIXER_FRAC_MASK (MIXER_FRAC_SCALE-1)

#endif

#define MAX_AUDIO_DEVICES 99

// for audio/video sync queue. (2^n-1 - don't change this! Queue buffer is already ~2.7MB in size)
#define SYNC_QUEUE_LEN 4095

typedef struct audio_t
{
	char *currInputDevice, *currOutputDevice, *lastWorkingAudioDeviceName;
	char *inputDeviceNames[MAX_AUDIO_DEVICES], *outputDeviceNames[MAX_AUDIO_DEVICES];
	volatile bool locked, resetSyncTickTimeFlag, volumeRampingFlag, interpolationFlag;
	bool linearFreqTable, rescanAudioDevicesSupported;
	int32_t inputDeviceNum, outputDeviceNum, lastWorkingAudioFreq, lastWorkingAudioBits;
	int32_t quickVolSizeVal, *mixBufferL, *mixBufferR, *mixBufferLUnaligned, *mixBufferRUnaligned;
	int32_t rampQuickVolMul, rampSpeedValMul;
	uint32_t freq;
	uint32_t audLatencyPerfValInt, audLatencyPerfValFrac, speedVal, musicTimeSpeedVal;
	uint64_t tickTime64, tickTime64Frac;
	double dAudioLatencyMs, dSpeedValMul, dPianoDeltaMul;
	SDL_AudioDeviceID dev;
	uint32_t wantFreq, haveFreq, wantSamples, haveSamples, wantChannels, haveChannels;
} audio_t;

typedef struct
{
	const int8_t *SBase8, *SRevBase8;
	const int16_t *SBase16, *SRevBase16;
	bool backwards, isFadeOutVoice;
	uint8_t SPan;
	uint16_t SVol;
	int32_t SLVol1, SRVol1, SLVol2, SRVol2, SLVolIP, SRVolIP;
	int32_t SPos, SLen, SRepS, SRepL;
	uint32_t SVolIPLen;

#if defined _WIN64 || defined __amd64__
	uint64_t SPosDec, SFrq;
#else
	uint32_t SPosDec, SFrq, SFrqRev;
#endif

	void (*mixRoutine)(void *, int32_t); // function pointer to mix routine
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
	pattSyncData_t data[SYNC_QUEUE_LEN + 1];
} pattSync_t;

typedef struct chSyncData_t
{
	syncedChannel_t channels[MAX_VOICES];
	uint64_t timestamp;
} chSyncData_t;

typedef struct chSync_t
{
	volatile int32_t readPos, writePos;
	chSyncData_t data[SYNC_QUEUE_LEN + 1];
} chSync_t;

// in ft2_audio.c
extern audio_t audio;
extern pattSyncData_t *pattSyncEntry;
extern chSyncData_t *chSyncEntry;
extern chSync_t chSync;
extern pattSync_t pattSync;

extern volatile bool pattQueueReading, pattQueueClearing, chQueueReading, chQueueClearing;

#if !defined __amd64__ && !defined _WIN64
void resetCachedMixerVars(void);
#endif

void calcAudioTables(void);

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
void setAudioAmp(int16_t ampFactor, int16_t master, bool bitDepth32Flag);
void setNewAudioFreq(uint32_t freq);
void setBackOldAudioFreq(void);
void setSpeed(uint16_t bpm);
void audioSetVolRamp(bool volRamp);
void audioSetInterpolation(bool interpolation);
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
void mix_SaveIPVolumes(void);
void mix_UpdateChannelVolPanFrq(void);
uint32_t mixReplayerTickToBuffer(uint8_t *stream, uint8_t bitDepth);
