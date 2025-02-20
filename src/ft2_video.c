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
#include "scopes/ft2_scopes.h"
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
static sprite_t sprites[SPRITE_NUM];

// for FPS counter
#define FPS_LINES 15
#define FPS_SCAN_FRAMES 60
#define FPS_RENDER_W 285
#define FPS_RENDER_H (((FONT1_CHAR_H + 1) * FPS_LINES) + 1)
#define FPS_RENDER_X 2
#define FPS_RENDER_Y 2

static char fpsTextBuf[1024];
static uint64_t frameStartTime;
static double dRunningFrameDuration, dAvgFPS;
// ------------------

static void drawReplayerData(void);

void resetFPSCounter(void)
{
	editor.framesPassed = 0;
	fpsTextBuf[0] = '\0';
	dRunningFrameDuration = 1000.0 / VBLANK_HZ;
}

void beginFPSCounter(void)
{
	if (video.showFPSCounter)
		frameStartTime = SDL_GetPerformanceCounter();
}

static void drawFPSCounter(void)
{
	SDL_version SDLVer;

	SDL_GetVersion(&SDLVer);

	if (editor.framesPassed >= FPS_SCAN_FRAMES && (editor.framesPassed % FPS_SCAN_FRAMES) == 0)
	{
		dAvgFPS = 1000.0 / (dRunningFrameDuration / FPS_SCAN_FRAMES);
		if (dAvgFPS < 0.0 || dAvgFPS > 99999999.9999)
			dAvgFPS = 99999999.9999; // prevent number from overflowing text box

		dRunningFrameDuration = 0.0;
	}

	clearRect(FPS_RENDER_X+2, FPS_RENDER_Y+2, FPS_RENDER_W, FPS_RENDER_H);
	vLineDouble(FPS_RENDER_X, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	vLineDouble(FPS_RENDER_X+FPS_RENDER_W, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y, FPS_RENDER_W, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y+FPS_RENDER_H+2, FPS_RENDER_W, PAL_FORGRND);

	// if enough frame data isn't collected yet, show a message
	if (editor.framesPassed < FPS_SCAN_FRAMES)
	{
		const char *text = "Gathering frame information...";
		const uint16_t textW = textWidth(text);
		textOut(FPS_RENDER_X+((FPS_RENDER_W/2)-(textW/2)), FPS_RENDER_Y+((FPS_RENDER_H/2)-(FONT1_CHAR_H/2)), PAL_FORGRND, text);
		return;
	}

	double dRefreshRate = video.dMonitorRefreshRate;
	if (dRefreshRate < 0.0 || dRefreshRate > 9999.9)
		dRefreshRate = 9999.9; // prevent number from overflowing text box

	sprintf(fpsTextBuf,
	             "SDL version: %u.%u.%u\n" \
	             "Frames per second: %.3f\n" \
	             "Monitor refresh rate: %.1fHz (+/-)\n" \
	             "59..61Hz GPU VSync used: %s\n" \
	             "HPC frequency (timer): %.4fMHz\n" \
	             "Audio frequency: %.1fkHz (expected %.1fkHz)\n" \
	             "Audio buffer samples: %d (expected %d)\n" \
	             "Render size: %dx%d (offset %d,%d)\n" \
	             "Disp. size: %dx%d (window: %dx%d)\n" \
	             "Render scaling: x=%.4f, y=%.4f\n" \
	             "DPI zoom factors: x=%.4f, y=%.4f\n" \
	             "Mouse pixel-space muls: x=%.4f, y=%.4f\n" \
	             "Relative mouse coords: %d,%d\n" \
	             "Absolute mouse coords: %d,%d\n" \
	             "Press CTRL+SHIFT+F to close this box.\n",
	             SDLVer.major, SDLVer.minor, SDLVer.patch,
	             dAvgFPS,
	             dRefreshRate,
	             video.vsync60HzPresent ? "yes" : "no",
	             hpcFreq.freq64 / (1000.0 * 1000.0),
	             audio.haveFreq / 1000.0, audio.wantFreq / 1000.0,
	             audio.haveSamples, audio.wantSamples,
	             video.renderW, video.renderH, video.renderX, video.renderY,
	             video.displayW, video.displayH, video.windowW, video.windowH,
	             (double)video.renderW / SCREEN_W, (double)video.renderH / SCREEN_H,
	             video.dDpiZoomFactorX, video.dDpiZoomFactorY,
	             video.dMouseXMul, video.dMouseYMul,
	             mouse.x, mouse.y,
	             mouse.absX, mouse.absY);

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
			xPos = FPS_RENDER_X+3;
			continue;
		}

		charOut(xPos, yPos, PAL_FORGRND, ch);
		xPos += charWidth(ch);
	}

	// draw framerate tester symbol

	const uint16_t symbolEnd = 115;

	// ping-pong movement
	uint16_t x = editor.framesPassed % (symbolEnd * 2);
	if (x >= symbolEnd)
		x = (symbolEnd * 2) - x;

	charOut(164 + x, 16, PAL_FORGRND, '*');
}

void endFPSCounter(void)
{
	if (video.showFPSCounter && frameStartTime > 0)
	{
		uint64_t frameTimeDiff64 = SDL_GetPerformanceCounter() - frameStartTime;
		if (frameTimeDiff64 > INT32_MAX)
			frameTimeDiff64 = INT32_MAX;

		dRunningFrameDuration += (int32_t)frameTimeDiff64 * hpcFreq.dFreqMulMs;
	}
}

void flipFrame(void)
{
	const uint32_t windowFlags = SDL_GetWindowFlags(video.window);
	bool minimized = (windowFlags & SDL_WINDOW_MINIMIZED) ? true : false;

	renderSprites();

	if (video.showFPSCounter)
		drawFPSCounter();

	SDL_UpdateTexture(video.texture, NULL, video.frameBuffer, SCREEN_W * sizeof (int32_t));

	// SDL 2.0.14 bug on Windows (?): This function consumes ever-increasing memory if the program is minimized
	if (!minimized)
		SDL_RenderClear(video.renderer);

	if (video.useCustomRenderRect)
		SDL_RenderCopy(video.renderer, video.texture, NULL, &video.renderRect);
	else
		SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);

	SDL_RenderPresent(video.renderer);

	eraseSprites();

	if (!video.vsync60HzPresent)
	{
		// we have no VSync, do crude thread sleeping to sync to ~60Hz
		hpc_Wait(&video.vblankHpc);
	}
	else
	{
		/* We have VSync, but it can unexpectedly get inactive in certain scenarios.
		** We have to force thread sleeping (to ~60Hz) if so.
		*/
#ifdef __APPLE__
		// macOS: VSync gets disabled if the window is 100% covered by another window. Let's add a (crude) fix:
		if (minimized || !(windowFlags & SDL_WINDOW_INPUT_FOCUS))
			hpc_Wait(&video.vblankHpc);
#elif __unix__
		/* *NIX: VSync can get disabled in fullscreen mode in some distros/systems. Let's add a fix.
		**
		** TODO/XXX: This is probably a BAD hack and can cause a poor fullscreen experience if VSync did
		**           in fact work in fullscreen mode...
		*/
		if (minimized || video.fullscreen)
			hpc_Wait(&video.vblankHpc);
#else
		if (minimized)
			hpc_Wait(&video.vblankHpc);
#endif
	}

	editor.framesPassed++;

	/* Reset audio/video sync timestamp every half an hour to prevent
	** possible sync drifting after hours of playing a song without
	** a single song stop (resets timestamp) in-between.
	*/
	if (editor.framesPassed >= VBLANK_HZ*60*30)
		audio.resetSyncTickTimeFlag = true;
}

void showErrorMsgBox(const char *fmt, ...)
{
	char strBuf[512+1];
	va_list args;

	// format the text string
	va_start(args, fmt);
	vsnprintf(strBuf, sizeof (strBuf)-1, fmt, args);
	va_end(args);

	// SDL message boxes can be very buggy on Windows XP, use MessageBoxA() instead
#ifdef _WIN32
	MessageBoxA(NULL, strBuf, "Error", MB_OK | MB_ICONERROR);
#else
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", strBuf, NULL);
#endif
}

static void updateRenderSizeVars(void)
{
	int32_t widthInPixels, heightInPixels;
	SDL_DisplayMode dm;

	int32_t di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.displayW = dm.w;
	video.displayH = dm.h;

	SDL_GetWindowSize(video.window, &video.windowW, &video.windowH);
	video.renderX = 0;
	video.renderY = 0;

	video.useCustomRenderRect = false;

	if (video.fullscreen)
	{
		if (config.specialFlags2 & STRETCH_IMAGE)
		{
			// "streched out" windowed fullscreen

			video.renderW = video.windowW;
			video.renderH = video.windowH;

			// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
			SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
			video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
			video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;
		}
		else
		{
			// centered windowed fullscreen, with pixel-perfect integer upscaling

			const int32_t maxUpscaleFactor = MIN(video.windowW / SCREEN_W, video.windowH / SCREEN_H);
			video.renderW = SCREEN_W * maxUpscaleFactor;
			video.renderH = SCREEN_H * maxUpscaleFactor;
			video.renderX = (video.windowW - video.renderW) / 2;
			video.renderY = (video.windowH - video.renderH) / 2;

			// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
			SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
			video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
			video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;

			video.renderRect.x = (int32_t)floor(video.renderX * video.dDpiZoomFactorX);
			video.renderRect.y = (int32_t)floor(video.renderY * video.dDpiZoomFactorY);
			video.renderRect.w = (int32_t)floor(video.renderW * video.dDpiZoomFactorX);
			video.renderRect.h = (int32_t)floor(video.renderH * video.dDpiZoomFactorY);
			video.useCustomRenderRect = true; // use the destination coordinates above in SDL_RenderCopy()
		}
	}
	else
	{
		// windowed mode

		SDL_GetWindowSize(video.window, &video.renderW, &video.renderH);

		// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
		SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
		video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
		video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;
	}

	// "hardware mouse" calculations
	video.mouseCursorUpscaleFactor = MIN(video.renderW / SCREEN_W, video.renderH / SCREEN_H);
	createMouseCursors();
}

void enterFullscreen(void)
{
	SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_Delay(15); // fixes possible issues

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();
}

void leaveFullscreen(void)
{
	SDL_SetWindowFullscreen(video.window, 0);
	SDL_Delay(15); // fixes possible issues

	setWindowSizeFromConfig(false); // false = do not change actual window size, only update variables
	SDL_SetWindowSize(video.window, SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor);

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();

#ifdef __unix__ // can be required on Linux... (or else the window keeps moving down every time you leave fullscreen)
	SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif
}

void toggleFullscreen(void)
{
	video.fullscreen ^= 1;

	if (video.fullscreen)
		enterFullscreen();
	else
		leaveFullscreen();
}

bool setupSprites(void)
{
	sprite_t *s;

	memset(sprites, 0, sizeof (sprites));

	// hide sprites
	s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
		s->x = s->y = INT16_MAX;

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

	if (video.frameBuffer != NULL)
	{
		free(video.frameBuffer);
		video.frameBuffer = NULL;
	}
}

void setWindowSizeFromConfig(bool updateRenderer)
{
#define MAX_UPSCALE_FACTOR 16 // 10112x6400 - ought to be good enough for many years to come

	uint8_t i;
	SDL_DisplayMode dm;

	uint8_t oldUpscaleFactor = video.windowModeUpscaleFactor;
	if (config.windowFlags & WINSIZE_AUTO)
	{
		int32_t di = SDL_GetWindowDisplayIndex(video.window);
		if (di < 0)
			di = 0; // return display index 0 (default) on error

		// find out which upscaling factor is the biggest to fit on screen
		if (SDL_GetDesktopDisplayMode(di, &dm) == 0)
		{
			for (i = MAX_UPSCALE_FACTOR; i >= 1; i--)
			{
				// height test is slightly taller because of window title, window borders and taskbar/menu/dock
				if (dm.w >= SCREEN_W*i && dm.h >= (SCREEN_H+64)*i)
				{
					video.windowModeUpscaleFactor = i;
					break;
				}
			}

			if (i == 0)
				video.windowModeUpscaleFactor = 1; // 1x is not going to fit, but use 1x anyways...
		}
		else
		{
			// couldn't get screen resolution, set to 1x
			video.windowModeUpscaleFactor = 1;
		}
	}
	else if (config.windowFlags & WINSIZE_1X) video.windowModeUpscaleFactor = 1;
	else if (config.windowFlags & WINSIZE_2X) video.windowModeUpscaleFactor = 2;
	else if (config.windowFlags & WINSIZE_3X) video.windowModeUpscaleFactor = 3;
	else if (config.windowFlags & WINSIZE_4X) video.windowModeUpscaleFactor = 4;

	if (updateRenderer)
	{
		SDL_SetWindowSize(video.window, SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor);

		if (oldUpscaleFactor != video.windowModeUpscaleFactor)
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

	int32_t di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.dMonitorRefreshRate = (double)dm.refresh_rate;
#ifdef __EMSCRIPTEN__
	if (dm.refresh_rate >= 59 && dm.refresh_rate <= 61)
		video.vsync60HzPresent = true;
#endif
	if (config.windowFlags & FORCE_VSYNC_OFF)
		video.vsync60HzPresent = false;

	video.window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor,
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

	SDL_SetRenderDrawBlendMode(video.renderer, SDL_BLENDMODE_NONE);

	if (!recreateTexture())
	{
		showErrorMsgBox("Couldn't create a %dx%d GPU texture:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
			SCREEN_W, SCREEN_H, SDL_GetError());
		return false;
	}

	// framebuffer used by SDL (for texture)
	video.frameBuffer = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof (uint32_t));
	if (video.frameBuffer == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	if (!setupSprites())
		return false;

	updateRenderSizeVars();
	updateMouseScaling();

	if (config.specialFlags2 & HARDWARE_MOUSE)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);

	SDL_SetRenderDrawColor(video.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	return true;
}

void handleRedrawing(void)
{
	if (!ui.configScreenShown && !ui.helpScreenShown)
	{
		if (ui.aboutScreenShown)
		{
			renderAboutScreenFrame();
		}
		else if (ui.nibblesShown)
		{
			if (editor.NI_Play)
				moveNibblesPlayers();
		}
		else
		{
			if (ui.updatePosSections)
			{
				ui.updatePosSections = false;

				if (!ui.diskOpShown)
				{
					drawSongLoopStart();
					drawSongLength();
					drawPosEdNums(editor.songPos);
					drawEditPattern(editor.editPattern);
					drawPatternLength(editor.editPattern);
					drawSongBPM(editor.BPM);
					drawSongSpeed(editor.speed);
					drawGlobalVol(editor.globalVolume);

					if (!songPlaying || editor.wavIsRendering)
						setScrollBarPos(SB_POS_ED, editor.songPos, false);

					// draw current mode text

					const char *str = NULL;
					     if (playMode == PLAYMODE_PATT)    str = "> Play ptn. <";
					else if (playMode == PLAYMODE_EDIT)    str = "> Editing <";
					else if (playMode == PLAYMODE_RECSONG) str = "> Rec. sng. <";
					else if (playMode == PLAYMODE_RECPATT) str = "> Rec. ptn. <";

					uint16_t areaWidth = 102;
					uint16_t maxStrWidth = 76; // wide enough
					uint16_t x = 101;
					uint16_t y = 80;

					if (ui.extendedPatternEditor)
					{
						y = 56;
						areaWidth = 443;
					}

					// clear area
					uint16_t clrX = x + ((areaWidth - maxStrWidth) / 2);
					fillRect(clrX, y, maxStrWidth, FONT1_CHAR_H+1, PAL_DESKTOP);

					// draw text (if needed)
					if (str != NULL)
					{
						x += (areaWidth - textWidth(str)) / 2;
						textOut(x, y, PAL_FORGRND, str);
					}
				}
			}

			if (ui.updatePosEdScrollBar)
			{
				ui.updatePosEdScrollBar = false;
				setScrollBarPos(SB_POS_ED, song.songPos, false);
				setScrollBarEnd(SB_POS_ED, (song.songLength - 1) + 5);
			}

			if (!ui.diskOpShown)
				drawPlaybackTime();

			if (!ui.extendedPatternEditor)
			{
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
				drawSongBPM(editor.BPM);
			}
			
			if (ui.drawSpeedFlag)
			{
				ui.drawSpeedFlag = false;
				drawSongSpeed(editor.speed);
			}

			if (ui.drawGlobVolFlag)
			{
				ui.drawGlobVolFlag = false;
				drawGlobalVol(editor.globalVolume);
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
			writePattern(editor.row, editor.editPattern);
	}
}
