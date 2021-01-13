#include <stdio.h> // sprintf()
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_bmp.h"
#include "ft2_video.h"
#include "ft2_structs.h"

#define NUM_STARS 1750
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
static char customText2[64];

static int16_t customText1X, customText2X, customTextY;
static int32_t lastStarScreenPos[NUM_STARS], starfieldFade, logoFade;
static uint32_t randSeed, frameCounter;
static float f2pi;
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
	const float xx = rotation.x * f2pi;
	const float sa = sinf(xx);
	const float ca = cosf(xx);

	const float yy = rotation.y * f2pi;
	const float sb = sinf(yy);
	const float cb = cosf(yy);

	const float zz = rotation.z * f2pi;
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
	matrix.y.z = 0.0f - sb;
	matrix.z.z = cb * cc;
}

static void aboutInit(void)
{
	f2pi = (float)(2.0 * 4.0 * atan(1.0)); // M_PI can not be trusted
	
	vector_t *s = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, s++)
	{
		s->x = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->y = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
		s->z = (float)(random32() * (1.0 / (UINT32_MAX+1.0)));
	}

	rotation.x = rotation.y = rotation.z = 0.0f;
	for (int32_t i = 0; i < NUM_STARS; i++)
		lastStarScreenPos[i] = -1;
}

static void starfield(void)
{
	vector_t *star = starPoints;
	for (int16_t i = 0; i < NUM_STARS; i++, star++)
	{
		// erase last star pixel
		int32_t screenBufferPos = lastStarScreenPos[i];
		if (screenBufferPos >= 0)
		{
			if (!(video.frameBuffer[screenBufferPos] & 0xFF000000))
				video.frameBuffer[screenBufferPos] = video.palette[PAL_BCKGRND];

			lastStarScreenPos[i] = -1;
		}

		star->z += 0.00015f;
		if (star->z >= 0.5f) star->z -= 1.0f;

		const float z = (matrix.x.z * star->x) + (matrix.y.z * star->y) + (matrix.z.z * star->z) + 0.5f;
		if (z <= 0.0f)
			continue;

		float y = (((matrix.x.y * star->x) + (matrix.y.y * star->y) + (matrix.z.y * star->z)) / z) * 400.0f;
		y += 2.0f + (ABOUT_SCREEN_H/2.0f);

		const int32_t outY = (int32_t)(y + 0.5f); // rounded
		if ((uint32_t)outY > 2+ABOUT_SCREEN_H)
			continue;

		float x = (((matrix.x.x * star->x) + (matrix.y.x * star->y) + (matrix.z.x * star->z)) / z) * 400.0f;
		x += 2.0f + (ABOUT_SCREEN_W/2.0f);

		const int32_t outX = (int32_t)(x + 0.5f); // rounded
		if ((uint32_t)outX > 2+ABOUT_SCREEN_W)
			continue;

		// render star pixel if the pixel under it is the background key
		screenBufferPos = ((uint32_t)outY * SCREEN_W) + (uint32_t)outX;
		if ((video.frameBuffer[screenBufferPos] >> 24) == PAL_BCKGRND)
		{
			int32_t d = (int32_t)(z * 255.0f);
			if (d > 255)
				d = 255;

			d ^= 255;
			d = (d * starfieldFade) >> 8;

			int32_t r = d - 66;
			if (r < 0)
				r = 0;

			int32_t g = d - 38;
			if (g < 0)
				g = 0;

			int32_t b = d + 64;
			if (b > 255)
				b = 255;

			video.frameBuffer[screenBufferPos] = RGB32(r, g, b);
			lastStarScreenPos[i] = screenBufferPos;
		}
	}
}

void aboutFrame(void)
{
	rotateMatrix();
	starfield();

	rotation.x += 0.00009f;
	rotation.y += 0.00007f;
	rotation.z -= 0.00005f;

	// fade in starfield
	if (starfieldFade < 256)
		starfieldFade += 4;

	// fade in logo after 1 second
	if (logoFade < 256 && frameCounter++ >= VBLANK_HZ*1)
	{
		blit32Fade(91, 31, bmp.ft2AboutLogo, ABOUT_LOGO_W, ABOUT_LOGO_H, logoFade);
		textOutFade(customText1X, customTextY, PAL_FORGRND, customText1, logoFade);
		logoFade += 4;
	}
}

void showAboutScreen(void) // called once when About screen is opened
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_EXIT_ABOUT);

	sprintf(customText2, "v%s (%s)", PROG_VER_STR, __DATE__);
	customText1X = (SCREEN_W - textWidth(customText1)) >> 1;
	customText2X = (SCREEN_W-8) - textWidth(customText2);
	customTextY = 157;
	textOut(customText2X, customTextY, PAL_FORGRND, customText2);

	aboutInit();
	frameCounter = starfieldFade = logoFade = 0;
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
