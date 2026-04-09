// Atari ST mode toggle and UI integration for ft2-clone
// See ft2_atari_mode.h for documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_atari_mode.h"
#include "ft2_atari_replayer.h"
#include "ft2_ym_export.h"
#include "ft2_sndh_export.h"
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_diskop.h"
#include "ft2_replayer.h"
#include "ft2_structs.h"

// Global Atari mode state
static bool atariModeActive = false;
static atariReplayer_t *atariReplayer = NULL;

void atariMode_init(void)
{
	// Allocate Atari replayer
	atariReplayer = (atariReplayer_t *)calloc(1, sizeof(atariReplayer_t));
	if (atariReplayer == NULL)
		return;

	// Initialize replayer
	atariReplayer_init(atariReplayer);

	// Atari mode is disabled by default
	atariModeActive = false;
}

void atariMode_free(void)
{
	if (atariReplayer != NULL)
	{
		free(atariReplayer);
		atariReplayer = NULL;
	}
}

void atariMode_toggle(void)
{
	atariModeActive = !atariModeActive;

	if (atariModeActive && atariReplayer != NULL)
	{
		// Entering Atari mode: reset replayer
		atariReplayer_reset(atariReplayer);
	}

	// Redraw UI to show mode change
	// (in a real implementation, this would update the mode indicator)
}

bool atariMode_isActive(void)
{
	return atariModeActive;
}

void atariMode_setActive(bool active)
{
	if (atariModeActive != active)
	{
		atariModeActive = active;

		if (atariModeActive && atariReplayer != NULL)
		{
			atariReplayer_reset(atariReplayer);
		}
	}
}

void atariMode_exportYM6(void)
{
	// Get filename from disk op dialog
	char *filename = getDiskOpFilename();
	if (filename == NULL || filename[0] == '\0')
		return;

	// Setup export settings
	ymExportSettings_t settings;
	memset(&settings, 0, sizeof(settings));

	// Use song name if available
	if (song.name[0] != '\0')
	{
		strncpy(settings.title, song.name, YM_EXPORT_MAX_TITLE - 1);
		settings.title[YM_EXPORT_MAX_TITLE - 1] = '\0';
	}
	else
	{
		strncpy(settings.title, "Untitled", YM_EXPORT_MAX_TITLE - 1);
	}

	// Set author (could be extended to read from module metadata)
	strncpy(settings.author, "Unknown", YM_EXPORT_MAX_AUTHOR - 1);
	strncpy(settings.comment, "Exported from ft2-clone", YM_EXPORT_MAX_COMMENT - 1);

	// Export full song
	settings.startPos = 0;
	settings.stopPos = (uint8_t)(song.songLength - 1);
	settings.playerHz = 50; // PAL by default

	// Perform export
	if (ymExportToFile(filename, &settings))
	{
		// Success - could show confirmation dialog
	}
	else
	{
		// Error - could show error dialog
	}
}

void atariMode_exportSNDH(void)
{
	// Get filename from disk op dialog
	char *filename = getDiskOpFilename();
	if (filename == NULL || filename[0] == '\0')
		return;

	// Setup export settings
	sndhExportSettings_t settings;
	memset(&settings, 0, sizeof(settings));

	// Use song name if available
	if (song.name[0] != '\0')
	{
		strncpy(settings.title, song.name, SNDH_EXPORT_MAX_TITLE - 1);
		settings.title[SNDH_EXPORT_MAX_TITLE - 1] = '\0';
	}
	else
	{
		strncpy(settings.title, "Untitled", SNDH_EXPORT_MAX_TITLE - 1);
	}

	// Set author
	strncpy(settings.author, "Unknown", SNDH_EXPORT_MAX_AUTHOR - 1);
	strncpy(settings.year, "2026", SNDH_EXPORT_MAX_YEAR - 1);

	// Export full song
	settings.startPos = 0;
	settings.stopPos = (uint8_t)(song.songLength - 1);
	settings.timerHz = 50; // PAL by default

	// Perform export
	if (sndhExportToFile(filename, &settings))
	{
		// Success - could show confirmation dialog
	}
	else
	{
		// Error - could show error dialog
	}
}

void atariMode_drawIndicator(void)
{
	if (!atariModeActive)
		return;

	// Draw a simple indicator showing Atari mode is active
	// In a real implementation, this would render text or an icon on the UI
	// For now, this is a placeholder that can be integrated with the GUI system

	// Example: textOut(x, y, PAL_FORGRND, "ATARI");
}

void atariMode_update(void)
{
	if (!atariModeActive || atariReplayer == NULL)
		return;

	// Update Atari replayer if song is playing
	if (songPlaying)
	{
		// In a real implementation, this would:
		// 1. Run atariReplayer_tick() to process pattern data
		// 2. Generate audio via atariReplayer_generateAudio()
		// 3. Mix with or replace the standard XM audio output

		// For now, this is a placeholder for integration with the audio system
	}
}
