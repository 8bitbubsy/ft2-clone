// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_about.h"
#include "ft2_video.h"
#include "ft2_config.h"
#include "ft2_audioselector.h"
#include "ft2_audio.h"
#include "ft2_help.h"
#include "ft2_sample_ed.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_pattern_ed.h"
#include "ft2_sample_loader.h"
#include "ft2_diskop.h"
#include "ft2_wav_renderer.h"
#include "ft2_trim.h"
#include "ft2_sampling.h"
#include "ft2_module_loader.h"
#include "ft2_midi.h"
#include "ft2_mouse.h"
#include "ft2_edit.h"
#include "ft2_sample_ed_features.h"
#include "ft2_smpfx.h"
#include "ft2_palette.h"
#include "ft2_structs.h"
#include "ft2_bmp.h"

#define BUTTON_GFX_BMP_WIDTH 90

pushButton_t pushButtons[NUM_PUSHBUTTONS] =
{
	// ------ RESERVED PUSHBUTTONS ------
	{ 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 }, { 0 },

	/*
	** -- STRUCT INFO: --
	**  x          = x position
	**  y          = y position
	**  w          = width
	**  h          = height
	**  p (0,1,2)  = button delay after the first trigger (f.ex. used on scrollbar buttons)
	**                0 = no delay, 1 = normal delay, 2 = two-phase delay
	**  d          = function call repeat delay while button is being held down (number of frames)
	**  text #1    = text line #1
	**  text #2    = text line #2 (if present, it will be rendered below text #1)
	**  funcOnDown = function to call when pressed
	**  funcOnUp   = function to call when released
	*/

	// ------ POSITION EDITOR PUSHBUTTONS ------
	//x,  y,  w,  h,  p, d, text #1,           text #2, funcOnDown,      funcOnUp
	{ 55,  2, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbPosEdPosDown,  NULL },
	{ 55, 36, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbPosEdPosUp,    NULL },
	{ 74,  2, 35, 16, 1, 6, "Ins.",            NULL,    pbPosEdIns,      NULL },
	{ 74, 19, 18, 13, 1, 6, ARROW_UP_STRING,   NULL,    pbPosEdPattUp,   NULL },
	{ 91, 19, 18, 13, 1, 6, ARROW_DOWN_STRING, NULL,    pbPosEdPattDown, NULL },
	{ 74, 33, 35, 16, 1, 6, "Del.",            NULL,    pbPosEdDel,      NULL },
	{ 74, 50, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbPosEdLenUp,    NULL },
	{ 91, 50, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbPosEdLenDown,  NULL },
	{ 74, 62, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbPosEdRepSUp,   NULL },
	{ 91, 62, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbPosEdRepSDown, NULL },

	// ------ SONG/PATTERN PUSHBUTTONS ------
	//x,   y,  w,  h,  p, d, text #1,           text #2, funcOnDown,     funcOnUp
	{ 168, 34, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbBPMUp,        NULL },
	{ 185, 34, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbBPMDown,      NULL },
	{ 168, 48, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbSpeedUp,      NULL },
	{ 185, 48, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbSpeedDown,    NULL },
	{ 168, 62, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbIncAdd,       NULL },
	{ 185, 62, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbDecAdd,       NULL },
	{ 253, 34, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbEditPattUp,   NULL },
	{ 270, 34, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbEditPattDown, NULL },
	{ 253, 48, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbPattLenUp,    NULL },
	{ 270, 48, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbPattLenDown,  NULL },
	{ 209, 62, 40, 13, 0, 0, "Expd.",           NULL,    NULL,           expandPattern },
	{ 248, 62, 40, 13, 0, 0, "Srnk.",           NULL,    NULL,           shrinkPattern },

	// ------ LOGO PUSHBUTTON ------
	//x,   y, w,   h,  p, d, text #1, text #2, funcOnDown, funcOnUp
	{ 112, 0, 154, 32, 0, 0, NULL,    NULL,    NULL,       pbToggleLogo  },
	{ 266, 0,  25, 32, 0, 0, NULL,    NULL,    NULL,       pbToggleBadge },

	// ------ MAIN SCREEN PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,      text #2, funcOnDown, funcOnUp
	{ 294,   2, 59, 16, 0, 0, "About",      NULL,    NULL,       showAboutScreen },
	{ 294,  19, 59, 16, 0, 0, "Nibbles",    NULL,    NULL,       pbNibbles },
	{ 294,  36, 59, 16, 0, 0, "Zap",        NULL,    NULL,       pbZap },
	{ 294,  53, 59, 16, 0, 0, "Trim",       NULL,    NULL,       toggleTrimScreen },
	{ 294,  70, 59, 16, 0, 0, "Extend",     NULL,    NULL,       patternEditorExtended },
	{ 294,  87, 59, 16, 0, 0, "Transps.",   NULL,    NULL,       toggleTranspose },
	{ 294, 104, 59, 16, 0, 0, "I.E.Ext.",   NULL,    NULL,       toggleInstEditorExt },
	{ 294, 121, 59, 16, 0, 0, "S.E.Ext.",   NULL,    NULL,       toggleSampleEditorExt },
	{ 294, 138, 59, 16, 0, 0, "Adv. Edit",  NULL,    NULL,       toggleAdvEdit },
	{ 294, 155, 30, 16, 0, 0, "Add",        NULL,    NULL,       pbAddChan },
	{ 323, 155, 30, 16, 0, 0, "Sub",        NULL,    NULL,       pbSubChan },
	{ 359,   2, 59, 16, 0, 0, "Play sng.",  NULL,    NULL,       pbPlaySong },
	{ 359,  19, 59, 16, 0, 0, "Play ptn.",  NULL,    NULL,       pbPlayPtn },
	{ 359,  36, 59, 16, 0, 0, "Stop",       NULL,    NULL,       stopPlaying },
	{ 359,  53, 59, 16, 0, 0, "Rec. sng.",  NULL,    NULL,       pbRecSng },
	{ 359,  70, 59, 16, 0, 0, "Rec. ptn.",  NULL,    NULL,       pbRecPtn },
	{ 359,  87, 59, 16, 0, 0, "Disk op.",   NULL,    NULL,       toggleDiskOpScreen },
	{ 359, 104, 59, 16, 0, 0, "Instr. Ed.", NULL,    NULL,       toggleInstEditor },
	{ 359, 121, 59, 16, 0, 0, "Smp. Ed.",   NULL,    NULL,       toggleSampleEditor },
	{ 359, 138, 59, 16, 0, 0, "Config",     NULL,    NULL,       showConfigScreen },
	{ 359, 155, 59, 16, 0, 0, "Help",       NULL,    NULL,       showHelpScreen },
	{ 115,  35, 46, 16, 0, 0, "Exit",       NULL,    NULL,       exitPatternEditorExtended },

	// ------ INSTRUMENT SWITCHER PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown,           funcOnUp
	{ 590,   2, 39, 16, 0, 0, "01-08",           NULL,    NULL,                 pbSetInstrBank1 },
	{ 590,  19, 39, 16, 0, 0, "09-10",           NULL,    NULL,                 pbSetInstrBank2 },
	{ 590,  36, 39, 16, 0, 0, "11-18",           NULL,    NULL,                 pbSetInstrBank3 },
	{ 590,  53, 39, 16, 0, 0, "19-20",           NULL,    NULL,                 pbSetInstrBank4 },
	{ 590,  73, 39, 16, 0, 0, "21-28",           NULL,    NULL,                 pbSetInstrBank5 },
	{ 590,  90, 39, 16, 0, 0, "29-30",           NULL,    NULL,                 pbSetInstrBank6 },
	{ 590, 107, 39, 16, 0, 0, "31-38",           NULL,    NULL,                 pbSetInstrBank7 },
	{ 590, 124, 39, 16, 0, 0, "39-40",           NULL,    NULL,                 pbSetInstrBank8 },
	{ 590,   2, 39, 16, 0, 0, "41-48",           NULL,    NULL,                 pbSetInstrBank9 },
	{ 590,  19, 39, 16, 0, 0, "49-50",           NULL,    NULL,                 pbSetInstrBank10 },
	{ 590,  36, 39, 16, 0, 0, "51-58",           NULL,    NULL,                 pbSetInstrBank11 },
	{ 590,  53, 39, 16, 0, 0, "59-60",           NULL,    NULL,                 pbSetInstrBank12 },
	{ 590,  73, 39, 16, 0, 0, "61-68",           NULL,    NULL,                 pbSetInstrBank13,},
	{ 590,  90, 39, 16, 0, 0, "69-70",           NULL,    NULL,                 pbSetInstrBank14 },
	{ 590, 107, 39, 16, 0, 0, "71-78",           NULL,    NULL,                 pbSetInstrBank15 },
	{ 590, 124, 39, 16, 0, 0, "79-80",           NULL,    NULL,                 pbSetInstrBank16 },
	{ 590, 144, 39, 27, 0, 0, "Swap",            "Bank",  NULL,                 pbSwapInstrBank },
	{ 566,  99, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    sampleListScrollUp,   NULL },
	{ 566, 140, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    sampleListScrollDown, NULL },

	// ------ NIBBLES SCREEN PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1, text #2, funcOnDown, funcOnUp
	{ 568, 104, 61, 16, 0, 0, "Play",  NULL,    NULL,       nibblesPlay },
	{ 568, 121, 61, 16, 0, 0, "Help",  NULL,    NULL,       nibblesHelp  },
	{ 568, 138, 61, 16, 0, 0, "Highs", NULL,    NULL,       nibblesHighScore },
	{ 568, 155, 61, 16, 0, 0, "Exit",  NULL,    NULL,       nibblesExit },

	// ------ ADVANCED EDIT PUSHBUTTONS ------
	//x,  y,   w,  h,  p, d, text #1,   text #2, funcOnDown, funcOnUp
	{  3, 138, 51, 16, 0, 0, "Track",   NULL,    NULL,       remapTrack },
	{ 55, 138, 52, 16, 0, 0, "Pattern", NULL,    NULL,       remapPattern },
	{  3, 155, 51, 16, 0, 0, "Song",    NULL,    NULL,       remapSong },
	{ 55, 155, 52, 16, 0, 0, "Block",   NULL,    NULL,       remapBlock },
	  
	// ------ ABOUT SCREEN PUSHBUTTONS ------
	//x, y,   w,  h,  p, d, text #1, text #2, funcOnDown, funcOnUp
	{ 4, 153, 59, 16, 0, 0, "Exit",  NULL,    NULL,       exitAboutScreen },

	// ------ HELP SCREEN PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown,     funcOnUp
	{   3, 155, 59, 16, 0, 0, "Exit",            NULL,    NULL,           exitHelpScreen },
	{ 611,   2, 18, 13, 1, 3, ARROW_UP_STRING,   NULL,    helpScrollUp,   NULL },
	{ 611, 158, 18, 13, 1, 3, ARROW_DOWN_STRING, NULL,    helpScrollDown, NULL },

	// ------ PATTERN EDITOR PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,         funcOnUp
	{   3, 385, 25, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    scrollChannelLeft,  NULL },
	{ 604, 385, 25, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    scrollChannelRight, NULL },

	// ------ TRANSPOSE PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,text #2, funcOnDown, funcOnUp
	{  56, 110, 21, 16, 0, 0, "up",   NULL,    NULL,       trackTranspCurInsUp },
	{  76, 110, 21, 16, 0, 0, "dn",   NULL,    NULL,       trackTranspCurInsDn },
	{  98, 110, 36, 16, 0, 0, "12up", NULL,    NULL,       trackTranspCurIns12Up },
	{ 133, 110, 36, 16, 0, 0, "12dn", NULL,    NULL,       trackTranspCurIns12Dn },
	{ 175, 110, 21, 16, 0, 0, "up",   NULL,    NULL,       trackTranspAllInsUp },
	{ 195, 110, 21, 16, 0, 0, "dn",   NULL,    NULL,       trackTranspAllInsDn },
	{ 217, 110, 36, 16, 0, 0, "12up", NULL,    NULL,       trackTranspAllIns12Up },
	{ 252, 110, 36, 16, 0, 0, "12dn", NULL,    NULL,       trackTranspAllIns12Dn },
	{  56, 125, 21, 16, 0, 0, "up",   NULL,    NULL,       pattTranspCurInsUp },
	{  76, 125, 21, 16, 0, 0, "dn",   NULL,    NULL,       pattTranspCurInsDn },
	{  98, 125, 36, 16, 0, 0, "12up", NULL,    NULL,       pattTranspCurIns12Up },
	{ 133, 125, 36, 16, 0, 0, "12dn", NULL,    NULL,       pattTranspCurIns12Dn },
	{ 175, 125, 21, 16, 0, 0, "up",   NULL,    NULL,       pattTranspAllInsUp },
	{ 195, 125, 21, 16, 0, 0, "dn",   NULL,    NULL,       pattTranspAllInsDn },
	{ 217, 125, 36, 16, 0, 0, "12up", NULL,    NULL,       pattTranspAllIns12Up },
	{ 252, 125, 36, 16, 0, 0, "12dn", NULL,    NULL,       pattTranspAllIns12Dn },
	{  56, 140, 21, 16, 0, 0, "up",   NULL,    NULL,       songTranspCurInsUp },
	{  76, 140, 21, 16, 0, 0, "dn",   NULL,    NULL,       songTranspCurInsDn },
	{  98, 140, 36, 16, 0, 0, "12up", NULL,    NULL,       songTranspCurIns12Up },
	{ 133, 140, 36, 16, 0, 0, "12dn", NULL,    NULL,       songTranspCurIns12Dn },
	{ 175, 140, 21, 16, 0, 0, "up",   NULL,    NULL,       songTranspAllInsUp },
	{ 195, 140, 21, 16, 0, 0, "dn",   NULL,    NULL,       songTranspAllInsDn },
	{ 217, 140, 36, 16, 0, 0, "12up", NULL,    NULL,       songTranspAllIns12Up },
	{ 252, 140, 36, 16, 0, 0, "12dn", NULL,    NULL,       songTranspAllIns12Dn },
	{  56, 155, 21, 16, 0, 0, "up",   NULL,    NULL,       blockTranspCurInsUp },
	{  76, 155, 21, 16, 0, 0, "dn",   NULL,    NULL,       blockTranspCurInsDn },
	{  98, 155, 36, 16, 0, 0, "12up", NULL,    NULL,       blockTranspCurIns12Up },
	{ 133, 155, 36, 16, 0, 0, "12dn", NULL,    NULL,       blockTranspCurIns12Dn },
	{ 175, 155, 21, 16, 0, 0, "up",   NULL,    NULL,       blockTranspAllInsUp },
	{ 195, 155, 21, 16, 0, 0, "dn",   NULL,    NULL,       blockTranspAllInsDn },
	{ 217, 155, 36, 16, 0, 0, "12up", NULL,    NULL,       blockTranspAllIns12Up },
	{ 252, 155, 36, 16, 0, 0, "12dn", NULL,    NULL,       blockTranspAllIns12Dn },

	// ------ SAMPLE EDITOR PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,            funcOnUp
	{   3, 331, 23, 13, 1, 3, ARROW_LEFT_STRING,  NULL,    scrollSampleDataLeft,  NULL },
	{ 606, 331, 23, 13, 1, 3, ARROW_RIGHT_STRING, NULL,    scrollSampleDataRight, NULL },
	{  38, 356, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    sampPlayNoteUp,        NULL },
	{  38, 368, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    sampPlayNoteDown,      NULL },
	{   3, 382, 53, 16, 0, 0, "Stop",             NULL,    NULL,                  smpEdStop},
	{  57, 348, 55, 16, 0, 0, "Wave",             NULL,    NULL,                  sampPlayWave },
	{  57, 365, 55, 16, 0, 0, "Range",            NULL,    NULL,                  sampPlayRange },
	{  57, 382, 55, 16, 0, 0, "Display",          NULL,    NULL,                  sampPlayDisplay },
	{ 118, 348, 63, 16, 0, 0, "Show r.",          NULL,    NULL,                  showRange },
	{ 118, 365, 63, 16, 0, 0, "Range all",        NULL,    NULL,                  rangeAll },
	{ 118, 382, 63, 16, 0, 0, "Sample",           NULL,    NULL,                  startSampling },
	{ 182, 348, 63, 16, 0, 0, "Zoom out",         NULL,    NULL,                  zoomOut },
	{ 182, 365, 63, 16, 0, 0, "Show all",         NULL,    NULL,                  showAll },
	{ 182, 382, 63, 16, 0, 0, "Save rng.",        NULL,    NULL,                  saveRange },
	{ 251, 348, 43, 16, 0, 0, "Cut",              NULL,    NULL,                  sampCut },
	{ 251, 365, 43, 16, 0, 0, "Copy",             NULL,    NULL,                  sampCopy },
	{ 251, 382, 43, 16, 0, 0, "Paste",            NULL,    NULL,                  sampPaste },
	{ 300, 348, 50, 16, 0, 0, "Crop",             NULL,    NULL,                  sampCrop },
	{ 300, 365, 50, 16, 0, 0, "Volume",           NULL,    NULL,                  pbSampleVolume },
	{ 300, 382, 50, 16, 0, 0, "Effects",          NULL,    NULL,                  pbEffects },
	{ 430, 348, 54, 16, 0, 0, "Exit",             NULL,    NULL,                  exitSampleEditor },
	{ 594, 348, 35, 13, 0, 0, "Clr S.",           NULL,    NULL,                  clearSample },
	{ 594, 360, 35, 13, 0, 0, "Min.",             NULL,    NULL,                  sampMinimize },
	{ 594, 373, 18, 13, 2, 4, ARROW_UP_STRING,    NULL,    sampRepeatUp,          NULL },
	{ 611, 373, 18, 13, 2, 4, ARROW_DOWN_STRING,  NULL,    sampRepeatDown,        NULL },
	{ 594, 385, 18, 13, 2, 4, ARROW_UP_STRING,    NULL,    sampReplenUp,          NULL },
	{ 611, 385, 18, 13, 2, 4, ARROW_DOWN_STRING,  NULL,    sampReplenDown,        NULL },

	// ------ SAMPLE EDITOR EFFECTS PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,              text #2, funcOnDown,      funcOnUp
	{  78, 350, 18, 13, 2, 2, ARROW_UP_STRING,      NULL,    pbSfxCyclesUp,   NULL },
	{  95, 350, 18, 13, 2, 2, ARROW_DOWN_STRING,    NULL,    pbSfxCyclesDown, NULL },
	{   3, 365, 54, 16, 0, 0, "Triangle",           NULL,    NULL,            pbSfxTriangle },
	{  59, 365, 54, 16, 0, 0, "Saw",                NULL,    NULL,            pbSfxSaw },
	{   3, 382, 54, 16, 0, 0, "Sine",               NULL,    NULL,            pbSfxSine },
	{  59, 382, 54, 16, 0, 0, "Square",             NULL,    NULL,            pbSfxSquare },
	{ 192, 350, 18, 13, 1, 2, ARROW_UP_STRING,      NULL,    pbSfxResoUp,     NULL },
	{ 209, 350, 18, 13, 1, 2, ARROW_DOWN_STRING,    NULL,    pbSfxResoDown,   NULL },
	{ 119, 365, 53, 16, 0, 0, "lp filter",          NULL,    NULL,            pbSfxLowPass },
	{ 174, 365, 53, 16, 0, 0, "hp filter",          NULL,    NULL,            pbSfxHighPass },
	{ 269, 350, 13, 13, 0, 0, "-",                  NULL,    NULL,            pbSfxSubBass },
	{ 281, 350, 13, 13, 0, 0, "+",                  NULL,    NULL,            pbSfxAddBass },
	{ 269, 367, 13, 13, 0, 0, "-",                  NULL,    NULL,            pbSfxSubTreble },
	{ 281, 367, 13, 13, 0, 0, "+",                  NULL,    NULL,            pbSfxAddTreble },
	{ 233, 382, 61, 16, 0, 0, "Amplitude",          NULL,    NULL,            pbSfxSetAmp },
	{ 300, 348, 50, 16, 0, 0, "Undo",               NULL,    NULL,            pbSfxUndo },
	{ 300, 365, 50, 16, 0, 0, "X-Fade",             NULL,    NULL,            sampXFade },
	{ 300, 382, 50, 16, 0, 0, "Back...",            NULL,    NULL,            hideSampleEffectsScreen },

	// ------ SAMPLE EDITOR EXTENSION PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,     text #2, funcOnDown, funcOnUp
	{   3, 138, 52, 16, 0, 0, "Clr. c.bf", NULL,    NULL,       clearCopyBuffer },
	{  56, 138, 49, 16, 0, 0, "Sign",      NULL,    NULL,       sampleChangeSign },
	{ 106, 138, 49, 16, 0, 0, "Echo",      NULL,    NULL,       pbSampleEcho },
	{   3, 155, 52, 16, 0, 0, "Backw.",    NULL,    NULL,       sampleBackwards },
	{  56, 155, 49, 16, 0, 0, "B. swap",   NULL,    NULL,       sampleByteSwap },
	{ 106, 155, 49, 16, 0, 0, "Fix DC",    NULL,    NULL,       fixDC },
	{ 161, 121, 60, 16, 0, 0, "Copy ins.", NULL,    NULL,       copyInstr },
	{ 222, 121, 66, 16, 0, 0, "Copy smp.", NULL,    NULL,       copySmp },
	{ 161, 138, 60, 16, 0, 0, "Xchg ins.", NULL,    NULL,       xchgInstr },
	{ 222, 138, 66, 16, 0, 0, "Xchg smp.", NULL,    NULL,       xchgSmp },
	{ 161, 155, 60, 16, 0, 0, "Resample",  NULL,    NULL,       pbSampleResample },
	{ 222, 155, 66, 16, 0, 0, "Mix smp.",  NULL,    NULL,       pbSampleMix },

	// ------ INSTRUMENT EDITOR PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,     funcOnUp
	{ 200, 175, 23, 12, 0, 0, SMALL_1_STRING,     NULL,    NULL,           volPreDef1 },
	{ 222, 175, 24, 12, 0, 0, SMALL_2_STRING,     NULL,    NULL,           volPreDef2 },
	{ 245, 175, 24, 12, 0, 0, SMALL_3_STRING,     NULL,    NULL,           volPreDef3 },
	{ 268, 175, 24, 12, 0, 0, SMALL_4_STRING,     NULL,    NULL,           volPreDef4 },
	{ 291, 175, 24, 12, 0, 0, SMALL_5_STRING,     NULL,    NULL,           volPreDef5 },
	{ 314, 175, 24, 12, 0, 0, SMALL_6_STRING,     NULL,    NULL,           volPreDef6 },
	{ 200, 262, 23, 12, 0, 0, SMALL_1_STRING,     NULL,    NULL,           panPreDef1 },
	{ 222, 262, 24, 12, 0, 0, SMALL_2_STRING,     NULL,    NULL,           panPreDef2 },
	{ 245, 262, 24, 12, 0, 0, SMALL_3_STRING,     NULL,    NULL,           panPreDef3 },
	{ 268, 262, 24, 12, 0, 0, SMALL_4_STRING,     NULL,    NULL,           panPreDef4 },
	{ 291, 262, 24, 12, 0, 0, SMALL_5_STRING,     NULL,    NULL,           panPreDef5 },
	{ 314, 262, 24, 12, 0, 0, SMALL_6_STRING,     NULL,    NULL,           panPreDef6 },
	{ 570, 276, 59, 16, 0, 0, "Exit",             NULL,    NULL,           exitInstEditor },
	{ 341, 175, 47, 16, 1, 4, "Add",              NULL,    volEnvAdd,      NULL },
	{ 389, 175, 46, 16, 1, 4, "Del",              NULL,    volEnvDel,      NULL },
	{ 398, 204, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    volEnvSusUp,    NULL },
	{ 416, 204, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    volEnvSusDown,  NULL },
	{ 398, 231, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    volEnvRepSUp,   NULL },
	{ 416, 231, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    volEnvRepSDown, NULL },
	{ 398, 245, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    volEnvRepEUp,   NULL },
	{ 416, 245, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    volEnvRepEDown, NULL },
	{ 341, 262, 47, 16, 1, 4, "Add",              NULL,    panEnvAdd,      NULL },
	{ 389, 262, 46, 16, 1, 4, "Del",              NULL,    panEnvDel,      NULL },
	{ 398, 291, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    panEnvSusUp,    NULL },
	{ 416, 291, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    panEnvSusDown,  NULL },
	{ 398, 318, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    panEnvRepSUp,   NULL },
	{ 416, 318, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    panEnvRepSDown, NULL },
	{ 398, 332, 19, 13, 1, 4, ARROW_UP_STRING,    NULL,    panEnvRepEUp,   NULL },
	{ 416, 332, 19, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    panEnvRepEDown, NULL },
	{ 521, 175, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    volDown,        NULL },
	{ 606, 175, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    volUp,          NULL },
	{ 521, 189, 23, 13, 2, 4, ARROW_LEFT_STRING,  NULL,    panDown,        NULL },
	{ 606, 189, 23, 13, 2, 4, ARROW_RIGHT_STRING, NULL,    panUp,          NULL },
	{ 521, 203, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    ftuneDown,      NULL },
	{ 606, 203, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    ftuneUp,        NULL },
	{ 521, 220, 23, 13, 2, 4, ARROW_LEFT_STRING,  NULL,    fadeoutDown,    NULL },
	{ 606, 220, 23, 13, 2, 4, ARROW_RIGHT_STRING, NULL,    fadeoutUp,      NULL },
	{ 521, 234, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    vibSpeedDown,   NULL },
	{ 606, 234, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    vibSpeedUp,     NULL },
	{ 521, 248, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    vibDepthDown,   NULL },
	{ 606, 248, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    vibDepthUp,     NULL },
	{ 521, 262, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    vibSweepDown,   NULL },
	{ 606, 262, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    vibSweepUp,     NULL },
	{ 441, 312, 94, 16, 1, 4, "Octave up",        NULL,    relativeNoteOctUp,   NULL },
	{ 536, 312, 93, 16, 1, 4, "Halftone up",      NULL,    relativeNoteUp,      NULL },
	{ 441, 329, 94, 16, 1, 4, "Octave down",      NULL,    relativeNoteOctDown, NULL },
	{ 536, 329, 93, 16, 1, 4, "Halftone down",    NULL,    relativeNoteDown,    NULL },

	// ------ INSTRUMENT EDITOR EXTENSION PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,   funcOnUp
	{ 172, 130, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    midiChDown,   NULL },
	{ 265, 130, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    midiChUp,     NULL },
	{ 172, 144, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    midiPrgDown,  NULL },
	{ 265, 144, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    midiPrgUp,    NULL },
	{ 172, 158, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    midiBendDown, NULL },
	{ 265, 158, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    midiBendUp,   NULL },

	// ------ TRIM SCREEN PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,     text #2, funcOnDown, funcOnUp
	{ 139, 155, 74, 16, 0, 0, "Calculate", NULL,    NULL,       pbTrimCalc },
	{ 214, 155, 74, 16, 0, 0, "Trim",      NULL,    NULL,       pbTrimDoTrim },

	// ------ CONFIG LEFT PANEL PUSHBUTTONS ------
	//x, y,   w,   h,  p, d, text #1,         text #2, funcOnDown, funcOnUp
	{ 3, 104, 104, 16, 0, 0, "Reset config",  NULL,    NULL,       resetConfig },
	{ 3, 121, 104, 16, 0, 0, "Load config",   NULL,    NULL,       loadConfig2 },
	{ 3, 138, 104, 16, 0, 0, "Save config",   NULL,    NULL,       saveConfig2 },
	{ 3, 155, 104, 16, 0, 0, "Exit",          NULL,    NULL,       exitConfigScreen },

	// ------ CONFIG AUDIO PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,                 funcOnUp
	{ 326,   2, 57, 13, 0, 0, "Re-scan",          NULL,    NULL,                       rescanAudioDevices },
	{ 365,  16, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    scrollAudOutputDevListUp,   NULL },
	{ 365,  72, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    scrollAudOutputDevListDown, NULL },
	{ 365, 103, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    scrollAudInputDevListUp,    NULL },
	{ 365, 137, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    scrollAudInputDevListDown,  NULL },
	{ 512, 117, 21, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configAmpDown,              NULL },
	{ 608, 117, 21, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configAmpUp,                NULL },
	{ 512, 143, 21, 13, 1, 0, ARROW_LEFT_STRING,  NULL,    configMasterVolDown,        NULL },
	{ 608, 143, 21, 13, 1, 0, ARROW_RIGHT_STRING, NULL,    configMasterVolUp,          NULL },

	// ------ CONFIG LAYOUT PUSHBUTTONS ------
	//x,   y,  w,  h,  p, d, text #1,            text #2, funcOnDown,        funcOnUp
	{ 513, 15, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configPalRDown,    NULL },
	{ 606, 15, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configPalRUp,      NULL },
	{ 513, 29, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configPalGDown,    NULL },
	{ 606, 29, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configPalGUp,      NULL },
	{ 513, 43, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configPalBDown,    NULL },
	{ 606, 43, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configPalBUp,      NULL },
	{ 513, 71, 23, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configPalContDown, NULL },
	{ 606, 71, 23, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configPalContUp,   NULL },

	// ------ CONFIG MISCELLANEOUS PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,            text #2, funcOnDown,          funcOnUp
	{ 370, 120, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    configQuantizeUp,    NULL },
	{ 387, 120, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    configQuantizeDown,  NULL },
	{ 594, 106, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    configMIDIChnUp,     NULL },
	{ 611, 106, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    configMIDIChnDown,   NULL },
	{ 594, 120, 18, 13, 1, 4, ARROW_UP_STRING,    NULL,    configMIDITransUp,   NULL },
	{ 611, 120, 18, 13, 1, 4, ARROW_DOWN_STRING,  NULL,    configMIDITransDown, NULL },
	{ 556, 158, 22, 13, 1, 4, ARROW_LEFT_STRING,  NULL,    configMIDISensDown,  NULL },
	{ 607, 158, 22, 13, 1, 4, ARROW_RIGHT_STRING, NULL,    configMIDISensUp,    NULL },

#ifdef HAS_MIDI
	// ------ CONFIG MIDI PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown,                 funcOnUp
	{ 483,   2, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    scrollMidiInputDevListUp,   NULL },
	{ 483, 158, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    scrollMidiInputDevListDown, NULL },
#endif

	// ------ DISK OP. PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,              text #2, funcOnDown,      funcOnUp
	{  70,   2, 58, 16, 0, 0, "Save",               NULL,    NULL,            pbDiskOpSave },
	{  70,  19, 58, 16, 0, 0, "Delete",             NULL,    NULL,            pbDiskOpDelete },
	{  70,  36, 58, 16, 0, 0, "Rename",             NULL,    NULL,            pbDiskOpRename },
	{  70,  53, 58, 16, 0, 0, "Make dir.",          NULL,    NULL,            pbDiskOpMakeDir },
	{  70,  70, 58, 16, 0, 0, "Refresh",            NULL,    NULL,            pbDiskOpRefresh },
	{  70,  87, 58, 16, 0, 0, "Set path",           NULL,    NULL,            pbDiskOpSetPath },
	{  70, 104, 58, 16, 0, 0, "Show all",           NULL,    NULL,            pbDiskOpShowAll },
	{  70, 121, 58, 19, 0, 0, "Exit",               NULL,    NULL,            pbDiskOpExit },
#ifdef _WIN32 // partition letters
	{ 134,   2, 31, 13, 0, 0, DISKOP_PARENT_STRING, NULL,    NULL,            pbDiskOpParent },
	{ 134,  16, 31, 12, 0, 0, "\\",                 NULL,    NULL,            pbDiskOpRoot },
	{ 134,  29, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive1 },
	{ 134,  43, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive2 },
	{ 134,  57, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive3 },
	{ 134,  71, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive4 },
	{ 134,  85, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive5 },
	{ 134,  99, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive6 },
	{ 134, 113, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive7 },
	{ 134, 127, 31, 13, 0, 0, NULL,                 NULL,    NULL,            pbDiskOpDrive8 },
#else
	{ 134,   2, 31, 13, 0, 0, "../",               NULL,    NULL,             pbDiskOpParent },
	{ 134,  16, 31, 12, 0, 0, "/",                 NULL,    NULL,             pbDiskOpRoot },
#endif
	{ 335,   2, 18, 13, 1, 3, ARROW_UP_STRING,     NULL,    pbDiskOpListUp,   NULL },
	{ 335, 158, 18, 13, 1, 3, ARROW_DOWN_STRING,   NULL,    pbDiskOpListDown, NULL },

	// ------ WAV RENDERER PUSHBUTTONS ------
	//x,   y,   w,  h,  p, d, text #1,           text #2, funcOnDown,         funcOnUp
	{   3, 138, 73, 16, 0, 0, "Export",          NULL,    NULL,               pbWavRender },
	{   3, 155, 73, 16, 0, 0, "Exit",            NULL,    NULL,               pbWavExit },
	{ 253, 114, 18, 13, 1, 6, ARROW_UP_STRING,   NULL,    pbWavFreqUp,        NULL },
	{ 270, 114, 18, 13, 1, 6, ARROW_DOWN_STRING, NULL,    pbWavFreqDown,      NULL },
	{ 253, 128, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbWavAmpUp,         NULL },
	{ 270, 128, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbWavAmpDown,       NULL },
	{ 253, 142, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbWavSongStartUp,   NULL },
	{ 270, 142, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbWavSongStartDown, NULL },
	{ 253, 156, 18, 13, 1, 4, ARROW_UP_STRING,   NULL,    pbWavSongEndUp,     NULL },
	{ 270, 156, 18, 13, 1, 4, ARROW_DOWN_STRING, NULL,    pbWavSongEndDown,   NULL }
};

static uint32_t tmpCounter;

void drawPushButton(uint16_t pushButtonID)
{
	uint16_t textX, textY, textW;

	assert(pushButtonID < NUM_PUSHBUTTONS);

	pushButton_t *b = &pushButtons[pushButtonID];
	if (!b->visible)
		return;

	uint8_t state = b->state;

	uint16_t x = b->x;
	uint16_t y = b->y;
	uint16_t w = b->w;
	uint16_t h = b->h;

	assert(x < SCREEN_W && y < SCREEN_H && w >= 4 && h >= 4);

	if (b->bitmapFlag)
	{
		blitFast(x, y, (state == PUSHBUTTON_UNPRESSED) ? b->bitmapUnpressed : b->bitmapPressed, w, h);
		return;
	}

	// fill button background
	fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);

	// draw outer border
	hLine(x,         y,         w, PAL_BCKGRND);
	hLine(x,         y + h - 1, w, PAL_BCKGRND);
	vLine(x,         y,         h, PAL_BCKGRND);
	vLine(x + w - 1, y,         h, PAL_BCKGRND);

	//draw inner borders
	if (state == PUSHBUTTON_UNPRESSED)
	{
		// top left corner inner border
		hLine(x + 1, y + 1, w - 3, PAL_BUTTON1);
		vLine(x + 1, y + 2, h - 4, PAL_BUTTON1);

		// bottom right corner inner border
		hLine(x + 1 - 0, y + h - 2, w - 2, PAL_BUTTON2);
		vLine(x + w - 2, y + 1 - 0, h - 3, PAL_BUTTON2);
	}
	else
	{
		// top left corner inner border
		hLine(x + 1, y + 1, w - 2, PAL_BUTTON2);
		vLine(x + 1, y + 2, h - 3, PAL_BUTTON2);
	}

	// render button text(s)
	if (b->caption != NULL && *b->caption != '\0')
	{
		// custom button graphics
		if ((uint8_t)b->caption[0] < 32 && b->caption[1] == '\0')
		{
			uint8_t *src8 = &bmp.buttonGfx[(b->caption[0]-1) * 8];
			const char ch = b->caption[0];

			textW = 8;
			if (ch == ARROW_UP_GFX_CHAR || ch == ARROW_DOWN_GFX_CHAR)
				textW = 6;
			else if (ch == ARROW_LEFT_GFX_CHAR || ch == ARROW_RIGHT_GFX_CHAR)
				textW = 7;
			else if (ch >= SMALL_1_GFX_CHAR && ch <= SMALL_6_GFX_CHAR)
				textW = 5;
			else if (ch == DISKOP_PARENT_GFX_CHAR)
				textW = 10;

			textX = x + ((w - textW) / 2);
			textY = y + ((h - 8) / 2);

			if (state == PUSHBUTTON_PRESSED)
			{
				textX++;
				textY++;
			}

			// blit graphics

			uint32_t *dst32 = &video.frameBuffer[(textY * SCREEN_W) + textX];
			for (y = 0; y < 8; y++, src8 += BUTTON_GFX_BMP_WIDTH, dst32 += SCREEN_W)
			{
				for (x = 0; x < textW; x++)
				{
					if (src8[x] != 0)
						dst32[x] = video.palette[PAL_BTNTEXT];
				}
			}
		}
		else // normal text
		{
			// button text #2 (if present)
			if (b->caption2 != NULL && *b->caption2 != '\0')
			{
				textW = textWidth(b->caption2);
				textX = x + ((w - textW) / 2);
				textY = y + 6 + ((h - (FONT1_CHAR_H - 2)) / 2);

				if (state == PUSHBUTTON_PRESSED)
					textOut(textX + 1, textY + 1, PAL_BTNTEXT, b->caption2);
				else
					textOut(textX, textY, PAL_BTNTEXT, b->caption2);

				y -= 5; // if two text lines, bias y position of first (upper) text
			}

			// button text #1
			textW = textWidth(b->caption);
			textX = x + ((w - textW) / 2);
			textY = y + ((h - (FONT1_CHAR_H - 2)) / 2);

			if (state == PUSHBUTTON_PRESSED)
				textOut(textX + 1, textY + 1, PAL_BTNTEXT, b->caption);
			else
				textOut(textX, textY, PAL_BTNTEXT, b->caption);
		}
	}
}

void showPushButton(uint16_t pushButtonID)
{
	assert(pushButtonID < NUM_PUSHBUTTONS);
	pushButtons[pushButtonID].visible = true;
	drawPushButton(pushButtonID);
}

void hidePushButton(uint16_t pushButtonID)
{
	assert(pushButtonID < NUM_PUSHBUTTONS);
	pushButtons[pushButtonID].state = 0;
	pushButtons[pushButtonID].visible = false;
}

void handlePushButtonsWhileMouseDown(void)
{
	int8_t buttonDelay;

	assert(mouse.lastUsedObjectID >= 0 && mouse.lastUsedObjectID < NUM_PUSHBUTTONS);
	pushButton_t *pushButton = &pushButtons[mouse.lastUsedObjectID];
	if (!pushButton->visible)
		return;

	pushButton->state = PUSHBUTTON_UNPRESSED;
	if (mouse.x >= pushButton->x && mouse.x < pushButton->x+pushButton->w &&
        mouse.y >= pushButton->y && mouse.y < pushButton->y+pushButton->h)
	{
		pushButton->state = PUSHBUTTON_PRESSED;
	}

	if (mouse.lastX != mouse.x || mouse.lastY != mouse.y)
	{
		mouse.lastX = mouse.x;
		mouse.lastY = mouse.y;

		drawPushButton(mouse.lastUsedObjectID);
	}

	// long delay before repeat
	if (pushButton->preDelay && mouse.firstTimePressingButton)
	{
		tmpCounter = 0;

		if (++mouse.buttonCounter >= BUTTON_DOWN_DELAY)
		{
			mouse.buttonCounter = 0;
			mouse.firstTimePressingButton = false;
		}
		else
		{
			return; // we're delaying
		}
	}

	if (pushButton->state == PUSHBUTTON_PRESSED)
	{
		// button delay stuff
		if (mouse.rightButtonPressed)
			buttonDelay = pushButton->delayFrames >> 1;
		else if (pushButton->preDelay == 2 && (!mouse.firstTimePressingButton && ++tmpCounter >= 50)) // special mode
			buttonDelay = 0;
		else
			buttonDelay = pushButton->delayFrames;

		// main repeat delay
		if (++mouse.buttonCounter >= buttonDelay)
		{
			mouse.buttonCounter = 0;
			if (pushButton->callbackFuncOnDown != NULL)
				pushButton->callbackFuncOnDown();
		}
	}
}

bool testPushButtonMouseDown(void)
{
	uint16_t start, end;

	if (ui.sysReqShown)
	{
		// if a system request is open, only test the first eight pushbuttons (reserved)
		start = 0;
		end = 8;
	}
	else
	{
		start = 8;
		end = NUM_PUSHBUTTONS;
	}

	pushButton_t *pushButton = &pushButtons[start];
	for (uint16_t i = start; i < end; i++, pushButton++)
	{
		if (!pushButton->visible)
			continue;

		if (mouse.x >= pushButton->x && mouse.x < pushButton->x+pushButton->w &&
		    mouse.y >= pushButton->y && mouse.y < pushButton->y+pushButton->h)
		{
			mouse.lastUsedObjectID = i;
			mouse.lastUsedObjectType = OBJECT_PUSHBUTTON;

			if (!mouse.rightButtonPressed)
			{
				mouse.firstTimePressingButton = true;
				mouse.buttonCounter = 0;

				pushButton->state = PUSHBUTTON_PRESSED;
				drawPushButton(i);

				if (pushButton->callbackFuncOnDown != NULL)
					pushButton->callbackFuncOnDown();
			}

			return true;
		}
	}

	return false;
}

int16_t testPushButtonMouseRelease(bool runCallback)
{
	if (mouse.lastUsedObjectType != OBJECT_PUSHBUTTON || mouse.lastUsedObjectID == OBJECT_ID_NONE)
		return -1;

	assert(mouse.lastUsedObjectID < NUM_PUSHBUTTONS);
	pushButton_t *pushButton = &pushButtons[mouse.lastUsedObjectID];
	if (!pushButton->visible)
		return -1;

	if (mouse.x >= pushButton->x && mouse.x < pushButton->x+pushButton->w &&
		mouse.y >= pushButton->y && mouse.y < pushButton->y+pushButton->h)
	{
		pushButton->state = PUSHBUTTON_UNPRESSED;
		drawPushButton(mouse.lastUsedObjectID);

		if (runCallback)
		{
			if (pushButton->callbackFuncOnUp != NULL)
				pushButton->callbackFuncOnUp();
		}

		return mouse.lastUsedObjectID;
	}

	return -1;
}
