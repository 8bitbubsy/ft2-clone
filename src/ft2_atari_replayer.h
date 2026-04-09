#pragma once

// Atari ST / YM2149 PSG replayer for ft2-clone
//
// This replayer runs in parallel with the main XM replayer.  When Atari mode
// is active it reads from the same pattern/song data structures (up to 3
// channels) and produces YM2149 register writes instead of PCM voices.
//
// PSG instrument macros follow the style used by Arkos Tracker / MusicMon:
//   - per-frame volume envelope (4-bit levels)
//   - arpeggio table (semitone offsets)
//   - pitch (period) envelope (fine offsets in YM period units)
//   - per-frame tone / noise / hardware-envelope flags
//
// Effects supported in Atari mode:
//   0xy  Arpeggio            (same as XM)
//   1xx  Pitch slide up
//   2xx  Pitch slide down
//   3xx  Portamento to note
//   4xy  Vibrato
//   7xx  Set noise period (R6)
//   8xx  Set mixer control for channel (overrides instrument mixer flags)
//   9xx  Set hardware envelope shape (R13)
//   Axy  Volume slide
//   Bxx  Position jump
//   Cxx  Set volume (0-15)
//   Dxx  Pattern break
//   Exx  Set envelope period fine (R11)
//   Fxx  Set speed / BPM  (same as XM: >=32 → BPM, else speed)
//   Gxx  Set envelope period coarse (R12)

#include <stdint.h>
#include <stdbool.h>
#include "ym2149.h"

// ---------------------------------------------------------------------------
// PSG instrument definition
// ---------------------------------------------------------------------------

#define MAX_PSG_ENVELOPE_LEN 64
#define MAX_PSG_ARPEGGIO_LEN 64
#define MAX_PSG_PITCH_LEN    64
#define MAX_PSG_INSTRUMENTS  128

// Sentinel: loop point value meaning "no loop"
#define PSG_NO_LOOP 0xFF

typedef struct psgInstrument_t
{
	char name[22+1];

	// Volume envelope: 4-bit values (0-15), per frame
	uint8_t volEnvelope[MAX_PSG_ENVELOPE_LEN];
	uint8_t volEnvLength;
	uint8_t volEnvLoopStart; // PSG_NO_LOOP = no loop

	// Arpeggio table: semitone offsets
	int8_t  arpTable[MAX_PSG_ARPEGGIO_LEN];
	uint8_t arpLength;
	uint8_t arpLoopStart;    // PSG_NO_LOOP = no loop

	// Pitch envelope: fine period offsets (in YM period units)
	int16_t pitchEnvelope[MAX_PSG_PITCH_LEN];
	uint8_t pitchEnvLength;
	uint8_t pitchEnvLoopStart; // PSG_NO_LOOP = no loop

	// Mixer flags (may be overridden per-note via effect 8xx)
	bool toneEnabled;
	bool noiseEnabled;

	// Hardware envelope settings
	bool    useHwEnvelope;
	uint8_t hwEnvShape;      // R13 value (4-bit)
	uint16_t hwEnvPeriod;    // R11-R12 value

	// Noise period used when noiseEnabled (R6 value, 0-31)
	uint8_t noisePeriod;
} psgInstrument_t;

// ---------------------------------------------------------------------------
// Per-channel Atari PSG replayer state
// ---------------------------------------------------------------------------

typedef struct psgChannel_t
{
	// Current instrument
	const psgInstrument_t *instr;

	// Base YM period for the triggered note (before pitch modulation)
	uint16_t basePeriod;

	// Final period after all modulations
	uint16_t finalPeriod;

	// Portamento target period
	uint16_t portaTargetPeriod;
	uint16_t portaSpeed;

	// Vibrato
	uint8_t vibratoPos;
	uint8_t vibratoSpeed;
	uint8_t vibratoDepth;

	// Macro positions
	uint8_t volEnvPos;
	uint8_t arpPos;
	uint8_t pitchEnvPos;

	// Current volume (4-bit, 0-15)
	uint8_t volume;

	// Pitch slide speeds
	uint8_t pitchSlideUpSpeed;
	uint8_t pitchSlideDownSpeed;

	// Arpeggio / effects
	uint8_t arpNote1;   // x nibble of 0xy
	uint8_t arpNote2;   // y nibble of 0xy
	uint8_t arpeggioTick;

	// Mixer override (-1 = use instrument setting, 0-63 = mixer byte for this channel)
	int8_t  mixerOverride; // -1 means "use instrument"

	// Hardware envelope override
	bool    hwEnvTriggered;

	// Volume slide
	uint8_t volSlideUp;
	uint8_t volSlideDown;

	// Note active flag
	bool    noteOn;

	// Last raw note (0 = none)
	uint8_t note;

	// Noise period for this channel (from instrument or 7xx effect)
	uint8_t noisePeriod;

	// Envelope period fine/coarse (from Exx/Gxx)
	uint8_t envPeriodFine;
	uint8_t envPeriodCoarse;
} psgChannel_t;

// ---------------------------------------------------------------------------
// Global Atari replayer state
// ---------------------------------------------------------------------------

#define ATARI_PSG_CHANNELS 3

typedef struct atariReplayer_t
{
	ym2149_t ym;                          // YM2149 emulator instance
	psgChannel_t ch[ATARI_PSG_CHANNELS];  // per-channel state
	psgInstrument_t instruments[MAX_PSG_INSTRUMENTS]; // bank of PSG instruments
	uint8_t lastRegs[14];                 // last written register values (for export)
} atariReplayer_t;

// ---------------------------------------------------------------------------
// YM period table
// ---------------------------------------------------------------------------

// ymPeriodTable[note] gives the 12-bit YM period for tracker notes 0-95
// (C-0 through B-7 in standard XM notation).
// Values of 0 mean the note is out of the YM's audible range.
extern uint16_t ymPeriodTable[10*12]; // 10 octaves (same size as XM note space)

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Initialise the Atari replayer (call once at startup or when entering Atari mode).
void atariReplayer_init(atariReplayer_t *ar);

// Reset all channel state (call on song stop / position change).
void atariReplayer_reset(atariReplayer_t *ar);

// Process one replayer tick for all 3 PSG channels.
// Reads note/effect data from the global `pattern[]` / `song` structures,
// applies PSG instrument macros and effects, then writes the resulting
// register values to ar->ym.
// Returns false when the song has finished (used by the export routines).
bool atariReplayer_tick(atariReplayer_t *ar);

// Copy the 14 register values currently held in ar->ym.regs into dst[0..13].
void atariReplayer_getRegs(const atariReplayer_t *ar, uint8_t dst[14]);

// Access the PSG instrument bank
psgInstrument_t *atariReplayer_getInstrument(atariReplayer_t *ar, int idx);

// Generate PCM audio from the YM2149 emulator (preview / monitoring)
// outBuf receives interleaved stereo int16_t samples (left = right = mono YM).
void atariReplayer_generateAudio(atariReplayer_t *ar, int16_t *outBuf,
                                 uint32_t numSamples, uint32_t sampleRate);
