#include <stdio.h> // sprintf()
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_bmp.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_gfxdata.h"
#include "ft2_pattern_ed.h" // exitPatternEditorExtended()

#define LOGO_ALPHA_PERCENTAGE 70
#define SINUS_PHASES 1024
#define NUM_STARS 2000
#define ABOUT_SCREEN_X 3
#define ABOUT_SCREEN_Y 3
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167
#define ABOUT_LOGO_W 449
#define ABOUT_LOGO_H 75

typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

static char *customText0 = "Original FT2 by Magnus \"Vogue\" H\224gdahl & Fredrik \"Mr.H\" Huss";
static char *customText1 = "Clone by Olav \"8bitbubsy\" S\025rensen";
static char *customText2 = "https://16-bits.org";
static char customText3[256];
static int16_t customText0X, customText0Y, customText1Y, customText2Y;
static int16_t customText3Y, customText1X, customText2X, customText3X;
static int16_t sin16[SINUS_PHASES];
static uint16_t logoAlpha16;
static uint32_t randSeed, sinp1, sinp2;
static vector_t starPoints[NUM_STARS], rotation;
static matrix_t matrix;

void seedAboutScreenRandom(uint32_t newseed)
{
	randSeed = newseed;
}

static int32_t random32(void)
{
	randSeed *= 134775813;
	randSeed += 1;
	return randSeed;
}

static void rotateMatrix(void)
{
#define MY_2PI_FLOAT 6.2831853071796f

	const float x2pi = rotation.x * MY_2PI_FLOAT;
	const float sa = sinf(x2pi);
	const float ca = cosf(x2pi);

	const float y2pi = rotation.y * MY_2PI_FLOAT;
	const float sb = sinf(y2pi);
	const float cb = cosf(y2pi);

	const float z2pi = rotation.z * MY_2PI_FLOAT;
	const float sc = sinf(z2pi);
	const float cc = cosf(z2pi);

	// x
	matrix.x.x = (ca * cc) + (sc * sa * sb);
	matrix.y.x = sa * cb;
	matrix.z.x = (cc * sa * sb) - (ca * sc);

	// y
	matrix.x.y = (sc * ca * sb) - (sa * cc);
	matrix.y.y = ca * cb;
	matrix.z.y = (sa * sc) + (cc * ca * sb);

	// z
	matrix.x.z = cb * sc;
	matrix.y.z = -sb;
	matrix.z.z = cb * cc;
}

void initAboutScreen(void)
{
	vector_t *s = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, s++)
	{
		s->x = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->y = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->z = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
	}

	// pre-calc sinus table
	for (int32_t i = 0; i < SINUS_PHASES; i++)
		sin16[i] = (int16_t)round(32767.0 * sin(i * M_PI * 2.0 / SINUS_PHASES));

	sinp1 = 0;
	sinp2 = SINUS_PHASES/4; // cosine offset
	logoAlpha16 = (65535 * LOGO_ALPHA_PERCENTAGE) / 100;
}

static void blendPixel(int32_t x, int32_t y, uint32_t r, uint32_t g, uint32_t b, uint16_t alpha)
{
	uint32_t *p = &video.frameBuffer[(y * SCREEN_W) + x];

	const uint32_t srcPixel = *p;

	const uint32_t srcR = RGB32_R(srcPixel);
	const uint32_t srcG = RGB32_G(srcPixel);
	const uint32_t srcB = RGB32_B(srcPixel);

	r = ((srcR * (alpha ^ 65535)) + (r * alpha)) >> 16;
	g = ((srcG * (alpha ^ 65535)) + (g * alpha)) >> 16;
	b = ((srcB * (alpha ^ 65535)) + (b * alpha)) >> 16;

	*p = RGB32(r, g, b);
}

static void starfield(void)
{
	vector_t *star = starPoints;
	for (int16_t i = 0; i < NUM_STARS; i++, star++)
	{
		star->z += 0.0001f;
		if (star->z >= 0.5f)
			star->z -= 1.0f;

		const float z = (matrix.x.z * star->x) + (matrix.y.z * star->y) + (matrix.z.z * star->z) + 0.5f;
		if (z <= 0.0f)
			continue;

		float y = (((matrix.x.y * star->x) + (matrix.y.y * star->y) + (matrix.z.y * star->z)) / z) * 400.0f;
		const int32_t outY = (ABOUT_SCREEN_Y+(ABOUT_SCREEN_H/2)) + (int32_t)y;
		if (outY < ABOUT_SCREEN_Y || outY >= ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
			continue;

		float x = (((matrix.x.x * star->x) + (matrix.y.x * star->y) + (matrix.z.x * star->z)) / z) * 400.0f;
		const int32_t outX = (ABOUT_SCREEN_X+(ABOUT_SCREEN_W/2)) + (int32_t)x;
		if (outX < ABOUT_SCREEN_X || outX >= ABOUT_SCREEN_X+ABOUT_SCREEN_W)
			continue;

		int32_t d = (int32_t)(z * 256.0f);
		if (d > 255)
			d = 255;
		d ^= 255;

		// add a tint of blue to the star pixel

		int32_t r = d - 79;
		if (r < 0)
			r = 0;

		int32_t g = d - 38;
		if (g < 0)
			g = 0;

		int32_t b = d + 64;
		if (b > 255)
			b = 255;

		// blend sides of star

		const uint16_t sidesAlpha = 13000;

		if (outX-1 >= ABOUT_SCREEN_X)
			blendPixel(outX-1, outY, r, g, b, sidesAlpha);

		if (outX+1 < ABOUT_SCREEN_X+ABOUT_SCREEN_W)
			blendPixel(outX+1, outY, r, g, b, sidesAlpha);

		if (outY-1 >= ABOUT_SCREEN_Y)
			blendPixel(outX, outY-1, r, g, b, sidesAlpha);

		if (outY+1 < ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
			blendPixel(outX, outY+1, r, g, b, sidesAlpha);

		// plot main star pixel
		blendPixel(outX, outY, r, g, b, 60000);
	}
}

static uint32_t blendPixelAB(uint32_t pixelA, uint32_t pixelB, uint16_t alpha)
{
	const int32_t r = ((RGB32_R(pixelA) * (alpha ^ 65535)) + (RGB32_R(pixelB) * alpha)) >> 16;
	const int32_t g = ((RGB32_G(pixelA) * (alpha ^ 65535)) + (RGB32_G(pixelB) * alpha)) >> 16;
	const int32_t b = ((RGB32_B(pixelA) * (alpha ^ 65535)) + (RGB32_B(pixelB) * alpha)) >> 16;
	return RGB32(r, g, b);
}

void renderAboutScreenFrame(void)
{
	// remember when you couldn't do this per frame?
	clearRect(ABOUT_SCREEN_X, ABOUT_SCREEN_Y, ABOUT_SCREEN_W, ABOUT_SCREEN_H);

	// 3D starfield
	rotateMatrix();
	rotation.x -= 0.0003f;
	rotation.y -= 0.0002f;
	rotation.z += 0.0001f;
	starfield();

	// waving FT2 logo

	uint32_t *dstPtr = video.frameBuffer + (ABOUT_SCREEN_Y * SCREEN_W) + ABOUT_SCREEN_X;
	for (int32_t y = 0; y < ABOUT_SCREEN_H; y++, dstPtr += SCREEN_W)
	{
		for (int32_t x = 0; x < ABOUT_SCREEN_W; x++)
		{
			int32_t srcX = (x - ((ABOUT_SCREEN_W-ABOUT_LOGO_W)/2)) + (sin16[(sinp1+x)     & (SINUS_PHASES-1)] >> 10);
			int32_t srcY = (y - ((ABOUT_SCREEN_H-ABOUT_LOGO_H)/2)+20) + (sin16[(sinp2+y+x+x) & (SINUS_PHASES-1)] >> 11);

			if ((uint32_t)srcX < ABOUT_LOGO_W && (uint32_t)srcY < ABOUT_LOGO_H)
			{
				const uint32_t logoPixel = bmp.ft2AboutLogo[(srcY * ABOUT_LOGO_W) + srcX];
				if (logoPixel != 0x00FF00) // transparency
					dstPtr[x] = blendPixelAB(dstPtr[x], logoPixel, logoAlpha16);
			}
		}
	}

	sinp1 = (sinp1 + 2) & (SINUS_PHASES-1);
	sinp2 = (sinp2 + 3) & (SINUS_PHASES-1);

	// static texts
	textOut(customText0X, customText0Y, PAL_FORGRND, customText0);
	textOut(customText1X, customText1Y, PAL_FORGRND, customText1);
	textOut(customText2X, customText2Y, PAL_FORGRND, customText2);
	textOut(customText3X, customText3Y, PAL_FORGRND, customText3);

	showPushButton(PB_EXIT_ABOUT); // yes, we have to redraw the exit button per frame :)
}

void showAboutScreen(void) // called once when about screen is opened
{
	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_EXIT_ABOUT);

	sprintf(customText3, "v%s (%s)", PROG_VER_STR, __DATE__);
	customText0X = (SCREEN_W    - textWidth(customText0)) / 2;
	customText1X = (SCREEN_W    - textWidth(customText1)) / 2;
	customText2X = (SCREEN_W-8) - textWidth(customText2);
	customText3X = (SCREEN_W-8) - textWidth(customText3);
	customText0Y = 157-28;
	customText1Y = 157-12;
	customText2Y = 157-12;
	customText3Y = 157;

	ui.aboutScreenShown = true;
}

void hideAboutScreen(void)
{
	hidePushButton(PB_EXIT_ABOUT);
	ui.aboutScreenShown = false;
}

void exitAboutScreen(void)
{
	hideAboutScreen();
	showTopScreen(true);
}
