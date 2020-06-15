#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_video.h"
#include "ft2_palette.h"

/* ----------------------------------------------------------------------- */
/*                          SCOPE DRAWING MACROS                           */
/* ----------------------------------------------------------------------- */

#define SCOPE_REGS_NO_LOOP \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t scopePixelColor = video.palette[PAL_PATTEXT]; \
	const uint32_t drawLen = x + w; \
	int32_t sample; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \

#define SCOPE_REGS_LOOP \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const int32_t SRepS = s->SRepS; \
	const int32_t SRepL = s->SRepL; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t scopePixelColor = video.palette[PAL_PATTEXT]; \
	const uint32_t drawLen = x + w; \
	int32_t sample; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \

#define SCOPE_REGS_PINGPONG \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const int32_t SRepS = s->SRepS; \
	const int32_t SRepL = s->SRepL; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t scopePixelColor = video.palette[PAL_PATTEXT]; \
	const uint32_t drawLen = x + w; \
	int32_t sample; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	int32_t drawPosDir = s->backwards ? -1 : 1; \

#define LINED_SCOPE_REGS_NO_LOOP \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t drawLen = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \

#define LINED_SCOPE_REGS_LOOP \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const int32_t SRepS = s->SRepS; \
	const int32_t SRepL = s->SRepL; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t drawLen = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \

#define LINED_SCOPE_REGS_PINGPONG \
	const int8_t vol = s->SVol; \
	const int32_t SLen = s->SLen; \
	const int32_t SRepS = s->SRepS; \
	const int32_t SRepL = s->SRepL; \
	const uint32_t scopeDrawDelta = s->DFrq; \
	const uint32_t drawLen = (x + w) - 1; \
	int32_t sample; \
	int32_t y1, y2; \
	int32_t scopeDrawPos = s->SPos; \
	int32_t scopeDrawFrac = 0; \
	int32_t drawPosDir = s->backwards ? -1 : 1; \

#define SCOPE_GET_SMP8 \
	if (s->active) \
	{ \
		assert(scopeDrawPos >= 0 && scopeDrawPos < SLen); \
		sample = (s->sampleData8[scopeDrawPos] * vol) >> 8; \
	} \
	else \
	{ \
		sample = 0; \
	} \

#define SCOPE_GET_SMP16 \
	if (s->active) \
	{ \
		assert(scopeDrawPos >= 0 && scopeDrawPos < SLen); \
		sample = (int8_t)((s->sampleData16[scopeDrawPos] * vol) >> 16); \
	} \
	else \
	{ \
		sample = 0; \
	} \

#define SCOPE_UPDATE_DRAWPOS \
	scopeDrawFrac += scopeDrawDelta; \
	scopeDrawPos += scopeDrawFrac >> SCOPE_DRAW_FRAC_BITS; \
	scopeDrawFrac &= SCOPE_DRAW_FRAC_MASK; \

#define SCOPE_UPDATE_DRAWPOS_PINGPONG \
	scopeDrawFrac += scopeDrawDelta; \
	scopeDrawPos += (scopeDrawFrac >> SCOPE_DRAW_FRAC_BITS) * drawPosDir; \
	scopeDrawFrac &= SCOPE_DRAW_FRAC_MASK; \

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
	if (scopeDrawPos >= SLen) \
		s->active = false; \

#define SCOPE_HANDLE_POS_LOOP \
	if (scopeDrawPos >= SLen) \
	{ \
		if (SRepL >= 2) \
			scopeDrawPos = SRepS + ((scopeDrawPos - SLen) % SRepL); \
		else \
			scopeDrawPos = SRepS; \
		\
		assert(scopeDrawPos >= SRepS && scopeDrawPos < SLen); \
	} \

#define SCOPE_HANDLE_POS_PINGPONG \
	if (drawPosDir == -1 && scopeDrawPos < SRepS) \
	{ \
		drawPosDir = 1; /* change direction to forwards */ \
		\
		if (SRepL >= 2) \
			scopeDrawPos = SRepS + ((SRepS - scopeDrawPos - 1) % SRepL); \
		else \
			scopeDrawPos = SRepS; \
		\
		assert(scopeDrawPos >= SRepS && scopeDrawPos < SLen); \
	} \
	else if (scopeDrawPos >= SLen) \
	{ \
		drawPosDir = -1; /* change direction to backwards */ \
		\
		if (SRepL >= 2) \
			scopeDrawPos = (SLen - 1) - ((scopeDrawPos - SLen) % SRepL); \
		else \
			scopeDrawPos = SLen - 1; \
		\
		assert(scopeDrawPos >= SRepS && scopeDrawPos < SLen); \
	} \
	assert(scopeDrawPos >= 0); \

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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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

	for (; x < drawLen; x++)
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
