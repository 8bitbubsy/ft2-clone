// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "scopes/ft2_scopes.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_pattern_ed.h"
#include "ft2_mouse.h"
#include "ft2_config.h"
#include "ft2_diskop.h"
#include "ft2_audioselector.h"
#include "ft2_midi.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

#define NUM_CURSORS 6

mouse_t mouse; // globalized

static bool mouseBusyGfxBackwards;
static int16_t mouseShape;
static int32_t mouseModeGfxOffs, mouseBusyGfxFrame;
static SDL_Cursor *cursors[NUM_CURSORS];

static bool setSystemCursor(SDL_Cursor *cur)
{
	if (config.specialFlags2 & USE_OS_MOUSE_POINTER)
	{
		SDL_SetCursor(SDL_GetDefaultCursor());
		return true;
	}

	if (cur == NULL)
	{
		SDL_SetCursor(SDL_GetDefaultCursor());
		return false;
	}

	SDL_SetCursor(cur);
	return true;
}

void freeMouseCursors(void)
{
	SDL_SetCursor(SDL_GetDefaultCursor());
	for (int32_t i = 0; i < NUM_CURSORS; i++)
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

	const uint8_t *cursorsSrc = bmp.mouseCursors;
	switch (config.mouseType)
	{
		case MOUSE_IDLE_SHAPE_NICE: cursorsSrc += 0 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_UGLY: cursorsSrc += 1 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_AWFUL: cursorsSrc += 2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		case MOUSE_IDLE_SHAPE_USABLE: cursorsSrc += 3 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
		default: break;
	}

	for (int32_t i = 0; i < NUM_CURSORS; i++)
	{
		const int32_t scaleFactor = video.yScale;

		SDL_Surface *surface = SDL_CreateRGBSurface(0, MOUSE_CURSOR_W*scaleFactor, MOUSE_CURSOR_H*scaleFactor, 32, 0, 0, 0, 0);
		if (surface == NULL)
		{
			freeMouseCursors();

			if (config.specialFlags2 & USE_OS_MOUSE_POINTER)
			{
				SDL_ShowCursor(SDL_TRUE);
			}
			else
			{
				// enable software mouse
				config.specialFlags2 &= ~HARDWARE_MOUSE;
				SDL_ShowCursor(SDL_FALSE);
			}

			return false;
		}

		const uint32_t colorkey = SDL_MapRGB(surface->format, 0x00, 0xFF, 0x00); // colorkey
		const uint32_t fg = SDL_MapRGB(surface->format, 0xFF, 0xFF, 0xFF); // foreground
		const uint32_t border = SDL_MapRGB(surface->format, 0x00, 0x00, 0x00); // border

		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
		SDL_SetColorKey(surface, SDL_TRUE, colorkey);
		SDL_SetSurfaceRLE(surface, SDL_TRUE);

		const uint8_t *srcPixels8;
		if (i == 3) // text edit cursor
			srcPixels8 = &bmp.mouseCursors[12 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
		else if (i == 4) // mouse busy (wall clock)
			srcPixels8 = &bmp.mouseCursorBusyClock[2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]; // pick a good still-frame
		else if (i == 5) // mouse busy (hourglass)
			srcPixels8 = &bmp.mouseCursorBusyGlass[2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]; // pick a good still-frame
		else // normal idle cursor + disk op. "delete/rename" cursors
			srcPixels8 = &cursorsSrc[i * (4 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H))];

		SDL_LockSurface(surface);

		uint32_t *dstPixels32 = (uint32_t *)surface->pixels;
		for (int32_t k = 0; k < surface->w*surface->h; k++) // fill surface with colorkey pixels
			dstPixels32[k] = colorkey;

		// blit upscaled cursor to surface
		for (int32_t y = 0; y < MOUSE_CURSOR_H; y++)
		{
			uint32_t *outX = &dstPixels32[(y * scaleFactor) * surface->w];
			for (int32_t yScale = 0; yScale < scaleFactor; yScale++)
			{
				const uint8_t *srcPtr = &srcPixels8[y * MOUSE_CURSOR_W];
				for (int32_t x = 0; x < MOUSE_CURSOR_W; x++)
				{
					const uint8_t srcPix = srcPtr[x];
					if (srcPix != PAL_TRANSPR)
					{
						uint32_t pixel = colorkey;
						if (srcPix == PAL_MOUSEPT)
							pixel = fg;
						else if (srcPix == PAL_BCKGRND)
							pixel = border;

						for (int32_t xScale = 0; xScale < scaleFactor; xScale++)
							outX[xScale] = pixel;
					}

					outX += scaleFactor;
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

			if (config.specialFlags2 & USE_OS_MOUSE_POINTER)
			{
				SDL_ShowCursor(SDL_TRUE);
			}
			else
			{
				// enable software mouse
				config.specialFlags2 &= ~HARDWARE_MOUSE;
				SDL_ShowCursor(SDL_FALSE);
			}

			return false;
		}

		SDL_FreeSurface(surface);
	}

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		     if (mouse.mode == MOUSE_MODE_NORMAL) setSystemCursor(cursors[0]);
		else if (mouse.mode == MOUSE_MODE_DELETE) setSystemCursor(cursors[1]);
		else if (mouse.mode == MOUSE_MODE_RENAME) setSystemCursor(cursors[2]);

		SDL_ShowCursor(SDL_TRUE);
	}
	else
	{
		SDL_ShowCursor(SDL_FALSE);
	}

	return true;
}

void setMousePosToCenter(void)
{
	if (video.fullscreen)
	{
		mouse.setPosX = video.displayW >> 1;
		mouse.setPosY = video.displayH >> 1;
	}
	else
	{
		mouse.setPosX = video.renderW >> 1;
		mouse.setPosY = video.renderH >> 1;
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
				&bmp.mouseCursorBusyClock[(mouseBusyGfxFrame % MOUSE_CLOCK_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]);
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
				&bmp.mouseCursorBusyGlass[mouseBusyGfxFrame * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)]);
		}
	}
}

void setMouseShape(int16_t shape)
{
	const uint8_t *gfxPtr;

	if (editor.busy)
	{
		if (config.mouseAnimType == MOUSE_BUSY_SHAPE_CLOCK)
			gfxPtr = &bmp.mouseCursorBusyClock[(mouseBusyGfxFrame % MOUSE_GLASS_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
		else
			gfxPtr = &bmp.mouseCursorBusyGlass[(mouseBusyGfxFrame % MOUSE_CLOCK_ANI_FRAMES) * (MOUSE_CURSOR_W * MOUSE_CURSOR_H)];
	}
	else
	{
		gfxPtr = &bmp.mouseCursors[mouseModeGfxOffs];
		switch (shape)
		{
			case MOUSE_IDLE_SHAPE_NICE:   gfxPtr +=  0 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_UGLY:   gfxPtr +=  1 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_AWFUL:  gfxPtr +=  2 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
			case MOUSE_IDLE_SHAPE_USABLE: gfxPtr +=  3 * (MOUSE_CURSOR_W * MOUSE_CURSOR_H); break;
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
	int32_t i;

	mouse.mouseOverTextBox = false;
	if (editor.busy || mouse.mode != MOUSE_MODE_NORMAL)
		return;

	const int32_t mx = mouse.x;
	const int32_t my = mouse.y;

	textBox_t *t = textBoxes;
	for (i = 0; i < NUM_TEXTBOXES; i++, t++)
	{
		if (ui.sysReqShown && i != 0) // Sys. Req can only have one (special) text box
			continue;

		if (!t->visible)
			continue;

		if (!t->changeMouseCursor && (!editor.editTextFlag || i != mouse.lastEditBox))
			continue; // some kludge of sorts

		if (my >= t->y && my < t->y+t->h && mx >= t->x && mx < t->x+t->w)
		{
			mouse.mouseOverTextBox = true;

			if (mouseShape != MOUSE_IDLE_TEXT_EDIT)
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
		ui.setMouseIdle = false;
		ui.setMouseBusy = true;
	}
	else
	{
		ui.setMouseBusy = false;
		ui.setMouseIdle = true;
	}
}

void mouseAnimOn(void)
{
	ui.setMouseBusy = false;
	ui.setMouseIdle = false;

	editor.busy = true;
	setMouseShape(config.mouseAnimType);
}

void mouseAnimOff(void)
{
	ui.setMouseBusy = false;
	ui.setMouseIdle = false;

	editor.busy = false;
	setMouseShape(config.mouseType);
}

static void mouseWheelDecRow(void)
{
	if (songPlaying)
		return;

	int16_t row = editor.row - 1;
	if (row < 0)
		row = patternNumRows[editor.editPattern] - 1;

	setPos(-1, row, true);
}

static void mouseWheelIncRow(void)
{
	if (songPlaying)
		return;

	int16_t row = editor.row + 1;
	if (row >= patternNumRows[editor.editPattern])
		row = 0;

	setPos(-1, row, true);
}

void mouseWheelHandler(bool directionUp)
{
	if (ui.sysReqShown || editor.editTextFlag)
		return;

	if (ui.extended)
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

		if (ui.helpScreenShown)
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
		else if (ui.diskOpShown)
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
		else if (ui.configScreenShown)
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
#ifdef HAS_MIDI
			else if (editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
			{
				// midi input device selector
				if (mouse.x >= 110 && mouse.x <= 503 && mouse.y <= 173)
					directionUp ? scrollMidiInputDevListUp() : scrollMidiInputDevListDown();
			}
#endif
		}

		if (!ui.aboutScreenShown  && !ui.helpScreenShown &&
			!ui.configScreenShown && !ui.nibblesShown)
		{
			if (mouse.x >= 421 && mouse.y <= 173)
			{
				     if (mouse.y <= 93) directionUp ? decCurIns() : incCurIns();
				else if (mouse.y >= 94) directionUp ? decCurSmp() : incCurSmp();
			}
			else if (!ui.diskOpShown && mouse.x <= 111 && mouse.y <= 76)
			{
				directionUp ? decSongPos() : incSongPos();
			}
		}
	}
	else
	{
		// bottom screens

		if (ui.sampleEditorShown)
		{
			if (mouse.y >= 174 && mouse.y <= 328)
				directionUp ? mouseZoomSampleDataIn() : mouseZoomSampleDataOut();
		}
		else if (ui.patternEditorShown)
		{
			directionUp ? mouseWheelDecRow() : mouseWheelIncRow();
		}
	}
}

static bool testSamplerDataMouseDown(void)
{
	if (ui.sampleEditorShown && mouse.y >= 174 && mouse.y <= 327 && ui.sampleDataOrLoopDrag == -1)
	{
		handleSampleDataMouseDown(false);
		return true;
	}

	return false;
}

static bool testPatternDataMouseDown(void)
{
	if (ui.patternEditorShown)
	{
		const int32_t y1 = ui.extended ? 56 : 176;
		const int32_t y2 = ui.pattChanScrollShown ? 382 : 396;

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
	if (mouseButton == SDL_BUTTON_LEFT)
	{
		mouse.leftButtonPressed = false;
		mouse.leftButtonReleased = true;

		if (ui.leftLoopPinMoving)
		{
			setLeftLoopPinState(false);
			ui.leftLoopPinMoving = false;
		}

		if (ui.rightLoopPinMoving)
		{
			setRightLoopPinState(false);
			ui.rightLoopPinMoving = false;
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
				fixSample(&instr[editor.curInstr]->smp[editor.curSmp]);

			resumeAudio();

			if (ui.sampleEditorShown)
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

	if (ui.sampleEditorShown)
		testSmpEdMouseUp();

	mouse.lastX = 0;
	mouse.lastY = 0;

	ui.sampleDataOrLoopDrag = -1;

	// check if we released a GUI object
	testDiskOpMouseRelease();
	testPushButtonMouseRelease(true);
	testCheckBoxMouseRelease();
	testScrollBarMouseRelease();
	testRadioButtonMouseRelease();

	// revert "delete/rename" mouse modes (disk op.)
	if (mouse.mode != MOUSE_MODE_NORMAL)
	{
		if (mouse.lastUsedObjectID != PB_DISKOP_DELETE && mouse.lastUsedObjectID != PB_DISKOP_RENAME)
			setMouseMode(MOUSE_MODE_NORMAL);
	}

	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.lastUsedObjectType = OBJECT_NONE;
}

void mouseButtonDownHandler(uint8_t mouseButton)
{
	// if already holding left button and clicking right, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_RIGHT && mouse.leftButtonPressed)
	{
		if (ui.sampleDataOrLoopDrag == -1)
		{
			mouse.rightButtonPressed = true;
			mouse.rightButtonReleased = false;
		}

		// kludge - we must do scope solo/unmute all here
		if (!ui.sysReqShown)
			testScopesMouseDown();

		return;
	}

	// if already holding right button and clicking left, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_LEFT && mouse.rightButtonPressed)
	{
		if (ui.sampleDataOrLoopDrag == -1)
		{
			mouse.leftButtonPressed = true;
			mouse.leftButtonReleased = false;
		}

		// kludge - we must do scope solo/unmute all here
		if (!ui.sysReqShown)
			testScopesMouseDown();

		return;
	}

	// mouse 0,0 = open exit dialog (also make sure the test always works in fullscreen mode)
	if (mouse.x == 0 && mouse.y == 0 || (video.fullscreen && (video.renderX > 0 || video.renderY > 0) && (mouse.rawX == 0 && mouse.rawY == 0)))
	{
		if (quitBox(false) == 1)
			editor.throwExit = true;

		// release button presses from okBox()
		mouse.leftButtonPressed = false;
		mouse.rightButtonPressed = false;
		mouse.leftButtonReleased = false;
		mouse.rightButtonReleased = false;

		return;
	}

	if (mouseButton == SDL_BUTTON_LEFT)
		mouse.leftButtonPressed = true;
	else if (mouseButton == SDL_BUTTON_RIGHT)
		mouse.rightButtonPressed = true;

	mouse.leftButtonReleased = false;
	mouse.rightButtonReleased = false;

	// don't do mouse down testing here if we already are using an object
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return;

	// kludge #2
	if (mouse.lastUsedObjectType != OBJECT_PUSHBUTTON && mouse.lastUsedObjectID != OBJECT_ID_NONE)
		return;

	// kludge #3 :(
	if (!mouse.rightButtonPressed)
		mouse.lastUsedObjectID = OBJECT_ID_NONE;

	// check if we pressed a GUI object

	/* test objects like this - clickable things *never* overlap, so no need to test all
	** other objects if we clicked on one already
	*/

	testInstrSwitcherMouseDown(); // kludge: allow right click to both change ins. and edit text

	if (testTextBoxMouseDown()) return;
	if (testPushButtonMouseDown()) return;
	if (testCheckBoxMouseDown()) return;
	if (testScrollBarMouseDown()) return;
	if (testRadioButtonMouseDown()) return;

	// at this point, we don't need to test more widgets if a system request is shown
	if (ui.sysReqShown)
		return;

	if (testInstrVolEnvMouseDown(false)) return;
	if (testInstrPanEnvMouseDown(false)) return;
	if (testDiskOpMouseDown(false)) return;
	if (testPianoKeysMouseDown(false)) return;
	if (testSamplerDataMouseDown()) return;
	if (testPatternDataMouseDown()) return;
	if (testScopesMouseDown()) return;
	if (testAudioDeviceListsMouseDown()) return;

#ifdef HAS_MIDI
	if (testMidiInputDeviceListMouseDown()) return;
#endif
}

static void sendMouseButtonUpEvent(uint8_t button)
{
	SDL_Event event;

	memset(&event, 0, sizeof (event));
	event.type = SDL_MOUSEBUTTONUP;
	event.button.button = button;

	SDL_PushEvent(&event);
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
				case OBJECT_PUSHBUTTON: handlePushButtonsWhileMouseDown(); break;
				case OBJECT_RADIOBUTTON: handleRadioButtonsWhileMouseDown(); break;
				case OBJECT_CHECKBOX: handleCheckBoxesWhileMouseDown(); break;
				case OBJECT_SCROLLBAR: handleScrollBarsWhileMouseDown(); break;
				case OBJECT_TEXTBOX: handleTextBoxWhileMouseDown(); break;
				default: break;
			}
		}
		else
		{
			// test non-standard GUI elements
			switch (mouse.lastUsedObjectType)
			{
				case OBJECT_INSTRSWITCH: testInstrSwitcherMouseDown(); break;
				case OBJECT_PATTERNMARK: handlePatternDataMouseDown(true); break;
				case OBJECT_DISKOPLIST: testDiskOpMouseDown(true); break;
				case OBJECT_SMPDATA: handleSampleDataMouseDown(true); break;
				case OBJECT_PIANO: testPianoKeysMouseDown(true); break;
				case OBJECT_INSVOLENV: testInstrVolEnvMouseDown(true); break;
				case OBJECT_INSPANENV: testInstrPanEnvMouseDown(true); break;
				default: break;
			}
		}

		/* Hack to send "mouse button up" events if we released the mouse button(s)
		** outside of the window...
		*/
		if (mouse.x < 0 || mouse.x >= SCREEN_W || mouse.y < 0 || mouse.y >= SCREEN_H)
		{
			if (mouse.leftButtonPressed && !(mouse.buttonState & SDL_BUTTON_LMASK))
				sendMouseButtonUpEvent(SDL_BUTTON_LEFT);

			if (mouse.rightButtonPressed && !(mouse.buttonState & SDL_BUTTON_RMASK))
				sendMouseButtonUpEvent(SDL_BUTTON_RIGHT);
		}
	}
}

void updateMouseScaling(void)
{
	if (video.renderW > 0.0) video.fMouseXMul = (float)SCREEN_W / video.renderW;
	if (video.renderH > 0.0) video.fMouseYMul = (float)SCREEN_H / video.renderH;
}

void readMouseXY(void)
{
	int32_t mx, my, windowX, windowY;

	if (mouse.setPosFlag)
	{
		if (!video.windowHidden)
			SDL_WarpMouseInWindow(video.window, mouse.setPosX, mouse.setPosY);

		mouse.setPosFlag = false;
		return;
	}

	if (video.useDesktopMouseCoords)
	{
		mouse.buttonState = SDL_GetGlobalMouseState(&mx, &my);

		// convert desktop coords to window coords
		SDL_GetWindowPosition(video.window, &windowX, &windowY);
		mx -= windowX;
		my -= windowY;
	}
	else
	{
		// special mode for KMSDRM (XXX: Confirm that this still works...)
		mouse.buttonState = SDL_GetMouseState(&mx, &my);
	}

	mouse.rawX = mx;
	mouse.rawY = my;

	if (video.fullscreen)
	{
		// centered fullscreen mode (not stretched) needs further coord translation
		if (!(config.specialFlags2 & STRETCH_IMAGE))
		{
			// if software mouse is enabled, warp mouse inside render space
			if (!(config.specialFlags2 & HARDWARE_MOUSE))
			{
				bool warpMouse = false;

				if (mx < video.renderX)
				{
					mx = video.renderX;
					warpMouse = true;
				}
				else if (mx >= video.renderX+video.renderW)
				{
					mx = (video.renderX + video.renderW) - 1;
					warpMouse = true;
				}

				if (my < video.renderY)
				{
					my = video.renderY;
					warpMouse = true;
				}
				else if (my >= video.renderY+video.renderH)
				{
					my = (video.renderY + video.renderH) - 1;
					warpMouse = true;
				}

				if (warpMouse)
					SDL_WarpMouseInWindow(video.window, mx, my);
			}

			// convert fullscreen coords to window (centered image) coords
			mx -= video.renderX;
			my -= video.renderY;
		}
	}

	// multiply coords by video upscaling factors (don't round)
	mouse.x = (int32_t)(mx * video.fMouseXMul);
	mouse.y = (int32_t)(my * video.fMouseYMul);

	if (config.specialFlags2 & HARDWARE_MOUSE)
	{
		// hardware mouse mode (OS)
		hideSprite(SPRITE_MOUSE_POINTER);
	}
	else
	{
		// software mouse mode (FT2 mouse)
		setSpritePos(SPRITE_MOUSE_POINTER, mouse.x + mouse.xBias, mouse.y + mouse.yBias);
	}

	changeCursorIfOverTextBoxes();
}
