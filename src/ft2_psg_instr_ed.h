#pragma once

// PSG instrument editor for ft2-clone Atari mode
//
// Provides a simple text-mode UI for editing a psgInstrument_t.
// The editor is shown in place of the normal instrument editor when
// Atari mode is active.

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ft2_atari_replayer.h"

// Show/hide/draw the PSG instrument editor
void showPsgInstrEditor(void);
void hidePsgInstrEditor(void);
void drawPsgInstrEditor(void);

// Redraw the current instrument fields (call after data changes)
void updatePsgInstrEditor(void);

// Input handling
bool testPsgInstrEditorMouseDown(void);
void psgInstrEditorKeyDown(SDL_Keycode key);

// PSG instrument serialization

// Save a PSG instrument to a binary file.
// File format (binary, little-endian):
//   4 bytes: magic "PSGI"
//   1 byte:  version (1)
//  22 bytes: name (null-padded)
//  64 bytes: volEnvelope[64]
//   1 byte:  volEnvLength
//   1 byte:  volEnvLoopStart
//  64 bytes: arpTable[64] (signed)
//   1 byte:  arpLength
//   1 byte:  arpLoopStart
// 128 bytes: pitchEnvelope[64] as little-endian int16
//   1 byte:  pitchEnvLength
//   1 byte:  pitchEnvLoopStart
//   1 byte:  flags (bit0=toneEnabled, bit1=noiseEnabled, bit2=useHwEnvelope)
//   1 byte:  hwEnvShape
//   2 bytes: hwEnvPeriod (little-endian)
//   1 byte:  noisePeriod
bool savePsgInstr(const char *path, const psgInstrument_t *ins);
bool loadPsgInstr(const char *path, psgInstrument_t *ins);

// Returns pointer to the PSG instrument currently being edited
// (indexed by editor.curInstr, 1-based). Returns NULL if not available.
psgInstrument_t *getCurPsgInstr(void);
