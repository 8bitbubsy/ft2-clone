#include <stdio.h> // sprintf()
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_bmp.h"
#include "ft2_video.h"
#include "ft2_structs.h"

#define NUM_STARS 3000
#define ALPHA_FADE_MILLISECS 2000 /* amount of milliseconds until content is fully faded in */
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167
#define ABOUT_LOGO_W 449
#define ABOUT_LOGO_H 110
#define ABOUT_TEXT_W 349
#define ABOUT_TEXT_H 29

typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

static char *customText1 = "Clone by Olav \"8bitbubsy\" S\025rensen";
static char *customText2 = "https://16-bits.org";
static char customText3[256];
static int16_t customText1Y, customText2Y, customText3Y, customText1X, customText2X, customText3X;
static uint32_t logoTimer, alphaValue, randSeed, frameCounter;
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
#define MY_PI_FLOAT 3.141592653589793f

	const float xx = rotation.x * MY_PI_FLOAT;
	const float sa = sinf(xx);
	const float ca = cosf(xx);

	const float yy = rotation.y * MY_PI_FLOAT;
	const float sb = sinf(yy);
	const float cb = cosf(yy);

	const float zz = rotation.z * MY_PI_FLOAT;
	const float sc = sinf(zz);
	const float cc = cosf(zz);

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

static void aboutInit(void)
{
	vector_t *s = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, s++)
	{
		s->x = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->y = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->z = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
	}

	rotation.x = rotation.y = rotation.z = 0.0f;
	alphaValue = 0;
	frameCounter = 0;
	logoTimer = 0;
}

static void blendPixel(int32_t x, int32_t y, int32_t r, int32_t g, int32_t b, int32_t alpha)
{
	uint32_t *p = &video.frameBuffer[(y * SCREEN_W) + x];

	const uint32_t srcPixel = *p;

	const int32_t srcR = RGB32_R(srcPixel);
	const int32_t srcG = RGB32_G(srcPixel);
	const int32_t srcB = RGB32_B(srcPixel);

	r = ((srcR * (65536-alpha)) + (r * alpha)) >> 16;
	g = ((srcG * (65536-alpha)) + (g * alpha)) >> 16;
	b = ((srcB * (65536-alpha)) + (b * alpha)) >> 16;

	*p = RGB32(r, g, b);
}

static void starfield(void)
{
	vector_t *star = starPoints;
	for (int16_t i = 0; i < NUM_STARS; i++, star++)
	{
		star->z += 0.00015f;
		if (star->z >= 0.5f)
			star->z -= 1.0f;

		const float z = (matrix.x.z * star->x) + (matrix.y.z * star->y) + (matrix.z.z * star->z) + 0.5f;
		if (z <= 0.0f)
			continue;

		float y = (ABOUT_SCREEN_H/2.0f) + ((((matrix.x.y * star->x) + (matrix.y.y * star->y) + (matrix.z.y * star->z)) / z) * 400.0f);
		const int32_t outY = (int32_t)y;
		if (outY < 3 || outY >= 3+ABOUT_SCREEN_H)
			continue;

		float x = (ABOUT_SCREEN_W/2.0f) + ((((matrix.x.x * star->x) + (matrix.y.x * star->y) + (matrix.z.x * star->z)) / z) * 400.0f);

		const int32_t outX = (int32_t)x;
		if (outX < 3 || outX >= 3+ABOUT_SCREEN_W)
			continue;

		int32_t d = (int32_t)(z * 255.0f);
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

		const int32_t sidesAlpha = 13000;

		if (outX-1 >= 3)
			blendPixel(outX-1, outY, r, g, b, sidesAlpha);

		if (outX+1 < 3+ABOUT_SCREEN_W)
			blendPixel(outX+1, outY, r, g, b, sidesAlpha);

		if (outY-1 >= 3)
			blendPixel(outX, outY-1, r, g, b, sidesAlpha);

		if (outY+1 < 3+ABOUT_SCREEN_H)
			blendPixel(outX, outY+1, r, g, b, sidesAlpha);

		// plot main star pixel
		blendPixel(outX, outY, r, g, b, 60000);
	}
}

void aboutFrame(void) // called every frame when the about screen is shown
{
	clearRect(3, 3, ABOUT_SCREEN_W, ABOUT_SCREEN_H);

	// 3D starfield
	rotateMatrix();
	starfield();
	rotation.x -= 0.00011f;
	rotation.z += 0.00006f;

	// logo + text
	blit32Alpha(91, 31, bmp.ft2AboutLogo, ABOUT_LOGO_W, ABOUT_LOGO_H, alphaValue);
	textOutAlpha(customText1X, customText1Y, PAL_FORGRND, customText1, alphaValue);
	textOutAlpha(customText2X, customText2Y, PAL_FORGRND, customText2, alphaValue);
	textOutAlpha(customText3X, customText3Y, PAL_FORGRND, customText3, alphaValue);

	if (logoTimer > (int32_t)(VBLANK_HZ/4.0))
	{
		alphaValue += (uint32_t)((65536.0 / (ALPHA_FADE_MILLISECS / (1000.0 / VBLANK_HZ))) + 0.5);
		if (alphaValue > 65536)
			alphaValue = 65536;
	}
	else
	{
		logoTimer++;
	}

	// the exit button has to be redrawn since it gets overwritten :)
	showPushButton(PB_EXIT_ABOUT);
}

void showAboutScreen(void) // called once when about screen is opened
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_EXIT_ABOUT);

	sprintf(customText3, "v%s (%s)", PROG_VER_STR, __DATE__);
	customText1X = (SCREEN_W - textWidth(customText1)) / 2;
	customText2X = (SCREEN_W-8) - textWidth(customText2);
	customText3X = (SCREEN_W-8) - textWidth(customText3);
	customText1Y = 157;
	customText2Y = 157-12;
	customText3Y = 157;

	aboutInit();
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
