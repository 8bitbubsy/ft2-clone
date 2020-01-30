#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_palette.h"
#include "ft2_audio.h"

enum
{
	SPRITE_LEFT_LOOP_PIN = 0,
	SPRITE_RIGHT_LOOP_PIN = 1,
	SPRITE_TEXT_CURSOR = 2,
	SPRITE_MOUSE_POINTER = 3, // priority above all other sprites

	SPRITE_NUM
};

struct video_t
{
	bool fullscreen, showFPSCounter;
	uint32_t xScale, yScale;
	uint32_t *frameBuffer, palette[PAL_NUM], vblankTimeLen, vblankTimeLenFrac;
#ifdef _WIN32
	HWND hWnd;
#endif
	SDL_Window *window;
	double dMonitorRefreshRate, dMouseXMul, dMouseYMul;
	uint8_t upscaleFactor;
	bool vsync60HzPresent;
	int32_t renderX, renderY, renderW, renderH, displayW, displayH;
	uint32_t *frameBufferUnaligned;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Surface *iconSurface;
} video;

typedef struct
{
	uint32_t *refreshBuffer;
	const uint8_t *data;
	bool visible;
	int16_t newX, newY, x, y;
	uint16_t w, h;
} sprite_t;

void resetFPSCounter(void);
void beginFPSCounter(void);
void endFPSCounter(void);
void flipFrame(void);
void showErrorMsgBox(const char *fmt, ...);
void updateWindowTitle(bool forceUpdate);
void handleScopesFromChQueue(chSyncData_t *chSyncData, uint8_t *scopeUpdateStatus);
bool setupWindow(void);
bool setupRenderer(void);
void closeVideo(void);
void setLeftLoopPinState(bool clicked);
void setRightLoopPinState(bool clicked);
int32_t getSpritePosX(uint8_t sprite);
void eraseSprites(void);
void renderLoopPins(void);
void renderSprites(void);
bool setupSprites(void);
void freeSprites(void);
void setSpritePos(uint8_t sprite, int16_t x, int16_t y);
void changeSpriteData(uint8_t sprite, const uint8_t *data);
void hideSprite(uint8_t sprite);
void handleRedrawing(void);
void enterFullscreen(void);
void leaveFullScreen(void);
void setWindowSizeFromConfig(bool updateRenderer);
bool recreateTexture(void);
void toggleFullScreen(void);
void setupWaitVBL(void);
void waitVBL(void);
