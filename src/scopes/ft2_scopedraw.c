#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_config.h"
#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

/* Pre-computed cosine table at 256 phases (scale = 2^15).
** This is just to make the scopes look smooth at low voice pitches.
**
** Formula:
** for (int32_t i = 0; i < 256; i++)
**     LUT[i] = round(32768.0 * (0.5 - (0.5 * cos(i * (PI / 256.0)))));
**
** Ideally, first value should be 0 and last value should be 32767.
*/
static const int16_t scopeIntrpLUT[SCOPE_INTRP_PHASES] =
{
	    0,     1,     5,    11,    20,    31,    44,    60,    79,   100,
	  123,   149,   177,   208,   241,   277,   315,   355,   398,   443,
	  491,   541,   593,   648,   705,   765,   827,   891,   958,  1027,
	 1098,  1171,  1247,  1325,  1406,  1488,  1573,  1660,  1749,  1841,
	 1935,  2030,  2128,  2229,  2331,  2435,  2542,  2651,  2761,  2874,
	 2989,  3105,  3224,  3345,  3468,  3592,  3719,  3847,  3978,  4110,
	 4244,  4380,  4518,  4657,  4799,  4942,  5087,  5233,  5381,  5531,
	 5682,  5835,  5990,  6146,  6304,  6463,  6624,  6786,  6950,  7115,
	 7282,  7449,  7619,  7789,  7961,  8134,  8308,  8484,  8661,  8839,
	 9018,  9198,  9379,  9561,  9745,  9929, 10114, 10300, 10487, 10676,
	10864, 11054, 11245, 11436, 11628, 11821, 12014, 12208, 12403, 12598,
	12794, 12991, 13188, 13385, 13583, 13781, 13980, 14179, 14378, 14578,
	14778, 14978, 15179, 15379, 15580, 15781, 15982, 16183, 16384, 16585,
	16786, 16987, 17188, 17389, 17589, 17790, 17990, 18190, 18390, 18589,
	18788, 18987, 19185, 19383, 19580, 19777, 19974, 20170, 20365, 20560,
	20754, 20947, 21140, 21332, 21523, 21714, 21904, 22092, 22281, 22468,
	22654, 22839, 23023, 23207, 23389, 23570, 23750, 23929, 24107, 24284,
	24460, 24634, 24807, 24979, 25149, 25319, 25486, 25653, 25818, 25982,
	26144, 26305, 26464, 26622, 26778, 26933, 27086, 27237, 27387, 27535,
	27681, 27826, 27969, 28111, 28250, 28388, 28524, 28658, 28790, 28921,
	29049, 29176, 29300, 29423, 29544, 29663, 29779, 29894, 30007, 30117,
	30226, 30333, 30437, 30539, 30640, 30738, 30833, 30927, 31019, 31108,
	31195, 31280, 31362, 31443, 31521, 31597, 31670, 31741, 31810, 31877,
	31941, 32003, 32063, 32120, 32175, 32227, 32277, 32325, 32370, 32413,
	32453, 32491, 32527, 32560, 32591, 32619, 32645, 32668, 32689, 32708,
	32724, 32737, 32748, 32757, 32763, 32767
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
