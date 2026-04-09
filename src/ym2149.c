// YM2149 / AY-3-8910 PSG emulation core
// Inspired by the MAME-derived AY8910 core in tildearrow/furnace (BSD-3-Clause).
// Rewritten in pure C for ft2-clone. No SDL or FT2 dependencies.
//
// References:
//   - tildearrow/furnace src/engine/platform/sound/ay8910.cpp (MAME, BSD-3-Clause)
//   - AY-3-8910/YM2149 datasheets
//   - Hardware measurements by MAME contributors

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ym2149.h"

// ---------------------------------------------------------------------------
// Volume / envelope lookup table construction
//
// The resistor-ladder DAC values below are taken from Furnace's measured
// parameters (ym2149_param and ym2149_param_env in ay8910.cpp, MAME-derived).
// The "res" arrays are proportional resistances; a lower value = louder level.
// We normalise so that the maximum (last index, highest level) maps to INT16_MAX.
// ---------------------------------------------------------------------------

// AY-3-8910 — 16 volume levels, same table used for envelope steps.
// Values are equivalent resistance (in arbitrary units) per DAC step;
// lower resistance = louder.  Index 0 = volume level 0 (quietest),
// index 15 = volume level 15 (loudest).
// Source: MAME ay8910_param (BSD-3), as used in tildearrow/furnace.
static const uint32_t kAyVolRes[16] =
{
	85785, 42625, 32798, 26959, 20620, 15948, 11289, 8124,
	 5765,  4042,  2871,  2049,  1366,   948,   615,  417
};

// YM2149 — 16 volume levels (for fixed-volume mode)
static const uint32_t kYmVolRes[16] =
{
	73770, 37586, 27458, 21451, 15864, 12371, 8922, 6796,
	 4763,  3521,  2403,  1737,  1123,   762,  438,  251
};

// YM2149 — 32 envelope steps (5-bit envelope, finer resolution)
static const uint32_t kYmEnvRes[32] =
{
	103350, 73770, 52657, 37586, 32125, 27458, 24269, 21451,
	 18447, 15864, 14009, 12371, 10506,  8922,  7787,  6796,
	  5689,  4763,  4095,  3521,  2909,  2403,  2043,  1737,
	  1397,  1123,   925,   762,   578,   438,   332,   251
};

// Build a 16-entry PCM amplitude table from resistor values.
// res[0] = highest resistance (volume level 0, quietest);
// res[15] = lowest resistance (volume level 15, loudest → maps to INT16_MAX).
static void buildVolTable16(const uint32_t *res, uint16_t *out)
{
	uint32_t minRes = res[15]; // lowest resistance = loudest = INT16_MAX
	for (int i = 0; i < 16; i++)
	{
		uint32_t v = (uint32_t)(((uint64_t)32767 * minRes) / res[i]);
		if (v > 32767) v = 32767;
		out[i] = (uint16_t)v;
	}
}

// Build a 32-entry PCM amplitude table from resistor values.
// res[0] = quietest, res[31] = loudest.
static void buildVolTable32(const uint32_t *res, uint16_t *out)
{
	uint32_t minRes = res[31];
	for (int i = 0; i < 32; i++)
	{
		uint32_t v = (uint32_t)(((uint64_t)32767 * minRes) / res[i]);
		if (v > 32767) v = 32767;
		out[i] = (uint16_t)v;
	}
}

// ---------------------------------------------------------------------------
// Envelope shape table
//
// The YM2149 / AY-3-8910 envelope has 8 distinct shapes encoded in the lower
// 4 bits of register 0x0D.  Shapes 0x00-0x03 and 0x09 behave like decay (\___),
// shapes 0x04-0x07 and 0x0F behave like attack-once (/___).
// The "canonical" shapes by upper 3 bits:
//   CONT ALT  HOLD
//   0    0    0    → \___  (decay once, then silence/off)
//   0    0    1    → \___  (same, hold at low / off)
//   0    1    0    → \/\/  (triangle / alternating)
//   0    1    1    → \¯¯¯  (decay once, then hold high)
//   1    0    0    → ////  (saw up / repeating attack)
//   1    0    1    → /¯¯¯  (attack once, then hold high)
//   1    1    0    → /\/\  (triangle / alternating attack-first)
//   1    1    1    → /___  (attack once, then silence/off)
//
// We decode shape flags from reg 0x0D at envelope-reset time.
// ---------------------------------------------------------------------------

static void envReset(ym2149_t *ym)
{
	uint8_t shape = ym->regs[YM_REG_ENV_SHAPE] & 0x0F;

	// ATTACK bit: shape bit 2
	ym->envAttack = (shape >> 2) & 1;
	// ALT bit:    shape bit 1
	ym->envAlt    = (shape >> 1) & 1;
	// HOLD bit:   shape bit 0
	// (hold is checked each cycle — see envClock)

	ym->envStep = 0;
	ym->envHold = 0;

	// Start at the appropriate end of the range depending on attack direction
	if (!ym->envAttack)
		ym->envStep = ym->envStepMask; // start high for decay shapes
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ym2149_init(ym2149_t *ym, uint32_t clock, bool isYM)
{
	memset(ym, 0, sizeof(*ym));
	ym->clockFreq = clock;
	ym->type      = isYM ? YM_TYPE_YM : YM_TYPE_AY;

	if (isYM)
	{
		ym->envStepMask = 0x1F; // 32 steps
		ym->zeroIsOff   = 0;    // vol 0 still produces DC offset
		buildVolTable16(kYmVolRes, ym->volTable);
		buildVolTable32(kYmEnvRes, ym->envTable);
	}
	else
	{
		ym->envStepMask = 0x0F; // 16 steps
		ym->zeroIsOff   = 1;    // vol 0 = channel off
		buildVolTable16(kAyVolRes, ym->volTable);
		// AY: same table for envelope (16 steps, index repeated for envTable[0..15])
		buildVolTable16(kAyVolRes, ym->envTable);
		// envTable[16..31] not used for AY but clear them for safety
		memset(&ym->envTable[16], 0, 16 * sizeof(uint16_t));
	}

	ym2149_reset(ym);
}

void ym2149_reset(ym2149_t *ym)
{
	memset(ym->regs, 0, sizeof(ym->regs));

	for (int i = 0; i < 3; i++)
	{
		ym->ch[i].period  = 1;
		ym->ch[i].counter = 0;
		ym->ch[i].output  = 0;
		ym->ch[i].volume  = 0;
	}

	ym->noiseLfsr    = 1;     // LFSR must never be zero
	ym->noisePeriod  = 1;
	ym->noiseCounter = 0;
	ym->noiseOutput  = 0;

	ym->envPeriod  = 1;
	ym->envCounter = 0;
	ym->envShape   = 0;
	ym->accumulator = 0;

	// Decode initial envelope shape (all zeros = decay)
	ym->envStepMask = (ym->type == YM_TYPE_YM) ? 0x1F : 0x0F;
	envReset(ym);
}

void ym2149_write(ym2149_t *ym, uint8_t reg, uint8_t val)
{
	if (reg >= YM_NUM_REGS)
		return;

	// Mask unused bits per register
	static const uint8_t kRegMask[YM_NUM_REGS] =
	{
		0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, // tone periods (A fine/coarse, B, C)
		0x1F,                                // noise period (5-bit)
		0xFF,                                // mixer
		0x1F, 0x1F, 0x1F,                    // amplitudes (bit4=env, bits0-3=vol)
		0xFF, 0xFF,                          // env period
		0x0F,                                // env shape
		0xFF, 0xFF                           // I/O ports
	};

	ym->regs[reg] = val & kRegMask[reg];

	switch (reg)
	{
		// Tone period registers — rebuild 12-bit period
		case YM_REG_TONE_A_FINE:
		case YM_REG_TONE_A_COARSE:
		{
			uint16_t p = ((uint16_t)(ym->regs[YM_REG_TONE_A_COARSE] & 0x0F) << 8)
			              | ym->regs[YM_REG_TONE_A_FINE];
			ym->ch[0].period = (p == 0) ? 1 : p;
			break;
		}
		case YM_REG_TONE_B_FINE:
		case YM_REG_TONE_B_COARSE:
		{
			uint16_t p = ((uint16_t)(ym->regs[YM_REG_TONE_B_COARSE] & 0x0F) << 8)
			              | ym->regs[YM_REG_TONE_B_FINE];
			ym->ch[1].period = (p == 0) ? 1 : p;
			break;
		}
		case YM_REG_TONE_C_FINE:
		case YM_REG_TONE_C_COARSE:
		{
			uint16_t p = ((uint16_t)(ym->regs[YM_REG_TONE_C_COARSE] & 0x0F) << 8)
			              | ym->regs[YM_REG_TONE_C_FINE];
			ym->ch[2].period = (p == 0) ? 1 : p;
			break;
		}

		case YM_REG_NOISE_PERIOD:
		{
			uint8_t p = ym->regs[YM_REG_NOISE_PERIOD] & 0x1F;
			ym->noisePeriod = (p == 0) ? 1 : p;
			break;
		}

		case YM_REG_AMP_A: ym->ch[0].volume = ym->regs[YM_REG_AMP_A]; break;
		case YM_REG_AMP_B: ym->ch[1].volume = ym->regs[YM_REG_AMP_B]; break;
		case YM_REG_AMP_C: ym->ch[2].volume = ym->regs[YM_REG_AMP_C]; break;

		case YM_REG_ENV_FINE:
		case YM_REG_ENV_COARSE:
		{
			uint32_t p = ((uint32_t)ym->regs[YM_REG_ENV_COARSE] << 8)
			              | ym->regs[YM_REG_ENV_FINE];
			// Period 0 is treated as 0x10000 (half of period 1) — matches hardware
			ym->envPeriod = (p == 0) ? 0x10000 : p;
			break;
		}

		case YM_REG_ENV_SHAPE:
			ym->envShape = val & 0x0F;
			envReset(ym);
			break;

		default:
			break;
	}
}

uint8_t ym2149_read(const ym2149_t *ym, uint8_t reg)
{
	if (reg >= YM_NUM_REGS)
		return 0;
	return ym->regs[reg];
}

// ---------------------------------------------------------------------------
// Internal: clock one PSG base tick.
//
// The real chip divides the master clock by 8 to derive the PSG base clock.
// Tone counters advance one step per PSG tick and toggle their output every
// `period` steps.  Noise and envelope share the same base clock.
// ---------------------------------------------------------------------------

static inline void ymClockTone(ym2149_t *ym, int ch)
{
	ym_channel_t *c = &ym->ch[ch];
	c->counter++;
	if (c->counter >= c->period)
	{
		c->counter = 0;
		c->output ^= 1;
	}
}

static inline void ymClockNoise(ym2149_t *ym)
{
	ym->noiseCounter++;
	if (ym->noiseCounter >= ym->noisePeriod)
	{
		ym->noiseCounter = 0;
		// 17-bit LFSR: feedback = bit0 XOR bit3 (hardware-verified per MAME)
		uint32_t lfsr = ym->noiseLfsr;
		uint32_t bit  = (lfsr ^ (lfsr >> 3)) & 1;
		ym->noiseLfsr = (lfsr >> 1) | (bit << 16);
		ym->noiseOutput = (uint8_t)(ym->noiseLfsr & 1);
	}
}

static inline void ymClockEnvelope(ym2149_t *ym)
{
	ym->envCounter++;
	if (ym->envCounter >= ym->envPeriod)
	{
		ym->envCounter = 0;

		if (!ym->envHold)
		{
			if (ym->envAttack)
				ym->envStep++;
			else
				ym->envStep--;

			// Check if we've gone past the end of the envelope cycle
			if ((ym->envStep & ~ym->envStepMask) != 0)
			{
				// Wrap step back into valid range
				ym->envStep &= ym->envStepMask;

				uint8_t shape = ym->regs[YM_REG_ENV_SHAPE] & 0x0F;
				uint8_t cont  = (shape >> 3) & 1;
				uint8_t alt   = (shape >> 1) & 1;
				uint8_t hold  = (shape >> 0) & 1;

				if (!cont)
				{
					// One-shot shapes (0x00-0x07): always terminate at silence.
					// This matches the canonical AY/YM envelope shapes \___  and /___.
					ym->envHold = 1;
					ym->envStep = 0;
				}
				else if (hold)
				{
					// Continue + hold: latch at the appropriate extreme.
					// ALT flips the effective direction for choosing the hold level:
					//   shape 0x09 (ATK=0,ALT=0): hold low  \___ → step=0
					//   shape 0x0B (ATK=0,ALT=1): hold high \¯¯¯ → step=mask
					//   shape 0x0D (ATK=1,ALT=0): hold high /¯¯¯ → step=mask
					//   shape 0x0F (ATK=1,ALT=1): hold low  /___ → step=0
					ym->envHold = 1;
					ym->envStep = (ym->envAttack ^ alt) ? ym->envStepMask : 0;
				}
				else if (alt)
				{
					// Alternate direction each cycle
					ym->envAttack ^= 1;
					ym->envStep = ym->envAttack ? 0 : ym->envStepMask;
				}
				// else: continue, no hold, no alt → just let it wrap (saw)
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Compute the 16-bit unsigned amplitude for one channel
// ---------------------------------------------------------------------------

static inline uint16_t ymChannelAmplitude(const ym2149_t *ym, int ch)
{
	uint8_t vol = ym->ch[ch].volume;

	if (vol & 0x10)
	{
		// Envelope mode: use current envelope step
		return ym->envTable[ym->envStep];
	}
	else
	{
		uint8_t level = vol & 0x0F;
		if (ym->zeroIsOff && level == 0)
			return 0;
		return ym->volTable[level];
	}
}

// ---------------------------------------------------------------------------
// ym2149_generate — render numSamples of 16-bit signed PCM per channel
//
// The AY/YM PSG base clock is the master clock divided by 8.  Tone counters
// advance one step per PSG base tick; the tone output toggles every `period`
// steps, giving: output frequency = clockFreq / (8 * 2 * period).
// Noise and envelope counters run at the same PSG base clock rate.
//
// We model this by accumulating a fixed-point fraction: each output sample
// advances the PSG base counter by (clockFreq/8) / sampleRate steps.
// ---------------------------------------------------------------------------

void ym2149_generate(ym2149_t *ym,
                     int16_t *outA, int16_t *outB, int16_t *outC,
                     uint32_t numSamples, uint32_t sampleRate)
{
	// PSG base clock: tone/noise/envelope counters run at clockFreq/8
	const uint32_t psgClock = ym->clockFreq / 8;

	uint8_t mixer = ym->regs[YM_REG_MIXER];

	for (uint32_t s = 0; s < numSamples; s++)
	{
		// Advance the accumulator and clock the chip as many times as needed
		// to stay in sync with the output sample rate.
		ym->accumulator += psgClock;
		while (ym->accumulator >= sampleRate)
		{
			ym->accumulator -= sampleRate;
			ymClockTone(ym, 0);
			ymClockTone(ym, 1);
			ymClockTone(ym, 2);
			ymClockNoise(ym);
			ymClockEnvelope(ym);
		}

		// Mixer: bits 0-2 = tone disable A/B/C (active high = disabled)
		//        bits 3-5 = noise disable A/B/C
		uint8_t toneDis  = mixer & 0x07;
		uint8_t noiseDis = (mixer >> 3) & 0x07;

		for (int ch = 0; ch < 3; ch++)
		{
			int16_t **out = NULL;
			if      (ch == 0 && outA) out = &outA;
			else if (ch == 1 && outB) out = &outB;
			else if (ch == 2 && outC) out = &outC;

			if (out == NULL)
				continue;

			// Gate: tone output (active when enabled AND counter bit set)
			uint8_t toneGate  = !(toneDis  & (1 << ch)) ? ym->ch[ch].output : 1;
			// Gate: noise output (active when enabled AND noise LFSR bit set)
			uint8_t noiseGate = !(noiseDis & (1 << ch)) ? ym->noiseOutput   : 1;

			// Both gates must be high for output
			uint8_t gate = toneGate & noiseGate;

			uint16_t amp = ymChannelAmplitude(ym, ch);
			// A low gate mutes the DAC contribution; do not phase-invert it.
			int16_t sample = gate ? (int16_t)amp : 0;

			(*out)[s] = sample;
		}
	}
}
