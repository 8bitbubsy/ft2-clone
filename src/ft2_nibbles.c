// directly ported from FT2 source code (except some routines)

#include <stdint.h>
#include <stdio.h>
#include <math.h> // round()
#include "ft2_keyboard.h"
#include "ft2_config.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_bmp.h"
#include "ft2_tables.h"
#include "ft2_structs.h"

#define STAGES_BMP_WIDTH 530

#define NI_MAXLEVEL 30

static const char *NI_HelpText[] =
{
	"Player 1 uses cursor keys to control movement.",
	"Player 2 uses the following keys:",
	"",
	"                  (W=Up)",
	"  (A=Left) (S=Down) (D=Right)",
	"",
	"The \"Wrap\" option controls whether it's possible to walk through",
	"the screen edges or not. Turn it on and use your brain to get",
	"the maximum out of this feature.",
	"The \"Surround\" option turns Nibbles into a completely different",
	"game. Don't change this option during play! (you'll see why)",
	"We wish you many hours of fun playing this game."
};
#define NIBBLES_HELP_LINES (sizeof (NI_HelpText) / sizeof (char *))

typedef struct
{
	int16_t antal;
	uint8_t data[8];
} nibbleBufferTyp;

typedef struct
{
	uint8_t x, y;
} nibbleCrd;

static const char nibblesCheatCode1[] = "skip", nibblesCheatCode2[] = "triton";
static char nibblesCheatBuffer[16];

static const char convHexTable2[10] = { 7, 8, 9, 10, 11, 12, 13, 16, 17, 18 };
static const uint8_t NI_Speeds[4] = { 12, 8, 6, 4 };
static bool NI_EternalLives;
static uint8_t NI_CheatIndex, NI_CurSpeed, NI_CurTick60Hz, NI_CurSpeed60Hz, NI_Screen[51][23], NI_Level;
static int16_t NI_P1Dir, NI_P2Dir, NI_P1Len, NI_P2Len, NI_Number, NI_NumberX, NI_NumberY, NI_P1NoRens, NI_P2NoRens;
static uint16_t NI_P1Lives, NI_P2Lives;
static int32_t NI_P1Score, NI_P2Score;
static nibbleCrd NI_P1[256], NI_P2[256];
static nibbleBufferTyp nibblesBuffer[2];

/* Non-FT2 feature: Check if either the Desktop or Buttons palette color
** is so close to black that the player would have troubles seeing the walls
** when playing Nibbles.
*/
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
static uint8_t rgb24ToLuminosity(uint32_t rgb24)
{
	const uint8_t R = RGB32_R(rgb24);
	const uint8_t G = RGB32_G(rgb24);
	const uint8_t B = RGB32_B(rgb24);

	// get highest channel value
	uint8_t hi = 0;
	if (hi < R) hi = R;
	if (hi < G) hi = G;
	if (hi < B) hi = B;

	// get lowest channel value
	uint8_t lo = 255;
	if (lo > R) lo = R;
	if (lo > G) lo = G;
	if (lo > B) lo = B;

	return (hi + lo) >> 1; // 0..255
}

static bool wallColorsAreCloseToBlack(void)
{
#define LUMINOSITY_THRESHOLD 4

	const uint8_t wallColor1L = rgb24ToLuminosity(video.palette[PAL_DESKTOP]);
	const uint8_t wallColor2L = rgb24ToLuminosity(video.palette[PAL_BUTTONS]);

	/* Since the rest of the wall colors are based on lower and higher
	** contrast values from these two primary colors, we don't really
	** need to check them all.
	*/

	if (wallColor1L <= LUMINOSITY_THRESHOLD || wallColor2L <= LUMINOSITY_THRESHOLD)
		return true;

	return false;
}
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

static void redrawNibblesScreen(void)
{
	if (!editor.NI_Play)
		return;

	for (int16_t x = 0; x < 51; x++)
	{
		for (int16_t y = 0; y < 23; y++)
		{
			const int16_t xs = 152 + (x * 8);
			const int16_t ys = 7 + (y * 7);

			const uint8_t c = NI_Screen[x][y];
			if (c < 16)
			{
				if (config.NI_Grid)
				{
					fillRect(xs + 0, ys + 0, 8 - 0, 7 - 0, PAL_BUTTON2);
					fillRect(xs + 1, ys + 1, 8 - 1, 7 - 1, c);
				}
				else
				{
					fillRect(xs, ys, 8, 7, c);
				}
			}
			else
			{
				charOut(xs + 2, ys, PAL_FORGRND, convHexTable2[NI_Number]);
			}
		}
	}

	// fix wrongly rendered grid
	if (config.NI_Grid)
	{
		vLine(560,   7, 161, PAL_BUTTON2);
		hLine(152, 168, 409, PAL_BUTTON2);
	}
	else
	{
		// if we turned grid off, clear lines
		vLine(560,   7, 161, PAL_BCKGRND);
		hLine(152, 168, 409, PAL_BCKGRND);
	}
}

static void nibblesAddBuffer(int16_t nr, uint8_t typ)
{
	nibbleBufferTyp *n = &nibblesBuffer[nr];
	if (n->antal < 8)
	{
		n->data[n->antal] = typ;
		n->antal++;
	}
}

static bool nibblesBufferFull(int16_t nr)
{
	return (nibblesBuffer[nr].antal > 0);
}

static int16_t nibblesGetBuffer(int16_t nr)
{
	nibbleBufferTyp *n = &nibblesBuffer[nr];
	if (n->antal > 0)
	{
		const int16_t dataOut = n->data[0];
		memmove(&n->data[0], &n->data[1], 7);
		n->antal--;

		return dataOut;
	}

	return -1;
}

static void nibblesGetLevel(int16_t nr)
{
	const int32_t readX = 1 + ((51+2) * (nr % 10));
	const int32_t readY = 1 + ((23+2) * (nr / 10));

	const uint8_t *stagePtr = &bmp.nibblesStages[(readY * STAGES_BMP_WIDTH) + readX];

	for (int32_t y = 0; y < 23; y++)
	{
		for (int32_t x = 0; x < 51; x++)
			NI_Screen[x][y] = stagePtr[x];

		stagePtr += STAGES_BMP_WIDTH;
	}
}

static void nibblesCreateLevel(int16_t nr)
{
	if (nr >= NI_MAXLEVEL)
		nr = NI_MAXLEVEL - 1;

	nibblesGetLevel(nr);

	int32_t x1 = 0;
	int32_t x2 = 0;
	int32_t y1 = 0;
	int32_t y2 = 0;

	for (int32_t y = 0; y < 23; y++)
	{
		for (int32_t x = 0; x < 51; x++)
		{
			if (NI_Screen[x][y] == 1 || NI_Screen[x][y] == 3)
			{
				const uint8_t c = NI_Screen[x][y];

				if (c == 3)
				{
					x1 = x;
					y1 = y;
				}

				if (c == 1)
				{
					x2 = x;
					y2 = y;
				}

				NI_Screen[x][y] = 0;
			}
		}
	}

	const int32_t readX = (51 + 2) * (nr % 10);
	const int32_t readY = (23 + 2) * (nr / 10);

	NI_P1Dir = bmp.nibblesStages[(readY * 530) + (readX + 1)];
	NI_P2Dir = bmp.nibblesStages[(readY * 530) + (readX + 0)];

	NI_P1Len = 5;
	NI_P2Len = 5;
	NI_P1NoRens = 0;
	NI_P2NoRens = 0;
	NI_Number = 0;
	nibblesBuffer[0].antal = 0;
	nibblesBuffer[1].antal = 0;

	for (int32_t i = 0; i < 256; i++)
	{
		NI_P1[i].x = (uint8_t)x1;
		NI_P1[i].y = (uint8_t)y1;
		NI_P2[i].x = (uint8_t)x2;
		NI_P2[i].y = (uint8_t)y2;
	}
}

static void nibbleWriteLevelSprite(int16_t xOut, int16_t yOut, int16_t nr)
{
	const int32_t readX = (51 + 2) * (nr % 10);
	const int32_t readY = (23 + 2) * (nr / 10);

	const uint8_t *src = (const uint8_t *)&bmp.nibblesStages[(readY * 530) + readX];
	uint32_t *dst = &video.frameBuffer[(yOut * SCREEN_W) + xOut];

	for (int32_t y = 0; y < 23+2; y++)
	{
		for (int32_t x = 0; x < 51+2; x++)
			dst[x] = video.palette[src[x]];

		src += 530;
		dst += SCREEN_W;
	}

	// overwrite start position pixels
	video.frameBuffer[(yOut * SCREEN_W) + (xOut + 0)] = video.palette[PAL_FORGRND];
	video.frameBuffer[(yOut * SCREEN_W) + (xOut + 1)] = video.palette[PAL_FORGRND];
}

static void highScoreTextOutClipX(uint16_t x, uint16_t y, uint8_t paletteIndex, uint8_t shadowPaletteIndex, const char *textPtr, uint16_t clipX)
{
	assert(textPtr != NULL);

	uint16_t currX = x;
	for (uint16_t i = 0; i < 22; i++)
	{
		const char ch = textPtr[i];
		if (ch == '\0')
			break;

		charOutClipX(currX + 1, y + 1, shadowPaletteIndex, ch, clipX); // shadow
		charOutClipX(currX, y, paletteIndex, ch, clipX); // foreground

		currX += charWidth(ch);
		if (currX >= clipX)
			break;
	}
}

void nibblesHighScore(void)
{
	if (editor.NI_Play)
	{
		okBox(0, "Nibbles message", "The highscore table is not available during play.");
		return;
	}

	clearRect(152, 7, 409, 162);

	bigTextOut(160, 10, PAL_FORGRND, "Fasttracker Nibbles Highscore");
	for (int16_t i = 0; i < 5; i++)
	{
		highScoreTextOutClipX(160, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, config.NI_HighScore[i].name, 160 + 70);
		hexOutShadow(160 + 76, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, config.NI_HighScore[i].score, 8);
		nibbleWriteLevelSprite(160 + 136, (42 - 9) + (26 * i), config.NI_HighScore[i].level);

		highScoreTextOutClipX(360, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, config.NI_HighScore[i+5].name, 360 + 70);
		hexOutShadow(360 + 76, 42 + (26 * i), PAL_FORGRND, PAL_DSKTOP2, config.NI_HighScore[i+5].score, 8);
		nibbleWriteLevelSprite(360 + 136, (42 - 9) + (26 * i), config.NI_HighScore[i+5].level);
	}
}

static void setNibbleDot(uint8_t x, uint8_t y, uint8_t c)
{
	const uint16_t xs = 152 + (x * 8);
	const uint16_t ys = 7 + (y * 7);

	if (config.NI_Grid)
	{
		fillRect(xs + 0, ys + 0, 8 - 0, 7 - 0, PAL_BUTTON2);
		fillRect(xs + 1, ys + 1, 8 - 1, 7 - 1, c);
	}
	else
	{
		fillRect(xs, ys, 8, 7, c);
	}

	NI_Screen[x][y] = c;
}

static void nibblesGenNewNumber(void)
{
	while (true)
	{
		const int16_t x = rand() % 51;
		const int16_t y = rand() % 23;

		bool blockIsSuitable;

		if (y < 22)
			blockIsSuitable = NI_Screen[x][y] == 0 && NI_Screen[x][y+1] == 0;
		else
			blockIsSuitable = NI_Screen[x][y] == 0; // FT2 bugfix: prevent look-up overflow

		if (blockIsSuitable)
		{
			NI_Number++;
			NI_Screen[x][y] = (uint8_t)(16 + NI_Number);
			NI_NumberX = x;
			NI_NumberY = y;

			const int16_t xs = 152 + (x * 8);
			const int16_t ys = 7 + (y * 7);

			if (config.NI_Grid)
			{
				fillRect(xs + 0, ys + 0, 8 - 0, 7 - 0, PAL_BUTTON2);
				fillRect(xs + 1, ys + 1, 8 - 1, 7 - 1, PAL_BCKGRND);
			}
			else
			{
				fillRect(xs, ys, 8, 7, PAL_BCKGRND);
			}

			charOut((x * 8) + 154, (y * 7) + 7, PAL_FORGRND, convHexTable2[NI_Number]);
			break;
		}
	}
}

static void newNibblesGame(void)
{
	nibblesCreateLevel(NI_Level);
	redrawNibblesScreen();

	setNibbleDot(NI_P1[0].x, NI_P1[0].y, 6);
	if (config.NI_AntPlayers == 1)
		setNibbleDot(NI_P2[0].x, NI_P2[0].y, 7);

	if (!config.NI_Surround)
		nibblesGenNewNumber();
}

static bool nibblesInvalid(int16_t x, int16_t y, int16_t d)
{
	if (!config.NI_Wrap)
	{
		if ((x == 0 && d == 0) || (x == 50 && d == 2) || (y == 0 && d == 3) || (y == 22 && d == 1))
			return true;
	}

	assert(x >= 0 && x < 51 && y >= 0 && y < 23);
	return (NI_Screen[x][y] >= 1 && NI_Screen[x][y] <= 15);
}

static void drawScoresLives(void)
{
	// player 1
	assert(NI_P1Lives < 100);
	hexOutBg(89, 27, PAL_FORGRND, PAL_DESKTOP, NI_P1Score, 8);
	textOutFixed(131, 39, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[NI_P1Lives]);

	// player 2
	assert(NI_P2Lives < 100);
	hexOutBg(89, 75, PAL_FORGRND, PAL_DESKTOP, NI_P2Score, 8);
	textOutFixed(131, 87, PAL_FORGRND, PAL_DESKTOP, dec2StrTab[NI_P2Lives]);
}

static void nibblesDecLives(int16_t l1, int16_t l2)
{
	char name[21+1];
	int16_t i, k;
	highScoreType *h;

	if (!NI_EternalLives)
	{
		NI_P1Lives -= l1;
		NI_P2Lives -= l2;
	}

	drawScoresLives();

	if (l1+l2 == 2)
	{
		okBox(0, "Nibbles message", "Both players died!");
	}
	else
	{
		if (l2 == 0)
			okBox(0, "Nibbles message", "Player 1 died!");
		else
			okBox(0, "Nibbles message", "Player 2 died!");
	}

	if (NI_P1Lives == 0 || NI_P2Lives == 0)
	{
		editor.NI_Play = false;
		okBox(0, "Nibbles message", "GAME OVER");

		// prevent highscore table from showing overflowing level graphics
		if (NI_Level >= NI_MAXLEVEL)
			NI_Level = NI_MAXLEVEL - 1;

		if (NI_P1Score > config.NI_HighScore[9].score)
		{
			strcpy(name, "Unknown");
			inputBox(0, "Player 1 - Enter your name:", name, sizeof (name) - 1);

			i = 0;
			while (NI_P1Score <= config.NI_HighScore[i].score)
				i++;

			for (k = 8; k >= i; k--)
				memcpy(&config.NI_HighScore[k+1], &config.NI_HighScore[k], sizeof (highScoreType));

			if (i == 0)
				okBox(0, "Nibbles message", "You've probably cheated!");

			h = &config.NI_HighScore[i];

			k = (int16_t)strlen(name);
			memset(h->name, 0, sizeof (h->name));
			memcpy(h->name, name, k);
			h->nameLen = (uint8_t)k;

			h->score = NI_P1Score;
			h->level = NI_Level;
		}

		if (NI_P2Score > config.NI_HighScore[9].score)
		{
			strcpy(name, "Unknown");
			inputBox(0, "Player 2 - Enter your name:", name, sizeof (name) - 1);

			i = 0;
			while (NI_P2Score <= config.NI_HighScore[i].score)
				i++;

			for (k = 8; k >= i; k--)
				memcpy(&config.NI_HighScore[k+1], &config.NI_HighScore[k], sizeof (highScoreType));

			if (i == 0)
				okBox(0, "Nibbles message", "You've probably cheated!");

			h = &config.NI_HighScore[i];
			k = (int16_t)strlen(name);

			memset(h->name, 0, sizeof (h->name));
			memcpy(h->name, name, k);
			h->nameLen = (uint8_t)k;

			h->score = NI_P2Score;
			h->level = NI_Level;
		}

		nibblesHighScore();
	}
	else
	{
		editor.NI_Play = true;
		newNibblesGame();
	}
}

static void nibblesEraseNumber(void)
{
	if (!config.NI_Surround)
		setNibbleDot((uint8_t)NI_NumberX, (uint8_t)NI_NumberY, 0);
}

static void nibblesNewLevel(void)
{
	char text[24];

	sprintf(text, "Level %d finished!", NI_Level+1);
	okBox(0, "Nibbles message", text);

	// cast to int16_t to simulate a bug in FT2
	NI_P1Score += 0x10000 + (int16_t)((12 - NI_CurSpeed) * 0x2000);
	if (config.NI_AntPlayers == 1)
		NI_P2Score += 0x10000;

	NI_Level++;

	if (NI_P1Lives < 99)
		NI_P1Lives++;

	if (config.NI_AntPlayers == 1)
	{
		if (NI_P2Lives < 99)
			NI_P2Lives++;
	}

	NI_Number = 0;
	nibblesCreateLevel(NI_Level);
	redrawNibblesScreen();

	nibblesGenNewNumber();
}

void moveNibblePlayers(void)
{
	if (ui.sysReqShown || --NI_CurTick60Hz != 0)
		return;

	if (nibblesBufferFull(0))
	{
		switch (nibblesGetBuffer(0))
		{
			case 0: if (NI_P1Dir != 2) NI_P1Dir = 0; break;
			case 1: if (NI_P1Dir != 3) NI_P1Dir = 1; break;
			case 2: if (NI_P1Dir != 0) NI_P1Dir = 2; break;
			case 3: if (NI_P1Dir != 1) NI_P1Dir = 3; break;
			default: break;
		}
	}

	if (nibblesBufferFull(1))
	{
		switch (nibblesGetBuffer(1))
		{
			case 0: if (NI_P2Dir != 2) NI_P2Dir = 0; break;
			case 1: if (NI_P2Dir != 3) NI_P2Dir = 1; break;
			case 2: if (NI_P2Dir != 0) NI_P2Dir = 2; break;
			case 3: if (NI_P2Dir != 1) NI_P2Dir = 3; break;
			default: break;
		}
	}

	memmove(&NI_P1[1], &NI_P1[0], 255 * sizeof (nibbleCrd));
	if (config.NI_AntPlayers == 1)
		memmove(&NI_P2[1], &NI_P2[0], 255 * sizeof (nibbleCrd));

	switch (NI_P1Dir)
	{
		case 0: NI_P1[0].x++; break;
		case 1: NI_P1[0].y--; break;
		case 2: NI_P1[0].x--; break;
		case 3: NI_P1[0].y++; break;
		default: break;
	}

	if (config.NI_AntPlayers == 1)
	{
		switch (NI_P2Dir)
		{
			case 0: NI_P2[0].x++; break;
			case 1: NI_P2[0].y--; break;
			case 2: NI_P2[0].x--; break;
			case 3: NI_P2[0].y++; break;
			default: break;
		}
	}

	if (NI_P1[0].x == 255) NI_P1[0].x = 50;
	if (NI_P2[0].x == 255) NI_P2[0].x = 50;
	if (NI_P1[0].y == 255) NI_P1[0].y = 22;
	if (NI_P2[0].y == 255) NI_P2[0].y = 22;

	NI_P1[0].x %= 51;
	NI_P1[0].y %= 23;
	NI_P2[0].x %= 51;
	NI_P2[0].y %= 23;

	if (config.NI_AntPlayers == 1)
	{
		if (nibblesInvalid(NI_P1[0].x, NI_P1[0].y, NI_P1Dir) && nibblesInvalid(NI_P2[0].x, NI_P2[0].y, NI_P2Dir))
		{
			nibblesDecLives(1, 1);
			goto NoMove;
		}
		else if (nibblesInvalid(NI_P1[0].x, NI_P1[0].y, NI_P1Dir))
		{
			nibblesDecLives(1, 0);
			goto NoMove;
		}
		else if (nibblesInvalid(NI_P2[0].x, NI_P2[0].y, NI_P2Dir))
		{
			nibblesDecLives(0, 1);
			goto NoMove;
		}
		else if (NI_P1[0].x == NI_P2[0].x && NI_P1[0].y == NI_P2[0].y)
		{
			nibblesDecLives(1, 1);
			goto NoMove;
		}
	}
	else
	{
		if (nibblesInvalid(NI_P1[0].x, NI_P1[0].y, NI_P1Dir))
		{
			nibblesDecLives(1, 0);
			goto NoMove;
		}
	}

	int16_t j = 0;
	int16_t i = NI_Screen[NI_P1[0].x][NI_P1[0].y];
	if (i >= 16)
	{
		NI_P1Score += (i & 15) * 999 * (NI_Level + 1);
		nibblesEraseNumber(); j = 1;
		NI_P1NoRens = NI_P1Len / 2;
	}

	if (config.NI_AntPlayers == 1)
	{
		i = NI_Screen[NI_P2[0].x][NI_P2[0].y];
		if (i >= 16)
		{
			NI_P2Score += ((i & 15) * 999 * (NI_Level + 1));
			nibblesEraseNumber(); j = 1;
			NI_P2NoRens = NI_P2Len / 2;
		}
	}

	NI_P1Score -= 17;
	if (config.NI_AntPlayers == 1)
		NI_P2Score -= 17;

	if (NI_P1Score < 0) NI_P1Score = 0;
	if (NI_P2Score < 0) NI_P2Score = 0;

	if (!config.NI_Surround)
	{
		if (NI_P1NoRens > 0 && NI_P1Len < 255)
		{
			NI_P1NoRens--;
			NI_P1Len++;
		}
		else
		{
			setNibbleDot(NI_P1[NI_P1Len].x, NI_P1[NI_P1Len].y, 0);
		}

		if (config.NI_AntPlayers == 1)
		{
			if (NI_P2NoRens > 0 && NI_P2Len < 255)
			{
				NI_P2NoRens--;
				NI_P2Len++;
			}
			else
			{
				setNibbleDot(NI_P2[NI_P2Len].x, NI_P2[NI_P2Len].y, 0);
			}
		}
	}

	setNibbleDot(NI_P1[0].x, NI_P1[0].y, 6);
	if (config.NI_AntPlayers == 1)
		setNibbleDot(NI_P2[0].x, NI_P2[0].y, 5);

	if (j == 1 && !config.NI_Surround)
	{
		if (NI_Number == 9)
		{
			nibblesNewLevel();
			NI_CurTick60Hz = NI_CurSpeed60Hz;
			return;
		}

		nibblesGenNewNumber();
	}

NoMove:
	NI_CurTick60Hz = NI_CurSpeed60Hz;
	drawScoresLives();
}

void showNibblesScreen(void)
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	ui.nibblesShown = true;

	drawFramework(0,     0, 632,   3, FRAMEWORK_TYPE1);
	drawFramework(0,     3, 148,  49, FRAMEWORK_TYPE1);
	drawFramework(0,    52, 148,  49, FRAMEWORK_TYPE1);
	drawFramework(0,   101, 148,  72, FRAMEWORK_TYPE1);
	drawFramework(148,   3, 417, 170, FRAMEWORK_TYPE1);
	drawFramework(150,   5, 413, 166, FRAMEWORK_TYPE2);
	drawFramework(565,   3,  67, 170, FRAMEWORK_TYPE1);

	bigTextOutShadow(4,   6,  PAL_FORGRND, PAL_DSKTOP2, "Player 1");
	bigTextOutShadow(4,  55,  PAL_FORGRND, PAL_DSKTOP2, "Player 2");

	textOutShadow(4,  27,  PAL_FORGRND, PAL_DSKTOP2, "Score");
	textOutShadow(4,  75,  PAL_FORGRND, PAL_DSKTOP2, "Score");
	textOutShadow(4,  39,  PAL_FORGRND, PAL_DSKTOP2, "Lives");
	textOutShadow(4,  87,  PAL_FORGRND, PAL_DSKTOP2, "Lives");
	textOutShadow(18, 106, PAL_FORGRND, PAL_DSKTOP2, "1 player");
	textOutShadow(18, 120, PAL_FORGRND, PAL_DSKTOP2, "2 players");
	textOutShadow(20, 135, PAL_FORGRND, PAL_DSKTOP2, "Surround");
	textOutShadow(20, 148, PAL_FORGRND, PAL_DSKTOP2, "Grid");
	textOutShadow(20, 161, PAL_FORGRND, PAL_DSKTOP2, "Wrap");
	textOutShadow(80, 105, PAL_FORGRND, PAL_DSKTOP2, "Difficulty:");
	textOutShadow(93, 118, PAL_FORGRND, PAL_DSKTOP2, "Novice");
	textOutShadow(93, 132, PAL_FORGRND, PAL_DSKTOP2, "Average");
	textOutShadow(93, 146, PAL_FORGRND, PAL_DSKTOP2, "Pro");
	textOutShadow(93, 160, PAL_FORGRND, PAL_DSKTOP2, "Triton");

	drawScoresLives();

	blitFast(569, 7, bmp.nibblesLogo, 59, 91);

	showPushButton(PB_NIBBLES_PLAY);
	showPushButton(PB_NIBBLES_HELP);
	showPushButton(PB_NIBBLES_HIGHS);
	showPushButton(PB_NIBBLES_EXIT);

	checkBoxes[CB_NIBBLES_SURROUND].checked = config.NI_Surround ? true : false;
	checkBoxes[CB_NIBBLES_GRID].checked = config.NI_Grid ? true : false;
	checkBoxes[CB_NIBBLES_WRAP].checked = config.NI_Wrap ? true : false;
	showCheckBox(CB_NIBBLES_SURROUND);
	showCheckBox(CB_NIBBLES_GRID);
	showCheckBox(CB_NIBBLES_WRAP);

	uncheckRadioButtonGroup(RB_GROUP_NIBBLES_PLAYERS);
	if (config.NI_AntPlayers == 0)
		radioButtons[RB_NIBBLES_1PLAYER].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_NIBBLES_2PLAYERS].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_NIBBLES_PLAYERS);

	uncheckRadioButtonGroup(RB_GROUP_NIBBLES_DIFFICULTY);
	switch (config.NI_Speed)
	{
		default:
		case 0: radioButtons[RB_NIBBLES_NOVICE].state  = RADIOBUTTON_CHECKED; break;
		case 1: radioButtons[RB_NIBBLES_AVERAGE].state = RADIOBUTTON_CHECKED; break;
		case 2: radioButtons[RB_NIBBLES_PRO].state     = RADIOBUTTON_CHECKED; break;
		case 3: radioButtons[RB_NIBBLES_MANIAC].state  = RADIOBUTTON_CHECKED; break;
	}
	showRadioButtonGroup(RB_GROUP_NIBBLES_DIFFICULTY);
}

void hideNibblesScreen(void)
{
	hidePushButton(PB_NIBBLES_PLAY);
	hidePushButton(PB_NIBBLES_HELP);
	hidePushButton(PB_NIBBLES_HIGHS);
	hidePushButton(PB_NIBBLES_EXIT);

	hideRadioButtonGroup(RB_GROUP_NIBBLES_PLAYERS);
	hideRadioButtonGroup(RB_GROUP_NIBBLES_DIFFICULTY);

	hideCheckBox(CB_NIBBLES_SURROUND);
	hideCheckBox(CB_NIBBLES_GRID);
	hideCheckBox(CB_NIBBLES_WRAP);

	ui.nibblesShown = false;
}

void exitNibblesScreen(void)
{
	hideNibblesScreen();
	showTopScreen(true);
}

// PUSH BUTTONS

void nibblesPlay(void)
{
	if (editor.NI_Play)
	{
		if (okBox(2, "Nibbles request", "Restart current game of Nibbles?") != 1)
			return;
	}

	if (config.NI_Surround && config.NI_AntPlayers == 0)
	{
		okBox(0, "Nibbles message", "Surround mode is not appropriate in one-player mode.");
		return;
	}

	if (wallColorsAreCloseToBlack())
		okBox(0, "Nibbles warning", "The Desktop/Button colors are set to values that make the walls hard to see!");

	assert(config.NI_Speed < 4);
	NI_CurSpeed = NI_Speeds[config.NI_Speed];

	// adjust for 70Hz -> 60Hz frames
	NI_CurSpeed60Hz = (uint8_t)round(NI_CurSpeed * ((double)VBLANK_HZ / FT2_VBLANK_HZ));
	NI_CurTick60Hz = (uint8_t)round(NI_Speeds[2] * ((double)VBLANK_HZ / FT2_VBLANK_HZ));

	editor.NI_Play = true;
	NI_P1Score = 0;
	NI_P2Score = 0;
	NI_P1Lives = 5;
	NI_P2Lives = 5;
	NI_Level = 0;

	newNibblesGame();
}

void nibblesHelp(void)
{
	if (editor.NI_Play)
	{
		okBox(0, "System message", "Help is not available during play.");
		return;
	}

	clearRect(152, 7, 409, 162);

	bigTextOut(160, 10, PAL_FORGRND, "Fasttracker Nibbles Help");
	for (uint16_t i = 0; i < NIBBLES_HELP_LINES; i++)
		textOut(160, 36 + (11 * i), PAL_BUTTONS, NI_HelpText[i]);
}

void nibblesExit(void)
{
	if (editor.NI_Play)
	{
		if (okBox(2, "System request", "Quit current game of Nibbles?") == 1)
		{
			editor.NI_Play = false;
			exitNibblesScreen();
		}

		return;
	}

	exitNibblesScreen();
}

// RADIO BUTTONS

void nibblesSet1Player(void)
{
	config.NI_AntPlayers = 0;
	checkRadioButton(RB_NIBBLES_1PLAYER);
}

void nibblesSet2Players(void)
{
	config.NI_AntPlayers = 1;
	checkRadioButton(RB_NIBBLES_2PLAYERS);
}

void nibblesSetNovice(void)
{
	config.NI_Speed = 0;
	checkRadioButton(RB_NIBBLES_NOVICE);
}

void nibblesSetAverage(void)
{
	config.NI_Speed = 1;
	checkRadioButton(RB_NIBBLES_AVERAGE);
}

void nibblesSetPro(void)
{
	config.NI_Speed = 2;
	checkRadioButton(RB_NIBBLES_PRO);
}

void nibblesSetTriton(void)
{
	config.NI_Speed = 3;
	checkRadioButton(RB_NIBBLES_MANIAC);
}

// CHECK BOXES

void nibblesToggleSurround(void)
{
	config.NI_Surround ^= 1;
	checkBoxes[CB_NIBBLES_SURROUND].checked = config.NI_Surround ? true : false;
	showCheckBox(CB_NIBBLES_SURROUND);
}

void nibblesToggleGrid(void)
{
	config.NI_Grid ^= 1;
	checkBoxes[CB_NIBBLES_GRID].checked = config.NI_Grid ? true : false;
	showCheckBox(CB_NIBBLES_GRID);

	if (editor.NI_Play)
		redrawNibblesScreen();
}

void nibblesToggleWrap(void)
{
	config.NI_Wrap ^= 1;

	checkBoxes[CB_NIBBLES_WRAP].checked = config.NI_Wrap ? true : false;
	showCheckBox(CB_NIBBLES_WRAP);
}

// GLOBAL FUNCTIONS

void nibblesKeyAdministrator(SDL_Scancode scancode)
{
	if (scancode == SDL_SCANCODE_ESCAPE)
	{
		if (okBox(2, "System request", "Quit current game of Nibbles?") == 1)
		{
			editor.NI_Play = false;
			exitNibblesScreen();
		}

		return;
	}

	switch (scancode)
	{
		// player 1
		case SDL_SCANCODE_RIGHT: nibblesAddBuffer(0, 0); break;
		case SDL_SCANCODE_UP:    nibblesAddBuffer(0, 1); break;
		case SDL_SCANCODE_LEFT:  nibblesAddBuffer(0, 2); break;
		case SDL_SCANCODE_DOWN:  nibblesAddBuffer(0, 3); break;

		// player 2
		case SDL_SCANCODE_D: nibblesAddBuffer(1, 0); break;
		case SDL_SCANCODE_W: nibblesAddBuffer(1, 1); break;
		case SDL_SCANCODE_A: nibblesAddBuffer(1, 2); break;
		case SDL_SCANCODE_S: nibblesAddBuffer(1, 3); break;

		default: break;
	}
}

bool testNibblesCheatCodes(SDL_Keycode keycode) // not directly ported, but same cheatcodes
{
	const char *codeStringPtr;
	uint8_t codeStringLen;

	// nibbles cheat codes can only be typed in while holding down left SHIFT+CTRL+ALT
	if (keyb.leftShiftPressed && keyb.leftCtrlPressed && keyb.leftAltPressed)
	{
		if (editor.NI_Play)
		{
			// during game: "S", "K", "I", "P" (skip to next level)
			codeStringPtr = nibblesCheatCode1;
			codeStringLen = sizeof (nibblesCheatCode1) - 1;
		}
		else
		{
			// not during game: "T", "R", "I", "T", "O", "N" (enable infinite lives)
			codeStringPtr = nibblesCheatCode2;
			codeStringLen = sizeof (nibblesCheatCode2) - 1;
		}

		nibblesCheatBuffer[NI_CheatIndex] = (char)keycode;
		if (nibblesCheatBuffer[NI_CheatIndex] != codeStringPtr[NI_CheatIndex])
		{
			NI_CheatIndex = 0; // start over again, one letter didn't match
			return true;
		}

		if (++NI_CheatIndex == codeStringLen) // cheat code was successfully entered
		{
			NI_CheatIndex = 0;

			if (editor.NI_Play)
			{
				nibblesNewLevel();
			}
			else
			{
				NI_EternalLives ^= 1;
				if (NI_EternalLives)
					okBox(0, "Triton productions declares:", "Eternal lives activated!");
				else
					okBox(0, "Triton productions declares:", "Eternal lives deactivated!");
			}
		}

		return true; // SHIFT+CTRL+ALT held down, don't test other keys
	}

	return false; // SHIFT+CTRL+ALT not held down, test other keys
}

void pbNibbles(void)
{
	showNibblesScreen();
}
