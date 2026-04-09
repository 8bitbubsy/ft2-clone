#pragma once

// Atari ST mode toggle and UI integration for ft2-clone
//
// This module provides:
//   - Global Atari mode enable/disable toggle
//   - UI elements for switching between standard XM and Atari PSG modes
//   - Integration with the main replayer
//   - Export menu entries for YM6 and SNDH formats
//
// When Atari mode is active:
//   - Only the first 3 channels are used (PSG has 3 voices)
//   - Pattern data is interpreted as PSG notes/effects
//   - Audio output comes from the YM2149 emulator
//   - Instruments are PSG instruments (see ft2_atari_replayer.h)

#include <stdint.h>
#include <stdbool.h>

// Forward declaration of atariReplayer_t (avoid circular includes)
typedef struct atariReplayer_t atariReplayer_t;

// Initialize Atari mode subsystem (call once at startup)
void atariMode_init(void);

// Free Atari mode resources (call on shutdown)
void atariMode_free(void);

// Toggle Atari mode on/off
void atariMode_toggle(void);

// Get current Atari mode state
bool atariMode_isActive(void);

// Set Atari mode state explicitly
void atariMode_setActive(bool active);

// UI callback: Export current song as YM6 file
void atariMode_exportYM6(void);

// UI callback: Export current song as SNDH file
void atariMode_exportSNDH(void);

// Draw Atari mode indicator on UI (called from main draw loop)
void atariMode_drawIndicator(void);

// Update Atari mode state (called per frame)
void atariMode_update(void);

// Get pointer to internal Atari replayer (may be NULL if not initialized)
atariReplayer_t *atariMode_getReplayer(void);
