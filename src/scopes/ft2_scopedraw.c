#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

/* Windowed-sinc interpolation LUT
**  (used on the scopes when cubic/sinc mixing interpolation is enabled)
**
** This table has been carefully crafted to have no overshoot so
** that it doesn't clip outside the scope frame.
**
**        Taps: 4
**      Phases: 64 (good enough for the scopes)
**       Scale: 32768 (truncated, no rounding)
** Window type: Kaiser-Bessel (beta = 9.0)
** Sinc cutoff: 0.5 (50%)
**
** Calculated with unity gain.
*/
static const int16_t scopeIntrpLUT[SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES] =
{
	 4770, 23227,  4770,     0,  4518, 23219,  5029,     0,
	 4276, 23194,  5297,     0,  4041, 23152,  5572,     1,
	 3815, 23094,  5856,     1,  3597, 23019,  6148,     2,
	 3387, 22928,  6447,     4,  3186, 22821,  6754,     5,
	 2992, 22698,  7069,     7,  2807, 22559,  7390,    10,
	 2629, 22405,  7719,    13,  2460, 22236,  8054,    16,
	 2298, 22052,  8396,    20,  2144, 21854,  8744,    25,
	 1997, 21641,  9097,    31,  1857, 21415,  9456,    37,
	 1725, 21176,  9821,    45,  1599, 20924, 10189,    54,
	 1480, 20659, 10563,    64,  1368, 20383, 10940,    75,
	 1262, 20096, 11320,    88,  1162, 19798, 11704,   102,
	 1068, 19489, 12090,   118,   980, 19171, 12478,   137,
	  897, 18844, 12868,   157,   820, 18509, 13258,   179,
	  748, 18165, 13650,   203,   681, 17815, 14040,   230,
	  618, 17457, 14431,   260,   560, 17094, 14820,   293,
	  506, 16725, 15207,   329,   456, 16351, 15591,   368,
	  410, 15973, 15973,   410,   368, 15591, 16351,   456,
	  329, 15207, 16725,   506,   293, 14820, 17094,   560,
	  260, 14431, 17457,   618,   230, 14040, 17815,   681,
	  203, 13650, 18165,   748,   179, 13258, 18509,   820,
	  157, 12868, 18844,   897,   137, 12478, 19171,   980,
	  118, 12090, 19489,  1068,   102, 11704, 19798,  1162,
	   88, 11320, 20096,  1262,    75, 10940, 20383,  1368,
	   64, 10563, 20659,  1480,    54, 10189, 20924,  1599,
	   45,  9821, 21176,  1725,    37,  9456, 21415,  1857,
	   31,  9097, 21641,  1997,    25,  8744, 21854,  2144,
	   20,  8396, 22052,  2298,    16,  8054, 22236,  2460,
	   13,  7719, 22405,  2629,    10,  7390, 22559,  2807,
	    7,  7069, 22698,  2992,     5,  6754, 22821,  3186,
	    4,  6447, 22928,  3387,     2,  6148, 23019,  3597,
	    1,  5856, 23094,  3815,     1,  5572, 23152,  4041,
	    0,  5297, 23194,  4276,     0,  5029, 23219,  4518
};

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, const uint32_t color);

/* ----------------------------------------------------------------------- */
/*                    NON-LINED SCOPE DRAWING ROUTINES                     */
/* ----------------------------------------------------------------------- */

static void scopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void scopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingpongLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8_PINGPONG
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void scopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void scopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingpongLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16_PINGPONG
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_PINGPONG
	}
}

/* ----------------------------------------------------------------------- */
/*                       LINED SCOPE DRAWING ROUTINES                      */
/* ----------------------------------------------------------------------- */

static void linedScopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT
	LINED_SCOPE_PREPARE_SMP8_LOOP
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP8_LOOP
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingpongLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT_PINGPONG
	LINED_SCOPE_PREPARE_SMP8_PINGPONG
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP8_PINGPONG
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void linedScopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT
	LINED_SCOPE_PREPARE_SMP16_LOOP
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP16_LOOP
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingpongLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT_PINGPONG
	LINED_SCOPE_PREPARE_SMP16_PINGPONG
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP16_PINGPONG
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_PINGPONG
	}
}

// -----------------------------------------------------------------------

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, const uint32_t color)
{
#ifdef _DEBUG
	if (x1 < 0 || x1 >= SCREEN_W || y1 < 0 || y1 >= SCREEN_H || y2 < 0 || y2 >= SCREEN_H)
		return;
#endif

	uint32_t *dst32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];

	*dst32 = color; // set first pixel

	const int32_t dy = y2 - y1;
	if (dy == 0) // y1 == y2
	{
		dst32[1] = color;
		return;
	}

	uint32_t ay = ABS(dy);
	int32_t d = 1 - ay;

	ay <<= 1;

	if (y1 > y2)
	{
		for (; y1 != y2; y1--)
		{
			if (d >= 0)
			{
				d -= ay;
				dst32++;
			}

			d += 2;

			dst32 -= SCREEN_W;
			*dst32 = color;
		}
	}
	else
	{
		for (; y1 != y2; y1++)
		{
			if (d >= 0)
			{
				d -= ay;
				dst32++;
			}

			d += 2;

			dst32 += SCREEN_W;
			*dst32 = color;
		}
	}
}

// -----------------------------------------------------------------------

const scopeDrawRoutine scopeDrawRoutineTable[12] =
{
	(scopeDrawRoutine)scopeDrawNoLoop_8bit,
	(scopeDrawRoutine)scopeDrawLoop_8bit,
	(scopeDrawRoutine)scopeDrawPingpongLoop_8bit,
	(scopeDrawRoutine)scopeDrawNoLoop_16bit,
	(scopeDrawRoutine)scopeDrawLoop_16bit,
	(scopeDrawRoutine)scopeDrawPingpongLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawPingpongLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawPingpongLoop_16bit
};
