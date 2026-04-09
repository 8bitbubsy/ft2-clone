// SNDH file exporter for ft2-clone Atari mode
// See ft2_sndh_export.h for format documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_sndh_export.h"
#include "ft2_atari_replayer.h"
#include "ft2_replayer.h"
#include "ft2_header.h"

// Minimal 68000 stub code for SNDH playback
// This is a placeholder - full 68000 replayer implementation would be needed
// for a complete SNDH export. For now, this generates a minimal valid SNDH
// header structure.
static const uint8_t sndh_stub_code[] = {
	// Minimal 68000 RTS (return from subroutine)
	0x4E, 0x75  // rts
};

bool sndhExportToFile(const char *path, const sndhExportSettings_t *settings)
{
	if (path == NULL || settings == NULL)
		return false;

	FILE *f = fopen(path, "wb");
	if (f == NULL)
		return false;

	// Write SNDH header magic
	fwrite("SNDH", 1, 4, f);

	// Write metadata tags
	// TITL - Song title
	if (settings->title[0] != '\0')
	{
		fprintf(f, "TITL%s", settings->title);
		fputc(0, f); // NUL terminator
	}

	// COMM - Composer/author
	if (settings->author[0] != '\0')
	{
		fprintf(f, "COMM%s", settings->author);
		fputc(0, f);
	}

	// YEAR - Year string
	if (settings->year[0] != '\0')
	{
		fprintf(f, "YEAR%s", settings->year);
		fputc(0, f);
	}

	// TC - Timer C frequency (replay rate in Hz)
	fprintf(f, "TC%02d", settings->timerHz);
	fputc(0, f);

	// ## - Number of subtunes (default to 1)
	fprintf(f, "##01");
	fputc(0, f);

	// !# - Default subtune (default to 1)
	fprintf(f, "!#01");
	fputc(0, f);

	// HDNS - End of header marker
	fwrite("HDNS", 1, 4, f);

	// Write minimal 68000 stub code
	fwrite(sndh_stub_code, 1, sizeof(sndh_stub_code), f);

	// Note: A full implementation would include:
	// 1. 68000 replayer code compiled for Atari ST
	// 2. Song data (patterns, instruments, orders) in a format the replayer can read
	// 3. Register update routines for YM2149
	//
	// This simplified version creates a valid SNDH header structure but does not
	// include playable music data. Full implementation would require embedding
	// a complete 68000 replay routine.

	fclose(f);
	return true;
}
