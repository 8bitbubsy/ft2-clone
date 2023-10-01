/* RAW (header-less) sample loader
**
** Note: Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

bool loadRAW(FILE *f, uint32_t filesize)
{
	sample_t *s = &tmpSmp;

	if (!allocateSmpData(s, filesize, false))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	if (fread(s->dataPtr, filesize, 1, f) != 1)
	{
		okBoxThreadSafe(0, "System message", "General I/O error during loading! Is the file in use?", NULL);
		return false;
	}

	s->length = filesize;
	s->volume = 64;
	s->panning = 128;

	return true;
}
