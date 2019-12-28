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

	sprintf(str, "%06d", WDFrequency);
	textOut(209, 116, PAL_FORGRND, str);

	sprintf(str, "%02d", WDAmp);
	textOut(237, 130, PAL_FORGRND, str);

	hexOut(237, 144, PAL_FORGRND, WDStartPos, 2);
	hexOut(237, 158, PAL_FORGRND, WDStopPos,  2);
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

	textOutShadow(4,   96, PAL_FORGRND, PAL_DSKTOP2, "Harddisk recording:");
	textOutShadow(156, 96, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
	textOutShadow(221, 96, PAL_FORGRND, PAL_DSKTOP2, "24-bit float");

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
	WDStopPos  = (uint8_t)song.len - 1;

	if (editor.ui.wavRendererShown)
		updateWavRenderer();
}

void showWavRenderer(void)
{
	if (editor.ui.extended)
		exitPatternEditorExtended();

	hideTopScreen();
	showTopScreen(false);

	editor.ui.wavRendererShown = true;
	editor.ui.scopesShown = false;

	WDStartPos = 0;
	WDStopPos = (uint8_t)song.len - 1;

	drawWavRenderer();
}

void hideWavRenderer(void)
{
	editor.ui.wavRendererShown = false;

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

	editor.ui.scopesShown = true;
	drawScopeFramework();
}

void exitWavRenderer(void)
{
	hideWavRenderer();
}

static bool dump_Init(uint32_t frq, int16_t amp, int16_t songPos)
{
	uint8_t i, oldMuteFlags[MAX_VOICES];
	uint32_t maxSamplesPerTick, sampleSize;

	maxSamplesPerTick = ((frq * 5) / 2) / MIN_BPM; // absolute max samples per tidck
	sampleSize = (WDBitDepth / 8) * 2; // 2 channels

	// *2 for stereo
	wavRenderBuffer = (uint8_t *)malloc((TICKS_PER_RENDER_CHUNK * maxSamplesPerTick) * sampleSize);
	if (wavRenderBuffer == NULL)
		return false;

	editor.wavIsRendering = true;

	setPos(songPos, 0, true);
	playMode = PLAYMODE_SONG;
	songPlaying = true;

	// store mute states
	for (i = 0; i < MAX_VOICES; i++)
		oldMuteFlags[i] = stm[i].stOff;

	resetChannels();

	// restore mute states
	for (i = 0; i < MAX_VOICES; i++)
		stm[i].stOff = oldMuteFlags[i];

	setNewAudioFreq(frq);
	setAudioAmp(amp, config.masterVol, (WDBitDepth == 32));

	stopVoices();
	song.globVol = 64;
	setSpeed(song.speed);

	song.musicTime = 0;
	return true;
}

static void dump_Close(FILE *f, uint32_t totalSamples)
{
	uint32_t tmpLen, totalBytes;
	wavHeader_t wavHeader;

	if (wavRenderBuffer != NULL)
	{
		free(wavRenderBuffer);
		wavRenderBuffer = NULL;
	}

	if (WDBitDepth == 16)
		totalBytes = totalSamples * sizeof (int16_t);
	else
		totalBytes = totalSamples * sizeof (float);

	if (totalBytes & 1)
		fputc(0, f); // write pad byte

	tmpLen = ftell(f) - 8;

	// go back and fill in WAV header
	fseek(f, 0, SEEK_SET);

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
	setSpeed(song.speed);
	setAudioAmp(config.boostLevel, config.masterVol, config.specialFlags & BITDEPTH_24);
	editor.wavIsRendering = false;

	setMouseBusy(false);
}

static bool dump_EndOfTune(int16_t endSongPos) // exactly the same as in real FT2
{
	bool returnValue = (editor.wavReachedEndFlag && song.pattPos == 0 && song.timer == 1) || (song.tempo == 0);

	if (song.songPos == endSongPos && song.pattPos == 0 && song.timer == 1)
		editor.wavReachedEndFlag = true;

	return returnValue;
}

uint32_t dump_RenderTick(uint8_t *buffer) // returns bytes mixed
{
	replayerBusy = true;

	if (audio.volumeRampingFlag)
		mix_SaveIPVolumes();

	mainPlayer();
	mix_UpdateChannelVolPanFrq();

	replayerBusy = false;

	return mixReplayerTickToBuffer(buffer, WDBitDepth);
}

static void updateVisuals(void)
{
	editor.editPattern = (uint8_t)song.pattNr;
	editor.pattPos = song.pattPos;
	editor.songPos = song.songPos;
	editor.speed = song.speed;
	editor.tempo = song.tempo;
	editor.globalVol = song.globVol;

	editor.ui.drawPosEdFlag = true;
	editor.ui.drawPattNumLenFlag = true; 
	editor.ui.drawReplayerPianoFlag = true;
	editor.ui.drawBPMFlag = true;
	editor.ui.drawSpeedFlag = true;
	editor.ui.drawGlobVolFlag = true;
	editor.ui.updatePatternEditor = true;
}

static int32_t SDLCALL renderWavThread(void *ptr)
{
	bool renderDone;
	uint8_t *ptr8, loopCounter;
	uint32_t samplesInChunk, tickSamples, sampleCounter;
	FILE *f;

	(void)ptr;

	f = (FILE *)editor.wavRendererFileHandle;
	fseek(f, sizeof (wavHeader_t), SEEK_SET);

	pauseAudio();

	if (!dump_Init(WDFrequency, WDAmp, WDStartPos))
	{
		resumeAudio();
		okBoxThreadSafe(0, "System message", "Not enough memory!");
		return true;
	}

	sampleCounter = 0;
	renderDone = false;
	loopCounter = 0;

	editor.wavReachedEndFlag = false;
	while (!renderDone && editor.wavIsRendering)
	{
		samplesInChunk = 0;

		// render several ticks at once to prevent frequent disk I/O (speeds up the process)
		ptr8 = wavRenderBuffer;
		for (uint32_t i = 0; i < TICKS_PER_RENDER_CHUNK; i++)
		{
			if (!editor.wavIsRendering || dump_EndOfTune(WDStopPos))
			{
				renderDone = true;
				break;
			}

			tickSamples = dump_RenderTick(ptr8) * 2; // *2 for stereo

			samplesInChunk += tickSamples;
			sampleCounter  += tickSamples;

			// increase buffer pointer
			if (WDBitDepth == 16)
				ptr8 += (tickSamples * sizeof (int16_t));
			else
				ptr8 += (tickSamples * sizeof (float));

			if (++loopCounter > 16)
			{
				loopCounter = 0;
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
	dump_Close(f, sampleCounter);

	resumeAudio();

	editor.diskOpReadOnOpen = true;
	return true;
}

static void createOverwriteText(char *name)
{
	char nameTmp[128];
	uint32_t nameLen;

	// read entry name to a small buffer
	nameLen = (uint32_t)strlen(name);
	memcpy(nameTmp, name, (nameLen >= sizeof (nameTmp)) ? sizeof (nameTmp) : (nameLen + 1));
	nameTmp[sizeof (nameTmp) - 1] = '\0';

	trimEntryName(nameTmp, false);

	sprintf(WAV_SysReqText, "Overwrite file \"%s\"?", nameTmp);
}

static void wavRender(bool checkOverwrite)
{
	char *filename;

	WDStartPos = (uint8_t)(MAX(0, MIN(WDStartPos, song.len - 1)));
	WDStopPos  = (uint8_t)(MAX(0, MIN(MAX(WDStartPos, WDStopPos), song.len - 1)));

	updateWavRenderer();

	diskOpChangeFilenameExt(".wav");

	filename = getDiskOpFilename();
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
		     if (WDFrequency == 8000)  WDFrequency = 11025;
		else if (WDFrequency == 11025) WDFrequency = 16000;
		else if (WDFrequency == 16000) WDFrequency = 22050;
		else if (WDFrequency == 22050) WDFrequency = 32000;
		else if (WDFrequency == 32000) WDFrequency = 44100;
		else if (WDFrequency == 44100) WDFrequency = 48000;
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
		else if (WDFrequency == 96000)  WDFrequency = 48000;
		else if (WDFrequency == 48000)  WDFrequency = 44100;
		else if (WDFrequency == 44100)  WDFrequency = 32000;
		else if (WDFrequency == 32000)  WDFrequency = 22050;
		else if (WDFrequency == 22050)  WDFrequency = 16000;
		else if (WDFrequency == 16000)  WDFrequency = 11025;
		else if (WDFrequency == 11025)  WDFrequency = 8000;

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
