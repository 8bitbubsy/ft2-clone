// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_scopes.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_pattern_ed.h"
#include "ft2_mouse.h"
#include "ft2_config.h"
#include "ft2_diskop.h"
#include "ft2_gfxdata.h"
#include "ft2_audioselector.h"
#include "ft2_midi.h"
#include "ft2_gfxdata.h"

#define NUM_CURSORS 6

static bool mouseBusyGfxBackwards;
static int16_t mouseShape;
static int32_t mouseModeGfxOffs, mouseBusyGfxFrame;
static SDL_Cursor *cursors[NUM_CURSORS];

static bool setSystemCursor(SDL_Cursor *cursor)
{
	if (cursor == NULL)
	{
		SDL_SetCursor(SDL_GetDefaultCursor());
		return false;
	}

	SDL_SetCursor(cursor);
	return true;
}

void freeMouseCursors(void)
{
	SDL_SetCursor(SDL_GetDefaultCursor());
	for (uint32_t i = 0; i < NUM_CURSORS; i++)
	{
		if (cursors[i] != NULL)
		{
			SDL_FreeCursor(cursors[i]);
			cursors[i] = NULL;
		}
	}
}

bool createMouseCursors(void) // creates scaled SDL surfaces for current mouse pointer shape
{
	freeMouseCursors();

	const uint8_t *cursorsSrc = mouseCursors;
	switch (config.mouseType)
	{
		case MOUSE_IDLE_SHAPE_NICE:   cursorsSrc += 0 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_UGLY:   cursorsSrc += 1 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_AWFUL:  cursorsSrc += 2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_USABLE: cursorsSrc += 3 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		default: break;
	}

	for (uint32_t i = 0; i < NUM_CURSORS; i++)
	{
		SDL_Surface *surface = SDL_CreateRGBSurface(0, MOUSE_CURSOR_W*video.yScale, MOUSE_CURSOR_H*video.yScale, 32, 0, 0, 0, 0);
		if (surface == NULL)
		{
			freeMouseCursors();
			config.specialFlags2 &= ~HARDWARE_MOUSE; // enable software mouse
			return false;
		}

		uint32_t colorkey = SDL_MapRGB(surface->format, 0x00, 0xFF, 0x00); // colorkey
		uint32_t fg = SDL_MapRGB(surface->format, 0xFF, 0xFF, 0xFF); // foreground
		uint32_t border = SDL_MapRGB(surface->format, 0x00, 0x00, 0x00); // border

		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
		SDL_SetColorKey(surface, SDL_TRUE, colorkey);
		SDL_SetSurfaceRLE(surface, SDL_TRUE);

		const uint8_t *srcPixels8;
		if (i == 3) // text edit cursor
			srcPixels8 = &mouseCursors[12 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
		else if (i == 4) // mouse busy (wall clock)
			srcPixels8 = &mouseCursorBusyClock[2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]; // pick a good still-frame
		else if (i == 5) // mouse busy (hourglass)
			srcPixels8 = &mouseCursorBusyGlass[2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]; // pick a good still-frame
		else // normal idle cursor + disk op. "delete/rename" cursors
			srcPixels8 = &cursorsSrc[i * (4 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H))];

		SDL_LockSurface(surface);

		uint32_t *dstPixels32 = (uint32_t *)surface->pixels;

		for (int32_t k = 0; k < surface->w*surface->h; k++) // fill surface with colorkey pixels
			dstPixels32[k] = colorkey;

		// blit upscaled cursor to surface
		for (uint32_t y = 0; y < MOUSE_CURSOR_H; y++)
		{
			uint32_t *outX = &dstPixels32[(y * video.yScale) * surface->w];
			for (uint32_t yScale = 0; yScale < video.yScale; yScale++)
			{
				for (uint32_t x = 0; x < MOUSE_CURSOR_W; x++)
				{
					uint8_t srcPix = srcPixels8[(y * MOUSE_CURSOR_W) + x];
					if (srcPix != PAL_TRANSPR)
					{
						uint32_t pixel = colorkey;
						if (srcPix == PAL_MOUSEPT)
							pixel = fg;
						else if (srcPix == PAL_BCKGRND)
							pixel = border;

						for (uint32_t xScale = 0; xScale < video.yScale; xScale++)
							outX[xScale] = pixel;
					}

					outX += video.xScale;
				}
			}
		}
		SDL_UnlockSurface(surface);

		uint32_t hotX = 0;
		uint32_t hotY = 0;

		if (i == 3) // text edit cursor bias
		{
			hotX = 2 * video.xScale;
			hotY = 6 * video.yScale;
		}

		cursors[i] = SDL_CreateColorCursor(surface, hotX, hotY);
		if (cursors[i] == NULL)
		{
			SDL_FreeSurface(surface);
			freeMouseCursors();
			config.specialFlags2 &= ~HARDWARE_MOUSE; // enable software mouse
			return false;
		}

		SDL_FreeSurface(surface);
	}

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		     if (mouse.mode == MOUSE_MODE_NORMAL) setSystemCursor(cursors[0]);
		else if (mouse.mode == MOUSE_MODE_DELETE) setSystemCursor(cursors[1]);
		else if (mouse.mode == MOUSE_MODE_RENAME) setSystemCursor(cursors[2]);
	}

	return true;
}

void setMousePosToCenter(void)
{
	if (video.fullscreen)
	{
		mouse.setPosX = video.displayW / 2;
		mouse.setPosY = video.displayH / 2;
	}
	else
	{
		mouse.setPosX = video.renderW / 2;
		mouse.setPosY = video.renderH / 2;
	}

	mouse.setPosFlag = true;
}

void animateBusyMouse(void)
{
	if (config.mouseAnimType == MOUSE_BUSY_SHAPE_CLOCK)
	{
		if (config.specialFlags2 & HARDWARE_MOUSE)
		{
			setSystemCursor(cursors[4]);
			return;
		}

		if ((editor.framesPassed % 7) == 6)
		{
			if (mouseBusyGfxBackwards)
			{
				if (--mouseBusyGfxFrame <= 0)
				{
					mouseBusyGfxFrame = 0;
					mouseBusyGfxBackwards = false;
				}
			}
			else
			{
				if (++mouseBusyGfxFrame >= MOUSE_CLOCK_ANI_FRAMES-1)
				{
					mouseBusyGfxFrame = MOUSE_CLOCK_ANI_FRAMES - 1;
					mouseBusyGfxBackwards = true;
				}
			}

			changeSpriteData(SPRITE_MOUSE_POINTER,
				&mouseCursorBusyClock[(mouseBusyGfxFrame % MOUSE_CLOCK_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]);
		}
	}
	else
	{
		if (config.specialFlags2 & HARDWARE_MOUSE)
		{
			setSystemCursor(cursors[5]);
			return;
		}

		if ((editor.framesPassed % 5) == 4)
		{
			mouseBusyGfxFrame = (mouseBusyGfxFrame + 1) % MOUSE_GLASS_ANI_FRAMES;

			changeSpriteData(SPRITE_MOUSE_POINTER,
				&mouseCursorBusyGlass[mouseBusyGfxFrame * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]);
		}
	}
}

void setMouseShape(int16_t shape)
{
	const uint8_t *gfxPtr;

	if (editor.busy)
	{
		if (config.mouseAnimType == MOUSE_BUSY_SHAPE_CLOCK)
			gfxPtr = &mouseCursorBusyClock[(mouseBusyGfxFrame % MOUSE_GLASS_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
		else
			gfxPtr = &mouseCursorBusyGlass[(mouseBusyGfxFrame % MOUSE_CLOCK_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
	}
	else
	{
		gfxPtr = &mouseCursors[mouseModeGfxOffs];
		switch (shape)
		{
			case MOUSE_IDLE_SHAPE_NICE:   gfxPtr += 0  * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_UGLY:   gfxPtr += 1  * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_AWFUL:  gfxPtr += 2  * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_USABLE: gfxPtr += 3  * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_TEXT_EDIT:    gfxPtr += 12 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			default: return;
		}
	}

	mouseShape = shape;
	changeSpriteData(SPRITE_MOUSE_POINTER, gfxPtr);

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		     if (mouse.mode == MOUSE_MODE_NORMAL) setSystemCursor(cursors[0]);
		else if (mouse.mode == MOUSE_MODE_DELETE) setSystemCursor(cursors[1]);
		else if (mouse.mode == MOUSE_MODE_RENAME) setSystemCursor(cursors[2]);
	}
}

static void setTextEditMouse(void)
{
	setMouseShape(MOUSE_IDLE_TEXT_EDIT);
	mouse.xBias = -2;
	mouse.yBias = -6;

	if (config.specialFlags2 & HARDWARE_MOUSE)
		setSystemCursor(cursors[3]);
}

static void clearTextEditMouse(void)
{
	setMouseShape(config.mouseType);
	mouse.xBias = 0;
	mouse.yBias = 0;

	if (config.specialFlags2 & HARDWARE_MOUSE)
		setSystemCursor(cursors[0]);
}

static void changeCursorIfOverTextBoxes(void)
{
	int16_t i, mx, my;
	textBox_t *t;

	mouse.mouseOverTextBox = false;
	if (editor.busy || mouse.mode != MOUSE_MODE_NORMAL)
		return;

	mx = mouse.x;
	my = mouse.y;

	for (i = 0; i < NUM_TEXTBOXES; i++)
	{
		if (editor.ui.sysReqShown && i > 0)
			continue;

		t = &textBoxes[i];
		if (!t->visible)
			continue;

		if (!t->changeMouseCursor && (!editor.editTextFlag || i != mouse.lastEditBox))
			continue; // some kludge of sorts

		if (my >= t->y && my < t->y+t->h && mx >= t->x && mx < t->x+t->w)
		{
			mouse.mouseOverTextBox = true;
			setTextEditMouse();
			return;
		}
	}

	// we're not inside a text edit box, set back mouse cursor
	if (i == NUM_TEXTBOXES && mouseShape == MOUSE_IDLE_TEXT_EDIT)
		clearTextEditMouse();
}

void setMouseMode(uint8_t mode)
{
	switch (mode)
	{
		case MOUSE_MODE_NORMAL: { mouse.mode = mode; mouseModeGfxOffs = 0 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); } break;
		case MOUSE_MODE_DELETE: { mouse.mode = mode; mouseModeGfxOffs = 4 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); } break;
		case MOUSE_MODE_RENAME: { mouse.mode = mode; mouseModeGfxOffs = 8 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); } break;

		default: return;
	}

	setMouseShape(config.mouseType);
}

void resetMouseBusyAnimation(void)
{
	mouseBusyGfxBackwards = false;
	mouseBusyGfxFrame = 0;
}

void setMouseBusy(bool busy) // can be called from other threads
{
	if (busy)
	{
		editor.ui.setMouseIdle = false;
		editor.ui.setMouseBusy = true;
	}
	else
	{
		editor.ui.setMouseBusy = false;
		editor.ui.setMouseIdle = true;
	}
}

void mouseAnimOn(void)
{
	editor.ui.setMouseBusy = false;
	editor.ui.setMouseIdle = false;

	editor.busy = true;
	setMouseShape(config.mouseAnimType);

	//if (config.specialFlags2 & HARDWARE_MOUSE && cBusy != NULL)
	//	SDL_SetCursor(cBusy);
}

void mouseAnimOff(void)
{
	editor.ui.setMouseBusy = false;
	editor.ui.setMouseIdle = false;

	editor.busy = false;
	setMouseShape(config.mouseType);

	//if (config.specialFlags2 & HARDWARE_MOUSE && cArrow != NULL)
	//	SDL_SetCursor(cArrow);
}

static void mouseWheelDecRow(void)
{
	int16_t pattPos;

	if (songPlaying)
		return;

	pattPos = editor.pattPos - 1;
	if (pattPos < 0)
		pattPos = pattLens[editor.editPattern] - 1;

	setPos(-1, pattPos, true);
}

static void mouseWheelIncRow(void)
{
	int16_t pattPos;

	if (songPlaying)
		return;

	pattPos = editor.pattPos + 1;
	if (pattPos > (pattLens[editor.editPattern] - 1))
		pattPos = 0;

	setPos(-1, pattPos, true);
}

void mouseWheelHandler(bool directionUp)
{
	if (editor.ui.sysReqShown || editor.editTextFlag)
		return;

	if (editor.ui.extended)
	{
		if (mouse.y <= 52)
		{
			     if (mouse.x <= 111) directionUp ? decSongPos() : incSongPos();
			else if (mouse.x >= 386) directionUp ?  decCurIns() :  incCurIns();
		}
		else
		{
			directionUp ? mouseWheelDecRow() : mouseWheelIncRow();
		}

		return;
	}

	if (mouse.y < 173)
	{
		// top screens

		if (editor.ui.helpScreenShown)
		{
			// help screen

			if (directionUp)
			{
				helpScrollUp();
				helpScrollUp();
			}
			else
			{
				helpScrollDown();
				helpScrollDown();
			}
		}
		else if (editor.ui.diskOpShown)
		{
			// disk op - 3x speed
			if (mouse.x <= 355)
			{
				if (directionUp)
				{
					pbDiskOpListUp();
					pbDiskOpListUp();
					pbDiskOpListUp();
				}
				else
				{
					pbDiskOpListDown();
					pbDiskOpListDown();
					pbDiskOpListDown();
				}
			}
		}
		else if (editor.ui.configScreenShown)
		{
			if (editor.currConfigScreen == CONFIG_SCREEN_IO_DEVICES)
			{
				// audio device selectors
				if (mouse.x >= 110 && mouse.x <= 355 && mouse.y <= 173)
				{
					if (mouse.y < 87)
						directionUp ? scrollAudOutputDevListUp() : scrollAudOutputDevListDown();
					else
						directionUp ? scrollAudInputDevListUp() : scrollAudInputDevListDown();
				}
			}
			else if (editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
			{
				// midi input device selector
				if (mouse.x >= 110 && mouse.x <= 503 && mouse.y <= 173)
					directionUp ? scrollMidiInputDevListUp() : scrollMidiInputDevListDown();
			}
		}

		if (!editor.ui.aboutScreenShown  && !editor.ui.helpScreenShown &&
			!editor.ui.configScreenShown && !editor.ui.nibblesShown)
		{
			if (mouse.x >= 421 && mouse.y <= 173)
			{
				     if (mouse.y <= 93) directionUp ? decCurIns() : incCurIns();
				else if (mouse.y >= 94) directionUp ? decCurSmp() : incCurSmp();
			}
			else if (!editor.ui.diskOpShown && mouse.x <= 111 && mouse.y <= 76)
			{
				directionUp ? decSongPos() : incSongPos();
			}
		}
	}
	else
	{
		// bottom screens

		if (editor.ui.sampleEditorShown)
		{
			if (mouse.y >= 174 && mouse.y <= 328)
				directionUp ? mouseZoomSampleDataIn() : mouseZoomSampleDataOut();
		}
		else if (editor.ui.patternEditorShown)
		{
			directionUp ? mouseWheelDecRow() : mouseWheelIncRow();
		}
	}
}

static bool testSamplerDataMouseDown(void)
{
	if (editor.ui.sampleEditorShown && mouse.y >= 174 && mouse.y <= 327 && editor.ui.sampleDataOrLoopDrag == -1)
	{
		handleSampleDataMouseDown(false);
		return true;
	}

	return false;
}

static bool testPatternDataMouseDown(void)
{
	uint16_t y1, y2;

	if (editor.ui.patternEditorShown)
	{
		y1 = editor.ui.extended ? 56 : 176;
		y2 = editor.ui.pattChanScrollShown ? 382 : 396;

		if (mouse.y >= y1 && mouse.y <= y2 && mouse.x >= 29 && mouse.x <= 602)
		{
			handlePatternDataMouseDown(false);
			return true;
		}
	}

	return false;
}

void mouseButtonUpHandler(uint8_t mouseButton)
{
#ifndef __APPLE__
	if (!video.fullscreen) // release mouse button trap
		SDL_SetWindowGrab(video.window, SDL_FALSE);
#endif

	if (mouseButton == SDL_BUTTON_LEFT)
	{
		mouse.leftButtonPressed = false;
		mouse.leftButtonReleased = true;

		if (editor.ui.leftLoopPinMoving)
		{
			setLeftLoopPinState(false);
			editor.ui.leftLoopPinMoving = false;
		}

		if (editor.ui.rightLoopPinMoving)
		{
			setRightLoopPinState(false);
			editor.ui.rightLoopPinMoving = false;
		}
	}
	else if (mouseButton == SDL_BUTTON_RIGHT)
	{
		mouse.rightButtonPressed = false;
		mouse.rightButtonReleased = true;

		
		if (editor.editSampleFlag)
		{
			// right mouse button released after hand-editing sample data
			if (instr[editor.curInstr] != NULL)
				fixSample(&instr[editor.curInstr]->samp[editor.curSmp]);

			resumeAudio();

			if (editor.ui.sampleEditorShown)
				writeSample(true);

			setSongModifiedFlag();
			editor.editSampleFlag = false;
		}

	}

	mouse.firstTimePressingButton = false;
	mouse.buttonCounter = 0;
	editor.textCursorBlinkCounter = 0;

	// if we used both mouse button at the same time and released *one*, don't release GUI object
	if ( mouse.leftButtonPressed && !mouse.rightButtonPressed) return;
	if (!mouse.leftButtonPressed &&  mouse.rightButtonPressed) return;

	if (editor.ui.sampleEditorShown)
		testSmpEdMouseUp();

	mouse.lastX = 0;
	mouse.lastY = 0;

	editor.ui.sampleDataOrLoopDrag = -1;

	// check if we released a GUI object
	testDiskOpMouseRelease();
	testPushButtonMouseRelease(true);
	testCheckBoxMouseRelease();
	testScrollBarMouseRelease();
	testRadioButtonMouseRelease();

	// revert "delete/rename" mouse modes (disk op.)
	if (mouse.lastUsedObjectID != PB_DISKOP_DELETE && mouse.lastUsedObjectID != PB_DISKOP_RENAME)
	{
		if (mouse.mode != MOUSE_MODE_NORMAL)
			setMouseMode(MOUSE_MODE_NORMAL);
	}

	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
}

void mouseButtonDownHandler(uint8_t mouseButton)
{
#ifndef __APPLE__
	if (!video.fullscreen) // trap mouse pointer while holding down left and/or right button
		SDL_SetWindowGrab(video.window, SDL_TRUE);
#endif

	// if already holding left button and clicking right, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_RIGHT && mouse.leftButtonPressed)
	{
		if (editor.ui.sampleDataOrLoopDrag == -1)
		{
			mouse.rightButtonPressed = true;
			mouse.rightButtonReleased = false;
		}

		// kludge - we must do scope solo/unmute all here
		if (!editor.ui.sysReqShown)
			testScopesMouseDown();

		return;
	}

	// if already holding right button and clicking left, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_LEFT && mouse.rightButtonPressed)
	{
		if (editor.ui.sampleDataOrLoopDrag == -1)
		{
			mouse.leftButtonPressed = true;
			mouse.leftButtonReleased = false;
		}

		// kludge - we must do scope solo/unmute all here
		if (!editor.ui.sysReqShown)
			testScopesMouseDown();

		return;
	}

	     if (mouseButton == SDL_BUTTON_LEFT)  mouse.leftButtonPressed = true;
	else if (mouseButton == SDL_BUTTON_RIGHT) mouse.rightButtonPressed = true;

	mouse.leftButtonReleased = false;
	mouse.rightButtonReleased = false;

	// mouse 0,0 = open exit dialog
	if (mouse.x == 0 && mouse.y == 0 && quitBox(false) == 1)
	{
		editor.throwExit = true;
		return;
	}

	// don't do mouse down testing here if we already are using an object
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return;

	// kludge #2
	if (mouse.lastUsedObjectType != OBJECT_PUSHBUTTON && mouse.lastUsedObjectID != OBJECT_ID_NONE)
		return;

	// kludge #3
	if (!mouse.rightButtonPressed)
		mouse.lastUsedObjectID = OBJECT_ID_NONE;

	// check if we pressed a GUI object

	/* test objects like this - clickable things *never* overlap, so no need to test all
	** other objects if we clicked on one already */

	testInstrSwitcherMouseDown(); // kludge: allow right click to both change ins. and edit text

	if (testTextBoxMouseDown())     return;
	if (testPushButtonMouseDown())  return;
	if (testCheckBoxMouseDown())    return;
	if (testScrollBarMouseDown())   return;
	if (testRadioButtonMouseDown()) return;

	// from this point, we don't need to test more widgets if a system request box is shown
	if (editor.ui.sysReqShown)
		return;

	if (testInstrVolEnvMouseDown(false))    return;
	if (testInstrPanEnvMouseDown(false))    return;
	if (testDiskOpMouseDown(false))         return;
	if (testPianoKeysMouseDown(false))      return;
	if (testSamplerDataMouseDown())         return;
	if (testPatternDataMouseDown())         return;
	if (testScopesMouseDown())              return;
	if (testAudioDeviceListsMouseDown())    return;
	if (testMidiInputDeviceListMouseDown()) return;
}

void handleLastGUIObjectDown(void)
{
	if (mouse.lastUsedObjectType == OBJECT_NONE)
		return;

	if (mouse.leftButtonPressed || mouse.rightButtonPressed)
	{
		if (mouse.lastUsedObjectID != OBJECT_ID_NONE)
		{
			switch (mouse.lastUsedObjectType)
			{
				case OBJECT_PUSHBUTTON:  handlePushButtonsWhileMouseDown();  break;
				case OBJECT_RADIOBUTTON: handleRadioButtonsWhileMouseDown(); break;
				case OBJECT_CHECKBOX:    handleCheckBoxesWhileMouseDown();   break;
				case OBJECT_SCROLLBAR:   handleScrollBarsWhileMouseDown();   break;
				case OBJECT_TEXTBOX:     handleTextBoxWhileMouseDown();      break;
				default: break;
			}
		}
		else
		{
			// test non-standard GUI elements
			switch (mouse.lastUsedObjectType)
			{
				case OBJECT_INSTRSWITCH: testInstrSwitcherMouseDown();       break;
				case OBJECT_PATTERNMARK: handlePatternDataMouseDown(true);   break;
				case OBJECT_DISKOPLIST:  testDiskOpMouseDown(true);          break;
				case OBJECT_SMPDATA:     handleSampleDataMouseDown(true);    break;
				case OBJECT_PIANO:       testPianoKeysMouseDown(true);       break;
				case OBJECT_INSVOLENV:   testInstrVolEnvMouseDown(true);     break;
				case OBJECT_INSPANENV:   testInstrPanEnvMouseDown(true);     break;
				default: break;
			}
		}
	}
}

void updateMouseScaling(void)
{
	video.dMouseXMul = (double)SCREEN_W / video.renderW;
	video.dMouseYMul = (double)SCREEN_H / video.renderH;
}

void readMouseXY(void)
{
	int16_t x, y;
	int32_t mx, my;

	if (mouse.setPosFlag)
	{
		mouse.setPosFlag = false;

		if (SDL_GetWindowFlags(video.window) & SDL_WINDOW_SHOWN)
			SDL_WarpMouseInWindow(video.window, mouse.setPosX, mouse.setPosY);

		return;
	}

	SDL_PumpEvents(); // gathers all pending input from devices into the event queue (less mouse lag)
	SDL_GetMouseState(&mx, &my);

	/* in centered fullscreen mode, trap the mouse inside the framed image
	** and subtract the coords to match the OS mouse position (fixes touch from touchscreens) */
	if (video.fullscreen && !(config.windowFlags & FILTERING))
	{
		if (mx < video.renderX)
		{
			mx = video.renderX;
			SDL_WarpMouseInWindow(video.window, mx, my);
		}
		else if (mx >= video.renderX+video.renderW)
		{
			mx = (video.renderX + video.renderW) - 1;
			SDL_WarpMouseInWindow(video.window, mx, my);
		}

		if (my < video.renderY)
		{
			my = video.renderY;
			SDL_WarpMouseInWindow(video.window, mx, my);
		}
		else if (my >= video.renderY+video.renderH)
		{
			my = (video.renderY + video.renderH) - 1;
			SDL_WarpMouseInWindow(video.window, mx, my);
		}

		mx -= video.renderX;
		my -= video.renderY;
	}

	if (mx < 0) mx = 0;
	if (my < 0) mx = 0;

	// multiply coords by video upscaling factors (don't round)
	mx = (uint32_t)(mx * video.dMouseXMul);
	my = (uint32_t)(my * video.dMouseYMul);

	if (mx >= SCREEN_W) mx = SCREEN_W - 1;
	if (my >= SCREEN_H) my = SCREEN_H - 1;

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		// hardware mouse (OS)
		mouse.x = (int16_t)mx;
		mouse.y = (int16_t)my;
		hideSprite(SPRITE_MOUSE_POINTER);
	}
	else
	{
		// software mouse (FT2 mouse)
		x = (int16_t)mx;
		y = (int16_t)my;

		mouse.x = x;
		mouse.y = y;

		// for text editing cursor (do this after clamp)
		x += mouse.xBias;
		y += mouse.yBias;

		if (x < 0) x = 0;
		if (y < 0) y = 0;

		setSpritePos(SPRITE_MOUSE_POINTER, x, y);
	}

	changeCursorIfOverTextBoxes();
}
