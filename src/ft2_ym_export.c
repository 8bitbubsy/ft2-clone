// YM6 file exporter for ft2-clone Atari mode
// See ft2_ym_export.h for format documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_ym_export.h"
#include "ft2_atari_replayer.h"
#include "ft2_replayer.h"
#include "ft2_header.h"

static void write16BE(FILE *f, uint16_t v)
{
	uint8_t buf[2];
	buf[0] = (uint8_t)(v >> 8);
	buf[1] = (uint8_t)(v & 0xFF);
	fwrite(buf, 1, 2, f);
}

static void write32BE(FILE *f, uint32_t v)
{
	uint8_t buf[4];
	buf[0] = (uint8_t)(v >> 24);
	buf[1] = (uint8_t)((v >> 16) & 0xFF);
	buf[2] = (uint8_t)((v >>  8) & 0xFF);
	buf[3] = (uint8_t)(v & 0xFF);
	fwrite(buf, 1, 4, f);
}

#define YM_MAX_FRAMES     200000
#define YM_NUM_REGS_EXPORT 14

bool ymExportToFile(const char *path, const ymExportSettings_t *settings)
{
	if (path == NULL || settings == NULL)
		return false;

	const uint16_t playerHz = (settings->playerHz > 0) ? settings->playerHz : 50;

	// Allocate frame buffer: frames[frame][reg]
	uint8_t (*frames)[YM_NUM_REGS_EXPORT] =
		(uint8_t (*)[YM_NUM_REGS_EXPORT])calloc(YM_MAX_FRAMES, sizeof(*frames));
	if (frames == NULL)
		return false;

	// Save song state
	int16_t  savedSongPos     = song.songPos;
	int16_t  savedRow         = song.row;
	int16_t  savedPattNum     = song.pattNum;
	uint16_t savedTick        = song.tick;
	uint16_t savedSpeed       = song.speed;
	uint16_t savedBPM         = song.BPM;
	bool     savedPlaying     = songPlaying;
	int8_t   savedPlayMode    = playMode;
	uint8_t  savedPBreakPos   = song.pBreakPos;
	bool     savedPBreakFlag  = song.pBreakFlag;
	bool     savedPosJumpFlag = song.posJumpFlag;
	int16_t  savedCurrNumRows = song.currNumRows;

	// Set song to export start
	song.songPos     = (int16_t)settings->startPos;
	song.row         = 0;
	song.pattNum     = song.orders[song.songPos];
	song.currNumRows = patternNumRows[song.pattNum];
	if (song.currNumRows <= 0) song.currNumRows = 1;
	song.tick        = 1;
	if (song.speed == 0) song.speed = 6;
	song.pBreakFlag  = false;
	song.posJumpFlag = false;
	song.pBreakPos   = 0;
	songPlaying      = true;
	playMode         = PLAYMODE_SONG;

	// Initialise replayer
	atariReplayer_t ar;
	atariReplayer_init(&ar);

	// Render: for each VBL frame, run enough replayer ticks then snapshot regs.
	// ticksPerFrame = BPM * speed / (2.5 * playerHz)  → use fixed-point 16.16
	uint32_t numFrames = 0;
	bool     songAlive = true;

	// Accumulator in 16.16 fixed point
	uint32_t fracAcc = 0;

	while (songAlive && numFrames < YM_MAX_FRAMES)
	{
		// Advance accumulator: add BPM*speed*2 (numerator of ticksPerFrame)
		fracAcc += (uint32_t)song.BPM * song.speed * 2;
		uint32_t divisor = (uint32_t)5 * playerHz;
		uint32_t ticksThisFrame = fracAcc / divisor;
		fracAcc %= divisor;

		if (ticksThisFrame == 0)
			ticksThisFrame = 1; // always at least one tick

		for (uint32_t t = 0; t < ticksThisFrame && songAlive; t++)
		{
			songAlive = atariReplayer_tick(&ar);

			// Stop if we've passed stopPos
			if (song.songPos > (int16_t)settings->stopPos)
			{
				songAlive = false;
				break;
			}
		}

		// Snapshot registers for this frame
		atariReplayer_getRegs(&ar, frames[numFrames]);
		numFrames++;
	}

	// Restore song state
	song.songPos     = savedSongPos;
	song.row         = savedRow;
	song.pattNum     = savedPattNum;
	song.tick        = savedTick;
	song.speed       = savedSpeed;
	song.BPM         = savedBPM;
	songPlaying      = savedPlaying;
	playMode         = savedPlayMode;
	song.pBreakPos   = savedPBreakPos;
	song.pBreakFlag  = savedPBreakFlag;
	song.posJumpFlag = savedPosJumpFlag;
	song.currNumRows = savedCurrNumRows;

	// Write YM6 file
	FILE *f = fopen(path, "wb");
	if (f == NULL)
	{
		free(frames);
		return false;
	}

	// Header identifiers
	fwrite("YM6!", 1, 4, f);
	fwrite("LeOnArD!", 1, 8, f);

	// Frame count
	write32BE(f, numFrames);

	// Attributes: bit0 = interleaved (1), bit1 = hasLoop (0)
	write32BE(f, 0x00000001);

	// Number of digidrums
	write32BE(f, 0);

	// YM master clock (2 MHz Atari ST)
	write32BE(f, 2000000);

	// Player frame rate
	write16BE(f, playerHz);

	// Loop frame (0 = no loop)
	write32BE(f, 0);

	// Additional data size
	write16BE(f, 0);

	// Metadata strings (NUL-terminated)
	fwrite(settings->title,   1, strlen(settings->title)   + 1, f);
	fwrite(settings->author,  1, strlen(settings->author)  + 1, f);
	fwrite(settings->comment, 1, strlen(settings->comment) + 1, f);

	// Interleaved register data: all R0 values, then all R1, ..., then all R13
	for (int r = 0; r < YM_NUM_REGS_EXPORT; r++)
	{
		for (uint32_t fr = 0; fr < numFrames; fr++)
			fputc(frames[fr][r], f);
	}

	// End marker
	fwrite("End!", 1, 4, f);

	fclose(f);
	free(frames);
	return true;
}
