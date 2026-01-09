#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"
#include "mixer/ft2_mix_interpolation.h"

enum
{
	// channel/voice status flags
	CS_UPDATE_VOL = 1,
	CF_UPDATE_PERIOD = 2,
	CS_TRIGGER_VOICE = 4,
	CS_UPDATE_PAN = 8,
	CS_USE_QUICK_VOLRAMP = 16, // use 5ms vol. ramp instead of the duration of a tick

	LOOP_DISABLED = 0,
	LOOP_FORWARD = 1,
	LOOP_PINGPONG = 2,

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

// do not touch these!
#define MIN_BPM 32
#define MAX_BPM 255
#define MAX_SPEED 31
#define MAX_CHANNELS 32
#define TRACK_WIDTH (5 * MAX_CHANNELS)
#define C4_FREQ 8363
#define NOTE_C4 (4*12)
#define NOTE_OFF 97
#define MAX_NOTES ((10*12*16)+16)
#define MAX_PATTERNS 256
#define MAX_PATT_LEN 256
#define MAX_INST 128
#define MAX_SMP_PER_INST 16
#define MAX_ORDERS 256
#define STD_ENV_SIZE ((6*2*12*2*2) + (6*8*2) + (6*5*2) + (6*2*2))
#define INSTR_HEADER_SIZE 263
#define INSTR_XI_HEADER_SIZE 298
#define MAX_SAMPLE_LEN 0x3FFFFFFF
#define FT2_QUICK_VOLRAMP_MILLISECONDS 5
#define PROG_NAME_STR "Fasttracker II clone"

enum // sample flags
{
	LOOP_OFF = 0,
	LOOP_FWD = 1,
	LOOP_BIDI = 2,
	SAMPLE_16BIT = 16,
	SAMPLE_STEREO = 32,
	SAMPLE_ADPCM = 64, // not an existing flag, but used by loader
};

enum // envelope flags
{
	ENV_ENABLED = 1,
	ENV_SUSTAIN = 2,
	ENV_LOOP    = 4
};

#define GET_LOOPTYPE(smpFlags) ((smpFlags) & (LOOP_FWD | LOOP_BIDI))
#define DISABLE_LOOP(smpFlags) ((smpFlags) &= ~(LOOP_FWD | LOOP_BIDI))
#define SAMPLE_LENGTH_BYTES(smp) (smp->length << !!(smp->flags & SAMPLE_16BIT))
#define FINETUNE_MOD2XM(f) (((uint8_t)(f) & 0x0F) << 4)
#define FINETUNE_XM2MOD(f) ((uint8_t)(f) >> 4)

/* Some of the following structs MUST be packed!
** Please do NOT edit these structs unless you
** absolutely know what you are doing!
*/

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct xmHdr_t
{
	char ID[17], name[20], x1A, progName[20];
	uint16_t version;
	int32_t headerSize;
	uint16_t numOrders, songLoopStart, numChannels, numPatterns;
	uint16_t numInstr, flags, speed, BPM;
	uint8_t orders[256];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
xmHdr_t;

typedef struct xmPatHdr_t
{
	int32_t headerSize;
	uint8_t type;
	int16_t numRows;
	uint16_t dataSize;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
xmPatHdr_t;

typedef struct modSmpHdr_t
{
	char name[22];
	uint16_t length;
	uint8_t finetune, volume;
	uint16_t loopStart, loopLength;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
modSmpHdr_t;

typedef struct modHdr_t
{
	char name[20];
	modSmpHdr_t smp[31];
	uint8_t numOrders, songLoopStart, orders[128];
	char ID[4];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
modHdr_t;

typedef struct xmSmpHdr_t
{
	uint32_t length, loopStart, loopLength;
	uint8_t volume;
	int8_t finetune;
	uint8_t flags, panning;
	int8_t relativeNote;
	uint8_t nameLength; // only handled before saving (ignored under load)
	char name[22];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
xmSmpHdr_t;

typedef struct xmInsHdr_t
{
	uint32_t instrSize;
	char name[22];
	uint8_t type;
	int16_t numSamples;
	int32_t sampleSize;
	uint8_t note2SampleLUT[96];
	int16_t volEnvPoints[12][2], panEnvPoints[12][2];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t vibType, vibSweep, vibDepth, vibRate;
	uint16_t fadeout;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	int8_t mute;
	uint8_t junk[15];
	xmSmpHdr_t smp[16];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
xmInsHdr_t;

typedef struct pattNote_t // must be packed!
{
	uint8_t note, instr, vol, efx, efxData;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
note_t;

typedef struct syncedChannel_t // used for audio/video sync queue (pack to save RAM)
{
	uint8_t status, pianoNoteNum, smpNum, instrNum, scopeVolume;
	uint16_t period;
	int32_t smpStartPos;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
syncedChannel_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

typedef struct sample_t
{
	char name[22+1];
	bool isFixed;
	int8_t finetune, relativeNote, *dataPtr, *origDataPtr;
	uint8_t volume, flags, panning;
	int32_t length, loopStart, loopLength;

	// fix for resampling interpolation taps
	int8_t leftEdgeTapSamples8[MAX_TAPS*2];
	int16_t leftEdgeTapSamples16[MAX_TAPS*2];
	int16_t fixedSmp[MAX_TAPS*2];
	int32_t fixedPos;
} sample_t;

typedef struct instr_t
{
	bool midiOn, mute;
	uint8_t midiChannel, note2SampleLUT[96];
	uint8_t volEnvLength, panEnvLength;
	uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
	uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
	uint8_t volEnvFlags, panEnvFlags;
	uint8_t autoVibType, autoVibSweep, autoVibDepth, autoVibRate;
	uint16_t fadeout;
	int16_t volEnvPoints[12][2], panEnvPoints[12][2], midiProgram, midiBend;
	int16_t numSamples; // used by loader only
	sample_t smp[16];
} instr_t;

typedef struct channel_t
{
	bool dontRenderThisChannel, keyOff, channelOff, mute, semitonePortaMode;
	volatile uint8_t status, tmpStatus;
	int8_t relativeNote, finetune;
	uint8_t smpNum, instrNum, efxData, efx, sampleOffset, tremorParam, tremorPos;
	uint8_t globVolSlideSpeed, panningSlideSpeed, vibTremCtrl, portamentoDirection;
	uint8_t vibratoPos, tremoloPos, vibratoSpeed, vibratoDepth, tremoloSpeed, tremoloDepth;
	uint8_t patternLoopStartRow, patternLoopCounter, volSlideSpeed, fVolSlideUpSpeed, fVolSlideDownSpeed;
	uint8_t fPitchSlideUpSpeed, fPitchSlideDownSpeed, efPitchSlideUpSpeed, efPitchSlideDownSpeed;
	uint8_t pitchSlideUpSpeed, pitchSlideDownSpeed, noteRetrigSpeed, noteRetrigCounter, noteRetrigVol;
	uint8_t volColumnVol, noteNum, panEnvPos, autoVibPos, volEnvPos, realVol, oldVol, outVol;
	uint8_t oldPan, outPan, finalPan;
	int16_t midiPitch;
	uint16_t outPeriod, realPeriod, finalPeriod, copyOfInstrAndNote, portamentoTargetPeriod, portamentoSpeed;
	uint16_t volEnvTick, panEnvTick, autoVibAmp, autoVibSweep;
	uint16_t midiVibDepth;
	int32_t fadeoutVol, fadeoutSpeed;
	int32_t smpStartPos;

	float fFinalVol, fVolEnvDelta, fPanEnvDelta, fVolEnvValue, fPanEnvValue;

	sample_t *smpPtr;
	instr_t *instrPtr;
} channel_t;

typedef struct song_t
{
	bool pBreakFlag, posJumpFlag, isModified;
	char name[20+1], instrName[1+MAX_INST][22+1];
	uint8_t curReplayerTick, curReplayerRow, curReplayerSongPos, curReplayerPattNum; // used for audio/video sync queue
	uint8_t pattDelTime, pattDelTime2, pBreakPos, orders[MAX_ORDERS];
	int16_t songPos, pattNum, row, currNumRows;
	uint16_t songLength, songLoopStart, BPM, speed, initialSpeed, globalVolume, tick;
	int32_t numChannels;

	uint32_t playbackSeconds;
	uint64_t playbackSecondsFrac;
} song_t;

double getSampleC4Rate(sample_t *s);

void setNewSongPos(int32_t pos);

void fixString(char *str, int32_t lastChrPos); // removes leading spaces and 0x1A chars
void fixSongName(void);
void fixInstrAndSampleNames(int16_t insNum);

void calcReplayerVars(int32_t rate);
void setSampleC4Hz(sample_t *s, double dC4Hz);

double dPeriod2Hz(uint32_t period);
uint64_t period2VoiceDelta(uint32_t period);
uint64_t period2ScopeDelta(uint32_t period);
uint64_t period2ScopeDrawDelta(uint32_t period);

int32_t getPianoKey(int32_t period, int8_t finetune, int8_t relativeNote); // for piano in Instr. Ed.
void triggerNote(uint8_t note, uint8_t efx, uint8_t efxData, channel_t *ch);
void updateVolPanAutoVib(channel_t *ch);

bool allocateInstr(int16_t insNum);
void freeInstr(int32_t insNum);
void freeAllInstr(void);
void freeSample(int16_t insNum, int16_t smpNum);

void freeAllPatterns(void);
void updateChanNums(void);
void calcMiscReplayerVars(void);
bool setupReplayer(void);
void closeReplayer(void);
void resetMusic(void);
void startPlaying(int8_t mode, int16_t row);
void stopPlaying(void);
void stopVoices(void);
void setPos(int16_t songPos, int16_t row, bool resetTimer);
void pauseMusic(void); // stops reading pattern data
void resumeMusic(void); // starts reading pattern data
void setSongModifiedFlag(void);
void removeSongModifiedFlag(void);
void playTone(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol, uint16_t midiVibDepth, uint16_t midiPitch);
void playSample(uint8_t chNum, uint8_t insNum, uint8_t smpNum, uint8_t note, uint16_t midiVibDepth, uint16_t midiPitch);
void playRange(uint8_t chNum, uint8_t insNum, uint8_t smpNum, uint8_t note, uint16_t midiVibDepth, uint16_t midiPitch, int32_t smpOffset, int32_t length);
void keyOff(channel_t *ch);
void conv8BitSample(int8_t *p, int32_t length, bool stereo); // changes sample sign
void conv16BitSample(int8_t *p, int32_t length, bool stereo); // changes sample sign
void delta2Samp(int8_t *p, int32_t length, uint8_t smpFlags);
void samp2Delta(int8_t *p, int32_t length, uint8_t smpFlags);
void setPatternLen(uint16_t pattNum, int16_t numRows);
void setLinearPeriods(bool linearPeriodsFlag);
void resetVolumes(channel_t *ch);
void triggerInstrument(channel_t *ch);
void tickReplayer(void); // periodically called from audio callback
void resetChannels(void);
bool patternEmpty(uint16_t pattNum);
int16_t getUsedSamples(int16_t smpNum);
int16_t getRealUsedSamples(int16_t smpNum);
void setStdEnvelope(instr_t *ins, int16_t i, uint8_t type);
void setNoEnvelope(instr_t *ins);
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
extern bool songPlaying, audioPaused, musicPaused;
extern volatile bool replayerBusy;
extern const uint16_t *note2PeriodLUT;
extern int16_t patternNumRows[MAX_PATTERNS];
extern channel_t channel[MAX_CHANNELS];
extern song_t song;
extern instr_t *instr[128+4];
extern note_t *pattern[MAX_PATTERNS];
