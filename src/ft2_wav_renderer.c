// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_diskop.h"
#include "scopes/ft2_scopes.h"
#include "ft2_config.h"
#include "ft2_mouse.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_audio.h"
#include "ft2_wav_renderer.h"
#include "ft2_structs.h"

#define UPDATE_VISUALS_AT_TICK 4
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

static bool useLegacyBPM = false;
static uint8_t WDBitDepth = 16, WDStartPos, WDStopPos, *wavRenderBuffer;
static int16_t WDAmp;
static uint32_t WDFrequency = 44100;
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

void cbToggleWavRenderBPMMode(void)
{
	useLegacyBPM ^= 1;
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

	textOutShadow(19, 114, PAL_FORGRND, PAL_DSKTOP2, "Imprecise");
	textOutShadow(4,  127, PAL_FORGRND, PAL_DSKTOP2, "BPM (FT2)");

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

	showCheckBox(CB_WAV_BPM_MODE);

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
	WDStopPos = (uint8_t)song.songLength - 1;

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
	WDStopPos = (uint8_t)song.songLength - 1;

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
	hideCheckBox(CB_WAV_BPM_MODE);
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
	int32_t bytesPerSample = (WDBitDepth / 8) * 2; // 2 channels
	int32_t maxSamplesPerTick = (int32_t)ceil(frq / (MIN_BPM / 2.5)) + 1;

	// *2 for stereo
	wavRenderBuffer = (uint8_t *)malloc((TICKS_PER_RENDER_CHUNK * maxSamplesPerTick) * bytesPerSample);
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
	song.globalVolume = 64;
	setMixerBPM(song.BPM);

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
	if (song.speed == 0)
		song.speed = 6;

	setBackOldAudioFreq();
	setMixerBPM(song.BPM);
	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
	editor.wavIsRendering = false;

	setMouseBusy(false);
}

static bool dump_EndOfTune(int16_t endSongPos)
{
	bool returnValue = (editor.wavReachedEndFlag && song.row == 0 && song.tick == 1) || (song.speed == 0);

	// FT2 bugfix for EEx (pattern delay) on first row of a pattern
	if (song.pattDelTime2 > 0)
		returnValue = false;

	if (song.songPos == endSongPos && song.row == 0 && song.tick == 1)
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
	editor.editPattern = (uint8_t)song.pattNum;
	editor.row = song.row;
	editor.songPos = song.songPos;
	editor.BPM = song.BPM;
	editor.speed = song.speed;
	editor.globalVolume = song.globalVolume;

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
	(void)ptr;

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
	bool overflow = false, renderDone = false;
	uint8_t tickCounter = UPDATE_VISUALS_AT_TICK;
	uint64_t tickSamplesFrac = 0;

	uint64_t bytesInFile = sizeof (wavHeader_t);

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

			dump_TickReplayer();
			uint32_t tickSamples = audio.samplesPerTickInt;

			if (!useLegacyBPM)
			{
				tickSamplesFrac += audio.samplesPerTickFrac;
				if (tickSamplesFrac >= BPM_FRAC_SCALE)
				{
					tickSamplesFrac &= BPM_FRAC_MASK;
					tickSamples++;
				}
			}

			mixReplayerTickToBuffer(tickSamples, ptr8, WDBitDepth);

			tickSamples *= 2; // stereo
			samplesInChunk += tickSamples;
			sampleCounter += tickSamples;

			// increase buffer pointer
			if (WDBitDepth == 16)
			{
				ptr8 += tickSamples * sizeof (int16_t);
				bytesInFile += tickSamples * sizeof (int16_t);
			}
			else
			{
				ptr8 += tickSamples * sizeof (float);
				bytesInFile += tickSamples * sizeof (float);
			}

			if (bytesInFile >= INT32_MAX)
			{
				renderDone = true;
				overflow = true;
				break;
			}

			if (++tickCounter >= UPDATE_VISUALS_AT_TICK)
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

	if (overflow)
		okBoxThreadSafe(0, "System message", "Rendering stopped, file exceeded 2GB!");

	editor.diskOpReadOnOpen = true;
	return true;
}

static void wavRender(bool checkOverwrite)
{
	WDStartPos = (uint8_t)(MAX(0, MIN(WDStartPos, song.songLength - 1)));
	WDStopPos  = (uint8_t)(MAX(0, MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1)));

	updateWavRenderer();

	diskOpChangeFilenameExt(".wav");

	char *filename = getDiskOpFilename();
	if (checkOverwrite && fileExistsAnsi(filename))
	{
		char buf[256];
		createFileOverwriteText(filename, buf);
		if (okBox(2, "System request", buf) != 1)
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
#if CPU_64BIT
		else if (WDFrequency == 48000) WDFrequency = 96000;
		else if (WDFrequency == 96000) WDFrequency = 192000;
#endif
		updateWavRenderer();
	}
}

void pbWavFreqDown(void)
{
	if (WDFrequency > MIN_WAV_RENDER_FREQ)
	{
#if CPU_64BIT
		     if (WDFrequency == 192000) WDFrequency = 96000;
		else if (WDFrequency == 96000) WDFrequency = 48000;
		else if (WDFrequency == 48000) WDFrequency = 44100;
#else
		if (WDFrequency == 48000) WDFrequency = 44100;
#endif
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
	if (WDStartPos >= song.songLength-1)
		return;

	WDStartPos++;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
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
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
	updateWavRenderer();
}

void pbWavSongEndDown(void)
{
	if (WDStopPos == 0)
		return;

	WDStopPos--;
	WDStopPos = (uint8_t)(MIN(MAX(WDStartPos, WDStopPos), song.songLength - 1));
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
