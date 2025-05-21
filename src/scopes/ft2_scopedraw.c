#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

static int16_t *scopeIntrpLUT;

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, const uint32_t color);

bool calcScopeIntrpLUT(void)
{
	scopeIntrpLUT = (int16_t *)malloc(SCOPE_INTRP_WIDTH * SCOPE_INTRP_PHASES * sizeof (int16_t));
	if (scopeIntrpLUT == NULL)
		return false;

	/* Several tests have been done to figure out what interpolation method is most suitable
	** for the tracker scopes. After testing linear, cubic, Gaussian and windowed-sinc
	** interpolation, I have come to the conclusion that 4-point cubic B-spline is the best.
	** This interpolation method also has no overshoot.
	*/

	// 4-point cubic B-spline (no overshoot)

	int16_t *ptr16 = scopeIntrpLUT;
	for (int32_t i = 0; i < SCOPE_INTRP_PHASES; i++)
	{
		const double x1 = i * (1.0 / SCOPE_INTRP_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		double t1 = (-(1.0/6.0) * x3) + ( (1.0/2.0) * x2) + (-(1.0/2.0) * x1) + (1.0/6.0);
		double t2 = ( (1.0/2.0) * x3) + (     -1.0  * x2)                     + (2.0/3.0);
		double t3 = (-(1.0/2.0) * x3) + ( (1.0/2.0) * x2) + ( (1.0/2.0) * x1) + (1.0/6.0);
		double t4 =   (1.0/6.0) * x3;

		// truncate, do not round!
		*ptr16++ = (int16_t)(t1 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t2 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t3 * SCOPE_INTRP_SCALE);
		*ptr16++ = (int16_t)(t4 * SCOPE_INTRP_SCALE);
	}

	return true;
}

void freeScopeIntrpLUT(void)
{
	if (scopeIntrpLUT != NULL)
	{
		free(scopeIntrpLUT);
		scopeIntrpLUT = NULL;
	}
}

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

static void scopeDrawBidiLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT_BIDI

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8_BIDI
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_BIDI
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

static void scopeDrawBidiLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_INIT_BIDI

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16_BIDI
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_BIDI
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

static void linedScopeDrawBidiLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT_BIDI
	LINED_SCOPE_PREPARE_SMP8_BIDI
	SCOPE_HANDLE_POS_BIDI

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP8_BIDI
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_BIDI
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

static void linedScopeDrawBidiLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_INIT_BIDI
	LINED_SCOPE_PREPARE_SMP16_BIDI
	SCOPE_HANDLE_POS_BIDI

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP16_BIDI
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_BIDI
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
	(scopeDrawRoutine)scopeDrawBidiLoop_8bit,
	(scopeDrawRoutine)scopeDrawNoLoop_16bit,
	(scopeDrawRoutine)scopeDrawLoop_16bit,
	(scopeDrawRoutine)scopeDrawBidiLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawBidiLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawBidiLoop_16bit
};
