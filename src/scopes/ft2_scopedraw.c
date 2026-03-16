#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

// pre-computed 4-point cubic B-spline at 64 phases (non-overshoot, scale = 2^15)
static const int16_t scopeIntrpLUT[SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES] =
{
	 5461, 21845,  5461,     0, 5209, 21837,  5721,     0,
	 4965, 21813,  5988,     0, 4728, 21775,  6263,     0,
	 4500, 21721,  6545,     1, 4278, 21653,  6833,     2,
	 4064, 21570,  7127,     4, 3858, 21474,  7427,     7,
	 3658, 21365,  7733,    10, 3466, 21242,  8043,    15,
	 3280, 21107,  8358,    20, 3101, 20960,  8678,    27,
	 2929, 20801,  9001,    36, 2763, 20630,  9328,    45,
	 2604, 20448,  9657,    57, 2451, 20256,  9990,    70,
	 2303, 20053, 10325,    85, 2162, 19840, 10662,   102,
	 2027, 19617, 11000,   121, 1898, 19386, 11340,   142,
	 1774, 19145, 11681,   166, 1656, 18896, 12022,   192,
	 1543, 18638, 12363,   221, 1435, 18373, 12704,   253,
	 1333, 18101, 13045,   288, 1235, 17821, 13384,   325,
	 1143, 17535, 13722,   366, 1055, 17243, 14059,   410,
	  972, 16945, 14393,   457,  893, 16641, 14725,   508,
	  818, 16332, 15053,   562,  748, 16019, 15379,   620,
	  682, 15701, 15701,   682,  620, 15379, 16019,   748,
	  562, 15053, 16332,   818,  508, 14725, 16641,   893,
	  457, 14393, 16945,   972,  410, 14059, 17243,  1055,
	  366, 13722, 17535,  1143,  325, 13384, 17821,  1235,
	  288, 13045, 18101,  1333,  253, 12704, 18373,  1435,
	  221, 12363, 18638,  1543,  192, 12022, 18896,  1656,
	  166, 11681, 19145,  1774,  142, 11340, 19386,  1898,
	  121, 11000, 19617,  2027,  102, 10662, 19840,  2162,
	   85, 10325, 20053,  2304,   70,  9990, 20256,  2451,
	   57,  9657, 20448,  2604,   45,  9328, 20630,  2763,
	   36,  9001, 20801,  2929,   27,  8678, 20960,  3101,
	   20,  8358, 21107,  3280,   15,  8043, 21242,  3466,
	   10,  7733, 21365,  3658,    7,  7427, 21474,  3858,
	    4,  7127, 21570,  4064,    2,  6833, 21653,  4278,
	    1,  6545, 21721,  4500,    0,  6263, 21775,  4728,
	    0,  5988, 21813,  4965,    0,  5721, 21837,  5209
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
