// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

// for detecting if musl or glibc is used
#ifndef _WIN32
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	  #include <features.h>
	  #ifndef __USE_GNU
	      #define __MUSL__
	  #endif
	  #undef _GNU_SOURCE /* don't contaminate other includes unnecessarily */
	#else
	  #include <features.h>
	  #ifndef __USE_GNU
	      #define __MUSL__
	  #endif
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
char *cp850ToUtf8(char *src)
{
	if (src == NULL)
		return NULL;

	size_t srcLen = strlen(src);
	if (srcLen <= 0)
		return NULL;

	iconv_t cd = iconv_open("UTF-8", "850");
	if (cd == (iconv_t)-1)
		return NULL;

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
		return NULL;
	}

	outBuf[outLen] = '\0';

	return outBuf;
}

char *utf8ToCp850(char *src, bool removeIllegalChars)
{
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
		return NULL;

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
		return NULL;
	}

	outBuf[outLen] = '\0';

	if (removeIllegalChars)
	{
		// remove illegal characters (only allow certain nordic ones)
		for (size_t i = 0; i < outLen; i++)
		{
			const int8_t ch = (const int8_t)outBuf[i];
			if (ch != '\0' && ch < 32 &&
			    ch != -124 && ch != -108 && ch != -122 && ch != -114 && ch != -103 &&
			    ch != -113 && ch != -101 && ch != -99 && ch != -111 && ch != -110)
			{
				outBuf[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return outBuf;
}
#endif
