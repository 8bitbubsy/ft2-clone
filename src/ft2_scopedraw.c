#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_video.h"
#include "ft2_palette.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS_NO_LOOP \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \

#define SCOPE_REGS_LOOP \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \

#define SCOPE_REGS_PINGPONG \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t color = video.palette[PAL_PATTEXT]; \
	const uint32_t width = x + w; \
	int32_t sample; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \
	int32_t direction = s->direction; \

#define LINED_SCOPE_REGS_NO_LOOP \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t width = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \

#define LINED_SCOPE_REGS_LOOP \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t width = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \

#define LINED_SCOPE_REGS_PINGPONG \
	const int32_t vol = s->vol; \
	const int32_t end = s->end; \
	const int32_t loopStart = s->loopStart; \
	const int32_t loopLength = s->loopLength; \
	const uint32_t delta = s->drawDelta; \
	const uint32_t width = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t pos = s->pos; \
	int32_t posFrac = 0; \
	int32_t direction = s->direction; \

#define SCOPE_GET_SMP8 \
	if (s->active) \
	{ \
		assert(pos >= 0 && pos < end); \
		sample = (s->base8[pos] * vol) >> 8; \
	} \
	else \
	{ \
		sample = 0; \
	} \

#define SCOPE_GET_SMP16 \
	if (s->active) \
	{ \
		assert(pos >= 0 && pos < end); \
		sample = (int8_t)((s->base16[pos] * vol) >> 16); \
	} \
	else \
	{ \
		sample = 0; \
	} \

#define SCOPE_UPDATE_DRAWPOS \
	posFrac += delta; \
	pos += posFrac >> SCOPE_DRAW_FRAC_BITS; \
	posFrac &= SCOPE_DRAW_FRAC_MASK; \

#define SCOPE_UPDATE_DRAWPOS_PINGPONG \
	posFrac += delta; \
	pos += (int32_t)(posFrac >> SCOPE_DRAW_FRAC_BITS) * direction; \
	posFrac &= SCOPE_DRAW_FRAC_MASK; \

#define SCOPE_DRAW_SMP \
	video.frameBuffer[((lineY - sample) * SCREEN_W) + x] = color;

#define LINED_SCOPE_PREPARE_SMP8 \
	SCOPE_GET_SMP8 \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_PREPARE_SMP16 \
	SCOPE_GET_SMP16 \
	y1 = lineY - sample; \
	SCOPE_UPDATE_DRAWPOS

#define LINED_SCOPE_DRAW_SMP \
	y2 = lineY - sample; \
	scopeLine(x, y1, y2); \
	y1 = y2; \

#define SCOPE_HANDLE_POS_NO_LOOP \
	if (pos >= end) \
		s->active = false; \

#define SCOPE_HANDLE_POS_LOOP \
	if (pos >= end) \
	{ \
		if (loopLength >= 2) \
			pos = loopStart + ((pos - end) % loopLength); \
		else \
			pos = loopStart; \
		\
		assert(pos >= loopStart && pos < end); \
	} \

#define SCOPE_HANDLE_POS_PINGPONG \
	if (direction == -1 && pos < loopStart) \
	{ \
		direction = 1; /* change direction to forwards */ \
		\
		if (loopLength >= 2) \
			pos = loopStart + ((loopStart - pos - 1) % loopLength); \
		else \
			pos = loopStart; \
		\
		assert(pos >= loopStart && pos < end); \
	} \
	else if (pos >= end) \
	{ \
		direction = -1; /* change direction to backwards */ \
		\
		if (loopLength >= 2) \
			pos = (end - 1) - ((pos - end) % loopLength); \
		else \
			pos = end - 1; \
		\
		assert(pos >= loopStart && pos < end); \
	} \
	assert(pos >= 0); \

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
/*                      LINED SCOPE DRAWING ROUTINES                       */
/* ----------------------------------------------------------------------- */

static void linedScopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_NO_LOOP
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
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
		SCOPE_GET_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_PINGPONG
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void linedScopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_NO_LOOP
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
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
		SCOPE_GET_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_LOOP
	}
}

static void linedScopeDrawPingPong_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS_PINGPONG
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_PINGPONG

	for (; x < width; x++)
	{
		SCOPE_GET_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
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
