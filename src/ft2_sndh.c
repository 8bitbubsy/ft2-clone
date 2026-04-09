// SNDH file header parser
// See ft2_sndh.h for format documentation.

// hide POSIX warnings on MSVC
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_sndh.h"

// Copy a NUL-terminated string from src into dst, limiting to dstLen-1 chars.
static void safeCopy(char *dst, const char *src, size_t dstLen)
{
	size_t i = 0;
	while (i < dstLen - 1 && src[i] != '\0')
	{
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

bool sndh_parseHeader(const uint8_t *data, uint32_t dataLen, sndh_info_t *info)
{
	if (data == NULL || dataLen < 16 || info == NULL)
		return false;

	// The SNDH magic "SNDH" appears at offset 12 in the file
	// (after the 68000 BRA.S instruction at offset 0, 2-byte opcode + 2-byte
	// displacement, then an 8-byte sub-tune init stub, giving offset 12).
	// Some converters place it immediately at offset 0.
	// We search within the first 512 bytes to be robust.

	const uint32_t searchLimit = (dataLen < 512) ? dataLen : 512;
	uint32_t magicOffset = UINT32_MAX;

	for (uint32_t i = 0; i + 4 <= searchLimit; i++)
	{
		if (data[i]   == 'S' && data[i+1] == 'N' &&
		    data[i+2] == 'D' && data[i+3] == 'H')
		{
			magicOffset = i;
			break;
		}
	}

	if (magicOffset == UINT32_MAX)
		return false;

	// Initialise output with sensible defaults
	memset(info, 0, sizeof(*info));
	info->timerFreqHz  = 50.0f;  // PAL VBL default
	info->numSubTunes  = 1;
	info->defaultSubTune = 1;

	// The tag block starts immediately after the "SNDH" magic.
	uint32_t pos = magicOffset + 4;

	while (pos + 4 <= dataLen)
	{
		const char *tag = (const char *)&data[pos];

		// HDNS — end of header
		if (tag[0] == 'H' && tag[1] == 'D' && tag[2] == 'N' && tag[3] == 'S')
			break;

		// TITL — song title (NUL-terminated string follows immediately after tag)
		if (tag[0] == 'T' && tag[1] == 'I' && tag[2] == 'T' && tag[3] == 'L')
		{
			pos += 4;
			const char *str = (const char *)&data[pos];
			safeCopy(info->title, str, sizeof(info->title));
			// Advance past the NUL-terminated string
			while (pos < dataLen && data[pos] != '\0')
				pos++;
			pos++; // skip NUL
			continue;
		}

		// COMM — composer / author
		if (tag[0] == 'C' && tag[1] == 'O' && tag[2] == 'M' && tag[3] == 'M')
		{
			pos += 4;
			const char *str = (const char *)&data[pos];
			safeCopy(info->author, str, sizeof(info->author));
			while (pos < dataLen && data[pos] != '\0')
				pos++;
			pos++;
			continue;
		}

		// YEAR — year string
		if (tag[0] == 'Y' && tag[1] == 'E' && tag[2] == 'A' && tag[3] == 'R')
		{
			pos += 4;
			const char *str = (const char *)&data[pos];
			safeCopy(info->year, str, sizeof(info->year));
			while (pos < dataLen && data[pos] != '\0')
				pos++;
			pos++;
			continue;
		}

		// ##xx — number of sub-tunes (2-digit decimal ASCII after "##")
		if (tag[0] == '#' && tag[1] == '#')
		{
			// The next two bytes are the count as ASCII digits
			if (pos + 4 <= dataLen)
			{
				int tens = (int)(data[pos + 2] - '0');
				int ones = (int)(data[pos + 3] - '0');
				if (tens >= 0 && tens <= 9 && ones >= 0 && ones <= 9)
					info->numSubTunes = tens * 10 + ones;
			}
			pos += 4;
			continue;
		}

		// !# — default sub-tune (1-based, 2-digit ASCII)
		if (tag[0] == '!' && tag[1] == '#')
		{
			if (pos + 4 <= dataLen)
			{
				int tens = (int)(data[pos + 2] - '0');
				int ones = (int)(data[pos + 3] - '0');
				if (tens >= 0 && tens <= 9 && ones >= 0 && ones <= 9)
					info->defaultSubTune = tens * 10 + ones;
			}
			pos += 4;
			continue;
		}

		// TC — timer C frequency (followed by 2 big-endian bytes giving Hz)
		// Some SNDH files use a 4-byte float or 2-byte integer here.
		if (tag[0] == 'T' && tag[1] == 'C')
		{
			pos += 2;
			if (pos + 2 <= dataLen)
			{
				uint16_t freq = ((uint16_t)data[pos] << 8) | data[pos + 1];
				if (freq > 0)
					info->timerFreqHz = (float)freq;
				pos += 2;
			}
			continue;
		}

		// Unknown tag — scan forward to the next NUL or non-ASCII byte
		// to stay in sync with the stream.
		pos++;
	}

	return true;
}
