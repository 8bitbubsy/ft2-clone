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
#define STARSHINE_ALPHA_PERCENTAGE 31
#define ABOUT_SCREEN_X 3
#define ABOUT_SCREEN_Y 3
#define ABOUT_SCREEN_W 626
#define ABOUT_SCREEN_H 167
#define ABOUT_LOGO_W 541
#define ABOUT_LOGO_H 78
#define BG_COLORKEY 0xFF000000
#define STAR_PIXEL_ID 0xFE000000

typedef struct
{
	int16_t x, y, z;
} vector_t;

typedef struct
{
	float x, y, z;
} rotate_t;

typedef struct
{
	vector_t x, y, z;
} matrix_t;

static char *customText0 = "Original FT2 by Magnus \"Vogue\" H\224gdahl & Fredrik \"Mr.H\" Huss";
static char *customText1 = "Clone by Olav \"8bitbubsy\" S\233rensen";
static char *customText2 = "https://16-bits.org";
static char customText3[128];
static int16_t customText0X, customText0Y, customText1X, customText1Y;
static int16_t customText2X, customText2Y, customText3X, customText3Y;
static int32_t lastStarPos[NUM_STARS];
static const uint16_t starShineAlpha16 = (65535 * STARSHINE_ALPHA_PERCENTAGE) / 100;
static vector_t starPoints[NUM_STARS];
static rotate_t starRotation;
static matrix_t starMatrix;

static void clearPixel(uint32_t x, uint32_t y)
{
	if (x >= ABOUT_SCREEN_X+ABOUT_SCREEN_W || y >= ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
		return;

	const uint32_t pos = (y * SCREEN_W) + x;
	if ((video.frameBuffer[pos] & 0xFF000000) == STAR_PIXEL_ID)
		video.frameBuffer[pos] = BG_COLORKEY;
}

static void putPixel(uint32_t x, uint32_t y, uint32_t color)
{
	if (x >= ABOUT_SCREEN_X+ABOUT_SCREEN_W || y >= ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
		return;

	const uint32_t pos = (y * SCREEN_W) + x;
	if (video.frameBuffer[pos] == BG_COLORKEY)
		video.frameBuffer[pos] = color | STAR_PIXEL_ID;
}

static void blendPixelsXY(uint32_t x, uint32_t y, uint32_t pixelB_r, uint32_t pixelB_g, uint32_t pixelB_b, uint16_t alpha)
{
	uint32_t *p = &video.frameBuffer[(y * SCREEN_W) + x];
	const uint32_t pixelA = *p;

	const uint16_t invAlpha = alpha ^ 0xFFFF;
	const int32_t r = ((RGB32_R(pixelA) * invAlpha) + (pixelB_r * alpha)) >> 16;
	const int32_t g = ((RGB32_G(pixelA) * invAlpha) + (pixelB_g * alpha)) >> 16;
	const int32_t b = ((RGB32_B(pixelA) * invAlpha) + (pixelB_b * alpha)) >> 16;

	if (*p == BG_COLORKEY)
		*p = RGB32(r, g, b) | STAR_PIXEL_ID;
}

static void starfield(void)
{
	vector_t *star = starPoints;
	for (int16_t i = 0; i < NUM_STARS; i++, star++)
	{
		// erase previous star
		if (lastStarPos[i] != -1)
		{
			const int32_t x = lastStarPos[i] >> 16, y = lastStarPos[i] & 0xFFFF;
			lastStarPos[i] = -1;

			// center
			clearPixel(x,   y);

			// sides
			clearPixel(x-1, y);
			clearPixel(x+1, y);
			clearPixel(x,   y-1);
			clearPixel(x,   y+1);
		}

		star->z += 3;
		if (star->z > 16384)
			star->z -= 32768;

		int32_t z = (((starMatrix.x.z * star->x) + (starMatrix.y.z * star->y) + (starMatrix.z.z * star->z)) >> 15) + 16384;
		if (z <= 0)
			continue;
		
		int32_t y = ((((starMatrix.x.y * star->x) + (starMatrix.y.y * star->y) + (starMatrix.z.y * star->z)) >> 15) * 400) / z;
		y += ABOUT_SCREEN_H / 2;
		if ((uint32_t)y >= ABOUT_SCREEN_H)
			continue;

		int32_t x = ((((starMatrix.x.x * star->x) + (starMatrix.y.x * star->y) + (starMatrix.z.x * star->z)) >> 15) * 400) / z;
		x += ABOUT_SCREEN_W / 2;
		if ((uint32_t)x >= ABOUT_SCREEN_W)
			continue;

		const int32_t outX = ABOUT_SCREEN_X + x;
		const int32_t outY = ABOUT_SCREEN_Y + y;
		
		// only draw star if it's not over logo and text
		if (video.frameBuffer[(outY * SCREEN_W) + outX] != BG_COLORKEY)
			continue;

		int32_t intensity255 = z >> 7;
		if (intensity255 > 255)
			intensity255 = 255;
		intensity255 ^= 255;

		// add a tint of blue to the star pixel

		int32_t r = intensity255 - 92;
		if (r < 0)
			r = 0;

		int32_t g = intensity255 - 36;
		if (g < 0)
			g = 0;

		int32_t b = intensity255 + 62;
		if (b > 255)
			b = 255;

		// plot center pixel
		putPixel(outX, outY, RGB32(r, g, b));

		// plot and blend sides of star (basic shine effect)

		if (outX-1 >= ABOUT_SCREEN_X)
			blendPixelsXY(outX-1, outY, r, g, b, starShineAlpha16);

		if (outX+1 < ABOUT_SCREEN_X+ABOUT_SCREEN_W)
			blendPixelsXY(outX+1, outY, r, g, b, starShineAlpha16);

		if (outY-1 >= ABOUT_SCREEN_Y)
			blendPixelsXY(outX, outY-1, r, g, b, starShineAlpha16);

		if (outY+1 < ABOUT_SCREEN_Y+ABOUT_SCREEN_H)
			blendPixelsXY(outX, outY+1, r, g, b, starShineAlpha16);

		lastStarPos[i] = (outX << 16) | outY;
	}
}

static void rotateStarfieldMatrix(void)
{
#define F_2PI (float)(2.0 * PI)
#define SCALE 32767.0f

	const float rx2p = starRotation.x * F_2PI;
	const float xsin = sinf(rx2p);
	const float xcos = cosf(rx2p);

	const float ry2p = starRotation.y * F_2PI;
	const float ysin = sinf(ry2p);
	const float ycos = cosf(ry2p);

	const float rz2p = starRotation.z * F_2PI;
	const float zsin = sinf(rz2p);
	const float zcos = cosf(rz2p);

	// x
	starMatrix.x.x = (int16_t)(SCALE * ((xcos * zcos) + (zsin * xsin * ysin)));
	starMatrix.y.x = (int16_t)(SCALE * (xsin * ycos));
	starMatrix.z.x = (int16_t)(SCALE * ((zcos * xsin * ysin) - (xcos * zsin)));

	// y
	starMatrix.x.y = (int16_t)(SCALE * ((zsin * xcos * ysin) - (xsin * zcos)));
	starMatrix.y.y = (int16_t)(SCALE * (xcos * ycos));
	starMatrix.z.y = (int16_t)(SCALE * ((xsin * zsin) + (zcos * xcos * ysin)));

	// z
	starMatrix.x.z = (int16_t)(SCALE * (ycos * zsin));
	starMatrix.y.z = (int16_t)(SCALE * (0.0f - ysin));
	starMatrix.z.z = (int16_t)(SCALE * (ycos * zcos));
}

void renderAboutScreenFrame(void)
{
	starfield();
	starRotation.x -= 0.0003f;
	starRotation.y -= 0.0002f;
	starRotation.z += 0.0001f;
	rotateStarfieldMatrix();

	showPushButton(PB_EXIT_ABOUT); // yes, we also have to redraw the exit button per frame :)
}

void showAboutScreen(void) // called once when about screen is opened
{
	memset(lastStarPos, -1, NUM_STARS * sizeof (int32_t));

	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideTopScreen();

	drawFramework(0, 0, 632, 173, FRAMEWORK_TYPE1);
	drawFramework(2, 2, 628, 169, FRAMEWORK_TYPE2);

	for (int32_t y = ABOUT_SCREEN_Y; y < ABOUT_SCREEN_Y+ABOUT_SCREEN_H; y++)
	{
		for (int32_t x = ABOUT_SCREEN_X; x < ABOUT_SCREEN_X+ABOUT_SCREEN_W; x++)
			video.frameBuffer[(y * SCREEN_W) + x] = BG_COLORKEY;
	}

	// FT2 logo
	blit32((SCREEN_W - ABOUT_LOGO_W) / 2, 30, bmp.ft2AboutLogo, ABOUT_LOGO_W, ABOUT_LOGO_H);

	// render static texts
	textOut(customText0X, customText0Y, PAL_FORGRND, customText0);
	textOut(customText1X, customText1Y, PAL_FORGRND, customText1);
	textOut(customText2X, customText2Y, PAL_FORGRND, customText2);
	textOut(customText3X, customText3Y, PAL_FORGRND, customText3);

	showPushButton(PB_EXIT_ABOUT);
	ui.aboutScreenShown = true;
}

void initAboutScreen(void)
{
	vector_t *s = starPoints;
	for (int32_t i = 0; i < NUM_STARS; i++, s++)
	{
		s->x = (int16_t)(randoml(32768) - 16384);
		s->y = (int16_t)(randoml(32768) - 16384);
		s->z = (int16_t)(randoml(32768) - 16384);
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
	showTopScreen(RESTORE_SCREENS);
}
