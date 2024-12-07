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

static float *fScopeIntrpLUT;

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, uint32_t color);

bool calcScopeIntrpLUT(void)
{
	fScopeIntrpLUT = (float *)malloc(SCOPE_INTRP_TAPS * SCOPE_INTRP_PHASES * sizeof (float));
	if (fScopeIntrpLUT == NULL)
		return false;

	// 6-point cubic B-spline (No overshoot w/ low filter cut-off. Very suitable for scopes.)
	float *fPtr = fScopeIntrpLUT;
	for (int32_t i = 0; i < SCOPE_INTRP_PHASES; i++)
	{
		const double x1 = i * (1.0 / SCOPE_INTRP_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3
		const double x4 = x3 * x1; // x^4
		const double x5 = x4 * x1; // x^5

		double t1 = (-(1.0/120.0) * x5) + ( (1.0/24.0) * x4) + (-(1.0/12.0) * x3) + ( (1.0/12.0) * x2) + (-(1.0/24.0) * x1) + ( 1.0/120.0);
		double t2 = ( (1.0/ 24.0) * x5) + (-(1.0/ 6.0) * x4) + ( (1.0/ 6.0) * x3) + ( (1.0/ 6.0) * x2) + (-(5.0/12.0) * x1) + (13.0/ 60.0);
		double t3 = (-(1.0/ 12.0) * x5) + ( (1.0/ 4.0) * x4) +                      (-(1.0/ 2.0) * x2)                      + (11.0/ 20.0);
		double t4 = ( (1.0/ 12.0) * x5) + (-(1.0/ 6.0) * x4) + (-(1.0/ 6.0) * x3) + ( (1.0/ 6.0) * x2) + ( (5.0/12.0) * x1) + (13.0/ 60.0);
		double t5 = (-(1.0/ 24.0) * x5) + ( (1.0/24.0) * x4) + ( (1.0/12.0) * x3) + ( (1.0/12.0) * x2) + ( (1.0/24.0) * x1) + ( 1.0/120.0);
		double t6 =   (1.0/120.0) * x5;

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
		*fPtr++ = (float)t5;
		*fPtr++ = (float)t6;
	}

	return true;
}

void freeScopeIntrpLUT(void)
{
	if (fScopeIntrpLUT != NULL)
	{
		free(fScopeIntrpLUT);
		fScopeIntrpLUT = NULL;
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
