#pragma once

#include <stdint.h>
#include "ft2_palette.h" // pal16 typedef
#include "ft2_pattern_ed.h" // pattCoord_t/pattCoord2_t/pattCoordsMouse_t/markCoord_t typedef
#include "ft2_header.h" // MAX_VOICES
#include "ft2_config.h" // CONFIG_FILE_SIZE

#define LOG_TABLE_BITS 24
#define FAST_SINC_TABLE_BITS 14

#define KEY2VOL_ENTRIES (signed)(sizeof (key2VolTab) / sizeof (SDL_Keycode))
#define KEY2EFX_ENTRIES (signed)(sizeof (key2EfxTab) / sizeof (SDL_Keycode))
#define KEY2HEX_ENTRIES (signed)(sizeof (key2HexTab) / sizeof (SDL_Keycode))

extern const int8_t vibSineTab[256]; // for auto-vibrato
extern const uint8_t vibTab[32];
extern const uint16_t amigaPeriod[12 * 8];
extern const uint16_t amigaFinePeriod[12 * 8];
extern const int16_t linearPeriods[1936];
extern const int16_t amigaPeriods[1936];
extern const uint32_t logTab[768];

extern const uint32_t panningTab[257];
extern const int16_t fastSincTable[256 * 4];

extern pal16 palTable[12][16];
extern const uint16_t chanWidths[6];
extern const pattCoordsMouse_t pattCoordMouseTable[2][2][2];
extern const int16_t sinusTables[256 * 5];
extern const uint8_t noteTab1[96];
extern const uint8_t noteTab2[96];
extern const uint8_t hex2Dec[256];
extern const pattCoord_t pattCoordTable[2][2][2];
extern const pattCoord2_t pattCoord2Table[2][2][2];
extern const markCoord_t markCoordTable[2][2][2];
extern const uint8_t pattCursorXTab[2 * 4 * 8];
extern const uint8_t pattCursorWTab[2 * 4 * 8];
extern const char chDecTab1[MAX_VOICES+1];
extern const char chDecTab2[MAX_VOICES+1];
extern const SDL_Keycode key2VolTab[16];
extern const SDL_Keycode key2EfxTab[36];
extern const SDL_Keycode key2HexTab[16];
extern const uint8_t scopeMuteBMPWidths[16];
extern const uint8_t scopeMuteBMPHeights[16];
extern const uint8_t *scopeMuteBMPPointers[16];
extern const uint16_t scopeLenTab[16][32];

extern const uint8_t defConfigData[CONFIG_FILE_SIZE];
