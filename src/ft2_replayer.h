#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

enum
{
	// voice flags
	IS_Vol = 1, // set volume
	IS_Period = 2, // set resampling rate
	IS_NyTon = 4, // trigger new sample
	IS_Pan = 8, // set panning
	IS_QuickVol = 16, // 5ms volramp instead of tick ms

	// tracker playback modes
	PLAYMODE_IDLE = 0,
	PLAYMODE_EDIT = 1,
	PLAYMODE_SONG = 2,
	PLAYMODE_PATT = 3,
	PLAYMODE_RECSONG = 4,
	PLAYMODE_RECPATT = 5,

	// note cursor positions
	CURSOR_NOTE = 0,
	CURSOR_INST1 = 1,
	CURSOR_INST2 = 2,
	CURSOR_VOL1 = 3,
	CURSOR_VOL2 = 4,
	CURSOR_EFX0 = 5,
	CURSOR_EFX1 = 6,
	CURSOR_EFX2 = 7
};

// DO NOT TOUCH!
#define MIN_BPM 32
#define MAX_VOICES 32
#define TRACK_WIDTH (5 * MAX_VOICES)
#define MAX_NOTES ((12 * 10 * 16) + 16)
#define MAX_PATTERNS 256
#define MAX_PATT_LEN 256
#define MAX_INST 128
#define MAX_SMP_PER_INST 16
#define MAX_ORDERS 256
#define STD_ENV_SIZE ((6*2*12*2*2) + (6*8*2) + (6*5*2) + (6*2*2))
#define INSTR_HEADER_SIZE 263
#define INSTR_XI_HEADER_SIZE 298
#define MAX_SAMPLE_LEN 0x3FFFFFFF
#define PROG_NAME_STR "Fasttracker II clone"

/* Some of the following structs must be packed (e.g. not padded) since they
** are loaded directly into with fread and stuff. */

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct songHeaderTyp_t // DO NOT TOUCH!
{
	char sig[17], name[21], progName[20];
	uint16_t ver;
	int32_t headerSize;
	uint16_t len, repS, antChn, antPtn, antInstrs, flags, defTempo, defSpeed;
	uint8_t songTab[256];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songHeaderTyp;

typedef struct patternHeaderTyp_t // DO NOT TOUCH!
{
	int32_t patternHeaderSize;
	uint8_t typ;
	int16_t pattLen;
	uint16_t dataLen;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
patternHeaderTyp;

typedef struct songMODInstrHeaderTyp_t
{
	char name[22];
	uint16_t len;
	uint8_t fine, vol;
	uint16_t repS, repL;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMODInstrHeaderTyp;

typedef struct songMOD15HeaderTyp_t
{
	char name[20];
	songMODInstrHeaderTyp instr[15];
	uint8_t len, repS, songTab[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMOD15HeaderTyp;

typedef struct songMOD31HeaderTyp_t
{
	char name[20];
	songMODInstrHeaderTyp instr[31];
	uint8_t len, repS, songTab[128];
	char sig[4];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMOD31HeaderTyp;

typedef struct sampleHeaderTyp_t // DO NOT TOUCH!
{
	int32_t len, repS, repL;
	uint8_t vol;
	int8_t fine;
	uint8_t typ, pan;
	int8_t relTon;
	uint8_t nameLen;
	char name[22];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
sampleHeaderTyp;

typedef struct instrHeaderTyp_t // DO NOT TOUCH!
{
	uint32_t instrSize;
	char name[22];
	uint8_t typ;
	int16_t antSamp;
	int32_t sampleSize;
	uint8_t ta[96];
	int16_t envVP[12][2], envPP[12][2];
	uint8_t envVPAnt, envPPAnt;
	uint8_t envVSust, envVRepS, envVRepE;
	uint8_t envPSust, envPRepS, envPRepE;
	uint8_t envVTyp, envPTyp;
	uint8_t vibTyp, vibSweep, vibDepth, vibRate;
	uint16_t fadeOut;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	int8_t mute;
	uint8_t reserved[15];
	sampleHeaderTyp samp[16];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
instrHeaderTyp;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct sampleTyp_t
{
	char name[22+1];
	bool fixed;
	int8_t fine, relTon, *pek;
	uint8_t vol, typ, pan;
	int16_t fixedSmp1;
#ifndef LERPMIX
	int16_t fixedSmp2;
#endif
	int32_t fixedPos, len, repS, repL;
} sampleTyp;

typedef struct instrTyp_t
{
	bool midiOn, mute;
	uint8_t midiChannel, ta[96];
	uint8_t envVPAnt, envPPAnt;
	uint8_t envVSust, envVRepS, envVRepE;
	uint8_t envPSust, envPRepS, envPRepE;
	uint8_t envVTyp, envPTyp;
	uint8_t vibTyp, vibSweep, vibDepth, vibRate;
	uint16_t fadeOut;
	int16_t envVP[12][2], envPP[12][2], midiProgram, midiBend;
	int16_t antSamp; // used by loader only
	sampleTyp samp[16];
} instrTyp;

typedef struct stmTyp_t
{
	bool envSustainActive, stOff, mute;
	volatile uint8_t status, tmpStatus;
	int8_t relTonNr, fineTune;
	uint8_t sampleNr, instrNr, effTyp, eff, smpOffset, tremorSave, tremorPos;
	uint8_t globVolSlideSpeed, panningSlideSpeed, waveCtrl, portaDir;
	uint8_t glissFunk, vibPos, tremPos, vibSpeed, vibDepth, tremSpeed, tremDepth;
	uint8_t pattPos, loopCnt, volSlideSpeed, fVolSlideUpSpeed, fVolSlideDownSpeed;
	uint8_t fPortaUpSpeed, fPortaDownSpeed, ePortaUpSpeed, ePortaDownSpeed;
	uint8_t portaUpSpeed, portaDownSpeed, retrigSpeed, retrigCnt, retrigVol;
	uint8_t volKolVol, tonNr, envPPos, eVibPos, envVPos, realVol, oldVol, outVol;
	uint8_t oldPan, outPan, finalPan;
	int16_t midiCurChannel, midiCurTone, midiCurVibDepth, midiCurPeriod, midiCurPitch;
	int16_t midiBend, midiPortaPeriod, midiPitch, realPeriod, envVIPValue, envPIPValue;
	uint16_t finalVol, outPeriod, finalPeriod, tonTyp, wantPeriod, portaSpeed;
	uint16_t envVCnt, envVAmp, envPCnt, envPAmp, eVibAmp, eVibSweep;
	uint16_t fadeOutAmp, fadeOutSpeed, midiVibDepth;
	int32_t smpStartPos;
	sampleTyp *smpPtr;
	instrTyp *instrSeg;
} stmTyp;

typedef struct songTyp_t
{
	bool pBreakFlag, posJumpFlag, isModified;
	char name[20+1], instrName[1+MAX_INST][22+1];
	uint8_t curReplayerTimer, curReplayerPattPos, curReplayerSongPos, curReplayerPattNr; // used for audio/video sync queue
	uint8_t antChn, pattDelTime, pattDelTime2, pBreakPos, songTab[MAX_ORDERS];
	int16_t songPos, pattNr, pattPos, pattLen;
	uint16_t len, repS, speed, tempo, globVol, timer, ver, initialTempo;
	uint32_t musicTime;
} songTyp;

typedef struct tonTyp_t
{
	uint8_t ton, instr, vol, effTyp, eff;
} tonTyp;

typedef struct syncedChannel_t // used for audio/video sync queue
{
	bool envSustainActive;
	int8_t fineTune, relTonNr;
	uint8_t status, sampleNr, instrNr;
	uint16_t finalPeriod, finalVol;
	int32_t smpStartPos;
	uint32_t voiceDelta;
} syncedChannel_t;

void fixSongName(void); // removes spaces from right side of song name
void fixSampleName(int16_t nr); // removes spaces from right side of ins/smp names

void calcReplayRate(uint32_t rate);
void resetOldRates(void);
void tuneSample(sampleTyp *s, uint32_t midCFreq);
uint32_t getFrequenceValue(uint16_t period);
int16_t relocateTon(int16_t period, int8_t relativeNote, stmTyp *ch);

bool allocateInstr(int16_t nr);
void freeInstr(int16_t nr);
void freeAllInstr(void);
void freeSample(int16_t nr, int16_t nr2);

void freeAllPatterns(void);
void updateChanNums(void);
bool setupReplayer(void);
void closeReplayer(void);
void resetMusic(void);
void startPlaying(int8_t mode, int16_t row);
void stopPlaying(void);
void stopVoices(void);
void setPos(int16_t songPos, int16_t pattPos, bool resetTimer);
void pauseMusic(void); // stops reading pattern data
void resumeMusic(void); // starts reading pattern data
void setSongModifiedFlag(void);
void removeSongModifiedFlag(void);
void playTone(uint8_t stmm, uint8_t inst, uint8_t ton, int8_t vol, uint16_t midiVibDepth, uint16_t midiPitch);
void playSample(uint8_t stmm, uint8_t inst, uint8_t smpNr, uint8_t ton, uint16_t midiVibDepth, uint16_t midiPitch);
void playRange(uint8_t stmm, uint8_t inst, uint8_t smpNr, uint8_t ton, uint16_t midiVibDepth, uint16_t midiPitch, int32_t offs, int32_t len);
void keyOff(stmTyp *ch);
void conv8BitSample(int8_t *p, int32_t len, bool stereo);
void conv16BitSample(int8_t *p, int32_t len, bool stereo);
void delta2Samp(int8_t *p, int32_t len, uint8_t typ);
void samp2Delta(int8_t *p, int32_t len, uint8_t typ);
void setPatternLen(uint16_t nr, int16_t len);
void setFrqTab(bool linear);
void mainPlayer(void); // periodically called from audio callback
void resetChannels(void);
bool patternEmpty(uint16_t nr);
int16_t getUsedSamples(int16_t nr);
int16_t getRealUsedSamples(int16_t nr);
void setStdEnvelope(instrTyp *ins, int16_t i, uint8_t typ);
void setNoEnvelope(instrTyp *ins);
void setSyncedReplayerVars(void);
void decSongPos(void);
void incSongPos(void);
void decCurIns(void);
void incCurIns(void);
void decCurSmp(void);
void incCurSmp(void);
void pbPlaySong(void);
void pbPlayPtn(void);
void pbRecSng(void);
void pbRecPtn(void);

// ft2_replayer.c
extern int8_t playMode;
extern bool linearFrqTab, songPlaying, audioPaused, musicPaused;
extern volatile bool replayerBusy;
extern int16_t *note2Period, pattLens[MAX_PATTERNS];
extern stmTyp stm[MAX_VOICES];
extern songTyp song;
extern instrTyp *instr[132];
extern tonTyp *patt[MAX_PATTERNS];
