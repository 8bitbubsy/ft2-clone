// Atari ST sidecar config persistence for ft2-clone
// See ft2_atari_config.h for documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_atari_config.h"
#include "ft2_structs.h"
#include "ft2_unicode.h"
#include "ft2_header.h"

atariConfig_t atariConfig;

void resetAtariConfig(void)
{
	memset(&atariConfig, 0, sizeof (atariConfig));
	atariConfig.ymTimerFreq    = 50;
	atariConfig.atariMode      = false;
	atariConfig.ymExportFormat = 0;
}

// Derive the sidecar path from editor.configFileLocationU by replacing the
// filename portion with "FT2-ATARI.CFG".
static void getSidecarPath(UNICHAR *outPath, int32_t outLen)
{
	UNICHAR_STRCPY(outPath, editor.configFileLocationU);

	// Find last directory separator
	int32_t len = (int32_t)UNICHAR_STRLEN(outPath);
	int32_t sep = -1;
	for (int32_t i = len - 1; i >= 0; i--)
	{
		if (outPath[i] == DIR_DELIMITER)
		{
			sep = i;
			break;
		}
	}

	if (sep >= 0)
		outPath[sep + 1] = '\0'; // keep trailing separator
	else
		outPath[0] = '\0'; // same directory

#ifdef _WIN32
	// Append wide-string filename
	wcscat(outPath, L"FT2-ATARI.CFG");
#else
	strcat(outPath, "FT2-ATARI.CFG");
#endif
	(void)outLen;
}

bool loadAtariConfig(void)
{
	if (editor.configFileLocationU == NULL)
	{
		resetAtariConfig();
		return false;
	}

	UNICHAR sidecarPath[PATH_MAX];
	getSidecarPath(sidecarPath, PATH_MAX);

	FILE *f = UNICHAR_FOPEN(sidecarPath, "rb");
	if (f == NULL)
	{
		resetAtariConfig();
		return false;
	}

	// Read and verify magic
	char magic[ATARI_CFG_MAGIC_LEN];
	if (fread(magic, 1, ATARI_CFG_MAGIC_LEN, f) != ATARI_CFG_MAGIC_LEN ||
	    memcmp(magic, ATARI_CFG_MAGIC, ATARI_CFG_MAGIC_LEN) != 0)
	{
		fclose(f);
		resetAtariConfig();
		return false;
	}

	// Read and verify version
	uint8_t version;
	if (fread(&version, 1, 1, f) != 1 || version != ATARI_CFG_VERSION)
	{
		fclose(f);
		resetAtariConfig();
		return false;
	}

	// Read struct
	if (fread(&atariConfig, 1, sizeof (atariConfig), f) != sizeof (atariConfig))
	{
		fclose(f);
		resetAtariConfig();
		return false;
	}

	fclose(f);
	return true;
}

bool saveAtariConfig(void)
{
	if (editor.configFileLocationU == NULL)
		return false;

	UNICHAR sidecarPath[PATH_MAX];
	getSidecarPath(sidecarPath, PATH_MAX);

	FILE *f = UNICHAR_FOPEN(sidecarPath, "wb");
	if (f == NULL)
		return false;

	fwrite(ATARI_CFG_MAGIC, 1, ATARI_CFG_MAGIC_LEN, f);

	const uint8_t version = ATARI_CFG_VERSION;
	fwrite(&version, 1, 1, f);

	fwrite(&atariConfig, 1, sizeof (atariConfig), f);

	fclose(f);
	return true;
}
