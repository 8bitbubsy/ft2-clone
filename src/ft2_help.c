// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_help.h"
#include "ft2_video.h"
#include "ft2_pattern_ed.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "helpdata/ft2_help_data.h"

typedef struct
{
	bool bigFont, noLine;
	uint8_t color;
	int16_t xPos;
	char text[100];
} helpRec;

#define HELP_LINES 15
#define MAX_HELP_LINES 768
#define HELP_SIZE sizeof (helpRec)
#define MAX_SUBJ 10
#define HELP_KOL 135
#define HELP_WIDTH (596 - HELP_KOL)

static uint8_t fHlp_Nr;
static int16_t textLine, fHlp_Line, subjLen[MAX_SUBJ];
static int32_t helpBufferPos;
static helpRec *subjPtrArr[MAX_SUBJ];

static void addText(helpRec *t, int16_t xPos, uint8_t color, char *text)
{
	if (*text == '\0')
		return;

	t->xPos = xPos;
	t->color = color;
	t->bigFont = false;
	t->noLine = false;
	strcpy(t->text, text);
	*text = '\0'; // empty old string

	textLine++;
}

static bool getLine(char *output)
{
	if (helpBufferPos >= (int32_t)sizeof (helpData))
	{
		*output = '\0';
		return false;
	}

	const uint8_t strLen = helpData[helpBufferPos++];
	memcpy(output, &helpData[helpBufferPos], strLen);
	output[strLen] = '\0';

	helpBufferPos += strLen;

	return true;
}

static int16_t controlCodeToNum(const char *controlCode)
{
	return (((controlCode[0]-'0')%10)*100) + (((controlCode[1]-'0')%10)*10) + ((controlCode[2]-'0')%10);
}

static char *ltrim(char *s)
{
	if (*s == '\0')
		return (s);

	while (*s == ' ') s++;

	return s;
}

static char *rtrim(char *s)
{
	if (*s == '\0')
		return (s);

	int32_t i = (int32_t)strlen(s) - 1;
	while (i >= 0)
	{
		if (s[i] != ' ')
		{
			s[i+1] = '\0';
			break;
		}

		i--;
	}

	return s;
}

static void readHelp(void) // this is really messy, directly ported from Pascal code...
{
	char text[256], text2[256], *s, *sEnd, *s3;
	int16_t a, b, i, k;

	helpRec *tempText = (helpRec *)malloc(HELP_SIZE * MAX_HELP_LINES);
	if (tempText == NULL)
	{
		okBox(0, "System message", "Not enough memory!");
		return;
	}
	
	text[0] = '\0';
	text2[0] = '\0';

	char *s2 = text2;

	helpBufferPos = 0;
	for (int16_t subj = 0; subj < MAX_SUBJ; subj++)
	{
		textLine = 0;
		int16_t currKol = 0;
		uint8_t currColor = PAL_FORGRND;

		getLine(text); s = text;
		while (strncmp(s, "END", 3) != 0)
		{
			if (*s == ';')
			{
				if (!getLine(text))
					break;

				s = text;
				continue;
			}

			if (*(uint16_t *)s == 0x4C40) // @L - "big font"
			{
				addText(&tempText[textLine], currKol, currColor, s2);
				s += 2;

				if (*(uint16_t *)s == 0x5840) // @X - "change X position"
				{
					currKol = controlCodeToNum(&s[2]);
					s += 5;
				}

				if (*(uint16_t *)s == 0x4340) // @C - "change color
				{
					currColor = (uint8_t)controlCodeToNum(&s[2]);
					currColor = (currColor < 2) ? PAL_FORGRND : PAL_BUTTONS;
					s += 5;
				}

				helpRec *t = &tempText[textLine];
				t->xPos = currKol;
				t->color = currColor;
				t->bigFont = true;
				t->noLine = false;
				strcpy(t->text, s);
				textLine++;

				t = &tempText[textLine];
				t->noLine = true;
				textLine++;
			}
			else
			{
				if (*s == '>')
				{
					addText(&tempText[textLine], currKol, currColor, s2);
					s++;
				}

				if (*(uint16_t *)s == 0x5840) // @X - "set X position (relative to help X start)"
				{
					currKol = controlCodeToNum(&s[2]);
					s += 5;
				}

				if (*(uint16_t *)s == 0x4340) // @C - "change color"
				{
					currColor = (uint8_t)controlCodeToNum(&s[2]);
					currColor = (currColor < 2) ? PAL_FORGRND : PAL_BUTTONS;
					s += 5;
				}

				s = ltrim(rtrim(s));
				if (*s == '\0')
				{
					addText(&tempText[textLine], currKol, currColor, s2);
					strcpy(s2, " ");
					addText(&tempText[textLine], currKol, currColor, s2);
				}

				int16_t sLen = (int16_t)strlen(s);

				sEnd = &s[sLen];
				while (s < sEnd)
				{
					if (sLen < 0)
						sLen = 0;

					i = 0;
					while (s[i] != ' ' && i < sLen) i++;
					i++;

					if (*(uint16_t *)s == 0x5440) // @T - "set absolute X position (in the middle of text)"
					{
						k = controlCodeToNum(&s[2]);
						s += 5; sLen -= 5;

						s3 = &s2[strlen(s2)];
						while (textWidth(s2) + charWidth(' ') + 1 < k-currKol)
						{
							s3[0] = ' ';
							s3[1] = '\0';
							s3++;
						}

						b = textWidth(s2) + 1;
						if (b < (k - currKol))
						{
							s3 = &s2[strlen(s2)];
							for (a = 0; a < k-b-currKol; a++)
								s3[a] = 127; // one-pixel spacer glyph
							s3[a] = '\0';
						}
					}

					if (textWidth(s2)+textNWidth(s,i)+2 > HELP_WIDTH-currKol)
						addText(&tempText[textLine], currKol, currColor, s2);

					strncat(s2, s, i);

					s += i; sLen -= i;
					if ((*s == '\0') || (s >= sEnd))
						strcat(s2, " ");
				}
			}

			if (textLine >= MAX_HELP_LINES || !getLine(text))
				break;

			s = text;
		}

		subjPtrArr[subj] = (helpRec *)malloc(HELP_SIZE * textLine);
		if (subjPtrArr[subj] == NULL)
		{
			okBox(0, "System message", "Not enough memory!");
			break;
		}

		memcpy(subjPtrArr[subj], tempText, HELP_SIZE * textLine);
		subjLen[subj] = textLine;
	}

	free(tempText);
}

static void bigTextOutHalf(uint16_t xPos, uint16_t yPos, uint8_t paletteIndex, bool lowerHalf, const char *textPtr)
{
	assert(textPtr != NULL);

	uint16_t currX = xPos;
	while (true)
	{
		const char chr = *textPtr++ & 0x7F;
		if (chr == '\0')
			break;

		if (chr != ' ')
		{
			const uint8_t *srcPtr = &bmp.font2[chr * FONT2_CHAR_W];
			if (!lowerHalf)
				srcPtr += (FONT2_CHAR_H / 2) * FONT2_WIDTH;

			uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + currX];
			const uint32_t pixVal = video.palette[paletteIndex];

			for (uint32_t y = 0; y < FONT2_CHAR_H/2; y++)
			{
				for (uint32_t x = 0; x < FONT2_CHAR_W; x++)
				{
					if (srcPtr[x])
						dstPtr[x] = pixVal;
				}

				srcPtr += FONT2_WIDTH;
				dstPtr += SCREEN_W;
			}
		}

		currX += charWidth16(chr);
	}
}

static void writeHelp(void)
{
	helpRec *pek = subjPtrArr[fHlp_Nr];
	if (pek == NULL)
		return;

	for (int16_t i = 0; i < HELP_LINES; i++)
	{
		const int16_t k = i + fHlp_Line;
		if (k >= subjLen[fHlp_Nr])
			break;

		clearRect(HELP_KOL, 5 + (i * 11), HELP_WIDTH, 11);

		if (pek[k].noLine)
		{
			if (i == 0)
				bigTextOutHalf(HELP_KOL + pek[k-1].xPos, 5 + (i * 11), PAL_FORGRND, false, pek[k-1].text);
		}
		else
		{
			if (pek[k].bigFont)
			{
				if (i == (HELP_LINES - 1))
				{
					bigTextOutHalf(HELP_KOL + pek[k].xPos, 5 + (i * 11), PAL_FORGRND, true, pek[k].text);
					return;
				}
				else
				{
					clearRect(HELP_KOL, 5 + ((i + 1) * 11), HELP_WIDTH, 11);
					bigTextOut(HELP_KOL + pek[k].xPos, 5 + (i * 11), PAL_FORGRND, pek[k].text);
					i++;
				}
			}
			else
			{
				textOut(HELP_KOL + pek[k].xPos, 5 + (i * 11), pek[k].color, pek[k].text);
			}
		}
	}
}

void helpScrollUp(void)
{
	if (fHlp_Line > 0)
	{
		scrollBarScrollUp(SB_HELP_SCROLL, 1);
		writeHelp();
	}
}

void helpScrollDown(void)
{
	if (fHlp_Line < subjLen[fHlp_Nr]-1)
	{
		scrollBarScrollDown(SB_HELP_SCROLL, 1);
		writeHelp();
	}
}

void helpScrollSetPos(uint32_t pos)
{
	if (fHlp_Line != (int16_t)pos)
	{
		fHlp_Line = (int16_t)pos;
		writeHelp();
	}
}

void showHelpScreen(void)
{
	uint16_t tmpID;

	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	ui.helpScreenShown = true;

	drawFramework(0,   0, 128, 173, FRAMEWORK_TYPE1);
	drawFramework(128, 0, 504, 173, FRAMEWORK_TYPE1);
	drawFramework(130, 2, 479, 169, FRAMEWORK_TYPE2);

	showPushButton(PB_HELP_EXIT);
	showPushButton(PB_HELP_SCROLL_UP);
	showPushButton(PB_HELP_SCROLL_DOWN);

	uncheckRadioButtonGroup(RB_GROUP_HELP);
	switch (fHlp_Nr)
	{
		default:
		case 0: tmpID = RB_HELP_FEATURES;       break;
		case 1: tmpID = RB_HELP_EFFECTS;        break;
		case 2: tmpID = RB_HELP_KEYBOARD;       break;
		case 3: tmpID = RB_HELP_HOW_TO_USE_FT2; break;
		case 4: tmpID = RB_HELP_FAQ;            break;
		case 5: tmpID = RB_HELP_KNOWN_BUGS;     break;
	}
	radioButtons[tmpID].state = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(RB_GROUP_HELP);

	showScrollBar(SB_HELP_SCROLL);

	textOutShadow(4,   4, PAL_FORGRND, PAL_DSKTOP2, "Subjects:");
	textOutShadow(21, 19, PAL_FORGRND, PAL_DSKTOP2, "Features");
	textOutShadow(21, 35, PAL_FORGRND, PAL_DSKTOP2, "Effects");
	textOutShadow(21, 51, PAL_FORGRND, PAL_DSKTOP2, "Keyboard");
	textOutShadow(21, 67, PAL_FORGRND, PAL_DSKTOP2, "How to use FT2");
	textOutShadow(21, 83, PAL_FORGRND, PAL_DSKTOP2, "Problems/FAQ");
	textOutShadow(21, 99, PAL_FORGRND, PAL_DSKTOP2, "Known bugs");

	writeHelp();
}

void hideHelpScreen(void)
{
	hidePushButton(PB_HELP_EXIT);
	hidePushButton(PB_HELP_SCROLL_UP);
	hidePushButton(PB_HELP_SCROLL_DOWN);

	hideRadioButtonGroup(RB_GROUP_HELP);
	hideScrollBar(SB_HELP_SCROLL);

	ui.helpScreenShown = false;
}

void exitHelpScreen(void)
{
	hideHelpScreen();
	showTopScreen(true);
}

static void setHelpSubject(uint8_t Nr)
{
	fHlp_Nr = Nr;
	fHlp_Line = 0;

	setScrollBarEnd(SB_HELP_SCROLL, subjLen[fHlp_Nr]);
	setScrollBarPos(SB_HELP_SCROLL, 0, false);
}

void rbHelpFeatures(void)
{
	checkRadioButton(RB_HELP_FEATURES);
	setHelpSubject(0);
	writeHelp();
}

void rbHelpEffects(void)
{
	checkRadioButton(RB_HELP_EFFECTS);
	setHelpSubject(1);
	writeHelp();
}

void rbHelpKeyboard(void)
{
	checkRadioButton(RB_HELP_KEYBOARD);
	setHelpSubject(2);
	writeHelp();
}

void rbHelpHowToUseFT2(void)
{
	checkRadioButton(RB_HELP_HOW_TO_USE_FT2);
	setHelpSubject(3);
	writeHelp();
}

void rbHelpFAQ(void)
{
	checkRadioButton(RB_HELP_FAQ);
	setHelpSubject(4);
	writeHelp();
}

void rbHelpKnownBugs(void)
{
	checkRadioButton(RB_HELP_KNOWN_BUGS);
	setHelpSubject(5);
	writeHelp();
}

void initFTHelp(void)
{
	readHelp();
	setHelpSubject(0);
}

void windUpFTHelp(void)
{
	for (int16_t i = 0; i < MAX_SUBJ; i++)
	{
		if (subjPtrArr[i] != NULL)
		{
			free(subjPtrArr[i]);
			subjPtrArr[i] = NULL;
		}
	}
}
