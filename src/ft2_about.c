#include <stdio.h> // sprintf()
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_bmp.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_gfxdata.h"
#include "ft2_pattern_ed.h" // exitPatternEditorExtended()
#include "ft2_config.h"
#include "ft2_random.h"

#define NUM_STARS 1500
#define STARSHINE_ALPHA_PERCENTAGE 33
#define ABOUT_SCREEN_X 3
#define ABOUT_SCREEN_Y 3
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167
#define ABOUT_LOGO_W 541
#define ABOUT_LOGO_H 78

typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

static char *customText0 = "Original FT2 by Magnus \"Vogue\" H\224gdahl & Fredrik \"Mr.H\" Huss";
static char *customText1 = "Clone by Olav \"8bitbubsy\" S\233rensen";
static char *customText2 = "Atari ST YM/SNDH Edition - added by Qumran";
static char customText3[128];
static int16_t customText0X, customText0Y, customText1X, customText1Y;
static int16_t customText2X, customText2Y, customText3X, customText3Y;
static const uint16_t starShineAlpha16 = (65535 * STARSHINE_ALPHA_PERCENTAGE) / 100;
static vector_t starPoints[NUM_STARS], starRotation;
static matrix_t starMatrix;

static void blendPixelsXY(uint32_t x, uint32_t y, uint32_t pixelB_r, uint32_t pixelB_g, uint32_t pixelB_b, uint16_t alpha)
{
	uint32_t *p = &video.frameBuffer[(y * SCREEN_W) + x];
	const uint32_t pixelA = *p;

	const uint16_t invAlpha = alpha ^ 0xFFFF;
	const int32_t r = ((RGB32_R(pixelA) * invAlpha) + (pixelB_r * alpha)) >> 16;
	const int32_t g = ((RGB32_G(pixelA) * invAlpha) + (pixelB_g * alpha)) >> 16;
	const int32_t b = ((RGB32_B(pixelA) * invAlpha) + (pixelB_b * alpha)) >> 16;

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

		const float z = (starMatrix.x.z * star->x) + (starMatrix.y.z * star->y) + (starMatrix.z.z * star->z) + 0.5f;
		if (z <= 0.0f)
			continue;

		float y = (((starMatrix.x.y * star->x) + (starMatrix.y.y * star->y) + (starMatrix.z.y * star->z)) / z) * 400.0f;
		const int32_t outY = (ABOUT_SCREEN_Y+(ABOUT_SCREEN_H/2)) + (int32_t)y;
		if (outY < ABOUT_SCREEN_Y || outY >= ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
			continue;

		float x = (((starMatrix.x.x * star->x) + (starMatrix.y.x * star->y) + (starMatrix.z.x * star->z)) / z) * 400.0f;
		const int32_t outX = (ABOUT_SCREEN_X+(ABOUT_SCREEN_W/2)) + (int32_t)x;
		if (outX < ABOUT_SCREEN_X || outX >= ABOUT_SCREEN_X+ABOUT_SCREEN_W)
			continue;

		int32_t intensity255 = (int32_t)(z * 256.0f);
		if (intensity255 > 255)
			intensity255 = 255;
		intensity255 ^= 255;

		// add a tint of blue to the star pixel

		int32_t r = intensity255 - 90;
		if (r < 0)
			r = 0;

		int32_t g = intensity255 - 38;
		if (g < 0)
			g = 0;

		int32_t b = intensity255 + 60;
		if (b > 255)
			b = 255;

		// plot and blend sides of star (basic shine effect)

		if (outX-1 >= ABOUT_SCREEN_X)
			blendPixelsXY(outX-1, outY, r, g, b, starShineAlpha16);

		if (outX+1 < ABOUT_SCREEN_X+ABOUT_SCREEN_W)
			blendPixelsXY(outX+1, outY, r, g, b, starShineAlpha16);

		if (outY-1 >= ABOUT_SCREEN_Y)
			blendPixelsXY(outX, outY-1, r, g, b, starShineAlpha16);

		if (outY+1 < ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
			blendPixelsXY(outX, outY+1, r, g, b, starShineAlpha16);

		// plot center pixel
		video.frameBuffer[(outY * SCREEN_W) + outX] = RGB32(r, g, b);
	}
}

static void rotateStarfieldMatrix(void)
{
#define F_2PI (float)(2.0 * PI)

	const float xrad = starRotation.x * F_2PI;
	const float xsin = sinf(xrad);
	const float xcos = cosf(xrad);

	const float yrad = starRotation.y * F_2PI;
	const float ysin = sinf(yrad);
	const float ycos = cosf(yrad);

	const float zrad = starRotation.z * F_2PI;
	const float zsin = sinf(zrad);
	const float zcos = cosf(zrad);

	// x
	starMatrix.x.x = (xcos * zcos) + (zsin * xsin * ysin);
	starMatrix.y.x = xsin * ycos;
	starMatrix.z.x = (zcos * xsin * ysin) - (xcos * zsin);

	// y
	starMatrix.x.y = (zsin * xcos * ysin) - (xsin * zcos);
	starMatrix.y.y = xcos * ycos;
	starMatrix.z.y = (xsin * zsin) + (zcos * xcos * ysin);

	// z
	starMatrix.x.z = ycos * zsin;
	starMatrix.y.z = 0.0f - ysin;
	starMatrix.z.z = ycos * zcos;
}

void renderAboutScreenFrame(void)
{
	clearRect(ABOUT_SCREEN_X, ABOUT_SCREEN_Y, ABOUT_SCREEN_W, ABOUT_SCREEN_H);

	// 3D starfield

	starfield();
	starRotation.x -= 0.0003f;
	starRotation.y -= 0.0002f;
	starRotation.z += 0.0001f;
	rotateStarfieldMatrix();

	// FT2 logo
	blit32((SCREEN_W - ABOUT_LOGO_W) / 2, 30, bmp.ft2AboutLogo, ABOUT_LOGO_W, ABOUT_LOGO_H);

	// render static texts
	textOut(customText0X, customText0Y, PAL_FORGRND, customText0);
	textOut(customText1X, customText1Y, PAL_FORGRND, customText1);
	textOut(customText2X, customText2Y, PAL_FORGRND, customText2);
	textOut(customText3X, customText3Y, PAL_FORGRND, customText3);

	showPushButton(PB_EXIT_ABOUT); // yes, we also have to redraw the exit button per frame :)
}

void showAboutScreen(void) // called once when about screen is opened
{
	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_EXIT_ABOUT);
	ui.aboutScreenShown = true;
}

void initAboutScreen(void)
{
	vector_t *s = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, s++)
	{
		s->x = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
		s->y = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
		s->z = (float)((randoml(INT32_MAX) - (INT32_MAX/2)) * (1.0 / INT32_MAX));
	}

	sprintf(customText3, "v%s (%s)", PROG_VER_STR, __DATE__);
	customText0X = (SCREEN_W    - textWidth(customText0)) / 2;
	customText1X = (SCREEN_W    - textWidth(customText1)) / 2;
	customText2X = (SCREEN_W-8) - textWidth(customText2);
	customText3X = (SCREEN_W-8) - textWidth(customText3);
	customText0Y = 157-34;
	customText1Y = 157-12;
	customText2Y = 157-12;
	customText3Y = 157;
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
