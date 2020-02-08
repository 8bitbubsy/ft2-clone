#include <stdint.h>
#include <stdbool.h>
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_keyboard.h"
#include "ft2_textboxes.h"
#include "ft2_video.h"

#define SYSTEM_REQUEST_H 67
#define SYSTEM_REQUEST_Y 249
#define SYSTEM_REQUEST_Y_EXT 91

#define NUM_SYSREQ_TYPES 10

static char *buttonText[NUM_SYSREQ_TYPES][5] =
{
	{ "OK", "","","","" },
	{ "OK", "Cancel", "","","" },
	{ "Yes", "No", "","","" },
	{ "=(", "Rules","","","" },
	{ "All", "Song", "Instruments", "Cancel", "" },
	{ "Read left", "Read right", "Convert", "", "" },
	{ "OK", "","","","" },
	{ "OK", "","","","" },
	{ "Sorry...", "","","","" },
	{ "Mono", "Stereo", "Cancel", "","" }
};

static SDL_Keycode shortCut[NUM_SYSREQ_TYPES][5] =
{
	{ SDLK_o, 0,      0,      0,      0 },
	{ SDLK_o, SDLK_c, 0,      0,      0 },
	{ SDLK_y, SDLK_n, 0,      0,      0 },
	{ SDLK_s, SDLK_r, 0,      0,      0 },
	{ SDLK_a, SDLK_s, SDLK_i, SDLK_c, 0 },
	{ SDLK_l, SDLK_r, SDLK_c, 0,      0 },
	{ SDLK_o, 0,      0,      0,      0 },
	{ SDLK_o, 0,      0,      0,      0 },
	{ SDLK_s, 0,      0,      0,      0 },
	{ SDLK_m, SDLK_s, SDLK_c, 0,      0 },
};

typedef struct quitType_t
{
	const char *text;
	uint8_t typ;
} quitType_t;

#define QUIT_MESSAGES 16

// 8bitbubsy: Removed the MS-DOS ones...
static quitType_t quitMessage[QUIT_MESSAGES] =
{
	{ "Do you really want to quit?", 2 },
	{ "Musicians, press >Cancel<.  Lamers, press >OK<", 1 },
	{ "Tired already?", 2 },
	{ "Dost thou wish to leave with such hasty abandon?", 2 },
	{ "So, you think you can quit this easily, huh?", 2 },
	{ "Hey, what is the matter? You are not quiting now, are you?", 2 },
	{ "Rome was not built in one day! Quit really?", 2 },
	{ "For Work and Worry, press YES.  For Delectation and Demos, press NO.", 2 },
	{ "Did you really press the right key?", 2 },
	{ "You are a lamer, aren't you? Press >OK< to confirm.", 1 },
	{ "Hope ya did some good. Press >OK< to quit.", 1 },
	{ "Quit? Only for a good reason you are allowed to press >OK<.", 1 },
	{ "Are we at the end of a Fasttracker round?", 2 },
	{ "Are you just another boring user?", 2 },
	{ "Hope you're doing the compulsory \"Exit ceremony\" before pressing >OK<.", 1 },
	{ "Fasttracker...", 3 }
};

static void drawWindow(uint16_t w)
{
	const uint16_t h = SYSTEM_REQUEST_H;
	uint16_t x, y;

	x = (SCREEN_W - w) / 2;
	y = editor.ui.extended ? 91 : SYSTEM_REQUEST_Y;

	// main fill
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// outer border
	vLine(x,         y,         h - 1, PAL_BUTTON1);
	hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
	vLine(x + w - 1, y,         h,     PAL_BUTTON2);
	hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);

	// inner border
	vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
	hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
	vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
	hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

	// title bottom line
	hLine(x + 3, y + 16, w - 6, PAL_BUTTON2);
	hLine(x + 3, y + 17, w - 6, PAL_BUTTON1);
}

static bool mouseButtonDownLogic(uint8_t mouseButton)
{
	// if already holding left button and clicking right, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_RIGHT && mouse.leftButtonPressed)
	{
		mouse.rightButtonPressed = true;
		return false;
	}

	// if already holding right button and clicking left, don't do mouse down handling
	if (mouseButton == SDL_BUTTON_LEFT && mouse.rightButtonPressed)
	{
		mouse.leftButtonPressed = true;
		return false;
	}

	if (mouseButton == SDL_BUTTON_LEFT)
		mouse.leftButtonPressed = true;
	else if (mouseButton == SDL_BUTTON_RIGHT)
		mouse.rightButtonPressed = true;

	// don't do mouse down testing here if we already are using an object
	if (mouse.lastUsedObjectType != OBJECT_NONE)
		return false;

	// kludge #2
	if (mouse.lastUsedObjectType != OBJECT_PUSHBUTTON && mouse.lastUsedObjectID != OBJECT_ID_NONE)
		return false;

	// kludge #3
	if (!mouse.rightButtonPressed)
		mouse.lastUsedObjectID = OBJECT_ID_NONE;

	return true;
}

static bool mouseButtonUpLogic(uint8_t mouseButton)
{
	if (mouseButton == SDL_BUTTON_LEFT)
		mouse.leftButtonPressed = false;
	else if (mouseButton == SDL_BUTTON_RIGHT)
		mouse.rightButtonPressed = false;

	editor.textCursorBlinkCounter = 0;

	// if we used both mouse button at the same time and released *one*, don't release GUI object
	if ( mouse.leftButtonPressed && !mouse.rightButtonPressed) return false;
	if (!mouse.leftButtonPressed &&  mouse.rightButtonPressed) return false;

	return true;
}

// WARNING: This routine must ONLY be called from the main input/video thread!
int16_t okBox(int16_t typ, const char *headline, const char *text)
{
#define PUSHBUTTON_W 80

	int16_t returnVal, oldLastUsedObjectID, oldLastUsedObjectType;
	uint16_t x, y, i, tlen, hlen, wlen, tx, knp, headlineX, textX;
	SDL_Event inputEvent;
	pushButton_t *p;
	checkBox_t *c;

#ifndef __APPLE__
	if (!video.fullscreen) // release mouse button trap
		SDL_SetWindowGrab(video.window, SDL_FALSE);
#endif

	if (editor.editTextFlag)
		exitTextEditing();

	// revert "delete/rename" mouse modes (disk op.)
	if (mouse.mode != MOUSE_MODE_NORMAL)
		setMouseMode(MOUSE_MODE_NORMAL);

	if (editor.ui.sysReqShown)
		return 0;

	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);

	editor.ui.sysReqShown = true;
	mouseAnimOff();

	oldLastUsedObjectID = mouse.lastUsedObjectID;
	oldLastUsedObjectType = mouse.lastUsedObjectType;

	// count number of buttons
	knp = 0;
	while (buttonText[typ][knp][0] != '\0' && knp < 5)
		knp++;

	tlen = textWidth(text);
	hlen = textWidth(headline);

	wlen = tlen;
	if (hlen > tlen)
		wlen = hlen;

	tx = (knp * 100) - 20;
	if (tx > wlen)
		wlen = tx;

	wlen += 100;
	if (wlen > 600)
		wlen = 600;

	headlineX = (SCREEN_W - hlen) / 2;
	textX = (SCREEN_W - tlen) / 2;
	x = (SCREEN_W - wlen) / 2;

	// the box y position differs in extended pattern editor mode
	y = editor.ui.extended ? SYSTEM_REQUEST_Y_EXT : SYSTEM_REQUEST_Y;

	// set up buttons
	for (i = 0; i < knp; i++)
	{
		p = &pushButtons[i];

		p->x = ((SCREEN_W - tx) / 2) + (i * 100);
		p->y = y + 42;
		p->w = PUSHBUTTON_W;
		p->h = 16;
		p->caption = buttonText[typ][i];
		p->visible = true;
	}

	// set up checkbox (special okBox types only!)
	if (typ >= 6 && typ <= 7)
	{
		c = &checkBoxes[0];
		c->x = x + 5;
		c->y = y + 50;
		c->clickAreaWidth = 116;
		c->clickAreaHeight = 12;
		c->checked = false;

		if (typ == 6)
		{
			// S3M load warning
			c->callbackFunc = configToggleS3MLoadWarning;
		}
		else if (typ == 7)
		{
			// "setting not yet applied"
			c->callbackFunc = configToggleNotYetAppliedWarning;
		}

		c->visible = true;
	}

	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.leftButtonPressed = 0;
	mouse.rightButtonPressed = 0;

	// input/rendering loop
	returnVal = 0;
	while (editor.ui.sysReqShown)
	{
		beginFPSCounter();
		readMouseXY();
		setSyncedReplayerVars();

		if (mouse.leftButtonPressed || mouse.rightButtonPressed)
		{
			if (mouse.lastUsedObjectType == OBJECT_PUSHBUTTON)
				handlePushButtonsWhileMouseDown();
			else if (mouse.lastUsedObjectType == OBJECT_CHECKBOX)
				handleCheckBoxesWhileMouseDown();
		}

		while (SDL_PollEvent(&inputEvent))
		{
			if (inputEvent.type == SDL_KEYDOWN)
			{
				if (inputEvent.key.keysym.sym == SDLK_ESCAPE)
				{
					returnVal = 0;
					editor.ui.sysReqShown = false;
				}
				else if (inputEvent.key.keysym.sym == SDLK_RETURN)
				{
					returnVal = 1;
					editor.ui.sysReqShown = false;
					keyb.ignoreCurrKeyUp = true; // don't handle key up event for any keys that were pressed
				}

				for (i = 0; i < knp; i++)
				{
					if (shortCut[typ][i] == inputEvent.key.keysym.sym)
					{
						returnVal = i + 1;
						editor.ui.sysReqShown = false;
						keyb.ignoreCurrKeyUp = true; // don't handle key up event for any keys that were pressed
						break;
					}
				}
			}
			else if (inputEvent.type == SDL_MOUSEBUTTONUP)
			{
				if (mouseButtonUpLogic(inputEvent.button.button))
				{
					if (typ >= 6 && typ <= 7)
						testCheckBoxMouseRelease();

					returnVal = testPushButtonMouseRelease(false) + 1;
					if (returnVal > 0)
						editor.ui.sysReqShown = false;

					mouse.lastUsedObjectID = OBJECT_ID_NONE;
					mouse.lastUsedObjectType = OBJECT_NONE;
				}
			}
			else if (inputEvent.type == SDL_MOUSEBUTTONDOWN)
			{
				if (mouseButtonDownLogic(inputEvent.button.button))
				{
					if (testPushButtonMouseDown()) continue;
					if (testCheckBoxMouseDown()) continue;
				}
			}

			if (!editor.ui.sysReqShown)
				break;
		}

		if (!editor.ui.sysReqShown)
			break;

		handleRedrawing();

		// draw OK box
		drawWindow(wlen);
		textOutShadow(headlineX, y +  4, PAL_BUTTON1, PAL_BUTTON2, headline);
		textOutShadow(textX,     y + 24, PAL_BUTTON1, PAL_BUTTON2, text);
		for (i = 0; i < knp; i++) drawPushButton(i);
		if (typ >= 6 && typ <= 7)
		{
			drawCheckBox(0);
			textOutShadow(x + 21, y + 52, PAL_BUTTON1, PAL_BUTTON2, "Don't show again");
		}

		flipFrame();
		endFPSCounter();
	}

	for (i = 0; i < knp; i++)
		hidePushButton(i);

	if (typ >= 6 && typ <= 7)
		hideCheckBox(0);

	mouse.lastUsedObjectID = oldLastUsedObjectID;
	mouse.lastUsedObjectType = oldLastUsedObjectType;
	unstuckLastUsedGUIElement();

	showBottomScreen();

	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	return returnVal;
}

/* WARNING:
** - This routine must ONLY be called from the main input/video thread!!
** - edText must be null-terminated
*/
int16_t inputBox(int16_t typ, const char *headline, char *edText, uint16_t maxStrLen)
{
#define PUSHBUTTON_W 80
#define TEXTBOX_W 250

	char *inputText;
	int16_t returnVal, oldLastUsedObjectID, oldLastUsedObjectType;
	uint16_t y, wlen, tx, knp, headlineX, i;
	SDL_Event inputEvent;
	pushButton_t *p;
	textBox_t *t;

	if (editor.editTextFlag)
		exitTextEditing();

	// revert "delete/rename" mouse modes (disk op.)
	if (mouse.mode != MOUSE_MODE_NORMAL)
		setMouseMode(MOUSE_MODE_NORMAL);

	if (editor.ui.sysReqShown)
		return 0;

	oldLastUsedObjectID = mouse.lastUsedObjectID;
	oldLastUsedObjectType = mouse.lastUsedObjectType;

	t = &textBoxes[0];

	// set up text box
	memset(t, 0, sizeof (textBox_t));
	t->w = TEXTBOX_W;
	t->h = 12;
	t->tx = 2;
	t->ty = 1;
	t->textPtr = edText;
	t->maxChars = maxStrLen;
	t->changeMouseCursor = true;
	t->renderBufW = (9 + 1) * t->maxChars; // 9 = max character/glyph width possible
	t->renderBufH = 10; // 10 = max character height possible
	t->renderW = t->w - (t->tx * 2);

	t->renderBuf = (uint8_t *)malloc(t->renderBufW * t->renderBufH * sizeof (int8_t));
	if (t->renderBuf == NULL)
	{
		okBox(0, "System message", "Not enough memory!");
		return 0;
	}

#ifndef __APPLE__
	if (!video.fullscreen) // release mouse button trap
		SDL_SetWindowGrab(video.window, SDL_FALSE);
#endif

	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);

	editor.ui.sysReqShown = true;
	mouseAnimOff();

	wlen = textWidth(headline);
	headlineX = (SCREEN_W - wlen) / 2;

	// count number of buttons
	knp = 0;
	while (buttonText[typ][knp][0] != '\0' && knp < 5)
		knp++;

	tx = TEXTBOX_W;
	if (tx > wlen)
		wlen = tx;

	tx = (knp * 100) - 20;
	if (tx > wlen)
		wlen = tx;

	wlen += 100;
	if (wlen > 600)
		wlen = 600;

	// the box y position differs in extended pattern editor mode
	y = editor.ui.extended ? SYSTEM_REQUEST_Y_EXT : SYSTEM_REQUEST_Y;

	// set further text box settings
	t->x = (SCREEN_W - TEXTBOX_W) / 2;
	t->y = y + 24;
	t->visible = true;

	// setup buttons
	for (i = 0; i < knp; i++)
	{
		p = &pushButtons[i];

		p->w = PUSHBUTTON_W;
		p->h = 16;
		p->x = ((SCREEN_W - tx) / 2) + (i * 100);
		p->y = y + 42;
		p->caption = buttonText[typ][i];
		p->visible = true;
	}

	keyb.leftShiftPressed = false; // kludge
	setTextCursorToEnd(t);

	mouse.lastEditBox = 0;
	editor.editTextFlag = true;
	SDL_StartTextInput();

	mouse.lastUsedObjectType = OBJECT_NONE;
	mouse.lastUsedObjectID = OBJECT_ID_NONE;
	mouse.leftButtonPressed = 0;
	mouse.rightButtonPressed = 0;

	// input/rendering loop
	returnVal = 0;
	while (editor.ui.sysReqShown)
	{
		beginFPSCounter();
		readMouseXY();
		readKeyModifiers();
		setSyncedReplayerVars();

		if (mouse.leftButtonPressed || mouse.rightButtonPressed)
		{
			if (mouse.lastUsedObjectType == OBJECT_PUSHBUTTON)
				handlePushButtonsWhileMouseDown();
			else if (mouse.lastUsedObjectType == OBJECT_TEXTBOX)
				handleTextBoxWhileMouseDown();
		}

		while (SDL_PollEvent(&inputEvent))
		{
			if (inputEvent.type == SDL_TEXTINPUT)
			{
				if (editor.editTextFlag)
				{
					if (keyb.ignoreTextEditKey)
					{
						keyb.ignoreTextEditKey = false;
						continue;
					}

					inputText = utf8ToCp437(inputEvent.text.text, false);
					if (inputText != NULL)
					{
						if (inputText[0] != '\0')
							handleTextEditInputChar(inputText[0]);

						free(inputText);
					}
				}
			}
			else if (inputEvent.type == SDL_KEYDOWN)
			{
				if (inputEvent.key.keysym.sym == SDLK_ESCAPE)
				{
					returnVal = 0;
					editor.ui.sysReqShown = false;
				}
				else if (inputEvent.key.keysym.sym == SDLK_RETURN)
				{
					returnVal = 1;
					editor.ui.sysReqShown = false;
					keyb.ignoreCurrKeyUp = true; // don't handle key up event for any keys that were pressed
				}

				if (editor.editTextFlag)
				{
					handleTextEditControl(inputEvent.key.keysym.sym);
				}
				else
				{
					for (i = 0; i < knp; i++)
					{
						if (shortCut[1][i] == inputEvent.key.keysym.sym)
						{
							returnVal = i + 1;
							editor.ui.sysReqShown = false;
							keyb.ignoreCurrKeyUp = true; // don't handle key up event for any keys that were pressed
							break;
						}
					}
				}
			}
			else if (inputEvent.type == SDL_MOUSEBUTTONUP)
			{
				if (mouseButtonUpLogic(inputEvent.button.button))
				{
					returnVal = testPushButtonMouseRelease(false) + 1;
					if (returnVal > 0)
						editor.ui.sysReqShown = false;

					mouse.lastUsedObjectID = OBJECT_ID_NONE;
					mouse.lastUsedObjectType = OBJECT_NONE;
				}
			}
			else if (inputEvent.type == SDL_MOUSEBUTTONDOWN)
			{
				if (mouseButtonDownLogic(inputEvent.button.button))
				{
					if (testTextBoxMouseDown()) continue;
					if (testPushButtonMouseDown()) continue;
				}
			}

			if (!editor.ui.sysReqShown)
				break;
		}

		if (!editor.ui.sysReqShown)
			break;

		handleRedrawing();

		// draw input box
		drawWindow(wlen);
		textOutShadow(headlineX, y + 4, PAL_BUTTON1, PAL_BUTTON2, headline);
		clearRect(t->x, t->y, t->w, t->h);
		hLine(t->x - 1,    t->y - 1,    t->w + 2, PAL_BUTTON2);
		vLine(t->x - 1,    t->y,        t->h + 1, PAL_BUTTON2);
		hLine(t->x,        t->y + t->h, t->w + 1, PAL_BUTTON1);
		vLine(t->x + t->w, t->y,        t->h,     PAL_BUTTON1);
		drawTextBox(0);
		for (i = 0; i < knp; i++) drawPushButton(i);

		flipFrame();
		endFPSCounter();
	}

	editor.editTextFlag = false;
	SDL_StopTextInput();

	for (i = 0; i < knp; i++)
		hidePushButton(i);
	hideTextBox(0);

	free(t->renderBuf);

	mouse.lastUsedObjectID = oldLastUsedObjectID;
	mouse.lastUsedObjectType = oldLastUsedObjectType;
	unstuckLastUsedGUIElement();

	showBottomScreen();

	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	return returnVal;
}

// WARNING: This routine must NOT be called from the main input/video thread!
int16_t okBoxThreadSafe(int16_t typ, const char *headline, const char *text)
{
	if (!editor.mainLoopOngoing)
		return 0; // main loop was not even started yet, bail out.

	// block multiple calls before they are completed (for safety)
	while (okBoxData.active)
		SDL_Delay(1000 / VBLANK_HZ);

	okBoxData.typ = typ;
	okBoxData.headline = headline;
	okBoxData.text = text;
	okBoxData.active = true;

	while (okBoxData.active)
		SDL_Delay(1000 / VBLANK_HZ);

	return okBoxData.returnData;
}

static bool askQuit_RandomMsg(void)
{
	uint8_t msg = rand() % QUIT_MESSAGES;
	int16_t button = okBox(quitMessage[msg].typ, "System request", quitMessage[msg].text);

	return (button == 1) ? true : false;
}

bool askUnsavedChanges(uint8_t type)
{
	int16_t button;
	
	if (type == ASK_TYPE_QUIT)
	{
		button = okBox(2, "System request",
			"You have unsaved changes in your song. Do you still want to quit and lose ALL changes?");
	}
	else
	{
		button = okBox(2, "System request",
			"You have unsaved changes in your song. Load new song and lose ALL changes?");
	}

	return (button == 1) ? true : false;
}

int16_t quitBox(bool skipQuitMsg)
{
	if (editor.ui.sysReqShown)
		return 0;

	if (!song.isModified && skipQuitMsg)
		return 1;

	if (song.isModified)
		return askUnsavedChanges(ASK_TYPE_QUIT);

	return askQuit_RandomMsg();
}
