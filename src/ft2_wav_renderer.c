// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_diskop.h"
#include "ft2_scopes.h"
#include "ft2_config.h"
#include "ft2_mouse.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_audio.h"
#include "ft2_wav_renderer.h"
#include "ft2_structs.h"

#define TICKS_PER_RENDER_CHUNK 64

enum
{
	WAV_FORMAT_PCM = 0x0001,
	WAV_FORMAT_IEEE_FLOAT = 0x0003
};

typedef struct wavHeader_t
{
	uint32_t chunkID, chunkSize, format, subchunk1ID, subchunk1Size;
	uint16_t audioFormat, numChannels;
	uint32_t sampleRate, byteRate;
	uint16_t blockAlign, bitsPerSample;
	uint32_t subchunk2ID, subchunk2Size;
} wavHeader_t;

static char WAV_SysReqText[192];
static uint8_t WDBitDepth = 16, WDStartPos, WDStopPos, *wavRenderBuffer;
static int16_t WDAmp;
static uint32_t WDFrequency = 48000;
static SDL_Thread *thread;

static void updateWavRenderer(void)
{
	char str[10];

	fillRect(209, 116, 41, 51, PAL_DESKTOP);

	sprintf(str, "%6d", WDFrequency);
	textOutFixed(209, 116, PAL_FORGRND, PAL_DESKTOP, str);

	sprintf(str, "%02d", WDAmp);
	textOut(237, 130, PAL_FORGRND, str);

	hexOut(237, 144, PAL_FORGRND, WDStartPos, 2);
	hexOut(237, 158, PAL_FORGRND, WDStopPos,  2);
}

void setWavRenderFrequency(int32_t freq)
{
	WDFrequency = CLAMP(freq, MIN_WAV_RENDER_FREQ, MAX_WAV_RENDER_FREQ);
	if (ui.wavRendererShown)
		updateWavRenderer();
}

void setWavRenderBitDepth(uint8_t bitDepth)
{
	if (bitDepth == 16)
		WDBitDepth = 16;
	else if (bitDepth == 32)
		WDBitDepth = 32;

	if (ui.wavRendererShown)
		updateWavRenderer();
}

void updateWavRendererSettings(void) // called when changing config.boostLevel
{
	WDAmp = config.boostLevel;
}

void drawWavRenderer(void)
{
	drawFramework(0,   92, 291, 17, FRAMEWORK_TYPE1);
	drawFramework(0,  109,  79, 64, FRAMEWORK_TYPE1);
	drawFramework(79, 109, 212, 64, FRAMEWORK_TYPE1);

	textOutShadow(4,   96, PAL_FORGRND, PAL_DSKTOP2, "WAV exporting:");
	textOutShadow(156, 96, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
	textOutShadow(221, 96, PAL_FORGRND, PAL_DSKTOP2, "32-bit float");

	textOutShadow(85, 116, PAL_FORGRND, PAL_DSKTOP2, "Frequency");
	textOutShadow(85, 130, PAL_FORGRND, PAL_DSKTOP2, "Amplification");
	textOutShadow(85, 144, PAL_FORGRND, PAL_DSKTOP2, "Start song position");
	textOutShadow(85, 158, PAL_FORGRND, PAL_DSKTOP2, "Stop song position");

	showPushButton(PB_WAV_RENDER);
	showPushButton(PB_WAV_EXIT);
	showPushButton(PB_WAV_FREQ_UP);
	showPushButton(PB_WAV_FREQ_DOWN);
	showPushButton(PB_WAV_AMP_UP);
	showPushButton(PB_WAV_AMP_DOWN);
	showPushButton(PB_WAV_START_UP);
	showPushButton(PB_WAV_START_DOWN);
	showPushButton(PB_WAV_END_UP);
	showPushButton(PB_WAV_END_DOWN);

	// bitdepth radiobuttons

	radioButtons[RB_WAV_RENDER_BITDEPTH16].state = RADIOBUTTON_UNCHECKED;
	radioButtons[RB_WAV_RENDER_BITDEPTH32].state = RADIOBUTTON_UNCHECKED;

	if (WDBitDepth == 16)
		radioButtons[RB_WAV_RENDER_BITDEPTH16].state = RADIOBUTTON_CHECKED;
	else
		radioButtons[RB_WAV_RENDER_BITDEPTH32].state = RADIOBUTTON_CHECKED;

	showRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);

	updateWavRenderer();
}

void resetWavRenderer(void)
{
	WDStartPos = 0;
	WDStopPos = (uint8_t)song.len - 1;

	if (ui.wavRendererShown)
		updateWavRenderer();
}

void showWavRenderer(void)
{
	if (ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	ui.wavRendererShown = true;
	ui.scopesShown = false;

	WDStartPos = 0;
	WDStopPos = (uint8_t)song.len - 1;

	drawWavRenderer();
}

void hideWavRenderer(void)
{
	ui.wavRendererShown = false;

	hidePushButton(PB_WAV_RENDER);
	hidePushButton(PB_WAV_EXIT);
	hidePushButton(PB_WAV_FREQ_UP);
	hidePushButton(PB_WAV_FREQ_DOWN);
	hidePushButton(PB_WAV_AMP_UP);
	hidePushButton(PB_WAV_AMP_DOWN);
	hidePushButton(PB_WAV_START_UP);
	hidePushButton(PB_WAV_START_DOWN);
	hidePushButton(PB_WAV_END_UP);
	hidePushButton(PB_WAV_END_DOWN);
	hideRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);

	ui.scopesShown = true;
	drawScopeFramework();
}

void exitWavRenderer(void)
{
	hideWavRenderer();
}

static bool dump_Init(uint32_t frq, int16_t amp, int16_t songPos)
{
	const int32_t maxSamplesPerTick = (const int32_t)ceil((frq * 2.5) / MIN_BPM); // absolute max samples per tick
	uint32_t sampleSize = (WDBitDepth / 8) * 2; // 2 channels

	// *2 for stereo
	wavRenderBuffer = (uint8_t *)malloc((TICKS_PER_RENDER_CHUNK * maxSamplesPerTick) * sampleSize);
	if (wavRenderBuffer == NULL)
		return false;

	editor.wavIsRendering = true;

	setPos(songPos, 0, true);
	playMode = PLAYMODE_SONG;
	songPlaying = true;

	resetChannels();
	setNewAudioFreq(frq);
	setAudioAmp(amp, config.masterVol, (WDBitDepth == 32));

	stopVoices();
	song.globVol = 64;
	P_SetSpeed(song.speed);

	resetPlaybackTime();
	return true;
}

static void dump_Close(FILE *f, uint32_t totalSamples)
{
	wavHeader_t wavHeader;

	if (wavRenderBuffer != NULL)
	{
		free(wavRenderBuffer);
		wavRenderBuffer = NULL;
	}

	uint32_t totalBytes;
	if (WDBitDepth == 16)
		totalBytes = totalSamples * sizeof (int16_t);
	else
		totalBytes = totalSamples * sizeof (float);

	if (totalBytes & 1)
		fputc(0, f); // write pad byte

	uint32_t tmpLen = ftell(f)-8;

	// go back and fill in WAV header
	rewind(f);

	wavHeader.chunkID = 0x46464952; // "RIFF"
	wavHeader.chunkSize = tmpLen;
	wavHeader.format = 0x45564157; // "WAVE"
	wavHeader.subchunk1ID = 0x20746D66; // "fmt "
	wavHeader.subchunk1Size = 16;

	if (WDBitDepth == 16)
		wavHeader.audioFormat = WAV_FORMAT_PCM;
	else
		wavHeader.audioFormat = WAV_FORMAT_IEEE_FLOAT;

	wavHeader.numChannels = 2;
	wavHeader.sampleRate = WDFrequency;
	wavHeader.byteRate = (wavHeader.sampleRate * wavHeader.numChannels * WDBitDepth) / 8;
	wavHeader.blockAlign = (wavHeader.numChannels * WDBitDepth) / 8;
	wavHeader.bitsPerSample = WDBitDepth;
	wavHeader.subchunk2ID = 0x61746164; // "data"
	wavHeader.subchunk2Size = totalBytes;

	// write main header
	fwrite(&wavHeader, 1, sizeof (wavHeader_t), f);
	fclose(f);

	stopPlaying();

	// kludge: set speed to 6 if speed was set to 0
	if (song.tempo == 0)
		song.tempo = 6;

	setBackOldAudioFreq();
	P_SetSpeed(song.speed);
	setAudioAmp(config.boostLevel, config.masterVol, config.specialFlags & BITDEPTH_32);
	editor.wavIsRendering = false;

	setMouseBusy(false);
}

static bool dump_EndOfTune(int16_t endSongPos)
{
	bool returnValue = (editor.wavReachedEndFlag && song.pattPos == 0 && song.timer == 1) || (song.tempo == 0);

	// 8bitbubsy: FT2 bugfix for EEx (pattern delay) on first row of a pattern
	if (song.pattDelTime2 > 0)
		returnValue = false;

	if (song.songPos == endSongPos && song.pattPos == 0 && song.timer == 1)
		editor.wavReachedEndFlag = true;

	return returnValue;
}

void dump_TickReplayer(void)
{
	replayerBusy = true;

	if (audio.volumeRampingFlag)
		resetRampVolumes();

	tickReplayer();
	updateVoices();

	replayerBusy = false;
}

static void updateVisuals(void)
{
	editor.editPattern = (uint8_t)song.pattNr;
	editor.pattPos = song.pattPos;
	editor.songPos = song.songPos;
	editor.speed = song.speed;
	editor.tempo = song.tempo;
	editor.globalVol = song.globVol;

	ui.drawPosEdFlag = true;
	ui.drawPattNumLenFlag = true;
	ui.drawReplayerPianoFlag = true;
	ui.drawBPMFlag = true;
	ui.drawSpeedFlag = true;
	ui.drawGlobVolFlag = true;
	ui.updatePatternEditor = true;
}

static int32_t SDLCALL renderWavThread(void *ptr)
{
	FILE *f = (FILE *)editor.wavRendererFileHandle;
	fseek(f, sizeof (wavHeader_t), SEEK_SET);

	pauseAudio();

	if (!dump_Init(WDFrequency, WDAmp, WDStartPos))
	{
		resumeAudio();
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	uint32_t sampleCounter = 0;
	bool renderDone = false;
	uint8_t tickCounter = 4;
	double dTickSampleCounter = 0.0;

	editor.wavReachedEndFlag = false;
	while (!renderDone)
	{
		uint32_t samplesInChunk = 0;

		// render several ticks at once to prevent frequent disk I/O (speeds up the process)
		uint8_t *ptr8 = wavRenderBuffer;
		for (uint32_t i = 0; i < TICKS_PER_RENDER_CHUNK; i++)
		{
			if (!editor.wavIsRendering || dump_EndOfTune(WDStopPos))
			{
				renderDone = true;
				break;
			}

			if (dTickSampleCounter <= 0.0)
			{
				// new replayer tick
				dump_TickReplayer();
				dTickSampleCounter += audio.dSamplesPerTick;
			}

			int32_t remainingTick = (int32_t)ceil(dTickSampleCounter);

			mixReplayerTickToBuffer(remainingTick, ptr8, WDBitDepth);
			dTickSampleCounter -= remainingTick;

			remainingTick *= 2; // stereo
			samplesInChunk += remainingTick;
			sampleCounter += remainingTick;

			// increase buffer pointer
			if (WDBitDepth == 16)
				ptr8 += remainingTick * sizeof (int16_t);
			else
				ptr8 += remainingTick * sizeof (float);

			if (++tickCounter >= 4)
			{
				tickCounter = 0;
				updateVisuals();
			}
		}

		// write buffer to disk
		if (samplesInChunk > 0)
		{
			if (WDBitDepth == 16)
				fwrite(wavRenderBuffer, sizeof (int16_t), samplesInChunk, f);
			else
				fwrite(wavRenderBuffer, sizeof (float), samplesInChunk, f);
		}
	}

	updateVisuals();
	drawPlaybackTime(); // this is needed after the song stopped

	dump_Close(f, sampleCounter);
	resumeAudio();

	editor.diskOpReadOnOpen = true;
	return true;

	(void)ptr;
}

static void createOverwriteText(char *name)
{
	char nameTmp[128];

	// read entry name to a small buffer
	uint32_t nameLen = (uint32_t)strlen(name);
	memcpy(nameTmp, name, (nameLen >= sizeof (nameTmp)) ? sizeof (nameTmp) : (nameLen + 1));
	nameTmp[sizeof (nameTmp) - 1] = '\0';

	trimEntryName(nameTmp, false);

	sprintf(WAV_SysReqText, "Overwrite file \"%s\"?", nameTmp);
}

static void wavRender(bool checkOverwrite)
{
	WDStartPos = (uint8_t)(MAX(0, MIN(WDStartPos, song.len - 1)));
	WDStopPos  = (uint8_t)(MAX(0, MIN(MAX(WDStartPos, WDStopPos), song.len - 1)));

	updateWavRenderer();

	diskOpChangeFilenameExt(".wav");

	char *filename = getDiskOpFilename();
	if (checkOverwrite && fileExistsAnsi(filename))
	{
		createOverwriteText(filename);
		if (okBox(2, "System request", WAV_SysReqText) != 1)
			return;
	}

	editor.wavRendererFileHandle = fopen(filename, "wb");
	if (editor.wavRendererFileHandle == NULL)
	{
		okBox(0, "System message", "General I/O error while writing to WAV (is the file in use)?");
		return;
	}

	mouseAnimOn();
	thread = SDL_CreateThread(renderWavThread, NULL, NULL);
	if (thread == NULL)
	{
		fclose((FILE *)editor.wavRendererFileHandle);
		okBox(0, "System message", "Couldn't create thread!");
		return;
	}

	SDL_DetachThread(thread);
}

void pbWavRender(void)
{
	wavRender(config.cfg_OverwriteWarning ? true : false);
}

void pbWavExit(void)
{
	exitWavRenderer();
}

void pbWavFreqUp(void)
{
	if (WDFrequency < MAX_WAV_RENDER_FREQ)
	{
		     if (WDFrequency == 44100) WDFrequency = 48000;
		else if (WDFrequency == 48000) WDFrequency = 96000;
		else if (WDFrequency == 96000) WDFrequency = 192000;
		updateWavRenderer();
	}
}

void pbWavFreqDown(void)
{
	if (WDFrequency > MIN_WAV_RENDER_FREQ)
	{
		     if (WDFrequency == 192000) WDFrequency = 96000;
		else if (WDFrequency == 96000) WDFrequency = 48000;
		else if (WDFrequency == 48000) WDFrequency = 44100;

		updateWavRenderer();
	}
}

void pbWavAmpUp(void)
{
	if (WDAmp >= 32)
		return;

	WDAmp++;
	updateWavRenderer();
}

void pbWavAmpDown(void)
{
	if (WDAmp <= 1)
		return;

	WDAmp--;
	updateWavRenderer();
}

void pbWavSongStartUp(void)
{
	if (WDStartPos >= song.len-1)
		return;

	WDStartPos++;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.len - 1));
	updateWavRenderer();
}

void pbWavSongStartDown(void)
{
	if (WDStartPos == 0)
		return;

	WDStartPos--;
	updateWavRenderer();
}

void pbWavSongEndUp(void)
{
	if (WDStopPos >= 255)
		return;

	WDStopPos++;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.len - 1));
	updateWavRenderer();
}

void pbWavSongEndDown(void)
{
	if (WDStopPos == 0)
		return;

	WDStopPos--;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.len - 1));
	updateWavRenderer();
}

void rbWavRenderBitDepth16(void)
{
	checkRadioButton(RB_WAV_RENDER_BITDEPTH16);
	WDBitDepth = 16;
}

void rbWavRenderBitDepth32(void)
{
	checkRadioButton(RB_WAV_RENDER_BITDEPTH32);
	WDBitDepth = 32;
}
