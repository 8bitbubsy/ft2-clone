#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

/* 4-point cubic B-spline (no overshoot) interpolation LUT (32 phases).
** Used on the scopes when using quadratic/cubic/sinc mixing interpolation,
** together with linear interpolation between the phases in the LUT.
**
** ----------------- C code for making LUT (has unity gain) ----------------
** #define SCALE 32768
** #define PHASES 32
** for (int32_t i = 0; i < PHASES; i++)
** {
** 		double t1 = i / (double)PHASES, t2 = t1 * t1, t3 = t2 * t1;
** 		double a = SCALE * ((1.0 - 3.0*t1 + 3.0*t2 - t3) / 6.0);
** 		double b = SCALE * ((4.0 - 6.0*t2 + 3.0*t3) / 6.0);
** 		double c = SCALE * ((1.0 + 3.0*t1 + 3.0*t2 - 3.0*t3) / 6.0);
** 		double d = SCALE * (t3 / 6.0);
** 		printf("%d, %d, %d, %d,\n", (int32_t)a, (int32_t)b, (int32_t)c, (int32_t)d);
** 	}
** 	printf("%d, %d, %d, %d // inverted copy of first phase (for interpolation)\n",
** 		0, (int32_t)(SCALE * (1.0 / 6.0)), (int32_t)(SCALE * (4.0 / 6.0)),
** 		(int32_t)(SCALE * (1.0 / 6.0)));
** ------------------------------------------------------------------------- */
static const int16_t scopeIntrpLUT[SCOPE_INTRP_WIDTH * (SCOPE_INTRP_PHASES+1)] =
{
	5461, 21845,  5461,    0,
	4965, 21813,  5988,    0,
	4500, 21721,  6545,    1,
	4064, 21570,  7127,    4,
	3658, 21365,  7733,   10,
	3280, 21107,  8358,   20,
	2929, 20801,  9001,   36,
	2604, 20448,  9657,   57,
	2304, 20053, 10325,   85,
	2027, 19617, 11000,  121,
	1774, 19145, 11681,  166,
	1543, 18638, 12363,  221,
	1333, 18101, 13045,  288,
	1143, 17535, 13722,  366,
	 972, 16945, 14393,  457,
	 818, 16332, 15053,  562,
	 682, 15701, 15701,  682,
	 562, 15053, 16332,  818,
	 457, 14393, 16945,  972,
	 366, 13722, 17535, 1143,
	 288, 13045, 18101, 1333,
	 221, 12363, 18638, 1543,
	 166, 11681, 19145, 1774,
	 121, 11000, 19617, 2027,
	  85, 10325, 20053, 2304,
	  57,  9657, 20448, 2604,
	  36,  9001, 20801, 2929,
	  20,  8358, 21107, 3280,
	  10,  7733, 21365, 3658,
	   4,  7127, 21570, 4064,
	   1,  6545, 21721, 4500,
	   0,  5988, 21813, 4965,
	   0,  5461, 21845, 5461 // inverted copy of first phase (for interpolation)
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
