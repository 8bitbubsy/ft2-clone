#pragma once

// YM6 file export for ft2-clone Atari mode
//
// Renders the current song using the Atari PSG replayer and writes a YM6
// register-dump file.  The format is:
//
//   "YM6!" (4 bytes)               — file identifier
//   "LeOnArD!" (8 bytes)           — check string
//   numFrames        (uint32_t BE) — number of VBL frames
//   attributes       (uint32_t BE) — bit0=interleaved, bit1=hasLoop
//   numDigidrums     (uint32_t BE) — 0 for plain export
//   ymClock          (uint32_t BE) — 2000000
//   playerHz         (uint16_t BE) — 50 (PAL) or 60 (NTSC)
//   loopFrame        (uint32_t BE) — loop frame index (0 = no loop)
//   additionalDataSz (uint16_t BE) — 0
//   songName\0                     — NUL-terminated
//   authorName\0                   — NUL-terminated
//   comment\0                      — NUL-terminated
//   register data (interleaved)    — all R0 values then all R1 … R13
//   "End!" (4 bytes)               — end marker
//
// The data block is NOT LHA-compressed (raw / uncompressed YM6).

#include <stdint.h>
#include <stdbool.h>

#define YM_EXPORT_MAX_TITLE   64
#define YM_EXPORT_MAX_AUTHOR  64
#define YM_EXPORT_MAX_COMMENT 128

typedef struct ymExportSettings_t
{
	char    title[YM_EXPORT_MAX_TITLE];
	char    author[YM_EXPORT_MAX_AUTHOR];
	char    comment[YM_EXPORT_MAX_COMMENT];
	uint8_t startPos;     // song order position to start from
	uint8_t stopPos;      // song order position to stop at
	uint16_t playerHz;    // 50 = PAL, 60 = NTSC
} ymExportSettings_t;

// Export the current song to a YM6 file at the given path.
// Returns true on success, false on error (sets errno / prints to stderr).
bool ymExportToFile(const char *path, const ymExportSettings_t *settings);
