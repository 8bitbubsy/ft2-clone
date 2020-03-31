#include <stdio.h> // sprintf()
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_bmp.h"
#include "ft2_video.h"
#include "ft2_tables.h"

// ported from original FT2 code

#define NUM_STARS 1000
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167
#define ABOUT_LOGO_W 449
#define ABOUT_LOGO_H 111
#define ABOUT_TEXT_W 349
#define ABOUT_TEXT_H 29

typedef struct
{
	int16_t x, y, z;
} vector_t;

typedef struct
{
	uint16_t x, y, z;
} rotate_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

static const uint8_t starColConv[24] = { 2,2,2,2,2,2,2,2, 2,2,2,1,1,1,3,3, 3,3,3,3,3,3,3,3 };
static const int16_t *sin32767 = sinusTables, *cos32767 = &sinusTables[256];
static int16_t hastighet;
static int32_t lastStarScreenPos[NUM_STARS];
static uint32_t randSeed;
static vector_t starcrd[NUM_STARS];
static rotate_t star_a;
static matrix_t starmat;

void seedAboutScreenRandom(uint32_t newseed)
{
	randSeed = newseed;
}

static inline int32_t random32(int32_t l) // Turbo Pascal Random() implementation
{
	int32_t r;

	randSeed *= 134775813;
	randSeed += 1;

	r = ((int64_t)randSeed * l) >> 32;
	return r;
}

static void fixaMatris(rotate_t a, matrix_t *mat)
{
	int16_t sa, sb, sc, ca, cb, cc;

	sa = sin32767[a.x >> 6];
	ca = cos32767[a.x >> 6];
	sb = sin32767[a.y >> 6];
	cb = cos32767[a.y >> 6];
	sc = sin32767[a.z >> 6];
	cc = cos32767[a.z >> 6];

	mat->x.x = ((ca * cc) >> 16) + ((sc * ((sa * sb) >> 16)) >> (16-1));
	mat->y.x = (sa * cb) >> 16;
	mat->z.x = ((cc * ((sa * sb) >> 16)) >> (16-1)) - ((ca * sc) >> 16);

	mat->x.y = ((sc * ((ca * sb) >> 16)) >> (16-1)) - ((sa * cc) >> 16);
	mat->y.y = (ca * cb) >> 16;
	mat->z.y = ((sa * sc) >> 16) + ((cc * ((ca * sb) >> 16)) >> (16-1));

	mat->x.z = (cb * sc) >> 16;
	mat->y.z = 0 - (sb >> 1);
	mat->z.z = (cb * cc) >> 16;
}

static inline int32_t sqr(int32_t x)
{
	return x * x;
}

static void aboutInit(void)
{
	uint8_t type;
	int16_t i;
	int32_t r, n, w, h;
	double ww;

	type = (uint8_t)random32(4);
	switch (type)
	{
		case 0:
		{
			hastighet = 309;
			for (i = 0; i < NUM_STARS; i++)
			{
				starcrd[i].z = (int16_t)random32(0xFFFF) - 0x8000;
				starcrd[i].y = (int16_t)random32(0xFFFF) - 0x8000;
				starcrd[i].x = (int16_t)random32(0xFFFF) - 0x8000;
			}
		}
		break;

		case 1:
		{
			hastighet = 0;
			for (i = 0; i < NUM_STARS; i++)
			{
				if (i < NUM_STARS/4)
				{
					starcrd[i].z = (int16_t)random32(0xFFFF) - 0x8000;
					starcrd[i].y = (int16_t)random32(0xFFFF) - 0x8000;
					starcrd[i].x = (int16_t)random32(0xFFFF) - 0x8000;
				}
				else
				{
					r = random32(30000);
					n = random32(5);
					w = ((2 * random32(2)) - 1) * sqr(random32(1000));
					ww = (((M_PI * 2.0) / 5.0) * n) + (r / 12000.0) + (w / 3000000.0);
					h = ((sqr(r) / 30000) * (random32(10000) - 5000)) / 12000;

					starcrd[i].x = (int16_t)(r * cos(ww));
					starcrd[i].y = (int16_t)(r * sin(ww));
					starcrd[i].z = (int16_t)h;
				}
			}
		}
		break;

		case 2:
		case 3:
		{
			hastighet = 0;
			for (i = 0; i < NUM_STARS; i++)
			{
				r = (int32_t)round(sqrt(random32(500) * 500));
				w = random32(3000);
				h = cos32767[(((w * 8) + r) / 16) & 1023] / 4;

				starcrd[i].z = (int16_t)((cos32767[w & 1023] * (w + r)) / 3500);
				starcrd[i].y = (int16_t)((sin32767[w & 1023] * (w + r)) / 3500);
				starcrd[i].x = (int16_t)((h * r) / 500);
			}
		}
		break;

		default:
			break;
	}

	star_a.x = 0;
	star_a.y = 748;
	star_a.z = 200;

	for (i = 0; i < NUM_STARS; i++)
		lastStarScreenPos[i] = -1;
}

static void realStars(void)
{
	uint8_t col;
	int16_t z, xx, xy, xz, yx, yy, yz, zx, zy, zz;
	int32_t x, y, zMul, screenBufferPos;
	vector_t *star;

	xx = starmat.x.x; xy = starmat.x.y; xz = starmat.x.z;
	yx = starmat.y.x; yy = starmat.y.y; yz = starmat.y.z;
	zx = starmat.z.x; zy = starmat.z.y; zz = starmat.z.z;

	for (int16_t i = 0; i < NUM_STARS; i++)
	{
		// erase last star pixel
		screenBufferPos = lastStarScreenPos[i];
		if (screenBufferPos >= 0)
		{
			if (!(video.frameBuffer[screenBufferPos] & 0xFF000000))
				video.frameBuffer[screenBufferPos] = video.palette[PAL_BCKGRND];

			lastStarScreenPos[i] = -1;
		}

		star = &starcrd[i];
		star->z += hastighet;

		z = ((xz * star->x) + (yz * star->y) + (zz * star->z)) >> 16;
		z += 9000;
		if (z <= 100) continue;
		zMul = 0xFFFFFFFF / z; // 8bitbubsy: optimization
		
		y = ((xy * star->x) + (yy * star->y) + (zy * star->z)) >> (16-7);
		y = ((int64_t)y * zMul) >> 32;
		y += (2+ABOUT_SCREEN_H)/2;
		if ((uint32_t)y > 2+ABOUT_SCREEN_H) continue;

		x = ((xx * star->x) + (yx * star->y) + (zx * star->z)) >> (16-7);
		x += x >> 2; // x *= 1.25
		x = ((int64_t)x * zMul) >> 32;
		x += (2+ABOUT_SCREEN_W)/2;
		if ((uint32_t)x > 2+ABOUT_SCREEN_W) continue;

		// render star pixel if the pixel under it is the background
		screenBufferPos = ((uint32_t)y * SCREEN_W) + (uint32_t)x;
		if ((video.frameBuffer[screenBufferPos] >> 24) == PAL_BCKGRND)
		{
			col = ((uint8_t)~(z >> 8) >> 3) - 14;
			if (col < 24)
			{
				video.frameBuffer[screenBufferPos] = video.palette[starColConv[col]] & 0x00FFFFFF;
				lastStarScreenPos[i] = screenBufferPos;
			}
		}
	}
}

void aboutFrame(void)
{
	star_a.x += 3*64;
	star_a.y += 2*64;
	star_a.z -= 1*64;

	fixaMatris(star_a, &starmat);
	realStars();
}

extern uint32_t *unpackedData;

void showAboutScreen(void) // called once when About screen is opened
{
#define TEXT_BORDER_COL 0x2E2E2E

	const char *infoString = "Clone by Olav \"8bitbubsy\" S\025rensen - https://16-bits.org";
	char verText[32];
	uint16_t x, y;

	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_EXIT_ABOUT);

	blit32(91, 31, bmp.ft2AboutLogo, ABOUT_LOGO_W, ABOUT_LOGO_H);

	setCustomPalColor(TEXT_BORDER_COL); // sets PAL_CUSTOM

	x = 5 + (SCREEN_W - textWidth(infoString)) / 2;
	y = 147;
	textOutBorder(x, y, PAL_FORGRND, PAL_CUSTOM, infoString);

	sprintf(verText, "v%s (compiled on %s)", PROG_VER_STR, __DATE__);
	x = ((3 + ABOUT_SCREEN_W) - textWidth(verText)) / 2;
	y = (3 + ABOUT_SCREEN_H) - ((FONT1_CHAR_H - 2) + 3);
	textOutBorder(x, y, PAL_FORGRND, PAL_CUSTOM, verText);

	aboutInit();

	editor.ui.aboutScreenShown = true;
}

void hideAboutScreen(void)
{
	hidePushButton(PB_EXIT_ABOUT);
	editor.ui.aboutScreenShown = false;
}

void exitAboutScreen(void)
{
	hideAboutScreen();
	showTopScreen(true);
}
