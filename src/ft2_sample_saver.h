#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

enum
{
	SAVE_NORMAL = 0,
	SAVE_RANGE = 1
};

void saveSample(UNICHAR *filenameU, bool saveAsRange);
