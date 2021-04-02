#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <limits.h> // PATH_MAX
#endif
#include "ft2_replayer.h"

#define PROG_VER_STR "1.46"

// do NOT change these! It will only mess things up...

/* "60Hz" ranges everywhere from 59..61Hz depending on the monitor, so with
** no vsync we will get stuttering because the rate is not perfect...
*/
#define VBLANK_HZ 60

// scopes must be clocked slightly higher than the nominal vblank rate
#define SCOPE_HZ 64

#define FT2_VBLANK_HZ 70
#define SCREEN_W 632
#define SCREEN_H 400

/* Amount of extra bytes to allocate for every instrument sample,
** this is used for a hack for resampling interpolation to be
** branchless in the inner channel mixer loop.
** Warning: Do not change this!
*/
#define LOOP_FIX_LEN 32
#define SMP_DAT_OFFSET 8

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

// fast 32-bit -> 8-bit clamp
#define CLAMP8(i) if ((int8_t)(i) != i) i = 0x7F ^ (i >> 31)

// fast 32-bit -> 16-bit clamp
#define CLAMP16(i) if ((int16_t)(i) != i) i = 0x7FFF ^ (i >> 31)

#define ALIGN_PTR(p, x) (((uintptr_t)(p) + ((x)-1)) & ~((x)-1))
#define MALLOC_PAD(size, pad) (malloc((size) + (pad)))

#define SWAP16(value) \
( \
	(((uint16_t)((value) & 0x00FF)) << 8) | \
	(((uint16_t)((value) & 0xFF00)) >> 8)   \
)

#define SWAP32(value) \
( \
	(((uint32_t)((value) & 0x000000FF)) << 24) | \
	(((uint32_t)((value) & 0x0000FF00)) <<  8) | \
	(((uint32_t)((value) & 0x00FF0000)) >>  8) | \
	(((uint32_t)((value) & 0xFF000000)) >> 24)   \
)
