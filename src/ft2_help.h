#pragma once

#include <stdint.h>

#define HELP_LINE_HEIGHT 11
#define HELP_WINDOW_HEIGHT 164
#define HELP_TEXT_BUFFER_W 472

void helpScrollUp(void);
void helpScrollDown(void);
void helpScrollSetPos(uint32_t pos);
void showHelpScreen(void);
void hideHelpScreen(void);
void exitHelpScreen(void);
void initFTHelp(void);
void windUpFTHelp(void);
void rbHelpFeatures(void);
void rbHelpEffects(void);
void rbHelpKeyboard(void);
void rbHelpHowToUseFT2(void);
void rbHelpFAQ(void);
void rbHelpKnownBugs(void);
