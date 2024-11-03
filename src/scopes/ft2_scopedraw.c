#include "../ft2_video.h"
#include "../ft2_palette.h"
#include "../mixer/ft2_gaussian.h"
#include "ft2_scopes.h"
#include "ft2_scopedraw.h"
#include "ft2_scope_macros.h"

/* 15-bit Gaussian interpolation LUT with no overshoot (sum <= 1.0).
** Suitable for tracker scopes.
*/
static const int16_t scopeGaussianLUT[4 * 256] =
{
	 4807,22963, 4871,   -1, 4744,22962, 4935,   -1,
	 4681,22960, 5000,   -1, 4619,22957, 5065,   -1,
	 4557,22953, 5131,   -1, 4495,22948, 5197,   -1,
	 4435,22942, 5264,   -1, 4374,22935, 5332,   -1,
	 4315,22927, 5399,   -1, 4255,22918, 5468,   -1,
	 4197,22908, 5536,   -1, 4138,22897, 5606,   -1,
	 4081,22885, 5676,   -1, 4023,22872, 5746,   -1,
	 3967,22857, 5817,   -1, 3910,22842, 5888,   -1,
	 3855,22826, 5959,    0, 3799,22809, 6032,    0,
	 3745,22791, 6104,    0, 3691,22772, 6177,    0,
	 3637,22752, 6251,    0, 3584,22731, 6325,    0,
	 3531,22709, 6400,    0, 3479,22686, 6475,    1,
	 3427,22662, 6550,    1, 3376,22637, 6626,    1,
	 3325,22611, 6702,    1, 3275,22584, 6779,    2,
	 3225,22556, 6856,    2, 3176,22527, 6934,    2,
	 3128,22498, 7012,    3, 3079,22467, 7091,    3,
	 3032,22435, 7170,    3, 2985,22402, 7249,    4,
	 2938,22369, 7329,    4, 2892,22334, 7409,    5,
	 2846,22299, 7490,    5, 2801,22262, 7571,    6,
	 2756,22225, 7653,    7, 2712,22187, 7735,    7,
	 2668,22148, 7817,    8, 2624,22107, 7900,    9,
	 2582,22066, 7983,    9, 2539,22025, 8066,   10,
	 2497,21982, 8150,   11, 2456,21938, 8234,   12,
	 2415,21893, 8319,   13, 2374,21848, 8404,   14,
	 2334,21801, 8489,   15, 2295,21754, 8575,   16,
	 2256,21706, 8661,   17, 2217,21657, 8748,   18,
	 2179,21607, 8834,   19, 2141,21556, 8922,   21,
	 2104,21505, 9009,   22, 2067,21452, 9097,   24,
	 2031,21399, 9185,   25, 1995,21345, 9273,   27,
	 1959,21290, 9362,   28, 1924,21235, 9451,   30,
	 1890,21178, 9541,   32, 1856,21121, 9630,   33,
	 1822,21063, 9720,   35, 1789,21004, 9811,   37,
	 1756,20944, 9901,   39, 1723,20884, 9992,   41,
	 1691,20822,10083,   44, 1660,20760,10174,   46,
	 1628,20698,10266,   48, 1598,20634,10358,   51,
	 1567,20570,10450,   53, 1537,20505,10542,   56,
	 1508,20439,10635,   58, 1479,20373,10727,   61,
	 1450,20306,10820,   64, 1422,20238,10913,   67,
	 1394,20169,11007,   70, 1366,20100,11100,   73,
	 1339,20030,11194,   77, 1312,19959,11288,   80,
	 1286,19888,11382,   84, 1260,19816,11476,   87,
	 1234,19744,11571,   91, 1209,19671,11665,   95,
	 1184,19597,11760,   99, 1160,19522,11855,  103,
	 1136,19447,11950,  107, 1112,19372,12045,  111,
	 1089,19295,12140,  116, 1066,19219,12236,  120,
	 1043,19141,12331,  125, 1020,19063,12427,  130,
	  999,18985,12522,  135,  977,18905,12618,  140,
	  956,18826,12714,  145,  935,18746,12809,  150,
	  914,18665,12905,  156,  894,18584,13001,  161,
	  874,18502,13097,  167,  854,18420,13193,  173,
	  835,18337,13289,  179,  816,18254,13385,  186,
	  797,18170,13481,  192,  779,18086,13577,  199,
	  761,18001,13673,  205,  743,17916,13769,  212,
	  726,17830,13865,  219,  708,17744,13961,  227,
	  692,17658,14056,  234,  675,17571,14152,  242,
	  659,17484,14248,  250,  643,17396,14343,  257,
	  627,17308,14439,  266,  612,17220,14534,  274,
	  597,17131,14630,  283,  582,17042,14725,  291,
	  567,16953,14820,  300,  553,16863,14915,  309,
	  539,16773,15010,  319,  525,16682,15104,  328,
	  512,16592,15199,  338,  498,16500,15293,  348,
	  485,16409,15387,  358,  473,16317,15481,  369,
	  460,16226,15575,  379,  448,16133,15669,  390,
	  436,16041,15762,  401,  424,15948,15855,  412,
	  412,15855,15948,  424,  401,15762,16041,  436,
	  390,15669,16133,  448,  379,15575,16226,  460,
	  369,15481,16317,  473,  358,15387,16409,  485,
	  348,15293,16500,  498,  338,15199,16592,  512,
	  328,15104,16682,  525,  319,15010,16773,  539,
	  309,14915,16863,  553,  300,14820,16953,  567,
	  291,14725,17042,  582,  283,14630,17131,  597,
	  274,14534,17220,  612,  266,14439,17308,  627,
	  257,14343,17396,  643,  250,14248,17484,  659,
	  242,14152,17571,  675,  234,14056,17658,  692,
	  227,13961,17744,  708,  219,13865,17830,  726,
	  212,13769,17916,  743,  205,13673,18001,  761,
	  199,13577,18086,  779,  192,13481,18170,  797,
	  186,13385,18254,  816,  179,13289,18337,  835,
	  173,13193,18420,  854,  167,13097,18502,  874,
	  161,13001,18584,  894,  156,12905,18665,  914,
	  150,12809,18746,  935,  145,12714,18826,  956,
	  140,12618,18905,  977,  135,12522,18985,  999,
	  130,12427,19063, 1020,  125,12331,19141, 1043,
	  120,12236,19219, 1066,  116,12140,19295, 1089,
	  111,12045,19372, 1112,  107,11950,19447, 1136,
	  103,11855,19522, 1160,   99,11760,19597, 1184,
	   95,11665,19671, 1209,   91,11571,19744, 1234,
	   87,11476,19816, 1260,   84,11382,19888, 1286,
	   80,11288,19959, 1312,   77,11194,20030, 1339,
	   73,11100,20100, 1366,   70,11007,20169, 1394,
	   67,10913,20238, 1422,   64,10820,20306, 1450,
	   61,10727,20373, 1479,   58,10635,20439, 1508,
	   56,10542,20505, 1537,   53,10450,20570, 1567,
	   51,10358,20634, 1598,   48,10266,20698, 1628,
	   46,10174,20760, 1660,   44,10083,20822, 1691,
	   41, 9992,20884, 1723,   39, 9901,20944, 1756,
	   37, 9811,21004, 1789,   35, 9720,21063, 1822,
	   33, 9630,21121, 1856,   32, 9541,21178, 1890,
	   30, 9451,21235, 1924,   28, 9362,21290, 1959,
	   27, 9273,21345, 1995,   25, 9185,21399, 2031,
	   24, 9097,21452, 2067,   22, 9009,21505, 2104,
	   21, 8922,21556, 2141,   19, 8834,21607, 2179,
	   18, 8748,21657, 2217,   17, 8661,21706, 2256,
	   16, 8575,21754, 2295,   15, 8489,21801, 2334,
	   14, 8404,21848, 2374,   13, 8319,21893, 2415,
	   12, 8234,21938, 2456,   11, 8150,21982, 2497,
	   10, 8066,22025, 2539,    9, 7983,22066, 2582,
	    9, 7900,22107, 2624,    8, 7817,22148, 2668,
	    7, 7735,22187, 2712,    7, 7653,22225, 2756,
	    6, 7571,22262, 2801,    5, 7490,22299, 2846,
	    5, 7409,22334, 2892,    4, 7329,22369, 2938,
	    4, 7249,22402, 2985,    3, 7170,22435, 3032,
	    3, 7091,22467, 3079,    3, 7012,22498, 3128,
	    2, 6934,22527, 3176,    2, 6856,22556, 3225,
	    2, 6779,22584, 3275,    1, 6702,22611, 3325,
	    1, 6626,22637, 3376,    1, 6550,22662, 3427,
	    1, 6475,22686, 3479,    0, 6400,22709, 3531,
	    0, 6325,22731, 3584,    0, 6251,22752, 3637,
	    0, 6177,22772, 3691,    0, 6104,22791, 3745,
	    0, 6032,22809, 3799,    0, 5959,22826, 3855,
	   -1, 5888,22842, 3910,   -1, 5817,22857, 3967,
	   -1, 5746,22872, 4023,   -1, 5676,22885, 4081,
	   -1, 5606,22897, 4138,   -1, 5536,22908, 4197,
	   -1, 5468,22918, 4255,   -1, 5399,22927, 4315,
	   -1, 5332,22935, 4374,   -1, 5264,22942, 4435,
	   -1, 5197,22948, 4495,   -1, 5131,22953, 4557,
	   -1, 5065,22957, 4619,   -1, 5000,22960, 4681,
	   -1, 4935,22962, 4744,   -1, 4871,22963, 4807
};

static void scopeLine(int32_t x1, int32_t y1, int32_t y2, uint32_t color);

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
