#pragma once

// SNDH (Sound Header) file format — header parser
// SNDH is the standard archive format for Atari ST YM2149 chip music.
// Files contain 68000 machine code music routines preceded by a tag-based
// ASCII header block.
//
// This module only parses the metadata header.  Actual 68000 execution and
// YM register streaming are handled separately (see ym2149.h).

#include <stdint.h>
#include <stdbool.h>

// Maximum field lengths (including NUL terminator)
#define SNDH_TITLE_LEN  64
#define SNDH_AUTHOR_LEN 64
#define SNDH_YEAR_LEN    8

// Parsed SNDH metadata
typedef struct sndh_info_t
{
	char  title[SNDH_TITLE_LEN];    // song title  (TITL tag)
	char  author[SNDH_AUTHOR_LEN];  // composer    (COMM tag)
	char  year[SNDH_YEAR_LEN];      // year string (YEAR tag)
	int   numSubTunes;              // number of sub-tunes (## tag)
	int   defaultSubTune;           // default sub-tune index (1-based, defaults to 1 if !# tag is absent)
	float timerFreqHz;              // replay rate in Hz (TC tag, default 50.0 Hz PAL VBL)
} sndh_info_t;

// Parse the SNDH header contained in data[0..dataLen-1].
// Returns true and fills *info on success.
// Returns false if the magic "SNDH" marker is not found or the header is malformed.
bool sndh_parseHeader(const uint8_t *data, uint32_t dataLen, sndh_info_t *info);
