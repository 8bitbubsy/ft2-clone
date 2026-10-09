// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

// for detecting if musl or glibc is used
#if defined(__linux__)
  /* Only Linux has glibc's <features.h>. On BSDs (including FreeBSD) and others,
     skip this block to avoid a missing-header error. */
  #ifdef __has_include
    #if __has_include(<features.h>)
      #include <features.h>
    #endif
  #else
    /* If the compiler doesn't support __has_include, assume features.h exists on glibc. */
    #include <features.h>
  #endif
  /* If <features.h> didn't define glibc's GNU extensions, assume musl. */
  #ifndef __USE_GNU
    #define __MUSL__
  #endif
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <iconv.h>
#endif
#include "ft2_unicode.h"

#ifdef _WIN32

// Windows routines
char *cp850ToUtf8(char *src)
{
	int32_t retVal;

	if (src == NULL)
		return NULL;

	int32_t srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	int32_t reqSize = MultiByteToWideChar(850, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	wchar_t *w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	retVal = MultiByteToWideChar(850, 0, src, srcLen, w, reqSize);
	if (!retVal)
	{
		free(w);
		return NULL;
	}

	srcLen = (int32_t)wcslen(w);
	if (srcLen <= 0)
		return NULL;

	reqSize = WideCharToMultiByte(CP_UTF8, 0, w, srcLen, 0, 0, 0, 0);
	if (reqSize <= 0)
	{
		free(w);
		return NULL;
	}

	char *x = (char *)malloc((reqSize + 1) * sizeof (char));
	if (x == NULL)
	{
		free(w);
		return NULL;
	}

	x[reqSize] = '\0';

	retVal = WideCharToMultiByte(CP_UTF8, 0, w, srcLen, x, reqSize, 0, 0);
	free(w);

	if (!retVal)
	{
		free(x);
		return NULL;
	}

	return x;
}

UNICHAR *cp850ToUnichar(char *src)
{
	if (src == NULL)
		return NULL;

	int32_t srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	int32_t reqSize = MultiByteToWideChar(850, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	UNICHAR *w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	int32_t retVal = MultiByteToWideChar(850, 0, src, srcLen, w, reqSize);
	if (!retVal)
	{
		free(w);
		return NULL;
	}

	return w;
}

char *utf8ToCp850(char *src, bool removeIllegalChars)
{
	if (src == NULL)
		return NULL;

	int32_t srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	int32_t reqSize = MultiByteToWideChar(CP_UTF8, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	wchar_t *w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	int32_t retVal = MultiByteToWideChar(CP_UTF8, 0, src, srcLen, w, reqSize);
	if (!retVal)
	{
		free(w);
		return NULL;
	}

	srcLen = (int32_t)wcslen(w);
	if (srcLen <= 0)
	{
		free(w);
		return NULL;
	}

	reqSize = WideCharToMultiByte(850, 0, w, srcLen, 0, 0, 0, 0);
	if (reqSize <= 0)
	{
		free(w);
		return NULL;
	}

	char *x = (char *)malloc((reqSize + 1) * sizeof (char));
	if (x == NULL)
	{
		free(w);
		return NULL;
	}

	x[reqSize] = '\0';

	retVal = WideCharToMultiByte(850, 0, w, srcLen, x, reqSize, 0, 0);
	free(w);

	if (!retVal)
	{
		free(x);
		return NULL;
	}

	if (removeIllegalChars)
	{
		// remove illegal characters (only allow certain nordic ones)
		for (int32_t i = 0; i < reqSize; i++)
		{
			const int8_t ch = (const int8_t)x[i];
			if (ch != '\0' && ch < 32 &&
			    ch != -124 && ch != -108 && ch != -122 && ch != -114 && ch != -103 &&
			    ch != -113 && ch != -101 && ch != -99 && ch != -111 && ch != -110)
			{
				x[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return x;
}

char *unicharToCp850(UNICHAR *src, bool removeIllegalChars)
{
	if (src == NULL)
		return NULL;

	int32_t srcLen = (int32_t)UNICHAR_STRLEN(src);
	if (srcLen <= 0)
		return NULL;

	int32_t reqSize = WideCharToMultiByte(850, 0, src, srcLen, 0, 0, 0, 0);
	if (reqSize <= 0)
		return NULL;

	char *x = (char *)malloc((reqSize + 1) * sizeof (char));
	if (x == NULL)
		return NULL;

	x[reqSize] = '\0';

	int32_t retVal = WideCharToMultiByte(850, 0, src, srcLen, x, reqSize, 0, 0);
	if (!retVal)
	{
		free(x);
		return NULL;
	}

	if (removeIllegalChars)
	{
		// remove illegal characters (only allow certain nordic ones)
		for (int32_t i = 0; i < reqSize; i++)
		{
			const int8_t ch = (const int8_t)x[i];
			if (ch != '\0' && ch < 32 &&
			    ch != -124 && ch != -108 && ch != -122 && ch != -114 && ch != -103 &&
			    ch != -113 && ch != -101 && ch != -99 && ch != -111 && ch != -110)
			{
				x[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return x;
}

#else

// non-Windows routines

/* Built-in CP850 <-> UTF-8 conversion, used if iconv can't do it (e.g. IRIX libc iconv
** doesn't know codepage 850).
*/
static const uint16_t cp850HighToUnicode[128] =
{
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
	0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0,
	0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
	0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE,
	0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
	0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
	0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
	0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8,
	0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0
};

static char *cp850ToUtf8Builtin(const char *src)
{
	char *outBuf = (char *)malloc((strlen(src) * 3) + 1); // max 3 UTF-8 bytes per char
	if (outBuf == NULL)
		return NULL;

	char *outPtr = outBuf;
	for (const uint8_t *in = (const uint8_t *)src; *in != '\0'; in++)
	{
		const uint16_t c = (*in < 128) ? *in : cp850HighToUnicode[*in - 128];
		if (c < 0x80)
		{
			*outPtr++ = (char)c;
		}
		else if (c < 0x800)
		{
			*outPtr++ = (char)(0xC0 | (c >> 6));
			*outPtr++ = (char)(0x80 | (c & 0x3F));
		}
		else
		{
			*outPtr++ = (char)(0xE0 | (c >> 12));
			*outPtr++ = (char)(0x80 | ((c >> 6) & 0x3F));
			*outPtr++ = (char)(0x80 | (c & 0x3F));
		}
	}
	*outPtr = '\0';

	return outBuf;
}

static char *utf8ToCp850Builtin(const char *src)
{
	char *outBuf = (char *)malloc(strlen(src) + 1); // never longer than the input
	if (outBuf == NULL)
		return NULL;

	char *outPtr = outBuf;
	const uint8_t *in = (const uint8_t *)src;
	while (*in != '\0')
	{
		// decode one UTF-8 sequence (invalid bytes are skipped)
		uint32_t c;
		int32_t extraBytes;

		if      (*in < 0x80)           { c = *in;        extraBytes = 0; }
		else if ((*in & 0xE0) == 0xC0) { c = *in & 0x1F; extraBytes = 1; }
		else if ((*in & 0xF0) == 0xE0) { c = *in & 0x0F; extraBytes = 2; }
		else if ((*in & 0xF8) == 0xF0) { c = *in & 0x07; extraBytes = 3; }
		else                           { in++; continue; }
		in++;

		bool valid = true;
		for (int32_t i = 0; i < extraBytes; i++)
		{
			if ((*in & 0xC0) != 0x80)
			{
				valid = false;
				break;
			}

			c = (c << 6) | (*in++ & 0x3F);
		}

		if (!valid)
			continue;

		if (c < 0x80)
		{
			*outPtr++ = (char)c;
			continue;
		}

		// find the CP850 char, or use '?' if there's none
		char outChar = '?';
		for (int32_t i = 0; i < 128; i++)
		{
			if (cp850HighToUnicode[i] == c)
			{
				outChar = (char)(128 + i);
				break;
			}
		}
		*outPtr++ = outChar;
	}
	*outPtr = '\0';

	return outBuf;
}

static void removeIllegalCp850Chars(char *s)
{
	// remove illegal characters (only allow certain nordic ones)
	for (; *s != '\0'; s++)
	{
		const int8_t ch = (const int8_t)*s;
		if (ch < 32 &&
		    ch != -124 && ch != -108 && ch != -122 && ch != -114 && ch != -103 &&
		    ch != -113 && ch != -101 && ch != -99 && ch != -111 && ch != -110)
		{
			*s = ' '; // character not allowed, turn it into space
		}
	}
}

char *cp850ToUtf8(char *src)
{
	if (src == NULL)
		return NULL;

	size_t srcLen = strlen(src);
	if (srcLen <= 0)
		return NULL;

	iconv_t cd = iconv_open("UTF-8", "850");
	if (cd == (iconv_t)-1)
		return cp850ToUtf8Builtin(src);

	size_t outLen = srcLen * 4; // should be sufficient

	char *outBuf = (char *)calloc(outLen + 1, sizeof (char));
	if (outBuf == NULL)
		return NULL;

	char *inPtr = src;
	size_t inLen = srcLen;
	char *outPtr = outBuf;

#if defined(__NetBSD__) || defined(__sun) || defined(sun)
	int32_t rc = iconv(cd, (const char **)&inPtr, &inLen, &outPtr, &outLen);
#else
	int32_t rc = iconv(cd, &inPtr, &inLen, &outPtr, &outLen);
#endif
	iconv(cd, NULL, NULL, &outPtr, &outLen); // flush
	iconv_close(cd);

	if (rc == -1)
	{
		free(outBuf);
		return cp850ToUtf8Builtin(src);
	}

	outBuf[outLen] = '\0';

	return outBuf;
}

char *utf8ToCp850(char *src, bool removeIllegalChars)
{
	char *outBuf;

	if (src == NULL)
		return NULL;

	size_t srcLen = strlen(src);
	if (srcLen <= 0)
		return NULL;

#ifdef __APPLE__
	iconv_t cd = iconv_open("850//TRANSLIT//IGNORE", "UTF-8-MAC");
#elif defined(__NetBSD__) || defined(__sun) || defined(sun)
	iconv_t cd = iconv_open("850", "UTF-8");
#elif defined(__MUSL__)
	iconv_t cd = iconv_open("cp850", "UTF-8");
#else
	iconv_t cd = iconv_open("850//TRANSLIT//IGNORE", "UTF-8");
#endif
	if (cd == (iconv_t)-1)
	{
		outBuf = utf8ToCp850Builtin(src);
		goto done;
	}

	size_t outLen = srcLen * 4; // should be sufficient

	outBuf = (char *)calloc(outLen + 1, sizeof (char));
	if (outBuf == NULL)
	{
		iconv_close(cd);
		return NULL;
	}

	char *inPtr = src;
	size_t inLen = srcLen;
	char *outPtr = outBuf;

#if defined(__NetBSD__) || defined(__sun) || defined(sun)
	int32_t rc = iconv(cd, (const char **)&inPtr, &inLen, &outPtr, &outLen);
#else
	int32_t rc = iconv(cd, &inPtr, &inLen, &outPtr, &outLen);
#endif
	iconv(cd, NULL, NULL, &outPtr, &outLen); // flush
	iconv_close(cd);

	if (rc == -1)
	{
		free(outBuf);
		outBuf = utf8ToCp850Builtin(src);
	}

done:
	if (outBuf != NULL && removeIllegalChars)
		removeIllegalCp850Chars(outBuf);

	return outBuf;
}
#endif
