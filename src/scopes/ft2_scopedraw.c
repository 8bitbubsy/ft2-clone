#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

/* 4-point cubic B-spline interpolation LUT with 128 phases. No overshoot.
** Used on the scopes when using quadratic/cubic/sinc mixing interpolation.
*/
static const int16_t scopeIntrpLUT[SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES] =
{
	 5461, 21845,  5461,     0,  5334, 21843,  5590,     0,
	 5209, 21837,  5721,     0,  5086, 21827,  5854,     0,
	 4965, 21813,  5988,     0,  4846, 21796,  6125,     0,
	 4728, 21775,  6263,     0,  4613, 21750,  6403,     0,
	 4500, 21721,  6545,     1,  4388, 21689,  6688,     1,
	 4278, 21653,  6833,     2,  4170, 21613,  6979,     3,
	 4064, 21570,  7127,     4,  3960, 21524,  7277,     5,
	 3858, 21474,  7427,     7,  3757, 21421,  7579,     8,
	 3658, 21365,  7733,    10,  3561, 21305,  7887,    12,
	 3466, 21242,  8043,    15,  3372, 21176,  8200,    17,
	 3280, 21107,  8358,    20,  3190, 21035,  8517,    24,
	 3101, 20960,  8678,    27,  3014, 20882,  8839,    31,
	 2929, 20801,  9001,    36,  2845, 20717,  9164,    40,
	 2763, 20630,  9328,    45,  2683, 20541,  9492,    51,
	 2604, 20448,  9657,    57,  2526, 20353,  9823,    63,
	 2451, 20256,  9990,    70,  2376, 20156, 10157,    77,
	 2304, 20053, 10325,    85,  2232, 19948, 10493,    93,
	 2162, 19840, 10662,   102,  2094, 19730, 10831,   111,
	 2027, 19617, 11000,   121,  1962, 19503, 11170,   131,
	 1898, 19386, 11340,   142,  1835, 19266, 11510,   154,
	 1774, 19145, 11681,   166,  1714, 19021, 11851,   179,
	 1656, 18896, 12022,   192,  1599, 18768, 12193,   207,
	 1543, 18638, 12363,   221,  1489, 18507, 12534,   237,
	 1435, 18373, 12704,   253,  1383, 18238, 12875,   270,
	 1333, 18101, 13045,   288,  1283, 17962, 13215,   306,
	 1235, 17821, 13384,   325,  1188, 17679, 13553,   345,
	 1143, 17535, 13722,   366,  1098, 17390, 13891,   387,
	 1055, 17243, 14059,   410,  1013, 17095, 14226,   433,
	  972, 16945, 14393,   457,   932, 16794, 14559,   482,
	  893, 16641, 14725,   508,   855, 16487, 14889,   534,
	  818, 16332, 15053,   562,   783, 16176, 15217,   591,
	  748, 16019, 15379,   620,   715, 15860, 15540,   651,
	  682, 15701, 15701,   682,   651, 15540, 15860,   715,
	  620, 15379, 16019,   748,   591, 15217, 16176,   783,
	  562, 15053, 16332,   818,   534, 14889, 16487,   855,
	  508, 14725, 16641,   893,   482, 14559, 16794,   932,
	  457, 14393, 16945,   972,   433, 14226, 17095,  1013,
	  410, 14059, 17243,  1055,   387, 13891, 17390,  1098,
	  366, 13722, 17535,  1143,   345, 13553, 17679,  1188,
	  325, 13384, 17821,  1235,   306, 13215, 17962,  1283,
	  288, 13045, 18101,  1333,   270, 12875, 18238,  1383,
	  253, 12704, 18373,  1435,   237, 12534, 18507,  1489,
	  221, 12363, 18638,  1543,   207, 12193, 18768,  1599,
	  192, 12022, 18896,  1656,   179, 11851, 19021,  1714,
	  166, 11681, 19145,  1774,   154, 11510, 19266,  1835,
	  142, 11340, 19386,  1898,   131, 11170, 19503,  1962,
	  121, 11000, 19617,  2027,   111, 10831, 19730,  2094,
	  102, 10662, 19840,  2162,    93, 10493, 19948,  2232,
	   85, 10325, 20053,  2304,    77, 10157, 20156,  2376,
	   70,  9990, 20256,  2451,    63,  9823, 20353,  2526,
	   57,  9657, 20448,  2604,    51,  9492, 20541,  2683,
	   45,  9328, 20630,  2763,    40,  9164, 20717,  2845,
	   36,  9001, 20801,  2929,    31,  8839, 20882,  3014,
	   27,  8678, 20960,  3101,    24,  8517, 21035,  3190,
	   20,  8358, 21107,  3280,    17,  8200, 21176,  3372,
	   15,  8043, 21242,  3466,    12,  7887, 21305,  3561,
	   10,  7733, 21365,  3658,     8,  7579, 21421,  3757,
		7,  7427, 21474,  3858,     5,  7277, 21524,  3960,
		4,  7127, 21570,  4064,     3,  6979, 21613,  4170,
		2,  6833, 21653,  4278,     1,  6688, 21689,  4388,
		1,  6545, 21721,  4500,     0,  6403, 21750,  4613,
		0,  6263, 21775,  4728,     0,  6125, 21796,  4846,
		0,  5988, 21813,  4965,     0,  5854, 21827,  5086,
		0,  5721, 21837,  5209,     0,  5590, 21843,  5334
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
