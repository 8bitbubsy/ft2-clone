// RAW (header-less) sample loader

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

bool loadRAW(FILE *f, uint32_t filesize)
{
	if (!allocateTmpSmpData(&tmpSmp, filesize))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	if (fread(tmpSmp.pek, filesize, 1, f) != 1)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?");
		return false;
	}

	tmpSmp.len = filesize;
	tmpSmp.vol = 64;
	tmpSmp.pan = 128;

	return true;
}
