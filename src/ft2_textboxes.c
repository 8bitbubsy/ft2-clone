// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_config.h"
#include "ft2_diskop.h"
#include "ft2_keyboard.h"
#include "ft2_mouse.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"

textBox_t textBoxes[NUM_TEXTBOXES] =
{
	// ------ RESERVED TEXTBOXES ------
	{ 0 },

	/*
	** -- STRUCT INFO: --
	**  x    = x position
	**  y    = y position
	**  w    = width
	**  h    = height
	**  tx   = left padding for text
	**  ty   = top padding for text
	**  maxc = max characters in box string
	**  rmb  = can only be triggered with right mouse button (f.ex. tracker instrument texts)
	**  cmc  = change mouse cursor when hovering over it
	*/

	// ------ INSTRUMENT/SAMPLE/SONG NAME TEXTBOXES ------
	// x,   y,   w,   h,  tx,ty, maxc,       rmb,   cmc
	{  446,   5, 140,  10, 1, 0, 22,         true,  false },
	{  446,  16, 140,  10, 1, 0, 22,         true,  false },
	{  446,  27, 140,  10, 1, 0, 22,         true,  false },
	{  446,  38, 140,  10, 1, 0, 22,         true,  false },
	{  446,  49, 140,  10, 1, 0, 22,         true,  false },
	{  446,  60, 140,  10, 1, 0, 22,         true,  false },
	{  446,  71, 140,  10, 1, 0, 22,         true,  false },
	{  446,  82, 140,  10, 1, 0, 22,         true,  false },
	{  446,  99, 116,  10, 1, 0, 22,         true,  false },
	{  446, 110, 116,  10, 1, 0, 22,         true,  false },
	{  446, 121, 116,  10, 1, 0, 22,         true,  false },
	{  446, 132, 116,  10, 1, 0, 22,         true,  false },
	{  446, 143, 116,  10, 1, 0, 22,         true,  false },
	{  424, 158, 160,  12, 2, 1, 20,         false, true  },

	// ------ DISK OP. TEXTBOXES ------
	// x,   y,   w,   h,  tx,ty, maxc,       rmb,   cmc
	{   31, 158, 134,  12, 2, 1, PATH_MAX,   false, true },

	// ------ CONFIG TEXTBOXES ------
	// x,   y,   w,   h,  tx,ty, maxc,       rmb,   cmc
	{ 486,   16, 143,  12, 2, 1, 80,         false, true },
	{ 486,   31, 143,  12, 2, 1, 80,         false, true },
	{ 486,   46, 143,  12, 2, 1, 80,         false, true },
	{ 486,   61, 143,  12, 2, 1, 80,         false, true },
	{ 486,   76, 143,  12, 2, 1, 80,         false, true }
};

static int16_t markX1, markX2;
static uint16_t oldCursorPos;
static int32_t oldMouseX;

static void moveTextCursorLeft(int16_t i, bool updateTextBox);
static void moveTextCursorRight(int16_t i, bool updateTextBox);

static void setSongModifiedFlagIfNeeded(void) // called during keystrokes in text boxes
{
	if (mouse.lastEditBox == TB_SONG_NAME ||
	   (mouse.lastEditBox >= TB_INST1 && mouse.lastEditBox <= TB_INST8) ||
	   (mouse.lastEditBox >= TB_SAMP1 && mouse.lastEditBox <= TB_SAMP5))
	{
		setSongModifiedFlag();
	}
}

bool textIsMarked(void)
{
	if (markX1 == markX2)
		return false;

	return true;
}

static void removeTextMarking(void)
{
	markX1 = 0;
	markX2 = 0;
}

static int16_t getTextMarkStart(void)
{
	if (markX2 < markX1)
		return markX2;

	return markX1;
}

static int16_t getTextMarkEnd(void)
{
	if (markX1 > markX2)
		return markX1;

	return markX2;
}

static int16_t getTextLength(textBox_t *t, uint16_t offset)
{
	uint16_t i;

	if (t->textPtr == NULL || offset >= t->maxChars)
		return 0;

	// count number of characters in text
	for (i = offset; i < t->maxChars; i++)
	{
		if (t->textPtr[i] == '\0')
			break;
	}

	i -= offset; // i now contains string length
	assert(i <= t->maxChars);

	return i;
}

static void deleteMarkedText(textBox_t *t)
{
	if (!textIsMarked())
		return;

	const int16_t start = getTextMarkStart();
	const int16_t end = getTextMarkEnd();

	assert(start < t->maxChars && end <= t->maxChars);

	// calculate pixel width of string to delete
	int32_t deleteTextWidth = 0;
	for (int32_t i = start; i < end; i++)
		deleteTextWidth += charWidth(t->textPtr[i]);

	// copy markEnd part to markStart, and add null termination
	const int32_t length = (int32_t)strlen(&t->textPtr[end]);
	if (length > 0)
		memcpy(&t->textPtr[start], &t->textPtr[end], length);
	t->textPtr[start+length] = '\0';

	// scroll buffer offset to the left if we are scrolled
	if (t->bufOffset >= deleteTextWidth)
		t->bufOffset -= deleteTextWidth;
	else
		t->bufOffset = 0;

	// set text cursor to markStart
	t->cursorPos = start;

	setSongModifiedFlagIfNeeded();
}

static void setCursorToMarkStart(textBox_t *t)
{
	if (!textIsMarked())
		return;

	const int16_t start = getTextMarkStart();
	assert(start < t->maxChars);
	t->cursorPos = start;

	int32_t startXPos = 0;
	for (int32_t i = 0; i < start; i++)
	{
		const char ch = t->textPtr[i];
		if (ch == '\0')
			break;

		startXPos += charWidth(ch);
	}

	// change buffer offset, if needed
	if (startXPos < t->bufOffset)
		t->bufOffset = startXPos;
}

static void setCursorToMarkEnd(textBox_t *t)
{
	if (!textIsMarked())
		return;

	const int16_t end = getTextMarkEnd();
	assert(end <= t->maxChars);
	t->cursorPos = end;

	int32_t endXPos = 0;
	for (int32_t i = 0; i < end; i++)
	{
		const char ch = t->textPtr[i];
		if (ch == '\0')
			break;

		endXPos += charWidth(ch);
	}

	// change buffer offset, if needed
	if (endXPos > t->bufOffset+t->renderW)
		t->bufOffset = endXPos - t->renderW;
}

static void copyMarkedText(textBox_t *t)
{
	if (!textIsMarked())
		return;

	const int32_t start = getTextMarkStart();
	const int32_t end = getTextMarkEnd();

	assert(start < t->maxChars && end <= t->maxChars);

	const int32_t length = end - start;
	if (length < 1)
		return;

	/* Change mark-end character to NUL so that we
	** we only copy the marked section of the string.
	** There's always room for a NUL at the end of
	** the text box string, so this is safe.
	**/
	const char oldChar = t->textPtr[end];
	t->textPtr[end] = '\0';

	char *utf8Text = cp437ToUtf8(&t->textPtr[start]);
	if (utf8Text != NULL)
	{
		SDL_SetClipboardText(utf8Text);
		free(utf8Text);
	}

	t->textPtr[end] = oldChar; // set back original character
}

static void cutMarkedText(textBox_t *t)
{
	if (!textIsMarked())
		return;

	copyMarkedText(t);
	deleteMarkedText(t);
	removeTextMarking();

	drawTextBox(mouse.lastEditBox);
}

static void pasteText(textBox_t *t)
{
	char *endPart;

	if (!SDL_HasClipboardText())
		return;

	// if we've marked text, delete it and remove text marking
	if (textIsMarked())
	{
		deleteMarkedText(t);
		removeTextMarking();
	}

	if (t->cursorPos >= t->maxChars)
		return;

	const int32_t textLength = getTextLength(t, 0);

	const int32_t roomLeft = t->maxChars - textLength;
	if (roomLeft <= 0)
		return; // no more room!
 
	char *copiedTextUtf8 = SDL_GetClipboardText();

	char *copiedText = utf8ToCp437(copiedTextUtf8, true);
	if (copiedText == NULL)
		return;

	int32_t copiedTextLength = (int32_t)strlen(copiedText);
	if (copiedTextLength > roomLeft)
		copiedTextLength = roomLeft;

	const uint16_t endOffset = t->cursorPos;
	endPart = NULL; // prevent false compiler warning

	const int32_t endPartLength = getTextLength(t, endOffset);
	if (endPartLength > 0)
	{
		endPart = (char *)malloc(endPartLength+1);
		if (endPart == NULL)
		{
			free(copiedText);
			okBox(0, "System message", "Not enough memory!");
			return;
		}
	}

	// make a copy of end data
	if (endPartLength > 0)
	{
		memcpy(endPart, &t->textPtr[endOffset], endPartLength);
		endPart[endPartLength] = '\0';
	}

	// paste copied data
	memcpy(&t->textPtr[endOffset], copiedText, copiedTextLength);
	t->textPtr[endOffset+copiedTextLength] = '\0';
	free(copiedText);

	// append end data
	if (endPartLength > 0)
	{
		strcat(&t->textPtr[endOffset+copiedTextLength], endPart);
		free(endPart);
	}

	for (int32_t i = 0; i < copiedTextLength; i++)
		moveTextCursorRight(mouse.lastEditBox, TEXTBOX_NO_UPDATE);

	drawTextBox(mouse.lastEditBox);

	setSongModifiedFlagIfNeeded();
}

void exitTextEditing(void)
{
	if (!editor.editTextFlag)
		return;

	if (mouse.lastEditBox >= 0 && mouse.lastEditBox < NUM_TEXTBOXES)
	{
		textBoxes[mouse.lastEditBox].bufOffset = 0;
		removeTextMarking();
		drawTextBox(mouse.lastEditBox);
	}

	if (mouse.lastEditBox == TB_DISKOP_FILENAME && getDiskOpItem() == DISKOP_ITEM_MODULE)
	{
		updateCurrSongFilename(); // for window title
		updateWindowTitle(true);
	}

	keyb.ignoreCurrKeyUp = true; // prevent a note being played (on enter key)
	editor.editTextFlag = false;

	hideSprite(SPRITE_TEXT_CURSOR);
	SDL_StopTextInput();
}

static int16_t cursorPosToX(textBox_t *t)
{
	assert(t->textPtr != NULL);

	int32_t x = -1; // cursor starts one pixel before character
	for (int16_t i = 0; i < t->cursorPos; i++)
		x += charWidth(t->textPtr[i]);
 
	x -= t->bufOffset; // subtract by buffer offset to get real X position
	return (int16_t)x;
}

int16_t getTextCursorX(textBox_t *t)
{
	return t->x + t->tx + cursorPosToX(t);
}

int16_t getTextCursorY(textBox_t *t)
{
	return t->y + t->ty;
}

static void scrollTextBufferLeft(textBox_t *t)
{
	// scroll buffer and clamp
	t->bufOffset -= TEXT_SCROLL_VALUE;
	if (t->bufOffset < 0)
		t->bufOffset = 0;
}

static void scrollTextBufferRight(textBox_t *t, uint16_t numCharsInText)
{
	assert(numCharsInText <= t->maxChars);

	// get end of text position
	int32_t textEnd = 0;
	for (uint16_t j = 0; j < numCharsInText; j++)
		textEnd += charWidth(t->textPtr[j]);

	// subtract by text box width and clamp to 0
	textEnd -= t->renderW;
	if (textEnd < 0)
		textEnd = 0;

	// scroll buffer and clamp
	t->bufOffset += TEXT_SCROLL_VALUE;
	if (t->bufOffset > textEnd)
		t->bufOffset = textEnd;
}

static void moveTextCursorToMouseX(uint16_t textBoxID)
{
	int16_t i;

	textBox_t *t = &textBoxes[textBoxID];
	if ((mouse.x == t->x && t->bufOffset == 0) || t->textPtr == NULL || t->textPtr[0] == '\0')
	{
		t->cursorPos = 0;
		return;
	}

	int16_t numChars = getTextLength(t, 0);

	// find out what character we are clicking at, and set cursor to that character
	const int32_t mx = t->bufOffset + mouse.x;
	int32_t tx = (t->x + t->tx) - 1;
	int32_t cw = -1;

	for (i = 0; i < numChars; i++)
	{
		cw = charWidth(t->textPtr[i]);
		const int32_t tx2 = tx + cw;

		if (mx >= tx && mx < tx2)
		{
			t->cursorPos = i;
			break;
		}

		tx += cw;
	}

	// set to last character if we clicked outside the end of the text
	if (i == numChars && mx >= tx)
		t->cursorPos = numChars;

	if (cw != -1)
	{
		const int16_t cursorPos = cursorPosToX(t);

		// scroll buffer to the right if needed
		if (cursorPos+cw > t->renderW)
			scrollTextBufferRight(t, numChars);

		// scroll buffer to the left if needed
		else if (cursorPos < 0-1)
			scrollTextBufferLeft(t);
	}

	editor.textCursorBlinkCounter = 0;
}

static void textOutBuf(uint8_t *dstBuffer, uint32_t dstWidth, uint8_t paletteIndex, char *text, uint32_t maxTextLen)
{
	assert(text != NULL);
	if (*text == '\0')
		return; // empty string

	uint16_t currX = 0;
	for (uint32_t i = 0; i < maxTextLen; i++)
	{
		const char chr = *text++ & 0x7F;
		if (chr == '\0')
			break;

		if (chr != ' ')
		{
			const uint8_t *srcPtr = &bmp.font1[chr * FONT1_CHAR_W];
			uint8_t *dstPtr = &dstBuffer[currX];

			for (uint32_t y = 0; y < FONT1_CHAR_H; y++)
			{
				for (uint32_t x = 0; x < FONT1_CHAR_W; x++)
				{
					if (srcPtr[x])
						dstPtr[x] = paletteIndex;
				}

				srcPtr += FONT1_WIDTH;
				dstPtr += dstWidth;
			}
		}

		currX += charWidth(chr);
	}
}

 // a lot of filling here, but textboxes are small so no problem...
void drawTextBox(uint16_t textBoxID)
{
	int8_t cw;
	uint8_t pal;

	assert(textBoxID < NUM_TEXTBOXES);
	textBox_t *t = &textBoxes[textBoxID];
	if (!t->visible)
		return;

	// test if buffer offset is not overflowing
#ifdef _DEBUG
	if (t->renderBufW > t->renderW)
		assert(t->bufOffset <= t->renderBufW-t->renderW);
#endif

	// fill text rendering buffer with transparency key
	memset(t->renderBuf, PAL_TRANSPR, t->renderBufW * t->renderBufH);

	if (t->textPtr == NULL)
		return;

	// draw text mark background
	if (textIsMarked())
	{
		hideSprite(SPRITE_TEXT_CURSOR);

		int32_t start = getTextMarkStart();
		int32_t end = getTextMarkEnd();

		assert(start < t->maxChars && end <= t->maxChars);

		// find pixel start/length from markX1 and markX2

		int32_t x1 = 0;
		int32_t x2 = 0;

		for (int32_t i = 0; i < end; i++)
		{
			const char ch = t->textPtr[i];
			if (ch == '\0')
				break;

			cw = charWidth(ch);
			if (i < start)
				x1 += cw;

			x2 += cw;
		}

		// render text mark background
		if (x1 != x2)
		{
			start = x1;
			const int32_t length = x2 - x1;

			assert(start+length <= t->renderBufW);

			uint8_t *ptr32 = &t->renderBuf[start];
			for (uint16_t y = 0; y < t->renderBufH; y++, ptr32 += t->renderBufW)
				memset(ptr32, PAL_TEXTMRK, length);
		}
	}

	// render text to text render buffer
	textOutBuf(t->renderBuf, t->renderBufW, PAL_FORGRND, t->textPtr, t->maxChars);

	// fill screen rect background color (not always needed, but I'm lazy)
	pal = video.frameBuffer[(t->y * SCREEN_W) + t->x] >> 24; // get background palette (stored in alpha channel)
	fillRect(t->x + t->tx, t->y + t->ty, t->renderW, 10, pal); // 10 = tallest possible glyph/char height

	// render visible part of text render buffer to screen
	blitClipX(t->x + t->tx, t->y + t->ty, &t->renderBuf[t->bufOffset], t->renderBufW, t->renderBufH, t->renderW);
}

void showTextBox(uint16_t textBoxID)
{
	assert(textBoxID < NUM_TEXTBOXES);
	textBoxes[textBoxID].visible = true;
}

void hideTextBox(uint16_t textBoxID)
{
	assert(textBoxID < NUM_TEXTBOXES);
	hideSprite(SPRITE_TEXT_CURSOR);
	textBoxes[textBoxID].visible = false;
}

static void setMarkX2ToMouseX(textBox_t *t)
{
	int16_t i;

	if (t->textPtr == NULL || t->textPtr[0] == '\0')
	{
		removeTextMarking();
		return;
	}

	if (markX2 < markX1 && mouse.x < t->x+t->tx)
	{
		markX2 = 0;
		return;
	}

	const int16_t numChars = getTextLength(t, 0);

	// find out what character we are clicking at, and set markX2 to that character
	const int32_t mx = t->bufOffset + mouse.x;
	int32_t tx = (t->x + t->tx) - 1;

	for (i = 0; i < numChars; i++)
	{
		const int32_t cw = charWidth(t->textPtr[i]);
		const int32_t tx2 = tx + cw;

		if (mx >= tx && mx < tx2)
		{
			markX2 = i;
			break;
		}

		tx += cw;
	}

	// set to last character if we clicked outside the end of the text
	if (i == numChars && mx >= tx)
		markX2 = numChars;

	if (mouse.x >= t->x+t->w-3)
	{
		scrollTextBufferRight(t, numChars);
		if (++markX2 > numChars)
			markX2 = numChars;
	}
	else if (mouse.x <= t->x+t->tx+3)
	{
		if (t->bufOffset > 0)
		{
			scrollTextBufferLeft(t);
			if (--markX2 < 0)
				markX2 = 0;
		}
	}

	t->cursorPos = markX2;
	assert(t->cursorPos >= 0 && t->cursorPos <= getTextLength(t, 0));

	editor.textCursorBlinkCounter = 0;
}

void handleTextBoxWhileMouseDown(void)
{
	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_TEXTBOXES);
	textBox_t *t = &textBoxes[mouse.lastUsedObjectID];
	if (!t->visible)
		return;

	if (mouse.x != oldMouseX)
	{
		oldMouseX = mouse.x;

		markX1 = oldCursorPos;
		setMarkX2ToMouseX(t);

		drawTextBox(mouse.lastUsedObjectID);
	}
}

bool testTextBoxMouseDown(void)
{
	uint16_t start, end;

	oldMouseX = mouse.x;
	oldCursorPos = 0;

	if (ui.sysReqShown)
	{
		// if a system request is open, only test the first textbox (reserved)
		start = 0;
		end = 1;
	}
	else
	{
		start = 1;
		end = NUM_TEXTBOXES;
	}

	const int32_t mx = mouse.x;
	const int32_t my = mouse.y;

	textBox_t *t = &textBoxes[start];
	for (uint16_t i = start; i < end; i++, t++)
	{
		if (!t->visible || t->textPtr == NULL)
			continue;

		if (my >= t->y && my < t->y+t->h &&
		    mx >= t->x && mx < t->x+t->w)
		{
			if (!mouse.rightButtonPressed && t->rightMouseButton)
				break;

			// if we were editing another text box and clicked on another one, properly end it
			if (editor.editTextFlag && i != mouse.lastEditBox)
				exitTextEditing();

			mouse.lastEditBox = i;
			moveTextCursorToMouseX(mouse.lastEditBox);

			oldCursorPos = t->cursorPos;
			removeTextMarking();
			drawTextBox(mouse.lastEditBox);

			editor.textCursorBlinkCounter  = 0;
			mouse.lastUsedObjectType = OBJECT_TEXTBOX;
			mouse.lastUsedObjectID = i;

			editor.editTextFlag = true;

			SDL_StartTextInput();
			return true;
		}
	}

	// if we were editing text and we clicked outside of a text box, exit text editing
	if (editor.editTextFlag)
		exitTextEditing();

	return false;
}

void updateTextBoxPointers(void)
{
	int32_t i;
	instrTyp *curIns = instr[editor.curInstr];

	// instrument names
	for (i = 0; i < 8; i++)
		textBoxes[TB_INST1+i].textPtr = song.instrName[1+editor.instrBankOffset+i];

	// sample names
	if (editor.curInstr == 0 || curIns == NULL)
	{
		for (i = 0; i < 5; i++)
			textBoxes[TB_SAMP1+i].textPtr = NULL;
	}
	else
	{
		for (i = 0; i < 5; i++)
			textBoxes[TB_SAMP1+i].textPtr = curIns->samp[editor.sampleBankOffset+i].name;
	}

	// song name
	textBoxes[TB_SONG_NAME].textPtr = song.name;
}

void setupInitialTextBoxPointers(void)
{
	textBoxes[TB_CONF_DEF_MODS_DIR].textPtr = config.modulesPath;
	textBoxes[TB_CONF_DEF_INSTRS_DIR].textPtr = config.instrPath;
	textBoxes[TB_CONF_DEF_SAMPS_DIR].textPtr = config.samplesPath;
	textBoxes[TB_CONF_DEF_PATTS_DIR].textPtr = config.patternsPath;
	textBoxes[TB_CONF_DEF_TRACKS_DIR].textPtr = config.tracksPath;
}

void setTextCursorToEnd(textBox_t *t)
{
	uint16_t numChars;

	// count number of chars and get full text width
	uint32_t textWidth = 0;
	for (numChars = 0; numChars < t->maxChars; numChars++)
	{
		const char ch = t->textPtr[numChars];
		if (ch == '\0')
			break;

		textWidth += charWidth(ch);
	}

	// if cursor is not at the end, handle text marking
	if (t->cursorPos < numChars)
	{
		if (keyb.leftShiftPressed)
		{
			if (!textIsMarked())
				markX1 = t->cursorPos;

			markX2 = numChars;
		}
		else
		{
			removeTextMarking();
		}
	}

	t->cursorPos = numChars;

	t->bufOffset = 0;
	if (textWidth > t->renderW)
		t->bufOffset = textWidth - t->renderW;

	drawTextBox(mouse.lastEditBox);
	editor.textCursorBlinkCounter = 0;
}

void handleTextEditControl(SDL_Keycode keycode)
{
	int16_t i;
	uint16_t numChars;
	int32_t textLength;
	uint32_t textWidth;

	assert(mouse.lastEditBox >= 0 && mouse.lastEditBox < NUM_TEXTBOXES);

	textBox_t *t = &textBoxes[mouse.lastEditBox];
	assert(t->textPtr != NULL);

	switch (keycode)
	{
		case SDLK_ESCAPE:
		{
			removeTextMarking();
			exitTextEditing();
		}
		break;

		case SDLK_a:
		{
			// CTRL+A - mark all text
			if (keyb.leftCtrlPressed)
			{
				// count number of chars and get full text width
				textWidth = 0;
				for (numChars = 0; numChars < t->maxChars; numChars++)
				{
					if (t->textPtr[numChars] == '\0')
						break;

					textWidth += charWidth(t->textPtr[numChars]);
				}

				markX1 = 0;
				markX2 = numChars;
				t->cursorPos = markX2;

				t->bufOffset = 0;
				if (textWidth > t->renderW)
					t->bufOffset = textWidth - t->renderW;

				drawTextBox(mouse.lastEditBox);
			}
		}
		break;

		case SDLK_x:
		{
			// CTRL+X - cut marked text
			if (keyb.leftCtrlPressed)
				cutMarkedText(t);
		}
		break;

		case SDLK_c:
		{
			// CTRL+C - copy marked text
			if (keyb.leftCtrlPressed)
				copyMarkedText(t);
		}
		break;

		case SDLK_v:
		{
			// CTRL+V - paste text
			if (keyb.leftCtrlPressed)
				pasteText(t);
		}
		break;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:
		{
			// ALT+ENTER = toggle fullscreen, even while text editing
			if (keyb.leftAltPressed)
				toggleFullScreen();
			else
				exitTextEditing();
		}
		break;

		case SDLK_LEFT:
		{
			if (keyb.leftShiftPressed)
			{
				if (!textIsMarked())
				{
					// no marking, mark character to left from cursor
					if (t->cursorPos > 0)
					{
						markX1 = t->cursorPos;
						moveTextCursorLeft(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
						markX2 = t->cursorPos;

						drawTextBox(mouse.lastEditBox);
					}
				}
				else
				{
					// marking, extend/shrink marking
					if (markX2 > 0)
					{
						t->cursorPos = markX2;
						markX2--;
						moveTextCursorLeft(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
						drawTextBox(mouse.lastEditBox);
					}
				}
			}
			else
			{
				if (textIsMarked())
				{
					setCursorToMarkStart(t);
					removeTextMarking();
				}
				else
				{
					removeTextMarking();
					moveTextCursorLeft(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
				}

				drawTextBox(mouse.lastEditBox);
			}
		}
		break;

		case SDLK_RIGHT:
		{
			if (keyb.leftShiftPressed)
			{
				textLength = getTextLength(t, 0);

				if (!textIsMarked())
				{
					if (t->cursorPos < textLength)
					{
						markX1 = t->cursorPos;
						moveTextCursorRight(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
						markX2 = t->cursorPos;

						drawTextBox(mouse.lastEditBox);
					}
				}
				else
				{
					// marking, extend/shrink marking
					if (markX2 < textLength)
					{
						t->cursorPos = markX2;
						markX2++;
						moveTextCursorRight(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
						drawTextBox(mouse.lastEditBox);
					}
				}
			}
			else
			{
				if (textIsMarked())
				{
					setCursorToMarkEnd(t);
					removeTextMarking();
				}
				else
				{
					removeTextMarking();
					moveTextCursorRight(mouse.lastEditBox, TEXTBOX_NO_UPDATE);
				}

				drawTextBox(mouse.lastEditBox);
			}
		}
		break;

		case SDLK_BACKSPACE:
		{
			if (textIsMarked())
			{
				deleteMarkedText(t);
				removeTextMarking();
				drawTextBox(mouse.lastEditBox);
				break;
			}

			removeTextMarking();

			if (t->cursorPos > 0 && t->textPtr[0] != '\0')
			{
				// scroll buffer offset if we are scrolled
				if (t->bufOffset > 0)
				{
					t->bufOffset -= charWidth(t->textPtr[t->cursorPos-1]);
					if (t->bufOffset < 0)
						t->bufOffset = 0;
				}

				moveTextCursorLeft(mouse.lastEditBox, TEXTBOX_UPDATE);

				i = t->cursorPos;
				while (i < t->maxChars)
				{
					t->textPtr[i] = t->textPtr[i+1];
					if (t->textPtr[i] == '\0')
						break;
					i++;
				}

				drawTextBox(mouse.lastEditBox);
				setSongModifiedFlagIfNeeded();
			}
		}
		break;

		case SDLK_DELETE:
		{
			if (textIsMarked())
			{
				deleteMarkedText(t);
				removeTextMarking();
				drawTextBox(mouse.lastEditBox);
				break;
			}

			if (t->textPtr[t->cursorPos] != '\0' && t->textPtr[0] != '\0' && t->cursorPos < t->maxChars)
			{
				// scroll buffer offset if we are scrolled
				if (t->bufOffset > 0)
				{
					t->bufOffset -= charWidth(t->textPtr[t->cursorPos]);
					if (t->bufOffset < 0)
						t->bufOffset = 0;
				}

				i = t->cursorPos;
				while (i < t->maxChars)
				{
					if (t->textPtr[i] == '\0')
						break;

					t->textPtr[i] = t->textPtr[i+1];
					i++;
				}

				drawTextBox(mouse.lastEditBox);
				setSongModifiedFlagIfNeeded();
			}
		}
		break;

		case SDLK_HOME:
		{
			if (keyb.leftShiftPressed)
			{
				if (!textIsMarked())
					markX1 = t->cursorPos;

				markX2 = 0;
				t->bufOffset = 0;
				t->cursorPos = 0;
			}
			else
			{
				removeTextMarking();

				if (t->cursorPos > 0)
				{
					t->cursorPos = 0;
					t->bufOffset = 0;

					editor.textCursorBlinkCounter = 0;
				}
			}

			drawTextBox(mouse.lastEditBox);
		}
		break;

		case SDLK_END:
		{
			setTextCursorToEnd(t);
		}
		break;

		default: break;
	}
}

void handleTextEditInputChar(char textChar)
{
	assert(mouse.lastEditBox >= 0 && mouse.lastEditBox < NUM_TEXTBOXES);

	textBox_t *t = &textBoxes[mouse.lastEditBox];
	if (t->textPtr == NULL)
		return;

	const int8_t ch = (const int8_t)textChar;
	if (ch < 32 && ch != -124 && ch != -108 && ch != -122 && ch != -114 && ch != -103 && ch != -113)
		return; // allow certain codepage 437 nordic characters

	if (textIsMarked())
	{
		deleteMarkedText(t);
		removeTextMarking();
	}

	if (t->cursorPos >= 0 && t->cursorPos < t->maxChars)
	{
		int32_t i = getTextLength(t, 0);
		if (i < t->maxChars) // do we have room for a new character?
		{
			t->textPtr[i+1] = '\0';

			// if string not empty, shift string to the right to make space for char insertion
			if (i > 0)
			{
				for (; i > t->cursorPos; i--)
					t->textPtr[i] = t->textPtr[i-1];
			}

			t->textPtr[t->cursorPos] = textChar;

			moveTextCursorRight(mouse.lastEditBox, TEXTBOX_UPDATE); // also updates textbox
			setSongModifiedFlagIfNeeded();
		}
	}
}

static void moveTextCursorLeft(int16_t i, bool updateTextBox)
{
	textBox_t *t = &textBoxes[i];
	if (t->cursorPos == 0)
		return;

	t->cursorPos--;

	// scroll buffer if needed
	if (cursorPosToX(t) < 0-1)
		scrollTextBufferLeft(t);

	if (updateTextBox)
		drawTextBox(i);

	editor.textCursorBlinkCounter = 0; // reset text cursor blink timer
}

static void moveTextCursorRight(int16_t i, bool updateTextBox)
{
	textBox_t *t = &textBoxes[i];

	const uint16_t numChars = getTextLength(t, 0);
	if (t->cursorPos >= numChars)
		return;

	t->cursorPos++;

	// scroll buffer if needed
	if (cursorPosToX(t) >= t->renderW)
		scrollTextBufferRight(t, numChars);

	if (updateTextBox)
		drawTextBox(i);

	editor.textCursorBlinkCounter = 0; // reset text cursor blink timer
}

void freeTextBoxes(void)
{
	// free text box buffers (skip first entry, it's reserved for inputBox())
	textBox_t *t = &textBoxes[1];
	for (int32_t i = 1; i < NUM_TEXTBOXES; i++, t++)
	{
		if (t->renderBuf != NULL)
		{
			free(t->renderBuf);
			t->renderBuf = NULL;
		}
	}
}
