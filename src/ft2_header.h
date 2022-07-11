#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <limits.h> // also has PATH_MAX
#endif
#include "ft2_replayer.h"

#define PROG_VER_STR "1.56"

// do NOT change these! It will only mess things up...

#define FT2_VBLANK_HZ 70.086302895323 /* nominal, 640x400 @ 70Hz */
#define SCREEN_W 632
#define SCREEN_H 400

/* "60Hz" ranges everywhere from 59..61Hz depending on the monitor, so with
** no vsync we will get stuttering because the rate is not perfect...
*/
#define VBLANK_HZ 60

// 70Hz (FT2 vblank) delta -> 60Hz vblank delta (rounded)
#define SCALE_VBLANK_DELTA(x) (int32_t)(((x) * ((double)VBLANK_HZ / FT2_VBLANK_HZ)) + 0.5)

// scopes must be clocked slightly higher than the nominal vblank rate
#define SCOPE_HZ 64


/* Amount of extra bytes to allocate for every instrument sample,
** this is used for a hack for resampling interpolation to be
** branchless in the inner channel mixer loop.
** Warning: Do not change this!
*/
#define SMP_DAT_OFFSET 32
#define SAMPLE_PAD_LENGTH (SMP_DAT_OFFSET+32)

#ifndef _WIN32
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define DIR_DELIMITER '/'
#else
#define DIR_DELIMITER '\\'
#define PATH_MAX MAX_PATH
#endif

#define SGN(x) (((x) >= 0) ? 1 : -1)
#define ABS(a) (((a) < 0) ? -(a) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define DROUND(x) \
	     if (x < 0.0) x -= 0.5; \
	else if (x > 0.0) x += 0.5

#define FROUND(x) \
	     if (x < 0.0f) x -= 0.5f; \
	else if (x > 0.0f) x += 0.5f

// fast 32-bit -> 8-bit clamp
#define CLAMP8(i) if ((int8_t)(i) != i) i = 0x7F ^ (i >> 31)

// fast 32-bit -> 16-bit clamp
#define CLAMP16(i) if ((int16_t)(i) != i) i = 0x7FFF ^ (i >> 31)

#define ALIGN_PTR(p, x) (((uintptr_t)(p) + ((x)-1)) & ~((x)-1))
#define MALLOC_PAD(size, pad) (malloc((size) + (pad)))

#define SWAP16(x) \
( \
	(((uint16_t)((x) & 0x00FF)) << 8) | \
	(((uint16_t)((x) & 0xFF00)) >> 8)   \
)

#define SWAP32(x) \
( \
	(((uint32_t)((x) & 0x000000FF)) << 24) | \
	(((uint32_t)((x) & 0x0000FF00)) <<  8) | \
	(((uint32_t)((x) & 0x00FF0000)) >>  8) | \
	(((uint32_t)((x) & 0xFF000000)) >> 24)   \
)

#define SWAP64(x) \
( \
	(((x) << 56) & 0xFF00000000000000ULL) | \
	(((x) << 40) & 0x00FF000000000000ULL) | \
	(((x) << 24) & 0x0000FF0000000000ULL) | \
	(((x) <<  8) & 0x000000FF00000000ULL) | \
	(((x) >>  8) & 0x00000000FF000000ULL) | \
	(((x) >> 24) & 0x0000000000FF0000ULL) | \
	(((x) >> 40) & 0x000000000000FF00ULL) | \
	(((x) >> 56) & 0x00000000000000FFULL)  \
)

typedef struct smpPtr_t
{
	int8_t *origPtr, *ptr;
} smpPtr_t;
