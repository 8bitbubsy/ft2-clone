#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

static void scopeLine(int32_t x1, int32_t y1, int32_t y2);

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
		SCOPE_UPDATE_DRAWPOS
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
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingPong_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void scopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
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
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void scopeDrawPingPong_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

/* ----------------------------------------------------------------------- */
/*                   INTERPOLATED SCOPE DRAWING ROUTINES                   */
/* ----------------------------------------------------------------------- */

static void linedScopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_NO_LOOP
	LINED_SCOPE_PREPARE_LERP_SMP8
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_LOOP
	LINED_SCOPE_PREPARE_LERP_SMP8
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_PINGPONG
	LINED_SCOPE_PREPARE_LERP_SMP8_BIDI
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP8_BIDI
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void linedScopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_NO_LOOP
	LINED_SCOPE_PREPARE_LERP_SMP16
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_LOOP
	LINED_SCOPE_PREPARE_LERP_SMP16
	SCOPE_HANDLE_POS_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_PINGPONG
	LINED_SCOPE_PREPARE_LERP_SMP16_BIDI
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_LERP_SMP16_BIDI
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

// -----------------------------------------------------------------------

static void scopeLine(int32_t x1, int32_t y1, int32_t y2)
{
	const int32_t dy = y2 - y1;
	const int32_t sy = SGN(dy);
	const uint32_t color = video.palette[PAL_PATTEXT];
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
