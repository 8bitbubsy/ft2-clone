/* MP3 sample loader (using minimp3)
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

// hide minimp3 compiler warnings
#ifdef _MSC_VER
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#pragma warning(disable: 4456)
#pragma warning(disable: 4706)
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"
#include "../ft2_header.h"
#include "../ft2_mouse.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

static bool mp3IsStereo = true;

bool detectMP3(FILE *f)
{
	uint8_t h[10];

	memset(h, 0, sizeof (h));
	fread(h, 1, 10, f);

	// skip IDv3 tag, if found
	if (!memcmp(h, "ID3", 3) && !((h[5] & 15) || (h[6] & 0x80) || (h[7] & 0x80) || (h[8] & 0x80) || (h[9] & 0x80)))
	{
		uint32_t bytesToSkip = ((h[6] & 0x7F) << 21) | ((h[7] & 0x7F) << 14) | ((h[8] & 0x7F) << 7) | (h[9] & 0x7F);
		if (h[5] & 16)
			bytesToSkip += 10; // footer present

		fseek(f, 10+bytesToSkip, SEEK_SET);

		h[0] = h[1] = h[2] = h[3] = 0;
		fread(h, 1, 4, f);
	}

	mp3IsStereo = (((h[3]) & 0xC0) != 0xC0);

	// we can now test if this is an MP3 file

	return h[0] == 0xFF &&
	    ((h[1] & 0xF0) == 0xF0 || (h[1] & 0xFE) == 0xE2) &&
	    (HDR_GET_LAYER(h) != 0) &&
	    (HDR_GET_BITRATE(h) != 15) &&
	    (HDR_GET_SAMPLE_RATE(h) != 3);
}

bool loadMP3(FILE *f, uint32_t filesize)
{
	mp3dec_t mp3d;
	mp3dec_file_info_t info;
	sample_t *s = &tmpSmp;

	int16_t stereoAction = -1;
	if (mp3IsStereo)
	{
		stereoAction = loaderSysReq(4, "System request", "This is a stereo sample. Which channel do you want to read?", NULL);
		setMouseBusy(true);
	}

	uint8_t *buf = (uint8_t *)malloc(filesize);
	if (buf == NULL)
	{
		loaderMsgBox("Out of memory!");
		return false;
	}

	if (fread(buf, 1, filesize, f) != filesize)
	{
		free(buf);
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	int32_t result = mp3dec_load_buf(&mp3d, buf, filesize, &info, NULL, NULL);
	free(buf);
	// 'f' is closed later in ft2_sample_loader.c

	if (result != 0)
	{
		switch (result)
		{
			case MP3D_E_MEMORY:
				loaderMsgBox("Out of memory!");
				break;

			case MP3D_E_IOERROR:
				loaderMsgBox("General I/O error during loading! Is the file in use?");
				break;

			default:
				loaderMsgBox("Unknown MP3 load error!");
				break;
		}

		return false;
	}

	int16_t *src16 = (int16_t *)info.buffer;
	uint32_t sampleLength = (uint32_t)info.samples;

	if (mp3IsStereo)
		sampleLength >>= 1;

	if (!allocateSmpData(s, sampleLength, true))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	int16_t *out16 = (int16_t *)s->dataPtr;
	if (mp3IsStereo)
	{
		switch (stereoAction)
		{
			case STEREO_SAMPLE_READ_LEFT_CHANNEL:
			{
				for (uint32_t i = 1; i < sampleLength; i++, src16 += 2)
					out16[i] = src16[0];
			}
			break;

			case STEREO_SAMPLE_READ_RIGHT_CHANNEL:
			{
				for (uint32_t i = 0; i < sampleLength; i++, src16 += 2)
					out16[i] = src16[1];
			}
			break;

			default:
			case STEREO_SAMPLE_MIX_TO_MONO:
			{
				for (uint32_t i = 0; i < sampleLength; i++, src16 += 2)
					out16[i] = (src16[0] + src16[1]) >> 1;
			}
			break;
		}
	}
	else
	{
		// mono
		memcpy(out16, src16, sampleLength * sizeof (uint16_t));
	}

	free(info.buffer);

	s->volume = 64;
	s->panning = 128;
	s->flags = SAMPLE_16BIT;
	s->length = sampleLength;
	setSampleC4Hz(s, info.hz);

	return true;
}
