#pragma once

// YM2149 / AY-3-8910 PSG emulation core
// Inspired by the MAME-derived AY8910 core used in tildearrow/furnace (BSD-3-Clause).
// Rewritten in pure C for ft2-clone. No SDL or FT2 dependencies.
//
// The YM2149 is the Yamaha clone of General Instrument's AY-3-8910, used in the Atari ST.
// Key differences vs. the original AY-3-8910:
//   - 32-step envelope resolution (AY has 16)
//   - volume level 0 still produces a small DC offset (AY silences the channel)
//   - different resistor-ladder DAC curve

#include <stdint.h>
#include <stdbool.h>

// Register addresses
#define YM_REG_TONE_A_FINE   0x00
#define YM_REG_TONE_A_COARSE 0x01
#define YM_REG_TONE_B_FINE   0x02
#define YM_REG_TONE_B_COARSE 0x03
#define YM_REG_TONE_C_FINE   0x04
#define YM_REG_TONE_C_COARSE 0x05
#define YM_REG_NOISE_PERIOD  0x06
#define YM_REG_MIXER         0x07
#define YM_REG_AMP_A         0x08
#define YM_REG_AMP_B         0x09
#define YM_REG_AMP_C         0x0A
#define YM_REG_ENV_FINE      0x0B
#define YM_REG_ENV_COARSE    0x0C
#define YM_REG_ENV_SHAPE     0x0D
#define YM_REG_IO_A          0x0E
#define YM_REG_IO_B          0x0F

#define YM_NUM_REGS 16

// Chip type selector
typedef enum
{
	YM_TYPE_AY = 0, // AY-3-8910
	YM_TYPE_YM      // YM2149 (Atari ST)
} ym_type_t;

// Internal per-channel state
typedef struct
{
	uint16_t period;    // tone period (12-bit, 1..4095; 0 treated as 1)
	uint16_t counter;   // tone counter
	uint8_t  output;    // current tone output bit (0 or 1)
	uint8_t  volume;    // amplitude register value (bits 0-3 = level, bit 4 = env mode)
} ym_channel_t;

// YM2149 / AY-3-8910 emulator state
typedef struct ym2149_t
{
	// Chip identity
	ym_type_t type;

	// Raw registers
	uint8_t regs[YM_NUM_REGS];

	// Tone channels A, B, C
	ym_channel_t ch[3];

	// Noise generator
	uint32_t noiseLfsr;     // 17-bit LFSR state (initialised to 1)
	uint16_t noisePeriod;   // noise period (5-bit, 0 treated as 1)
	uint16_t noiseCounter;  // noise counter
	uint8_t  noiseOutput;   // current noise output bit

	// Envelope generator
	uint32_t envPeriod;     // 16-bit envelope period (0 treated as 1 in counting)
	uint32_t envCounter;    // envelope counter
	uint8_t  envShape;      // 4-bit shape register
	uint8_t  envStep;       // current step (0..15 for AY, 0..31 for YM)
	uint8_t  envHold;       // envelope is held (no further stepping)
	uint8_t  envAttack;     // current attack/direction bit
	uint8_t  envAlt;        // alternate direction flag
	uint8_t  envStepMask;   // 0x0F (AY) or 0x1F (YM)
	uint8_t  zeroIsOff;     // 1 for AY (vol 0 = channel off), 0 for YM

	// Clock-to-sample accumulator for fractional stepping
	uint32_t clockFreq;     // chip clock in Hz (typically 2000000)
	uint32_t accumulator;   // sub-sample accumulator

	// Volume lookup tables (built once on init)
	// volTable[16]    maps 4-bit volume (0-15) to 16-bit unsigned PCM amplitude
	// envTable[32]    maps 5-bit envelope step (0-31) to 16-bit unsigned PCM amplitude
	uint16_t volTable[16];
	uint16_t envTable[32];
} ym2149_t;

// Initialise the emulator. clock is the chip clock in Hz (2000000 for Atari ST).
// isYM selects YM2149 mode (true) or AY-3-8910 mode (false).
void ym2149_init(ym2149_t *ym, uint32_t clock, bool isYM);

// Reset all registers and internal state to power-on defaults.
void ym2149_reset(ym2149_t *ym);

// Write a value to one of the 16 YM2149 registers.
void ym2149_write(ym2149_t *ym, uint8_t reg, uint8_t val);

// Read back a register value (I/O port registers 0x0E-0x0F are returned as stored).
uint8_t ym2149_read(const ym2149_t *ym, uint8_t reg);

// Render numSamples of audio at sampleRate Hz.
// outA, outB, outC receive 16-bit signed PCM for channels A, B, C respectively.
// Any of the output pointers may be NULL if that channel is not needed.
void ym2149_generate(ym2149_t *ym,
                     int16_t *outA, int16_t *outB, int16_t *outC,
                     uint32_t numSamples, uint32_t sampleRate);
