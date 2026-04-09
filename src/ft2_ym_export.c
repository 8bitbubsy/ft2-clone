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

static void write16LE(FILE *f, uint16_t v)
{
	uint8_t buf[2];
	buf[0] = (uint8_t)(v & 0xFF);
	buf[1] = (uint8_t)(v >> 8);
	fwrite(buf, 1, 2, f);
}

static void write32LE(FILE *f, uint32_t v)
{
	uint8_t buf[4];
	buf[0] = (uint8_t)(v & 0xFF);
	buf[1] = (uint8_t)((v >> 8)  & 0xFF);
	buf[2] = (uint8_t)((v >> 16) & 0xFF);
	buf[3] = (uint8_t)(v >> 24);
	fwrite(buf, 1, 4, f);
}

/*
 * Write a minimal LHA level-0 "store" (no compression, method "-lh0-") header
 * followed by the raw data bytes, then a zero end-of-archive marker.
 *
 * LHA level-0 header layout (all multi-byte fields are little-endian):
 *   1  headerSize   = bytes from method[0] to osType (inclusive), does NOT count
 *                     headerSize or checksum bytes themselves
 *   1  checksum     = sum of the header bytes (from method[0] through osType)
 *                     mod 256
 *   5  method       = "-lh0-"
 *   4  compressedSz = same as originalSz (no compression)
 *   4  originalSz
 *   2  modTime      = 0
 *   2  modDate      = 0
 *   1  attribute    = 0x20
 *   1  level        = 0
 *   1  fileNameLen
 *   N  fileName
 *   2  crc16        = 0 (skipped; many players do not verify)
 *   1  osType       = 0
 */
static void writeLha0Block(FILE *f, const void *data, uint32_t len,
                           const char *internalName)
{
	if (internalName == NULL)
		internalName = "song.ym";

	uint8_t nameLen = (uint8_t)strlen(internalName);

	/* Build the portion of the header that enters the checksum calculation.
	   That starts at method[0] and goes through osType (inclusive). */
	uint8_t hdr[5 + 4 + 4 + 2 + 2 + 1 + 1 + 1 + 255 + 2 + 1]; // generous max
	int hdrPos = 0;

	// method
	memcpy(hdr + hdrPos, "-lh0-", 5); hdrPos += 5;
	// compressedSize (LE)
	hdr[hdrPos++] = (uint8_t)(len & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 8)  & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 16) & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 24) & 0xFF);
	// originalSize (LE)
	hdr[hdrPos++] = (uint8_t)(len & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 8)  & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 16) & 0xFF);
	hdr[hdrPos++] = (uint8_t)((len >> 24) & 0xFF);
	// modTime, modDate
	hdr[hdrPos++] = 0; hdr[hdrPos++] = 0;
	hdr[hdrPos++] = 0; hdr[hdrPos++] = 0;
	// attribute
	hdr[hdrPos++] = 0x20;
	// level
	hdr[hdrPos++] = 0;
	// fileNameLen
	hdr[hdrPos++] = nameLen;
	// fileName
	memcpy(hdr + hdrPos, internalName, nameLen); hdrPos += nameLen;
	// CRC16 = 0
	hdr[hdrPos++] = 0; hdr[hdrPos++] = 0;
	// osType = 0
	hdr[hdrPos++] = 0;

	uint8_t headerSize = (uint8_t)hdrPos;

	// compute checksum = sum of hdr[0..hdrPos-1] mod 256
	uint8_t checksum = 0;
	for (int i = 0; i < hdrPos; i++)
		checksum = (uint8_t)(checksum + hdr[i]);

	// Write the two preamble bytes
	fputc(headerSize, f);
	fputc(checksum,   f);

	// Write the header body
	fwrite(hdr, 1, hdrPos, f);

	// Write the raw data
	fwrite(data, 1, len, f);

	// End-of-archive marker
	fputc(0, f);
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

	/* Build the interleaved register data block that will be LHA-wrapped */
	uint32_t regDataLen = (uint32_t)YM_NUM_REGS_EXPORT * numFrames;
	uint8_t *regData = (uint8_t *)malloc(regDataLen > 0 ? regDataLen : 1);
	if (regData == NULL)
	{
		free(frames);
		return false;
	}

	for (int r = 0; r < YM_NUM_REGS_EXPORT; r++)
		for (uint32_t fr = 0; fr < numFrames; fr++)
			regData[r * numFrames + fr] = frames[fr][r];

	free(frames);

	/* Build YM6 header + metadata into a memory buffer, then wrap with LHA */
	/* We write the whole YM6 payload (header + reg data + End!) as one block */
	FILE *f = fopen(path, "wb");
	if (f == NULL)
	{
		free(regData);
		return false;
	}

	/* Compute total YM6 payload size to build a memory block for LHA wrap */
	size_t titleLen   = strlen(settings->title)   + 1;
	size_t authorLen  = strlen(settings->author)  + 1;
	size_t commentLen = strlen(settings->comment) + 1;
	size_t payloadSize = 4 + 8          /* YM6! + LeOnArD! */
	                   + 4 + 4 + 4 + 4 + 2 + 4 + 2  /* header fields */
	                   + titleLen + authorLen + commentLen
	                   + regDataLen
	                   + 4;             /* End! */

	uint8_t *payload = (uint8_t *)malloc(payloadSize);
	if (payload == NULL)
	{
		fclose(f);
		free(regData);
		return false;
	}

	uint8_t *p = payload;

	// "YM6!"
	memcpy(p, "YM6!", 4); p += 4;
	// "LeOnArD!"
	memcpy(p, "LeOnArD!", 8); p += 8;
	// numFrames (BE)
	p[0] = (uint8_t)(numFrames >> 24); p[1] = (uint8_t)(numFrames >> 16);
	p[2] = (uint8_t)(numFrames >>  8); p[3] = (uint8_t)(numFrames & 0xFF);
	p += 4;
	// attributes: bit0=interleaved (BE)
	p[0]=0; p[1]=0; p[2]=0; p[3]=1; p += 4;
	// numDigidrums (BE)
	p[0]=0; p[1]=0; p[2]=0; p[3]=0; p += 4;
	// YM clock 2000000 (BE)
	p[0]=0; p[1]=0x1E; p[2]=0x84; p[3]=0x80; p += 4;
	// playerHz (BE)
	p[0] = (uint8_t)(playerHz >> 8); p[1] = (uint8_t)(playerHz & 0xFF); p += 2;
	// loopFrame (BE)
	p[0]=0; p[1]=0; p[2]=0; p[3]=0; p += 4;
	// additionalDataSz (BE)
	p[0]=0; p[1]=0; p += 2;
	// metadata strings
	memcpy(p, settings->title,   titleLen);   p += titleLen;
	memcpy(p, settings->author,  authorLen);  p += authorLen;
	memcpy(p, settings->comment, commentLen); p += commentLen;
	// interleaved register data
	memcpy(p, regData, regDataLen); p += regDataLen;
	// "End!"
	memcpy(p, "End!", 4); p += 4;

	free(regData);

	// Wrap in LHA level-0 store
	writeLha0Block(f, payload, (uint32_t)(p - payload), "song.ym");

	fclose(f);
	free(payload);
	return true;
}
