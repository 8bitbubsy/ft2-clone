#pragma once

// SNDH file export for ft2-clone Atari mode
//
// Exports the current song to an SNDH file containing:
//   - SNDH metadata header (tags: TITL, COMM, YEAR, TC, ## tag for subtunes)
//   - Atari PSG replayer code (68000 machine code stub)
//   - Song data (pattern data, instruments, orders)
//
// SNDH is the standard archive format for Atari ST chip music files.
// The format consists of:
//   - ASCII tag-based header (TITL, COMM, YEAR, TC, ##, !#, HDNS)
//   - 68000 machine code music routine
//   - Song data embedded in the binary
//
// For simplicity, this implementation generates a minimal SNDH wrapper around
// the YM register dump rather than a full 68000 replayer binary.

#include <stdint.h>
#include <stdbool.h>

#define SNDH_EXPORT_MAX_TITLE   64
#define SNDH_EXPORT_MAX_AUTHOR  64
#define SNDH_EXPORT_MAX_YEAR    8

typedef struct sndhExportSettings_t
{
	char    title[SNDH_EXPORT_MAX_TITLE];
	char    author[SNDH_EXPORT_MAX_AUTHOR];
	char    year[SNDH_EXPORT_MAX_YEAR];
	uint8_t startPos;     // song order position to start from
	uint8_t stopPos;      // song order position to stop at
	uint16_t timerHz;     // replay rate (50 = PAL, 60 = NTSC)
} sndhExportSettings_t;

// Export the current song to an SNDH file at the given path.
// Returns true on success, false on error.
// Note: This is a simplified implementation that generates a minimal SNDH
// wrapper. Full 68000 replayer code generation is not implemented.
bool sndhExportToFile(const char *path, const sndhExportSettings_t *settings);
