#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_video.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS \
	int32_t sample; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	uint32_t scopePixelColor = video.palette[PAL_PATTEXT]; \
	uint32_t len = x + w; \

#define SCOPE_REGS_PINGPONG \
	int32_t sample; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	int32_t drawPosDir = s->SPosDir; \
	uint32_t scopePixelColor = video.palette[PAL_PATTEXT]; \
	uint32_t len = x + w; \

#define LINED_SCOPE_REGS \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	uint32_t len = (x + w) - 1; \

#define LINED_SCOPE_REGS_PINGPONG \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	int32_t drawPosDir = s->SPosDir; \
	uint32_t len = (x + w) - 1; \

#define SCOPE_GET_SMP8 \
	if (!s->active) \
	{ \
		sample = 0; \
	} \
	else \
	{ \
		assert(scopeDrawPos >= 0 && scopeDrawPos < s->SLen); \
		sample = (s->sampleData8[scopeDrawPos] * s->SVol) >> 8; \
	} \

#define SCOPE_GET_SMP16 \
	if (!s->active) \
	{ \
		sample = 0; \
	} \
	else \
	{ \
		assert(scopeDrawPos >= 0 && scopeDrawPos < s->SLen); \
		sample = (int8_t)((s->sampleData16[scopeDrawPos] * s->SVol) >> 16); \
	} \

#define SCOPE_UPDATE_DRAWPOS \
	scopeDrawFrac += s->SFrq >> 6; \
	scopeDrawPos += scopeDrawFrac >> 16; \
	scopeDrawFrac &= 0xFFFF; \

#define SCOPE_UPDATE_DRAWPOS_PINGPONG \
	scopeDrawFrac += s->SFrq >> 6; \
	scopeDrawPos += (scopeDrawFrac >> 16) * drawPosDir; \
	scopeDrawFrac &= 0xFFFF; \

#define SCOPE_DRAW_SMP \
	video.frameBuffer[((lineY - sample) * SCREEN_W) + x] = scopePixelColor;

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
	if (scopeDrawPos >= s->SLen) \
		s->active = false; \

#define SCOPE_HANDLE_POS_LOOP \
	if (scopeDrawPos >= s->SLen) \
	{ \
		if (s->SRepL < 2) \
			scopeDrawPos = s->SRepS; \
		else \
			scopeDrawPos = s->SRepS + ((scopeDrawPos - s->SLen) % s->SRepL); \
		\
		assert(scopeDrawPos >= s->SRepS && scopeDrawPos < s->SLen); \
	} \

#define SCOPE_HANDLE_POS_PINGPONG \
	if (drawPosDir == -1 && scopeDrawPos < s->SRepS) \
	{ \
		drawPosDir = 1; /* change direction to forwards */ \
		\
		if (s->SRepL < 2) \
			scopeDrawPos = s->SRepS; \
		else \
			scopeDrawPos = s->SRepS + ((s->SRepS - scopeDrawPos - 1) % s->SRepL); \
		\
		assert(scopeDrawPos >= s->SRepS && scopeDrawPos < s->SLen); \
	} \
	else if (scopeDrawPos >= s->SLen) \
	{ \
		drawPosDir = -1; /* change direction to backwards */ \
		\
		if (s->SRepL < 2) \
			scopeDrawPos = s->SLen - 1; \
		else \
			scopeDrawPos = (s->SLen - 1) - ((scopeDrawPos - s->SLen) % s->SRepL); \
		\
		assert(scopeDrawPos >= s->SRepS && scopeDrawPos < s->SLen); \
	} \
	assert(scopeDrawPos >= 0); \

static void scopeLine(int32_t x1, int32_t y1, int32_t y2)
{
	int32_t pitch, d, sy, dy;
	uint32_t ay, pixVal, *dst32;

	dy = y2 - y1;
	ay = ABS(dy);
	sy = SGN(dy);

	pixVal = video.palette[PAL_PATTEXT];
	pitch = sy * SCREEN_W;

	dst32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];
	*dst32 = pixVal;

	if (ay <= 1)
	{
		if (ay != 0)
			dst32 += pitch;

		*++dst32 = pixVal;
		return;
	}

	d = 2 - ay;

	ay *= 2;
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
		*dst32 = pixVal;
	}
}

/* ----------------------------------------------------------------------- */
/*                         SCOPE DRAWING ROUTINES                          */
/* ----------------------------------------------------------------------- */

static void scopeDrawNoLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS

	for (; x < len; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void scopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS

	for (; x < len; x++)
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

	for (; x < len; x++)
	{
		SCOPE_GET_SMP8
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void scopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS

	for (; x < len; x++)
	{
		SCOPE_GET_SMP16
		SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void scopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	SCOPE_REGS

	for (; x < len; x++)
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

	for (; x < len; x++)
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
	LINED_SCOPE_REGS
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < len; x++)
	{
		SCOPE_GET_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_8bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS
	LINED_SCOPE_PREPARE_SMP8
	SCOPE_HANDLE_POS_LOOP

	for (; x < len; x++)
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

	for (; x < len; x++)
	{
		SCOPE_GET_SMP8
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS_PINGPONG
		SCOPE_HANDLE_POS_PINGPONG
	}
}

static void linedScopeDrawNoLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_NO_LOOP

	for (; x < len; x++)
	{
		SCOPE_GET_SMP16
		LINED_SCOPE_DRAW_SMP
		SCOPE_UPDATE_DRAWPOS
		SCOPE_HANDLE_POS_NO_LOOP
	}
}

static void linedScopeDrawLoop_16bit(scope_t *s, uint32_t x, uint32_t lineY, uint32_t w)
{
	LINED_SCOPE_REGS
	LINED_SCOPE_PREPARE_SMP16
	SCOPE_HANDLE_POS_LOOP

	for (; x < len; x++)
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

	for (; x < len; x++)
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
