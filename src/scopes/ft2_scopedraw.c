#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "../mixer/ft2_gaussian.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

static int16_t *scopeIntrpLUT;

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, uint32_t color);

bool calcScopeIntrpLUT(void)
{
	scopeIntrpLUT = (int16_t *)malloc(4 * SCOPE_INTRP_PHASES * sizeof (int16_t));
	if (scopeIntrpLUT == NULL)
		return false;

	int16_t *ptr = scopeIntrpLUT;
	for (int32_t i = 0; i < SCOPE_INTRP_PHASES; i++)
	{
#define PI_MULTIPLIER 2.048

		const int32_t i1 = SCOPE_INTRP_PHASES + i;
		const int32_t i2 = i;
		const int32_t i3 = (SCOPE_INTRP_PHASES-1) - i;
		const int32_t i4 = ((SCOPE_INTRP_PHASES*2)-1) - i;

		const double x1 = (0.5 + i1) * (1.0 / ((SCOPE_INTRP_PHASES*4)-1));
		const double x2 = (0.5 + i2) * (1.0 / ((SCOPE_INTRP_PHASES*4)-1));
		const double x3 = (0.5 + i3) * (1.0 / ((SCOPE_INTRP_PHASES*4)-1));
		const double x4 = (0.5 + i4) * (1.0 / ((SCOPE_INTRP_PHASES*4)-1));

		// Blackman window
		const double w1 = (0.42 + (0.50 * cos(2.0 * PI * x1)) + (0.08 * cos(4.0 * PI * x1))) / x1;
		const double w2 = (0.42 + (0.50 * cos(2.0 * PI * x2)) + (0.08 * cos(4.0 * PI * x2))) / x2;
		const double w3 = (0.42 + (0.50 * cos(2.0 * PI * x3)) + (0.08 * cos(4.0 * PI * x3))) / x3;
		const double w4 = (0.42 + (0.50 * cos(2.0 * PI * x4)) + (0.08 * cos(4.0 * PI * x4))) / x4;

		const double t1 = sin(PI_MULTIPLIER * PI * x1) * w1;
		const double t2 = sin(PI_MULTIPLIER * PI * x2) * w2;
		const double t3 = sin(PI_MULTIPLIER * PI * x3) * w3;
		const double t4 = sin(PI_MULTIPLIER * PI * x4) * w4;

		// calculate normalization value (also assures unity gain when summing taps)
		const double dScale = SCOPE_INTRP_SCALE / (t1 + t2 + t3 + t4);

		*ptr++ = (int16_t)((t1 * dScale) + 0.5);
		*ptr++ = (int16_t)((t2 * dScale) + 0.5);
		*ptr++ = (int16_t)((t3 * dScale) + 0.5);
		*ptr++ = (int16_t)((t4 * dScale) + 0.5);
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
/*                         SCOPE DRAWING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void scopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_NO_LOOP

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
	SCOPE_REGS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingPong_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_BIDI

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
	SCOPE_REGS_NO_LOOP

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
	SCOPE_REGS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingPong_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_BIDI

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16_BIDI
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_BIDI
	}
}

/* ----------------------------------------------------------------------- */
/*                   INTERPOLATED SCOPE DRAWING ROUTINES                   */
/* ----------------------------------------------------------------------- */

static void linedScopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_NO_LOOP
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
	LINED_SCOPE_REGS_LOOP
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_BIDI
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
	LINED_SCOPE_REGS_NO_LOOP
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
	LINED_SCOPE_REGS_LOOP
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_INTERPOLATED_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_READPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_BIDI
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

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, uint32_t color)
{
	const int32_t dy = y2 - y1;
	const int32_t sy = SGN(dy);
	const int32_t pitch = sy * SCREEN_W;

	uint32_t *dst32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];

	*dst32 = color; // set first pixel

	int32_t ay = ABS(dy);
	if (ay <= 1)
	{
		if (ay != 0)
			dst32 += pitch;

		*++dst32 = color;
		return;
	}

	int32_t d = 1 - ay;

	ay <<= 1;
	while (y1 != y2)
	{
		if (d >= 0)
		{
			d -= ay;
			dst32++;
		}

		y1 += sy;
		d += 2;

		dst32 += pitch;
		*dst32 = color;
	}
}

// -----------------------------------------------------------------------

const scopeDrawRoutine scopeDrawRoutineTable[12] =
{
	(scopeDrawRoutine)scopeDrawNoLoop_8bit,
	(scopeDrawRoutine)scopeDrawLoop_8bit,
	(scopeDrawRoutine)scopeDrawPingPong_8bit,
	(scopeDrawRoutine)scopeDrawNoLoop_16bit,
	(scopeDrawRoutine)scopeDrawLoop_16bit,
	(scopeDrawRoutine)scopeDrawPingPong_16bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawLoop_8bit,
	(scopeDrawRoutine)linedScopeDrawPingPong_8bit,
	(scopeDrawRoutine)linedScopeDrawNoLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawLoop_16bit,
	(scopeDrawRoutine)linedScopeDrawPingPong_16bit
};
