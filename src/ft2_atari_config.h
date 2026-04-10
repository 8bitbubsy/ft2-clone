#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ATARI_CFG_MAGIC   "FT2-ATARI-CFG\x1A"
#define ATARI_CFG_MAGIC_LEN 14
#define ATARI_CFG_VERSION 1

typedef struct atariConfig_t
{
	bool     atariMode;           // global PSG mode enable
	uint8_t  ymExportFormat;      // 0=YM6, 1=SNDH
	uint16_t ymTimerFreq;         // 50 or 60
	bool     ymDigiDrums;         // reserved for future use
	char     sndhAuthor[32];
	char     sndhTitle[32];
} atariConfig_t;

extern atariConfig_t atariConfig;

void resetAtariConfig(void);
bool loadAtariConfig(void);
bool saveAtariConfig(void);
