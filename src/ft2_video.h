#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_palette.h"
#include "ft2_audio.h"
#include "ft2_hpc.h"

enum
{
	SPRITE_LEFT_LOOP_PIN = 0,
	SPRITE_RIGHT_LOOP_PIN = 1,
	SPRITE_TEXT_CURSOR = 2,
	SPRITE_MOUSE_POINTER = 3, // priority above all other sprites

	SPRITE_NUM
};

typedef struct video_t
{
	bool fullscreen, showFPSCounter, useCustomRenderRect, vsync60HzPresent, windowHidden;
	uint8_t windowModeUpscaleFactor;
	int32_t renderX, renderY, renderW, renderH, displayW, displayH, windowW, windowH, displayIndex;
	uint32_t mouseCursorUpscaleFactor, *frameBuffer, palette[PAL_NUM];
	double dMonitorRefreshRate, dDpiZoomFactorX, dDpiZoomFactorY, widthRatio, heightRatio;
#ifdef _WIN32
	HWND hWnd;
#endif
	hpc_t vblankHpc;
	SDL_Window *window;
	SDL_Rect renderRect;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Surface *iconSurface;
} video_t;

typedef struct
{
	uint32_t *refreshBuffer;
	const uint8_t *data;
	bool visible;
	int16_t newX, newY, x, y;
	uint16_t w, h;
} sprite_t;

extern video_t video; // ft2_video.c

void resetFPSCounter(void);
void beginFPSCounter(void);
void endFPSCounter(void);
void flipFrame(void);
void showErrorMsgBox(const char *fmt, ...);
void updateWindowTitle(bool forceUpdate);
void handleScopesFromChQueue(chSyncData_t *chSyncData, uint8_t *scopeUpdateStatus);
bool initWindow(void);
void resizeWindow(uint32_t w, uint32_t h);
void updateWindowRenderSize(void);
bool setupRenderer(void);
void closeVideo(void);
void setLeftLoopPinState(bool clicked);
void setRightLoopPinState(bool clicked);
int32_t getSpritePosX(int32_t sprite);
void eraseSprites(void);
void renderLoopPins(void);
void renderSprites(void);
bool setupSprites(void);
void freeSprites(void);
void setSpritePos(int32_t sprite, int32_t x, int32_t y);
void changeSpriteData(int32_t sprite, const uint8_t *data);
void hideSprite(int32_t sprite);
void handleRedrawing(void);
void enterFullscreen(void);
void leaveFullscreen(void);
bool recreateTexture(void);
void toggleFullscreen(void);
