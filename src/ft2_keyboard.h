#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

struct keyb_t
{
	uint8_t keyRepeat, keyPressed;
	bool ignoreCurrKeyUp, ignoreTextEditKey, numPadPlusPressed;
	bool keyModifierDown, commandPressed, leftCommandPressed;
	bool leftShiftPressed, leftCtrlPressed, ctrlPressed, leftAltPressed;
} keyb;

int8_t scancodeKeyToNote(SDL_Scancode scancode);
void keyUpHandler(SDL_Scancode scancode, SDL_Keycode keycode);
void keyDownHandler(SDL_Scancode scancode, SDL_Keycode keycode, bool keyWasRepeated);
void readKeyModifiers(void);
