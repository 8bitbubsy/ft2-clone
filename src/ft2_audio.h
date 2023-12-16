#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ft2_replayer.h"
#include "ft2_cpu.h"

enum
{
	FREQ_TABLE_LINEAR = 0,
	FREQ_TABLE_AMIGA = 1,
};

#define DEFAULT_AUDIO_FREQ 48000

#define MIN_AUDIO_FREQ 44100

#if CPU_64BIT
#define MAX_AUDIO_FREQ 96000
#else
#define MAX_AUDIO_FREQ 48000
#endif

#define MAX_AUDIO_DEVICES 99

// more bits makes little sense here

#define BPM_FRAC_BITS 52
#define BPM_FRAC_SCALE (1ULL << BPM_FRAC_BITS)
#define BPM_FRAC_MASK (BPM_FRAC_SCALE-1)

#define TICK_TIME_FRAC_BITS 52
#define TICK_TIME_FRAC_SCALE (1ULL << TICK_TIME_FRAC_BITS)
#define TICK_TIME_FRAC_MASK (TICK_TIME_FRAC_SCALE-1)

// for audio/video sync queue. (2^n-1 - don't change this! Queue buffer is already BIG in size)
#define SYNC_QUEUE_LEN 4095

typedef struct audio_t
{
	char *currInputDevice, *currOutputDevice, *lastWorkingAudioDeviceName;
	char *inputDeviceNames[MAX_AUDIO_DEVICES], *outputDeviceNames[MAX_AUDIO_DEVICES];
	volatile bool locked, resetSyncTickTimeFlag, volumeRampingFlag;
	bool linearPeriodsFlag, rescanAudioDevicesSupported;
	volatile uint8_t interpolationType;
	int32_t inputDeviceNum, outputDeviceNum, lastWorkingAudioFreq, lastWorkingAudioBits;
	uint32_t quickVolRampSamples, freq;

	uint32_t tickSampleCounter, samplesPerTickInt, samplesPerTickIntTab[(MAX_BPM-MIN_BPM)+1];
	uint64_t tickSampleCounterFrac, samplesPerTickFrac, samplesPerTickFracTab[(MAX_BPM-MIN_BPM)+1];

	uint32_t audLatencyPerfValInt, tickTimeIntTab[(MAX_BPM-MIN_BPM)+1];
	uint64_t audLatencyPerfValFrac, tickTimeFracTab[(MAX_BPM-MIN_BPM)+1];

	uint64_t tickTime64, tickTime64Frac;

	float *fMixBufferL, *fMixBufferR;
	double dHz2MixDeltaMul, dAudioLatencyMs;

	SDL_AudioDeviceID dev;
	uint32_t wantFreq, haveFreq, wantSamples, haveSamples;
} audio_t;

typedef struct
{
	const int8_t *base8, *revBase8;
	const int16_t *base16, *revBase16;
	bool active, samplingBackwards, isFadeOutVoice, hasLooped;
	uint8_t mixFuncOffset, panning, loopType, scopeVolume;
	int32_t position, sampleEnd, loopStart, loopLength, oldPeriod;
	uint32_t volumeRampLength;

	uintCPUWord_t positionFrac, delta, oldDelta, scopeDelta;

	// if (loopEnabled && hasLooped && samplingPos <= loopStart+MAX_LEFT_TAPS) readFixedTapsFromThisPointer();
	const int8_t *leftEdgeTaps8;
	const int16_t *leftEdgeTaps16;

	const float *fSincLUT;
	float fVolume, fCurrVolumeL, fCurrVolumeR, fVolumeLDelta, fVolumeRDelta, fTargetVolumeL, fTargetVolumeR;
} voice_t;

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct pattSyncData_t // used for audio/video sync queue (pack to save RAM)
{
	uint8_t pattNum, globalVolume, songPos, tick, speed, row, BPM;
	uint64_t timestamp;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pattSyncData_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct pattSync_t
{
	volatile int32_t readPos, writePos;
	pattSyncData_t data[SYNC_QUEUE_LEN+1];
} pattSync_t;

typedef struct chSyncData_t
{
	syncedChannel_t channels[MAX_CHANNELS];
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

void decreaseMasterVol(void);
void increaseMasterVol(void);

void calcPanningTable(void);
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag);
void setNewAudioFreq(uint32_t freq);
void setBackOldAudioFreq(void);
void setMixerBPM(int32_t bpm);
void audioSetVolRamp(bool volRamp);
void audioSetInterpolationType(uint8_t interpolationType);
void stopVoice(int32_t i);
bool setupAudio(bool showErrorMsg);
void closeAudio(void);
void pauseAudio(void);
void resumeAudio(void);
bool setNewAudioSettings(void);
void lockAudio(void);
void unlockAudio(void);
void lockMixerCallback(void);
void unlockMixerCallback(void);
void resetRampVolumes(void);
void updateVoices(void);
void mixReplayerTickToBuffer(uint32_t samplesToMix, void *stream, uint8_t bitDepth);

// in ft2_audio.c
extern audio_t audio;
extern pattSyncData_t *pattSyncEntry;
extern chSyncData_t *chSyncEntry;
extern chSync_t chSync;
extern pattSync_t pattSync;

extern volatile bool pattQueueClearing, chQueueClearing;
