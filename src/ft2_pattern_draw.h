#pragma once

#include <stdint.h>

void drawPatternBorders(void);
void writePattern(int16_t currRow, int16_t pattern);
void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t paletteIndex, uint8_t val);
