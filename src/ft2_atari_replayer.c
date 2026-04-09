// Atari ST / YM2149 PSG replayer for ft2-clone
// Reads XM pattern data (3 channels) and produces YM2149 register writes.

// for MSVC: suppress POSIX name warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "ft2_atari_replayer.h"
#include "ft2_replayer.h"   // song_t, note_t, pattern[], song, etc.
#include "ft2_header.h"

// ---------------------------------------------------------------------------
// YM period table
//
// The YM2149 tone period for a note with frequency f is:
//   period = round( clockHz / (16 * f) )
// where clockHz = 2,000,000 Hz (Atari ST PSG clock).
//
// We use standard equal temperament with A4 = 440 Hz.
// Note index 0 = C-0, 1 = C#0, ..., 9*12+11 = B-9.
// Indices that produce a period > 4095 or == 0 are clamped to 0 (invalid).
// ---------------------------------------------------------------------------

// 12-TET frequencies for C-0 through B-9 (10 octaves)
// C-0 ≈ 16.352 Hz; each semitone multiplied by 2^(1/12)
static double noteFrequency(int note) // note 0 = C-0
{
	// A4 is note index (4*12 + 9) = 57
	return 440.0 * pow(2.0, (note - 57) / 12.0);
}

// Build the table at startup (called from atariReplayer_init)
uint16_t ymPeriodTable[10*12];

static void buildPeriodTable(void)
{
	for (int n = 0; n < 10*12; n++)
	{
		double f = noteFrequency(n);
		double p = 2000000.0 / (16.0 * f);
		long ip = (long)(p + 0.5);
		if (ip < 1 || ip > 4095)
			ymPeriodTable[n] = 0; // out of range
		else
			ymPeriodTable[n] = (uint16_t)ip;
	}
}

// ---------------------------------------------------------------------------
// Sine table for vibrato (same pattern as XM replayer)
// ---------------------------------------------------------------------------

static const int8_t kSinTable[32] =
{
	  0, 24, 49, 74, 97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120, 97, 74, 49, 24
};

static int8_t vibSin(uint8_t pos)
{
	pos &= 63;
	if (pos < 32)
		return  kSinTable[pos];
	else
		return -kSinTable[pos - 32];
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Write a 12-bit period to the correct channel's tone registers
static void writeChannelPeriod(ym2149_t *ym, int ch, uint16_t period)
{
	if (period == 0) period = 1;
	if (period > 4095) period = 4095;

	// Registers: ch 0 → R0/R1, ch 1 → R2/R3, ch 2 → R4/R5
	uint8_t base = (uint8_t)(ch * 2);
	ym2149_write(ym, base,     (uint8_t)(period & 0xFF));
	ym2149_write(ym, base + 1, (uint8_t)((period >> 8) & 0x0F));
}

// Set channel amplitude (4-bit) — or enable envelope mode (bit 4)
static void writeChannelVolume(ym2149_t *ym, int ch, uint8_t vol)
{
	ym2149_write(ym, YM_REG_AMP_A + ch, vol & 0x1F);
}

// Build the R7 mixer byte from per-channel tone/noise enable flags.
// R7 bits (active-low enable):  bit0=toneA, bit1=toneB, bit2=toneC,
//                                bit3=noiseA, bit4=noiseB, bit5=noiseC
static uint8_t buildMixer(const psgChannel_t ch[3])
{
	uint8_t r7 = 0xFF; // all disabled by default
	for (int i = 0; i < 3; i++)
	{
		if (!ch[i].noteOn)
			continue;

		const psgInstrument_t *ins = ch[i].instr;
		bool toneEn  = ins ? ins->toneEnabled  : true;
		bool noiseEn = ins ? ins->noiseEnabled : false;

		// mixer override from effect 8xx
		if (ch[i].mixerOverride >= 0)
		{
			uint8_t ov = (uint8_t)ch[i].mixerOverride;
			toneEn  = !(ov & (1 << i));
			noiseEn = !(ov & (1 << (i + 3)));
		}

		if (toneEn)
			r7 &= ~(uint8_t)(1 << i);       // clear tone-disable bit
		if (noiseEn)
			r7 &= ~(uint8_t)(1 << (i + 3)); // clear noise-disable bit
	}
	return r7;
}

// ---------------------------------------------------------------------------
// Note trigger
// ---------------------------------------------------------------------------

static void triggerPsgNote(atariReplayer_t *ar, int chIdx,
                           uint8_t note, uint8_t instrNum)
{
	psgChannel_t *ch = &ar->ch[chIdx];

	// Get instrument (1-based instrument number, 0 means keep current)
	if (instrNum > 0 && instrNum <= MAX_PSG_INSTRUMENTS)
		ch->instr = &ar->instruments[instrNum - 1];

	if (note == 0 || note == NOTE_OFF)
	{
		if (note == NOTE_OFF)
			ch->noteOn = false;
		return;
	}

	// Map XM note (1-based, 1=C-0) to period table (0-based)
	int noteIdx = (int)note - 1;
	if (noteIdx < 0 || noteIdx >= 10*12)
		return;

	uint16_t period = ymPeriodTable[noteIdx];
	if (period == 0)
		return; // out of YM range

	ch->note       = note;
	ch->basePeriod = period;
	ch->finalPeriod = period;
	ch->noteOn     = true;

	// Reset macro positions
	ch->volEnvPos   = 0;
	ch->arpPos      = 0;
	ch->pitchEnvPos = 0;
	ch->arpeggioTick = 0;
	ch->vibratoPos  = 0;
	ch->hwEnvTriggered = false;

	// Apply instrument initial settings
	const psgInstrument_t *ins = ch->instr;
	if (ins != NULL)
	{
		ch->noisePeriod = ins->noisePeriod;
		if (ins->useHwEnvelope)
		{
			ch->envPeriodFine   = (uint8_t)(ins->hwEnvPeriod & 0xFF);
			ch->envPeriodCoarse = (uint8_t)(ins->hwEnvPeriod >> 8);
			ym2149_write(&ar->ym, YM_REG_ENV_FINE,   ch->envPeriodFine);
			ym2149_write(&ar->ym, YM_REG_ENV_COARSE, ch->envPeriodCoarse);
			ym2149_write(&ar->ym, YM_REG_ENV_SHAPE,  ins->hwEnvShape & 0x0F);
			ch->hwEnvTriggered = true;
		}
	}
}

// ---------------------------------------------------------------------------
// Per-tick macro / effect processing
// ---------------------------------------------------------------------------

static void tickPsgChannel(atariReplayer_t *ar, int chIdx,
                           uint8_t efx, uint8_t efxData, bool tickZero)
{
	psgChannel_t *ch = &ar->ch[chIdx];

	if (!ch->noteOn)
		return;

	const psgInstrument_t *ins = ch->instr;

	// --- Volume envelope ---
	uint8_t vol = ch->volume;
	if (ins != NULL && ins->volEnvLength > 0)
	{
		vol = ins->volEnvelope[ch->volEnvPos] & 0x0F;
		ch->volume = vol;

		if (ch->volEnvPos + 1 < ins->volEnvLength)
		{
			ch->volEnvPos++;
		}
		else if (ins->volEnvLoopStart != PSG_NO_LOOP &&
		         ins->volEnvLoopStart < ins->volEnvLength)
		{
			ch->volEnvPos = ins->volEnvLoopStart;
		}
	}

	// --- Arpeggio table ---
	int8_t arpOffset = 0;
	if (ins != NULL && ins->arpLength > 0)
	{
		arpOffset = ins->arpTable[ch->arpPos];

		if (ch->arpPos + 1 < ins->arpLength)
		{
			ch->arpPos++;
		}
		else if (ins->arpLoopStart != PSG_NO_LOOP &&
		         ins->arpLoopStart < ins->arpLength)
		{
			ch->arpPos = ins->arpLoopStart;
		}
	}

	// --- Pitch envelope ---
	int16_t pitchOffset = 0;
	if (ins != NULL && ins->pitchEnvLength > 0)
	{
		pitchOffset = ins->pitchEnvelope[ch->pitchEnvPos];

		if (ch->pitchEnvPos + 1 < ins->pitchEnvLength)
		{
			ch->pitchEnvPos++;
		}
		else if (ins->pitchEnvLoopStart != PSG_NO_LOOP &&
		         ins->pitchEnvLoopStart < ins->pitchEnvLength)
		{
			ch->pitchEnvPos = ins->pitchEnvLoopStart;
		}
	}

	// --- Effect: tick-zero ---
	if (tickZero)
	{
		switch (efx)
		{
			case 0x01: // pitch slide up speed
				ch->pitchSlideUpSpeed = efxData;
				break;
			case 0x02: // pitch slide down speed
				ch->pitchSlideDownSpeed = efxData;
				break;
			case 0x03: // portamento to note — set speed
				ch->portaSpeed = efxData;
				break;
			case 0x04: // vibrato
				if (efxData >> 4)   ch->vibratoSpeed = (efxData >> 4) * 4;
				if (efxData & 0x0F) ch->vibratoDepth = (efxData & 0x0F);
				break;
			case 0x07: // set noise period
				ch->noisePeriod = efxData & 0x1F;
				ym2149_write(&ar->ym, YM_REG_NOISE_PERIOD, ch->noisePeriod);
				break;
			case 0x08: // set mixer override
				ch->mixerOverride = (int8_t)efxData;
				break;
			case 0x09: // set hardware envelope shape
				ym2149_write(&ar->ym, YM_REG_ENV_SHAPE, efxData & 0x0F);
				break;
			case 0x0A: // volume slide — load
				if (efxData >> 4)   ch->volSlideUp   = efxData >> 4;
				if (efxData & 0x0F) ch->volSlideDown = efxData & 0x0F;
				break;
			case 0x0C: // set volume (0-15)
				ch->volume = efxData & 0x0F;
				break;
			case 0x0E: // set envelope fine period
				ch->envPeriodFine = efxData;
				ym2149_write(&ar->ym, YM_REG_ENV_FINE, efxData);
				break;
			case 0x10: // G effect: set envelope coarse period
				ch->envPeriodCoarse = efxData;
				ym2149_write(&ar->ym, YM_REG_ENV_COARSE, efxData);
				break;
			default:
				break;
		}
	}

	// --- Effect: per-tick (non-zero ticks) ---
	if (!tickZero)
	{
		switch (efx)
		{
			case 0x01: // pitch slide up
				if (ch->pitchSlideUpSpeed > 0)
				{
					if (ch->basePeriod > ch->pitchSlideUpSpeed)
						ch->basePeriod -= ch->pitchSlideUpSpeed;
					else
						ch->basePeriod = 1;
				}
				break;
			case 0x02: // pitch slide down
				if (ch->pitchSlideDownSpeed > 0)
				{
					ch->basePeriod += ch->pitchSlideDownSpeed;
					if (ch->basePeriod > 4095) ch->basePeriod = 4095;
				}
				break;
			case 0x03: // portamento to note
				if (ch->portaTargetPeriod != 0 && ch->portaSpeed != 0)
				{
					if (ch->basePeriod < ch->portaTargetPeriod)
					{
						ch->basePeriod += ch->portaSpeed;
						if (ch->basePeriod >= ch->portaTargetPeriod)
							ch->basePeriod = ch->portaTargetPeriod;
					}
					else if (ch->basePeriod > ch->portaTargetPeriod)
					{
						if (ch->basePeriod > ch->portaSpeed)
							ch->basePeriod -= ch->portaSpeed;
						else
							ch->basePeriod = 1;
						if (ch->basePeriod <= ch->portaTargetPeriod)
							ch->basePeriod = ch->portaTargetPeriod;
					}
				}
				break;
			case 0x0A: // volume slide
				if (ch->volSlideUp > 0)
				{
					vol = ch->volume + ch->volSlideUp;
					if (vol > 15) vol = 15;
					ch->volume = vol;
				}
				else if (ch->volSlideDown > 0)
				{
					if (ch->volume >= ch->volSlideDown)
						ch->volume -= ch->volSlideDown;
					else
						ch->volume = 0;
				}
				break;
			default:
				break;
		}
	}

	// --- Vibrato (always running after trigger) ---
	if (ch->vibratoDepth > 0 && ch->vibratoSpeed > 0)
	{
		int16_t vib = (int16_t)vibSin(ch->vibratoPos) * ch->vibratoDepth / 32;
		ch->vibratoPos = (ch->vibratoPos + ch->vibratoSpeed) & 63;
		pitchOffset += vib;
	}

	// --- Arpeggio effect 0xy (overrides instrument arp table) ---
	if (efx == 0x00 && efxData != 0)
	{
		static const int8_t arpSeq[3] = { 0, 0, 0 };
		int8_t semis[3];
		semis[0] = 0;
		semis[1] = (int8_t)(efxData >> 4);
		semis[2] = (int8_t)(efxData & 0x0F);

		int noteIdx = (int)ch->note - 1 + semis[ch->arpeggioTick % 3];
		if (noteIdx >= 0 && noteIdx < 10*12)
		{
			uint16_t p = ymPeriodTable[noteIdx];
			if (p != 0)
				ch->basePeriod = p;
		}
		ch->arpeggioTick++;
		arpOffset = 0; // don't double-apply arp
		(void)arpSeq;
	}

	// --- Apply arp semitone offset from instrument table ---
	if (arpOffset != 0 && ch->note > 0)
	{
		int noteIdx = (int)ch->note - 1 + arpOffset;
		if (noteIdx >= 0 && noteIdx < 10*12)
		{
			uint16_t p = ymPeriodTable[noteIdx];
			if (p != 0)
				ch->finalPeriod = p;
		}
	}
	else
	{
		ch->finalPeriod = ch->basePeriod;
	}

	// Apply pitch envelope offset
	int32_t fp = (int32_t)ch->finalPeriod + pitchOffset;
	if (fp < 1) fp = 1;
	if (fp > 4095) fp = 4095;
	ch->finalPeriod = (uint16_t)fp;

	// --- Write channel period to YM ---
	writeChannelPeriod(&ar->ym, chIdx, ch->finalPeriod);

	// --- Write channel volume ---
	if (ins != NULL && ins->useHwEnvelope)
	{
		// Hardware envelope mode: set bit 4 of amplitude register
		writeChannelVolume(&ar->ym, chIdx, 0x10);
	}
	else
	{
		writeChannelVolume(&ar->ym, chIdx, ch->volume & 0x0F);
	}

	// --- Write noise period ---
	ym2149_write(&ar->ym, YM_REG_NOISE_PERIOD, ch->noisePeriod & 0x1F);
}

// ---------------------------------------------------------------------------
// Read next song position (mirrors getNextPos() logic in ft2_replayer.c)
// ---------------------------------------------------------------------------

// Extern globals from ft2_replayer.c
extern int8_t  playMode;
extern bool    songPlaying;
extern song_t  song;
extern note_t *pattern[MAX_PATTERNS];
extern int16_t patternNumRows[MAX_PATTERNS];

static bool atari_getNextPos(void)
{
	if (song.pBreakFlag || song.posJumpFlag)
	{
		song.pBreakFlag  = false;
		song.posJumpFlag = false;
		song.row = song.pBreakPos;
		song.pBreakPos = 0;

		song.songPos++;
		if (song.songPos >= song.songLength)
		{
			song.songPos = (int16_t)song.songLoopStart;
			return false; // reached end
		}

		song.pattNum = song.orders[song.songPos];
		song.currNumRows = patternNumRows[song.pattNum];
		if (song.currNumRows <= 0) song.currNumRows = 1;
		return true;
	}

	song.row++;
	if (song.row >= song.currNumRows)
	{
		song.row = 0;
		song.songPos++;
		if (song.songPos >= song.songLength)
		{
			song.songPos = (int16_t)song.songLoopStart;
			return false;
		}

		song.pattNum = song.orders[song.songPos];
		song.currNumRows = patternNumRows[song.pattNum];
		if (song.currNumRows <= 0) song.currNumRows = 1;
	}

	return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void atariReplayer_init(atariReplayer_t *ar)
{
	memset(ar, 0, sizeof(*ar));
	buildPeriodTable();
	ym2149_init(&ar->ym, 2000000, true); // YM2149, 2 MHz Atari ST clock

	// Default instrument: tone only, full volume
	for (int i = 0; i < MAX_PSG_INSTRUMENTS; i++)
	{
		ar->instruments[i].toneEnabled  = true;
		ar->instruments[i].noiseEnabled = false;
		ar->instruments[i].volEnvLength = 0;
		ar->instruments[i].arpLength    = 0;
		ar->instruments[i].pitchEnvLength = 0;
		ar->instruments[i].volEnvLoopStart  = PSG_NO_LOOP;
		ar->instruments[i].arpLoopStart     = PSG_NO_LOOP;
		ar->instruments[i].pitchEnvLoopStart = PSG_NO_LOOP;
	}

	atariReplayer_reset(ar);
}

void atariReplayer_reset(atariReplayer_t *ar)
{
	ym2149_reset(&ar->ym);

	for (int i = 0; i < ATARI_PSG_CHANNELS; i++)
	{
		memset(&ar->ch[i], 0, sizeof(ar->ch[i]));
		ar->ch[i].mixerOverride = -1;
		ar->ch[i].volume = 0x0F; // max volume
	}

	// Silence mixer (all channels disabled initially)
	ym2149_write(&ar->ym, YM_REG_MIXER, 0xFF);
	for (int i = 0; i < 3; i++)
		ym2149_write(&ar->ym, YM_REG_AMP_A + i, 0);

	memset(ar->lastRegs, 0, sizeof(ar->lastRegs));
}

bool atariReplayer_tick(atariReplayer_t *ar)
{
	// Mirror of ft2_replayer tickReplayer() but for PSG only
	if (!songPlaying)
		return false;

	bool tickZero = false;
	if (--song.tick == 0)
	{
		song.tick = song.speed;
		tickZero = true;
	}

	static note_t nilLine[MAX_CHANNELS];

	if (tickZero && song.pattDelTime2 == 0)
	{
		// Read new notes for PSG channels 0-2
		const note_t *p = nilLine;
		if (pattern[song.pattNum] != NULL)
			p = &pattern[song.pattNum][song.row * MAX_CHANNELS];

		for (int i = 0; i < ATARI_PSG_CHANNELS; i++)
		{
			const note_t *n = &p[i];
			psgChannel_t  *ch = &ar->ch[i];

			uint8_t note  = n->note;
			uint8_t instr = n->instr;
			uint8_t efx   = n->efx;
			uint8_t efxD  = n->efxData;

			// Handle volume column Cxx (set volume 0-63 → map to 0-15)
			if (n->vol >= 0x10 && n->vol <= 0x50)
				ch->volume = (uint8_t)((n->vol - 0x10) >> 2); // 0-64 → 0-15

			// Trigger note (may update instr pointer)
			if (note > 0 && note != NOTE_OFF && efx != 0x03)
				triggerPsgNote(ar, i, note, instr);
			else if (note == NOTE_OFF)
				ch->noteOn = false;
			else if (instr > 0 && instr <= MAX_PSG_INSTRUMENTS)
				ch->instr = &ar->instruments[instr - 1]; // instr change without note

			// Portamento: store target period
			if (efx == 0x03 && note > 0 && note != NOTE_OFF)
			{
				int nidx = (int)note - 1;
				if (nidx >= 0 && nidx < 10*12)
				{
					uint16_t tp = ymPeriodTable[nidx];
					if (tp != 0) ch->portaTargetPeriod = tp;
				}
			}

			// Handle song-level effects (Bxx, Dxx, Fxx) regardless of channel
			if (i == 0) // process song effects from channel 0 only
			{
				if (efx == 0x0B) // Bxx: position jump
				{
					int16_t pos = (int16_t)efxD - 1;
					if (pos < 0) pos = 0;
					if (pos >= song.songLength) pos = song.songLength - 1;
					song.songPos = pos;
					song.pBreakPos = 0;
					song.posJumpFlag = true;
				}
				else if (efx == 0x0D) // Dxx: pattern break
				{
					song.pBreakPos = (uint8_t)(((efxD >> 4) * 10) + (efxD & 0x0F));
					if (song.pBreakPos > 63) song.pBreakPos = 0;
					song.posJumpFlag = true;
				}
				else if (efx == 0x0F) // Fxx: set speed / BPM
				{
					if (efxD >= 32)
						song.BPM = efxD;
					else
						song.tick = song.speed = efxD;
				}
			}

			tickPsgChannel(ar, i, efx, efxD, true);
		}

		// Pattern delay
		if (song.pattDelTime > 0)
		{
			song.pattDelTime2 = song.pattDelTime;
			song.pattDelTime  = 0;
		}
	}
	else
	{
		// Non-zero tick: apply running effects
		for (int i = 0; i < ATARI_PSG_CHANNELS; i++)
		{
			const note_t *p = nilLine;
			if (pattern[song.pattNum] != NULL)
				p = &pattern[song.pattNum][song.row * MAX_CHANNELS];

			tickPsgChannel(ar, i, p[i].efx, p[i].efxData, false);
		}
	}

	if (song.pattDelTime2 > 0)
		song.pattDelTime2--;

	// Update R7 mixer
	ym2149_write(&ar->ym, YM_REG_MIXER, buildMixer(ar->ch));

	// Advance song position after tick-zero
	bool songAlive = true;
	if (tickZero)
		songAlive = atari_getNextPos();

	// Snapshot register state for export
	for (int r = 0; r < 14; r++)
		ar->lastRegs[r] = ym2149_read(&ar->ym, (uint8_t)r);

	return songAlive;
}

void atariReplayer_getRegs(const atariReplayer_t *ar, uint8_t dst[14])
{
	memcpy(dst, ar->lastRegs, 14);
}

psgInstrument_t *atariReplayer_getInstrument(atariReplayer_t *ar, int idx)
{
	if (idx < 0 || idx >= MAX_PSG_INSTRUMENTS)
		return NULL;
	return &ar->instruments[idx];
}

void atariReplayer_generateAudio(atariReplayer_t *ar, int16_t *outBuf,
                                 uint32_t numSamples, uint32_t sampleRate)
{
	// Allocate temporary per-channel buffers on the stack for small chunks,
	// or skip if numSamples is too large.
	if (numSamples == 0)
		return;

	int16_t *bufA = (int16_t *)malloc(numSamples * sizeof(int16_t));
	int16_t *bufB = (int16_t *)malloc(numSamples * sizeof(int16_t));
	int16_t *bufC = (int16_t *)malloc(numSamples * sizeof(int16_t));

	if (bufA == NULL || bufB == NULL || bufC == NULL)
	{
		free(bufA); free(bufB); free(bufC);
		return;
	}

	ym2149_generate(&ar->ym, bufA, bufB, bufC, numSamples, sampleRate);

	// Mix all 3 channels and output as interleaved stereo (mono)
	for (uint32_t i = 0; i < numSamples; i++)
	{
		int32_t mixed = (int32_t)bufA[i] + bufB[i] + bufC[i];
		// Attenuate to prevent clipping (3 channels summed)
		mixed /= 3;
		if (mixed >  32767) mixed =  32767;
		if (mixed < -32768) mixed = -32768;
		int16_t s = (int16_t)mixed;
		outBuf[i * 2 + 0] = s; // left
		outBuf[i * 2 + 1] = s; // right
	}

	free(bufA);
	free(bufB);
	free(bufC);
}
