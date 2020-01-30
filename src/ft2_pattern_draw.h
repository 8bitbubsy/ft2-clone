#pragma once

#include <stdint.h>

void updatePattFontPtrs(void);
void drawPatternBorders(void);
void writePattern(int32_t currRow, int32_t pattern);
void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color);
