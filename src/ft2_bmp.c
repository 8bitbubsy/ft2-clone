// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_palette.h"
#include "ft2_gfxdata.h"
#include "ft2_bmp.h"
#include "ft2_video.h"

enum
{
	COMP_RGB = 0,
	COMP_RLE8 = 1,
	COMP_RLE4 = 2
};

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct bmpHeader_t
{
	char BM[2];
	uint32_t bfSizebfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
	uint32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	int32_t biClrUsed;
	int32_t biClrImportant;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
bmpHeader_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint32_t *loadBMPTo32Bit(const uint8_t *src);
static uint8_t *loadBMPTo1Bit(const uint8_t *src);
static uint8_t *loadBMPTo4BitPal(const uint8_t *src);

bmp_t bmp; // globalized

// if you match a color in this table, you have the real palette index (LUT pos)
#define NUM_CUSTOM_PALS 17
static const uint32_t bmpCustomPalette[NUM_CUSTOM_PALS] =
{ // 0xC0FFEE = spacers
	0x000000, 0x5397FF, 0x000067, 0x4BFFFF, 0xAB7787,
	0xFFFFFF, 0x7F7F7F, 0xABCDEF, 0x733747, 0xF7CBDB,
	0x434343, 0xD3D3D3, 0xFFFF00, 0xC0FFEE, 0xC0FFEE,
	0xC0FFEE, 0xFF0000 // last value = loop pin line
};

static int8_t getFT2PalNrFromPixel(uint32_t pixel32)
{
	for (int32_t i = 0; i < NUM_CUSTOM_PALS; i++)
	{
		if (pixel32 == bmpCustomPalette[i])
			return (int8_t)i;
	}

	return PAL_TRANSPR;
}

bool loadBMPs(void)
{
	memset(&bmp, 0, sizeof (bmp));

	bmp.ft2OldAboutLogo = loadBMPTo4BitPal(ft2OldAboutLogoBMP);
	bmp.ft2AboutLogo = loadBMPTo32Bit(ft2AboutLogoBMP);
	bmp.buttonGfx = loadBMPTo1Bit(buttonGfxBMP);
	bmp.font1 = loadBMPTo1Bit(font1BMP);
	bmp.font2 = loadBMPTo1Bit(font2BMP);
	bmp.font3 = loadBMPTo1Bit(font3BMP);
	bmp.font4 = loadBMPTo1Bit(font4BMP);
	bmp.font6 = loadBMPTo1Bit(font6BMP);
	bmp.font7 = loadBMPTo1Bit(font7BMP);
	bmp.font8 = loadBMPTo1Bit(font8BMP);
	bmp.ft2LogoBadges = loadBMPTo4BitPal(ft2LogoBadgesBMP);
	bmp.ft2ByBadges = loadBMPTo4BitPal(ft2ByBadgesBMP);
	bmp.midiLogo = loadBMPTo4BitPal(midiLogoBMP);
	bmp.nibblesLogo = loadBMPTo4BitPal(nibblesLogoBMP);
	bmp.nibblesStages = loadBMPTo4BitPal(nibblesStagesBMP);
	bmp.loopPins = loadBMPTo4BitPal(loopPinsBMP);
	bmp.mouseCursors = loadBMPTo4BitPal(mouseCursorsBMP);
	bmp.mouseCursorBusyClock = loadBMPTo4BitPal(mouseCursorBusyClockBMP);
	bmp.mouseCursorBusyGlass = loadBMPTo4BitPal(mouseCursorBusyGlassBMP);
	bmp.whitePianoKeys = loadBMPTo4BitPal(whitePianoKeysBMP);
	bmp.blackPianoKeys = loadBMPTo4BitPal(blackPianoKeysBMP);
	bmp.vibratoWaveforms = loadBMPTo4BitPal(vibratoWaveformsBMP);
	bmp.scopeRec = loadBMPTo4BitPal(scopeRecBMP);
	bmp.scopeMute = loadBMPTo4BitPal(scopeMuteBMP);
	bmp.radiobuttonGfx = loadBMPTo4BitPal(radiobuttonGfxBMP);
	bmp.checkboxGfx = loadBMPTo4BitPal(checkboxGfxBMP);

	if (bmp.ft2OldAboutLogo == NULL || bmp.ft2AboutLogo == NULL || bmp.buttonGfx == NULL || bmp.font1 == NULL || bmp.font2 == NULL ||
		bmp.font3 == NULL || bmp.font4 == NULL || bmp.font6 == NULL || bmp.font7 == NULL ||
		bmp.font8 == NULL || bmp.ft2LogoBadges == NULL || bmp.ft2ByBadges == NULL ||
		bmp.midiLogo == NULL || bmp.nibblesLogo == NULL || bmp.nibblesStages == NULL ||
		bmp.loopPins == NULL || bmp.mouseCursors == NULL || bmp.mouseCursorBusyClock == NULL ||
		bmp.mouseCursorBusyGlass == NULL || bmp.whitePianoKeys == NULL || bmp.blackPianoKeys == NULL ||
		bmp.vibratoWaveforms == NULL || bmp.scopeRec == NULL || bmp.scopeMute == NULL ||
		bmp.radiobuttonGfx == NULL || bmp.checkboxGfx == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	return true;
}

void freeBMPs(void)
{
	if (bmp.ft2OldAboutLogo != NULL) { free(bmp.ft2OldAboutLogo); bmp.ft2OldAboutLogo = NULL; }
	if (bmp.ft2AboutLogo != NULL) { free(bmp.ft2AboutLogo); bmp.ft2AboutLogo = NULL; }
	if (bmp.buttonGfx != NULL) { free(bmp.buttonGfx); bmp.buttonGfx = NULL; }
	if (bmp.font1 != NULL) { free(bmp.font1); bmp.font1 = NULL; }
	if (bmp.font2 != NULL) { free(bmp.font2); bmp.font2 = NULL; }
	if (bmp.font3 != NULL) { free(bmp.font3); bmp.font3 = NULL; }
	if (bmp.font4 != NULL) { free(bmp.font4); bmp.font4 = NULL; }
	if (bmp.font6 != NULL) { free(bmp.font6); bmp.font6 = NULL; }
	if (bmp.font7 != NULL) { free(bmp.font7); bmp.font7 = NULL; }
	if (bmp.font8 != NULL) { free(bmp.font8); bmp.font8 = NULL; }
	if (bmp.ft2LogoBadges != NULL) { free(bmp.ft2LogoBadges); bmp.ft2LogoBadges = NULL; }
	if (bmp.ft2ByBadges != NULL) { free(bmp.ft2ByBadges); bmp.ft2ByBadges = NULL; }
	if (bmp.midiLogo != NULL) { free(bmp.midiLogo); bmp.midiLogo = NULL; }
	if (bmp.nibblesLogo != NULL) { free(bmp.nibblesLogo); bmp.nibblesLogo = NULL; }
	if (bmp.nibblesStages != NULL) { free(bmp.nibblesStages); bmp.nibblesStages = NULL; }
	if (bmp.loopPins != NULL) { free(bmp.loopPins); bmp.loopPins = NULL; }
	if (bmp.mouseCursors != NULL) { free(bmp.mouseCursors); bmp.mouseCursors = NULL; }
	if (bmp.mouseCursorBusyClock != NULL) { free(bmp.mouseCursorBusyClock); bmp.mouseCursorBusyClock = NULL; }
	if (bmp.mouseCursorBusyGlass != NULL) { free(bmp.mouseCursorBusyGlass); bmp.mouseCursorBusyGlass = NULL; }
	if (bmp.whitePianoKeys != NULL) { free(bmp.whitePianoKeys); bmp.whitePianoKeys = NULL; }
	if (bmp.blackPianoKeys != NULL) { free(bmp.blackPianoKeys); bmp.blackPianoKeys = NULL; }
	if (bmp.vibratoWaveforms != NULL) { free(bmp.vibratoWaveforms); bmp.vibratoWaveforms = NULL; }
	if (bmp.scopeRec != NULL) { free(bmp.scopeRec); bmp.scopeRec = NULL; }
	if (bmp.scopeMute != NULL) { free(bmp.scopeMute); bmp.scopeMute = NULL; }
	if (bmp.radiobuttonGfx != NULL) { free(bmp.radiobuttonGfx); bmp.radiobuttonGfx = NULL; }
	if (bmp.checkboxGfx != NULL) { free(bmp.checkboxGfx); bmp.checkboxGfx = NULL; }
}

static uint32_t *loadBMPTo32Bit(const uint8_t *src)
{
	int32_t len, byte, palIdx;
	uint32_t *tmp32, color, color2, pal[256];

	bmpHeader_t *hdr = (bmpHeader_t *)src;
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;
	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;

	if (hdr->biCompression == COMP_RGB || palEntries > 256)
		return NULL;

	uint32_t *outData = (uint32_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint32_t));
	if (outData == NULL)
		return NULL;

	const uint8_t *p = (uint8_t *)&src[0x36];
	for (int32_t i = 0; i < palEntries; i++)
	{
		const uint8_t b8 = *p++;
		const uint8_t g8 = *p++;
		const uint8_t r8 = *p++;
		p++;

		pal[i] = (r8 << 16) | (g8 << 8) | b8;
	}

	// pre-fill image with first palette color
	for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = pal[0];

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint32_t *dst32 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1;

	while (true)
	{
		byte = *src8++;
		if (byte == 0) // escape control
		{
			byte = *src8++;
			if (byte == 0) // end of line
			{
				x = 0;
				y--;
			}
			else if (byte == 1) // end of bitmap
			{
				break;
			}
			else if (byte == 2) // add to x/y position
			{
				x += *src8++;
				y -= *src8++;
			}
			else // absolute bytes
			{
				if (hdr->biCompression == COMP_RLE8)
				{
					tmp32 = &dst32[(y * hdr->biWidth) + x];
					for (int32_t i = 0; i < byte; i++)
						*tmp32++ = pal[*src8++];

					if (byte & 1)
						src8++;

					x += byte;
				}
				else
				{

					len = byte >> 1;
					tmp32 = &dst32[y * hdr->biWidth];
					for (int32_t i = 0; i < len; i++)
					{
						palIdx = *src8++;
						tmp32[x++] = pal[palIdx >> 4];

						if (x < lineEnd)
							tmp32[x++] = pal[palIdx & 0xF];
					}

					if (((byte + 1) >> 1) & 1)
						src8++;
				}
			}
		}
		else
		{
			palIdx = *src8++;

			if (hdr->biCompression == COMP_RLE8)
			{
				color = pal[palIdx];
				tmp32 = &dst32[(y * hdr->biWidth) + x];
				for (int32_t i = 0; i < byte; i++)
					*tmp32++ = color;

				x += byte;
			}
			else
			{
				color = pal[palIdx >> 4];
				color2 = pal[palIdx & 0x0F];

				len = byte >> 1;
				tmp32 = &dst32[y * hdr->biWidth];
				for (int32_t i = 0; i < len; i++)
				{
					tmp32[x++] = color;
					if (x < lineEnd)
						tmp32[x++] = color2;
				}
			}
		}
	}

	return outData;
}

static uint8_t *loadBMPTo1Bit(const uint8_t *src) // supports 4-bit RLE only
{
	uint8_t palIdx, color, color2, *tmp8;
	int32_t len, byte;
	uint8_t pal[16];

	bmpHeader_t *hdr = (bmpHeader_t *)src;
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;
	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;

	if (hdr->biCompression != COMP_RLE4 || palEntries > 16)
		return NULL;

	uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
	if (outData == NULL)
		return NULL;
	
	const uint8_t *p = (uint8_t *)&src[0x36];
	for (int32_t i = 0; i < palEntries; i++)
	{
		const uint8_t b8 = *p++;
		const uint8_t g8 = *p++;
		const uint8_t r8 = *p++;
		p++;

		pal[i] = ((r8 + g8 + b8) != 0) ? 1 : 0;
	}

	// pre-fill image with first palette color
	for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = pal[0];

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint8_t *dst8 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1;

	while (true)
	{
		byte = *src8++;
		if (byte == 0) // escape control
		{
			byte = *src8++;
			if (byte == 0) // end of line
			{
				x = 0;
				y--;
			}
			else if (byte == 1) // end of bitmap
			{
				break;
			}
			else if (byte == 2) // add to x/y position
			{
				x += *src8++;
				y -= *src8++;
			}
			else // absolute bytes
			{
				len = byte >> 1;
				tmp8 = &dst8[y * hdr->biWidth];
				for (int32_t i = 0; i < len; i++)
				{
					palIdx = *src8++;
					tmp8[x++] = pal[palIdx >> 4];
					
					if (x < lineEnd)
						tmp8[x++] = pal[palIdx & 0xF];
				}

				if (((byte + 1) >> 1) & 1)
					src8++;
			}
		}
		else
		{
			palIdx = *src8++;

			color = pal[palIdx >> 4];
			color2 = pal[palIdx & 0x0F];

			len = byte >> 1;
			tmp8 = &dst8[y * hdr->biWidth];
			for (int32_t i = 0; i < len; i++)
			{
				tmp8[x++] = color;
				if (x < lineEnd)
					tmp8[x++] = color2;
			}
		}
	}

	return outData;
}

static uint8_t *loadBMPTo4BitPal(const uint8_t *src) // supports 4-bit RLE only
{
	uint8_t palIdx, *tmp8, pal1, pal2;
	int32_t len, byte;
	uint32_t pal[16];

	bmpHeader_t *hdr = (bmpHeader_t *)src;
	const uint8_t *pData = &src[hdr->bfOffBits];
	const int32_t colorsInBitmap = 1 << hdr->biBitCount;
	const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;

	if (hdr->biCompression != COMP_RLE4 || palEntries > 16)
		return NULL;

	uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
	if (outData == NULL)
		return NULL;

	const uint8_t *p = (uint8_t *)&src[0x36];
	for (int32_t i = 0; i < palEntries; i++)
	{
		const uint8_t b8 = *p++;
		const uint8_t g8 = *p++;
		const uint8_t r8 = *p++;
		p++;

		pal[i] = (r8 << 16) | (g8 << 8) | b8;
	}

	// pre-fill image with first palette color
	palIdx = getFT2PalNrFromPixel(pal[0]);
	for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
		outData[i] = palIdx;

	const int32_t lineEnd = hdr->biWidth;
	const uint8_t *src8 = pData;
	uint8_t *dst8 = outData;
	int32_t x = 0;
	int32_t y = hdr->biHeight - 1;

	while (true)
	{
		byte = *src8++;
		if (byte == 0) // escape control
		{
			byte = *src8++;
			if (byte == 0) // end of line
			{
				x = 0;
				y--;
			}
			else if (byte == 1) // end of bitmap
			{
				break;
			}
			else if (byte == 2) // add to x/y position
			{
				x += *src8++;
				y -= *src8++;
			}
			else // absolute bytes
			{
				tmp8 = &dst8[y * hdr->biWidth];
				len = byte >> 1;
				for (int32_t i = 0; i < len; i++)
				{
					palIdx = *src8++;
					tmp8[x++] = getFT2PalNrFromPixel(pal[palIdx >> 4]);

					if (x < lineEnd)
						tmp8[x++] = getFT2PalNrFromPixel(pal[palIdx & 0xF]);
				}

				if (((byte + 1) >> 1) & 1)
					src8++;
			}
		}
		else
		{
			palIdx = *src8++;

			pal1 = getFT2PalNrFromPixel(pal[palIdx >> 4]);
			pal2 = getFT2PalNrFromPixel(pal[palIdx & 0x0F]);

			tmp8 = &dst8[y * hdr->biWidth];
			len = byte >> 1;
			for (int32_t i = 0; i < len; i++)
			{
				tmp8[x++] = pal1;
				if (x < lineEnd)
					tmp8[x++] = pal2;
			}
		}
	}

	return outData;
}
