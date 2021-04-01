// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#else
#include <unistd.h> // usleep()
#endif
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_events.h"
#include "ft2_mouse.h"
#include "ft2_scopes.h"
#include "ft2_pattern_ed.h"
#include "ft2_pattern_draw.h"
#include "ft2_sample_ed.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_about.h"
#include "ft2_trim.h"
#include "ft2_sampling.h"
#include "ft2_module_loader.h"
#include "ft2_midi.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

static const uint8_t textCursorData[12] =
{
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND
};

video_t video; // globalized

static bool songIsModified;
static char wndTitle[256];
static uint64_t timeNext64, timeNext64Frac;
static sprite_t sprites[SPRITE_NUM];

// for FPS counter
#define FPS_SCAN_FRAMES 60
#define FPS_RENDER_W 285
#define FPS_RENDER_H (((FONT1_CHAR_H + 1) * 8) + 1)
#define FPS_RENDER_X 2
#define FPS_RENDER_Y 2

static char fpsTextBuf[1024];
static uint64_t frameStartTime;
static double dRunningFPS, dFrameTime, dAvgFPS;
// ------------------

static void drawReplayerData(void);

void resetFPSCounter(void)
{
	editor.framesPassed = 0;
	fpsTextBuf[0] = '\0';
	dRunningFPS = VBLANK_HZ;
	dFrameTime = 1000.0 / VBLANK_HZ;
}

void beginFPSCounter(void)
{
	if (video.showFPSCounter)
		frameStartTime = SDL_GetPerformanceCounter();
}

static void drawFPSCounter(void)
{
	if (editor.framesPassed >= FPS_SCAN_FRAMES && (editor.framesPassed % FPS_SCAN_FRAMES) == 0)
	{
		dAvgFPS = dRunningFPS * (1.0 / FPS_SCAN_FRAMES);
		if (dAvgFPS < 0.0 || dAvgFPS > 99999999.9999)
			dAvgFPS = 99999999.9999; // prevent number from overflowing text box

		dRunningFPS = 0.0;
	}

	clearRect(FPS_RENDER_X+2, FPS_RENDER_Y+2, FPS_RENDER_W, FPS_RENDER_H);
	vLineDouble(FPS_RENDER_X, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	vLineDouble(FPS_RENDER_X+FPS_RENDER_W, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y, FPS_RENDER_W, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y+FPS_RENDER_H+2, FPS_RENDER_W, PAL_FORGRND);

	// test if enough data is collected yet
	if (editor.framesPassed < FPS_SCAN_FRAMES)
	{
		textOut(FPS_RENDER_X+53, FPS_RENDER_Y+39, PAL_FORGRND, "Collecting frame information...");
		return;
	}

	double dRefreshRate = video.dMonitorRefreshRate;
	if (dRefreshRate < 0.0 || dRefreshRate > 9999.9)
		dRefreshRate = 9999.9; // prevent number from overflowing text box

	double dAudLatency = audio.dAudioLatencyMs;
	if (dAudLatency < 0.0 || dAudLatency > 999999999.9999)
		dAudLatency = 999999999.9999; // prevent number from overflowing text box

	sprintf(fpsTextBuf, "Frames per second: %.4f\n" \
	             "Monitor refresh rate: %.1fHz (+/-)\n" \
	             "59..61Hz GPU VSync used: %s\n" \
	             "Audio frequency: %.1fkHz (expected %.1fkHz)\n" \
	             "Audio buffer samples: %d (expected %d)\n" \
	             "Audio channels: %d (expected %d)\n" \
	             "Audio latency: %.1fms (expected %.1fms)\n" \
	             "Press CTRL+SHIFT+F to close this box.\n",
	             dAvgFPS, dRefreshRate,
	             video.vsync60HzPresent ? "yes" : "no",
	             audio.haveFreq * (1.0 / 1000.0), audio.wantFreq * (1.0 / 1000.0),
	             audio.haveSamples, audio.wantSamples,
	             audio.haveChannels, audio.wantChannels,
	             dAudLatency, ((audio.wantSamples * 1000.0) / audio.wantFreq));

	// draw text

	uint16_t xPos = FPS_RENDER_X+3;
	uint16_t yPos = FPS_RENDER_Y+3;

	char *textPtr = fpsTextBuf;
	while (*textPtr != '\0')
	{
		const char ch = *textPtr++;
		if (ch == '\n')
		{
			yPos += FONT1_CHAR_H+1;
			xPos = FPS_RENDER_X + 3;
			continue;
		}

		charOut(xPos, yPos, PAL_FORGRND, ch);
		xPos += charWidth(ch);
	}
}

void endFPSCounter(void)
{
	if (video.showFPSCounter && frameStartTime > 0)
	{
		const uint64_t frameTimeDiff = SDL_GetPerformanceCounter() - frameStartTime;
		const double dHz = 1000.0 / (frameTimeDiff * editor.dPerfFreqMulMs);
		dRunningFPS += dHz;
	}
}

void flipFrame(void)
{
	renderSprites();

	if (video.showFPSCounter)
		drawFPSCounter();

	SDL_UpdateTexture(video.texture, NULL, video.frameBuffer, SCREEN_W * sizeof (int32_t));
	SDL_RenderClear(video.renderer);
	SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);
	SDL_RenderPresent(video.renderer);
	eraseSprites();

	if (!video.vsync60HzPresent)
	{
		waitVBL(); // we have no VSync, do crude thread sleeping to sync to ~60Hz
	}
	else
	{
		uint32_t windowFlags = SDL_GetWindowFlags(video.window);

		/* We have VSync, but it can unexpectedly get inactive in certain scenarios.
		** We have to force thread sleeping (to ~60Hz) if so.
		*/
#ifdef __APPLE__
		// macOS: VSync gets disabled if the window is 100% covered by another window. Let's add a (crude) fix:
		if ((windowFlags & SDL_WINDOW_MINIMIZED) || !(windowFlags & SDL_WINDOW_INPUT_FOCUS))
			waitVBL();
#elif __unix__
		// *NIX: VSync gets disabled in fullscreen mode (at least on some distros/systems). Let's add a fix:
		if ((windowFlags & SDL_WINDOW_MINIMIZED) || video.fullscreen)
			waitVBL();
#else
		if (windowFlags & SDL_WINDOW_MINIMIZED)
			waitVBL();
#endif
	}

	editor.framesPassed++;
}

void showErrorMsgBox(const char *fmt, ...)
{
	char strBuf[256];
	va_list args;

	// format the text string
	va_start(args, fmt);
	vsnprintf(strBuf, sizeof (strBuf), fmt, args);
	va_end(args);

	// window can be NULL here, no problem...
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", strBuf, video.window);
}

static void updateRenderSizeVars(void)
{
	float fXScale, fYScale;
	SDL_DisplayMode dm;

	int32_t di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.displayW = dm.w;
	video.displayH = dm.h;

	if (video.fullscreen)
	{
		if (config.specialFlags2 & STRETCH_IMAGE)
		{
			video.renderW = video.displayW;
			video.renderH = video.displayH;
			video.renderX = 0;
			video.renderY = 0;
		}
		else
		{
			SDL_RenderGetScale(video.renderer, &fXScale, &fYScale);

			video.renderW = (int32_t)(SCREEN_W * fXScale);
			video.renderH = (int32_t)(SCREEN_H * fYScale);

			// retina high-DPI hackery (SDL2 is bad at reporting actual rendering sizes on macOS w/ high-DPI)
#ifdef __APPLE__
			int32_t actualScreenW, actualScreenH;
			SDL_GL_GetDrawableSize(video.window, &actualScreenW, &actualScreenH);

			const double dXUpscale = (const double)actualScreenW / video.displayW;
			const double dYUpscale = (const double)actualScreenH / video.displayH;

			// downscale back to correct sizes
			if (dXUpscale != 0.0) video.renderW = (int32_t)(video.renderW / dXUpscale);
			if (dYUpscale != 0.0) video.renderH = (int32_t)(video.renderH / dYUpscale);
#endif
			video.renderX = (video.displayW - video.renderW) >> 1;
			video.renderY = (video.displayH - video.renderH) >> 1;
		}
	}
	else
	{
		SDL_GetWindowSize(video.window, &video.renderW, &video.renderH);

		video.renderX = 0;
		video.renderY = 0;
	}

	// for mouse cursor creation
	video.xScale = (uint32_t)round(video.renderW * (1.0 / SCREEN_W));
	video.yScale = (uint32_t)round(video.renderH * (1.0 / SCREEN_H));
	createMouseCursors();
}

void enterFullscreen(void)
{
	SDL_DisplayMode dm;

	if (config.specialFlags2 & STRETCH_IMAGE)
	{
		SDL_GetDesktopDisplayMode(0, &dm);
		SDL_RenderSetLogicalSize(video.renderer, dm.w, dm.h);
	}
	else
	{
		SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);
	}

	SDL_SetWindowSize(video.window, SCREEN_W, SCREEN_H);
	SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_SetWindowGrab(video.window, SDL_TRUE);

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();
}

void leaveFullScreen(void)
{
	SDL_SetWindowFullscreen(video.window, 0);
	SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);

	setWindowSizeFromConfig(false); // also updates mouse scaling and render size vars
	SDL_SetWindowSize(video.window, SCREEN_W * video.upscaleFactor, SCREEN_H * video.upscaleFactor);
	SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowGrab(video.window, SDL_FALSE);

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();
}

void toggleFullScreen(void)
{
	video.fullscreen ^= 1;

	if (video.fullscreen)
		enterFullscreen();
	else
		leaveFullScreen();
}

bool setupSprites(void)
{
	sprite_t *s;

	memset(sprites, 0, sizeof (sprites));

	s = &sprites[SPRITE_MOUSE_POINTER];
	s->data = bmp.mouseCursors;
	s->w = MOUSE_CURSOR_W;
	s->h = MOUSE_CURSOR_H;

	s = &sprites[SPRITE_LEFT_LOOP_PIN];
	s->data = &bmp.loopPins[0*(154*16)];
	s->w = 16;
	s->h = SAMPLE_AREA_HEIGHT;

	s = &sprites[SPRITE_RIGHT_LOOP_PIN];
	s->data = &bmp.loopPins[2*(154*16)];
	s->w = 16;
	s->h = SAMPLE_AREA_HEIGHT;

	s = &sprites[SPRITE_TEXT_CURSOR];
	s->data = textCursorData;
	s->w = 1;
	s->h = 12;

	hideSprite(SPRITE_MOUSE_POINTER);
	hideSprite(SPRITE_LEFT_LOOP_PIN);
	hideSprite(SPRITE_RIGHT_LOOP_PIN);
	hideSprite(SPRITE_TEXT_CURSOR);

	// setup refresh buffer (used to clear sprites after each frame)
	s = sprites;
	for (uint32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		s->refreshBuffer = (uint32_t *)malloc(s->w * s->h * sizeof (int32_t));
		if (s->refreshBuffer == NULL)
			return false;
	}

	return true;
}

void changeSpriteData(int32_t sprite, const uint8_t *data)
{
	sprites[sprite].data = data;
	memset(sprites[sprite].refreshBuffer, 0, sprites[sprite].w * sprites[sprite].h * sizeof (int32_t));
}

void freeSprites(void)
{
	sprite_t *s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		if (s->refreshBuffer != NULL)
		{
			free(s->refreshBuffer);
			s->refreshBuffer = NULL;
		}
	}
}

void setLeftLoopPinState(bool clicked)
{
	changeSpriteData(SPRITE_LEFT_LOOP_PIN, clicked ? &bmp.loopPins[1*(154*16)] : &bmp.loopPins[0*(154*16)]);
}

void setRightLoopPinState(bool clicked)
{
	changeSpriteData(SPRITE_RIGHT_LOOP_PIN, clicked ? &bmp.loopPins[3*(154*16)] : &bmp.loopPins[2*(154*16)]);
}

int32_t getSpritePosX(int32_t sprite)
{
	return sprites[sprite].x;
}

void setSpritePos(int32_t sprite, int32_t x, int32_t y)
{
	sprites[sprite].newX = (int16_t)x;
	sprites[sprite].newY = (int16_t)y;
}

void hideSprite(int32_t sprite)
{
	sprites[sprite].newX = SCREEN_W;
}

void eraseSprites(void)
{
	sprite_t *s = &sprites[SPRITE_NUM-1];
	for (int32_t i = SPRITE_NUM-1; i >= 0; i--, s--) // erasing must be done in reverse order
	{
		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		assert(s->refreshBuffer != NULL);

		int32_t sw = s->w;
		int32_t sh = s->h;
		int32_t sx = s->x;
		int32_t sy = s->y;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			sy = 0;
		}

		const uint32_t *src32 = s->refreshBuffer;
		uint32_t *dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		const int32_t srcPitch = s->w - sw;
		const int32_t dstPitch = SCREEN_W - sw;

		for (int32_t y = 0; y < sh; y++)
		{
			for (int32_t x = 0; x < sw; x++)
				*dst32++ = *src32++;

			src32 += srcPitch;
			dst32 += dstPitch;
		}
	}
}

void renderSprites(void)
{
	sprite_t *s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		if (i == SPRITE_LEFT_LOOP_PIN || i == SPRITE_RIGHT_LOOP_PIN)
			continue; // these need special drawing (done elsewhere)

		// don't render the text edit cursor if window is inactive
		if (i == SPRITE_TEXT_CURSOR)
		{
			assert(video.window != NULL);
			const uint32_t windowFlags = SDL_GetWindowFlags(video.window);
			if (!(windowFlags & SDL_WINDOW_INPUT_FOCUS))
				continue;
		}

		// set new sprite position
		s->x = s->newX;
		s->y = s->newY;

		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		assert(s->data != NULL && s->refreshBuffer != NULL);

		int32_t sw = s->w;
		int32_t sh = s->h;
		int32_t sx = s->x;
		int32_t sy = s->y;
		const uint8_t *src8 = s->data;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			src8 += (-sy * s->w); // addition
			sy = 0;
		}

		if (sw <= 0 || sh <= 0) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		uint32_t *dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];
		uint32_t *clr32 = s->refreshBuffer;

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		const int32_t srcPitch = s->w - sw;
		const int32_t dstPitch = SCREEN_W - sw;

		if (mouse.mouseOverTextBox && i == SPRITE_MOUSE_POINTER)
		{
			// text edit mouse pointer (has color changing depending on content under it)
			for (int32_t y = 0; y < sh; y++)
			{
				for (int32_t x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer

					if (*src8 != PAL_TRANSPR)
					{
						if (!(*dst32 & 0xFFFFFF) || *dst32 == video.palette[PAL_TEXTMRK])
							*dst32 = 0xB3DBF6;
						else
							*dst32 = 0x004ECE;
					}

					dst32++;
					src8++;
				}

				clr32 += srcPitch;
				src8 += srcPitch;
				dst32 += dstPitch;
			}
		}
		else
		{
			// normal sprites
			for (int32_t y = 0; y < sh; y++)
			{
				for (int32_t x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer

					if (*src8 != PAL_TRANSPR)
					{
						assert(*src8 < PAL_NUM);
						*dst32 = video.palette[*src8];
					}

					dst32++;
					src8++;
				}

				clr32 += srcPitch;
				src8 += srcPitch;
				dst32 += dstPitch;
			}
		}
	}
}

void renderLoopPins(void)
{
	const uint8_t *src8;
	int32_t sx, x, y, sw, sh, srcPitch, dstPitch;
	uint32_t *clr32, *dst32;

	// left loop pin

	sprite_t *s = &sprites[SPRITE_LEFT_LOOP_PIN];
	assert(s->data != NULL && s->refreshBuffer != NULL);

	// set new sprite position
	s->x = s->newX;
	s->y = s->newY;

	if (s->x < SCREEN_W) // loop pin shown?
	{
		sw = s->w;
		sh = s->h;
		sx = s->x;

		src8 = s->data;
		clr32 = s->refreshBuffer;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		dst32 = &video.frameBuffer[(s->y * SCREEN_W) + sx];

		// handle x clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		for (y = 0; y < sh; y++)
		{
			for (x = 0; x < sw; x++)
			{
				*clr32++ = *dst32; // fill clear buffer

				if (*src8 != PAL_TRANSPR)
				{
					assert(*src8 < PAL_NUM);
					*dst32 = video.palette[*src8];
				}

				dst32++;
				src8++;
			}

			src8 += srcPitch;
			clr32 += srcPitch;
			dst32 += dstPitch;
		}
	}

	// right loop pin

	s = &sprites[SPRITE_RIGHT_LOOP_PIN];
	assert(s->data != NULL && s->refreshBuffer != NULL);

	// set new sprite position
	s->x = s->newX;
	s->y = s->newY;

	if (s->x < SCREEN_W) // loop pin shown?
	{
		s->x = s->newX;
		s->y = s->newY;

		sw = s->w;
		sh = s->h;
		sx = s->x;

		src8 = s->data;
		clr32 = s->refreshBuffer;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		dst32 = &video.frameBuffer[(s->y * SCREEN_W) + sx];

		// handle x clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		for (y = 0; y < sh; y++)
		{
			for (x = 0; x < sw; x++)
			{
				*clr32++ = *dst32;

				if (*src8 != PAL_TRANSPR)
				{
					assert(*src8 < PAL_NUM);
					if (y < 9 && *src8 == PAL_LOOPPIN)
					{
						// don't draw marker line on top of left loop pin's thumb graphics
						const uint8_t pal = *dst32 >> 24;
						if (pal != PAL_DESKTOP && pal != PAL_DSKTOP1 && pal != PAL_DSKTOP2)
							*dst32 = video.palette[*src8];
					}
					else
					{
						*dst32 = video.palette[*src8];
					}
				}

				dst32++;
				src8++;
			}

			src8 += srcPitch;
			clr32 += srcPitch;
			dst32 += dstPitch;
		}
	}
}

void setupWaitVBL(void)
{
	// set next frame time
	timeNext64 = SDL_GetPerformanceCounter() + video.vblankTimeLen;
	timeNext64Frac = video.vblankTimeLenFrac;
}

void waitVBL(void)
{
	// this routine almost never delays if we have 60Hz vsync, but it's still needed in some occasions

	uint64_t time64 = SDL_GetPerformanceCounter();
	if (time64 < timeNext64)
	{
		time64 = timeNext64 - time64;
		if (time64 > INT32_MAX)
			time64 = INT32_MAX;

		const int32_t diff32 = (int32_t)time64;

		// convert and round to microseconds
		const int32_t time32 = (int32_t)((diff32 * editor.dPerfFreqMulMicro) + 0.5);

		// delay until we have reached the next frame
		if (time32 > 0)
			usleep(time32);
	}

	// update next frame time
	timeNext64 += video.vblankTimeLen;
	timeNext64Frac += video.vblankTimeLenFrac;
	if (timeNext64Frac > UINT32_MAX)
	{
		timeNext64Frac &= UINT32_MAX;
		timeNext64++;
	}
}

void closeVideo(void)
{
	if (video.texture != NULL)
	{
		SDL_DestroyTexture(video.texture);
		video.texture = NULL;
	}

	if (video.renderer != NULL)
	{
		SDL_DestroyRenderer(video.renderer);
		video.renderer = NULL;
	}

	if (video.window != NULL)
	{
		SDL_DestroyWindow(video.window);
		video.window = NULL;
	}

	if (video.frameBufferUnaligned != NULL)
	{
		free(video.frameBufferUnaligned);
		video.frameBufferUnaligned = NULL;
	}

	video.frameBuffer = NULL;
}

void setWindowSizeFromConfig(bool updateRenderer)
{
#define MAX_UPSCALE_FACTOR 16 // 10112x6400 - ought to be good enough for many years to come

	uint8_t i;
	SDL_DisplayMode dm;

	/* Kludge for Raspbarry Pi. Upscaling of 3x or higher makes everything slow as a snail.
	** This hack unfortunately applies to any ARM based device, but I doubt 3x/4x would run
	** smooth on any ARM device suitable for the FT2 clone anyway (excluding tablets/phones).
	*/
#ifdef __arm__
	if ((config.windowFlags & WINSIZE_3X) || (config.windowFlags & WINSIZE_4X))
	{
		config.windowFlags &= ~(WINSIZE_1X + WINSIZE_2X + WINSIZE_3X + WINSIZE_4X);
		config.windowFlags |= WINSIZE_AUTO;
	}
#endif

	uint8_t oldUpscaleFactor = video.upscaleFactor;
	if (config.windowFlags & WINSIZE_AUTO)
	{
		// find out which upscaling factor is the biggest to fit on screen
		if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
		{
			for (i = MAX_UPSCALE_FACTOR; i >= 1; i--)
			{
				// slightly bigger than 632x400 because of window title, window borders and taskbar/menu
				if (dm.w >= 640*i && dm.h >= 450*i)
				{
					video.upscaleFactor = i;
					break;
				}
			}

			if (i == 0)
				video.upscaleFactor = 1; // 1x is not going to fit, but use 1x anyways...

			// kludge (read comment above)
#ifdef __arm__
			if (video.upscaleFactor > 2)
				video.upscaleFactor = 2;
#endif
		}
		else
		{
			// couldn't get screen resolution, set to 1x
			video.upscaleFactor = 1;
		}
	}
	else if (config.windowFlags & WINSIZE_1X) video.upscaleFactor = 1;
	else if (config.windowFlags & WINSIZE_2X) video.upscaleFactor = 2;
	else if (config.windowFlags & WINSIZE_3X) video.upscaleFactor = 3;
	else if (config.windowFlags & WINSIZE_4X) video.upscaleFactor = 4;

	if (updateRenderer)
	{
		SDL_SetWindowSize(video.window, SCREEN_W * video.upscaleFactor, SCREEN_H * video.upscaleFactor);

		if (oldUpscaleFactor != video.upscaleFactor)
			SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		updateRenderSizeVars();
		updateMouseScaling();
		setMousePosToCenter();
	}
}

void updateWindowTitle(bool forceUpdate)
{
	if (!forceUpdate && songIsModified == song.isModified)
		return; // window title is already set to the same

	char *songTitle = getCurrSongFilename();
	if (songTitle != NULL)
	{
		char songTitleTrunc[128];
		strncpy(songTitleTrunc, songTitle, sizeof (songTitleTrunc)-1);
		songTitleTrunc[sizeof (songTitleTrunc)-1] = '\0';

		if (song.isModified)
			sprintf(wndTitle, "Fasttracker II clone v%s - \"%s\" (unsaved)", PROG_VER_STR, songTitleTrunc);
		else
			sprintf(wndTitle, "Fasttracker II clone v%s - \"%s\"", PROG_VER_STR, songTitleTrunc);
	}
	else
	{
		if (song.isModified)
			sprintf(wndTitle, "Fasttracker II clone v%s - \"untitled\" (unsaved)", PROG_VER_STR);
		else
			sprintf(wndTitle, "Fasttracker II clone v%s - \"untitled\"", PROG_VER_STR);
	}

	SDL_SetWindowTitle(video.window, wndTitle);
	songIsModified = song.isModified;
}

bool recreateTexture(void)
{
	if (video.texture != NULL)
	{
		SDL_DestroyTexture(video.texture);
		video.texture = NULL;
	}

	if (config.windowFlags & PIXEL_FILTER)
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "best");
	else
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "nearest");

	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	if (video.texture == NULL)
	{
		showErrorMsgBox("Couldn't create a %dx%d GPU texture:\n\"%s\"\n\nIs your GPU (+ driver) too old?", SCREEN_W, SCREEN_H, SDL_GetError());
		return false;
	}

	SDL_SetTextureBlendMode(video.texture, SDL_BLENDMODE_NONE);
	return true;
}

bool setupWindow(void)
{
	SDL_DisplayMode dm;

	video.vsync60HzPresent = false;

	uint32_t windowFlags = SDL_WINDOW_ALLOW_HIGHDPI;
#if defined (__APPLE__) || defined (_WIN32) // yet another quirk!
	windowFlags |= SDL_WINDOW_HIDDEN;
#endif

	setWindowSizeFromConfig(false);

#if SDL_PATCHLEVEL >= 5 // SDL 2.0.5 or later
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#endif

	SDL_GetDesktopDisplayMode(0, &dm);
	video.dMonitorRefreshRate = (double)dm.refresh_rate;

	if (dm.refresh_rate >= 59 && dm.refresh_rate <= 61)
		video.vsync60HzPresent = true;

	if (config.windowFlags & FORCE_VSYNC_OFF)
		video.vsync60HzPresent = false;

	video.window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_W * video.upscaleFactor, SCREEN_H * video.upscaleFactor,
		windowFlags);

	if (video.window == NULL)
	{
		showErrorMsgBox("Couldn't create SDL window:\n%s", SDL_GetError());
		return false;
	}

#ifdef __APPLE__ // for macOS we need to do this here for reasons I have forgotten
	SDL_PumpEvents();
	SDL_ShowWindow(video.window);
#endif

	updateWindowTitle(true);
	return true;
}

bool setupRenderer(void)
{
	uint32_t rendererFlags = 0;
	if (video.vsync60HzPresent)
		rendererFlags |= SDL_RENDERER_PRESENTVSYNC;

	video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
	if (video.renderer == NULL)
	{
		if (video.vsync60HzPresent)
		{
			// try again without vsync flag
			video.vsync60HzPresent = false;

			rendererFlags &= ~SDL_RENDERER_PRESENTVSYNC;
			video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
		}

		if (video.renderer == NULL)
		{
			showErrorMsgBox("Couldn't create SDL renderer:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
				SDL_GetError());
			return false;
		}
	}

	SDL_RenderSetLogicalSize(video.renderer, SCREEN_W, SCREEN_H);

#if SDL_PATCHLEVEL >= 5
	SDL_RenderSetIntegerScale(video.renderer, SDL_TRUE);
#endif

	SDL_SetRenderDrawBlendMode(video.renderer, SDL_BLENDMODE_NONE);

	if (!recreateTexture())
	{
		showErrorMsgBox("Couldn't create a %dx%d GPU texture:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
			SCREEN_W, SCREEN_H, SDL_GetError());
		return false;
	}

	// framebuffer used by SDL (for texture)
	video.frameBufferUnaligned = (uint32_t *)MALLOC_PAD(SCREEN_W * SCREEN_H * sizeof (int32_t), 256);
	if (video.frameBufferUnaligned == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// we want an aligned pointer
	video.frameBuffer = (uint32_t *)ALIGN_PTR(video.frameBufferUnaligned, 256);

	if (!setupSprites())
		return false;

	updateRenderSizeVars();
	updateMouseScaling();

	if (config.specialFlags2 & HARDWARE_MOUSE)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);

	// Workaround: SDL_GetGlobalMouseState() doesn't work with KMSDRM
	video.useDesktopMouseCoords = true;
	const char *videoDriver = SDL_GetCurrentVideoDriver();
	if (videoDriver != NULL && strcmp("KMSDRM", videoDriver) == 0)
		video.useDesktopMouseCoords = false;

	return true;
}

void handleRedrawing(void)
{
	if (!ui.configScreenShown && !ui.helpScreenShown)
	{
		if (ui.aboutScreenShown)
		{
			aboutFrame();
		}
		else if (ui.nibblesShown)
		{
			if (editor.NI_Play)
				moveNibblePlayers();
		}
		else
		{
			if (ui.updatePosSections)
			{
				ui.updatePosSections = false;

				if (!ui.diskOpShown)
				{
					drawSongRepS();
					drawSongLength();
					drawPosEdNums(editor.songPos);
					drawEditPattern(editor.editPattern);
					drawPatternLength(editor.editPattern);
					drawSongBPM(editor.speed);
					drawSongSpeed(editor.tempo);
					drawGlobalVol(editor.globalVol);

					if (!songPlaying || editor.wavIsRendering)
						setScrollBarPos(SB_POS_ED, editor.songPos, false);

					// draw current mode text (not while in extended pattern editor mode)
					if (!ui.extended)
					{
						fillRect(115, 80, 74, 10, PAL_DESKTOP);

						     if (playMode == PLAYMODE_PATT)    textOut(115, 80, PAL_FORGRND, "> Play ptn. <");
						else if (playMode == PLAYMODE_EDIT)    textOut(121, 80, PAL_FORGRND, "> Editing <");
						else if (playMode == PLAYMODE_RECSONG) textOut(114, 80, PAL_FORGRND, "> Rec. sng. <");
						else if (playMode == PLAYMODE_RECPATT) textOut(115, 80, PAL_FORGRND, "> Rec. ptn. <");
					}
				}
			}

			if (ui.updatePosEdScrollBar)
			{
				ui.updatePosEdScrollBar = false;
				setScrollBarPos(SB_POS_ED, song.songPos, false);
				setScrollBarEnd(SB_POS_ED, (song.len - 1) + 5);
			}

			if (!ui.extended)
			{
				if (!ui.diskOpShown)
					drawPlaybackTime();

				if (ui.sampleEditorExtShown)
					handleSampleEditorExtRedrawing();
				else if (ui.scopesShown)
					drawScopes();
			}
		}
	}

	drawReplayerData();

	if (ui.instEditorShown)
		handleInstEditorRedrawing();
	else if (ui.sampleEditorShown)
		handleSamplerRedrawing();

	// blink text edit cursor
	if (editor.editTextFlag && mouse.lastEditBox != -1)
	{
		assert(mouse.lastEditBox >= 0 && mouse.lastEditBox < NUM_TEXTBOXES);

		textBox_t *txt = &textBoxes[mouse.lastEditBox];
		if (editor.textCursorBlinkCounter < 256/2 && !textIsMarked() && !(mouse.leftButtonPressed | mouse.rightButtonPressed))
			setSpritePos(SPRITE_TEXT_CURSOR, getTextCursorX(txt), getTextCursorY(txt) - 1); // show text cursor
		else
			hideSprite(SPRITE_TEXT_CURSOR); // hide text cursor

		editor.textCursorBlinkCounter += TEXT_CURSOR_BLINK_RATE;
	}

	if (editor.busy)
		animateBusyMouse();

	renderLoopPins();
}

static void drawReplayerData(void)
{
	if (songPlaying)
	{
		if (ui.drawReplayerPianoFlag)
		{
			ui.drawReplayerPianoFlag = false;
			if (ui.instEditorShown && chSyncEntry != NULL)
				drawPiano(chSyncEntry);
		}

		bool drawPosText = true;
		if (ui.configScreenShown || ui.nibblesShown     ||
			ui.helpScreenShown   || ui.aboutScreenShown ||
			ui.diskOpShown)
		{
			drawPosText = false;
		}

		if (drawPosText)
		{
			if (ui.drawBPMFlag)
			{
				ui.drawBPMFlag = false;
				drawSongBPM(editor.speed);
			}
			
			if (ui.drawSpeedFlag)
			{
				ui.drawSpeedFlag = false;
				drawSongSpeed(editor.tempo);
			}

			if (ui.drawGlobVolFlag)
			{
				ui.drawGlobVolFlag = false;
				drawGlobalVol(editor.globalVol);
			}

			if (ui.drawPosEdFlag)
			{
				ui.drawPosEdFlag = false;
				drawPosEdNums(editor.songPos);
				setScrollBarPos(SB_POS_ED, editor.songPos, false);
			}

			if (ui.drawPattNumLenFlag)
			{
				ui.drawPattNumLenFlag = false;
				drawEditPattern(editor.editPattern);
				drawPatternLength(editor.editPattern);
			}
		}
	}
	else if (ui.instEditorShown)
	{
		drawPiano(NULL);
	}

	// handle pattern data updates
	if (ui.updatePatternEditor)
	{
		ui.updatePatternEditor = false;
		if (ui.patternEditorShown)
			writePattern(editor.pattPos, editor.editPattern);
	}
}
