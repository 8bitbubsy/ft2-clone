// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
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
char *cp437ToUtf8(char *src)
{
	char *x;
	int32_t reqSize, retVal, srcLen;
	wchar_t *w;

	if (src == NULL)
		return NULL;

	srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	reqSize = MultiByteToWideChar(437, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	retVal = MultiByteToWideChar(437, 0, src, srcLen, w, reqSize);
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

	x = (char *)malloc((reqSize + 2) * sizeof (char));
	if (x == NULL)
	{
		free(w);
		return NULL;
	}

	x[reqSize+0] = '\0';
	x[reqSize+1] = '\0';

	retVal = WideCharToMultiByte(CP_UTF8, 0, w, srcLen, x, reqSize, 0, 0);
	free(w);

	if (!retVal)
	{
		free(x);
		return NULL;
	}

	return x;
}

UNICHAR *cp437ToUnichar(char *src)
{
	int32_t reqSize, retVal, srcLen;
	UNICHAR *w;

	if (src == NULL)
		return NULL;

	srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	reqSize = MultiByteToWideChar(437, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	retVal = MultiByteToWideChar(437, 0, src, srcLen, w, reqSize);
	if (!retVal)
	{
		free(w);
		return NULL;
	}

	return w;
}

char *utf8ToCp437(char *src, bool removeIllegalChars)
{
	char *x;
	int8_t ch;
	int32_t reqSize, retVal, srcLen;
	wchar_t *w;

	if (src == NULL)
		return NULL;

	srcLen = (int32_t)strlen(src);
	if (srcLen <= 0)
		return NULL;

	reqSize = MultiByteToWideChar(CP_UTF8, 0, src, srcLen, 0, 0);
	if (reqSize <= 0)
		return NULL;

	w = (wchar_t *)malloc((reqSize + 1) * sizeof (wchar_t));
	if (w == NULL)
		return NULL;

	w[reqSize] = 0;

	retVal = MultiByteToWideChar(CP_UTF8, 0, src, srcLen, w, reqSize);
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

	reqSize = WideCharToMultiByte(437, 0, w, srcLen, 0, 0, 0, 0);
	if (reqSize <= 0)
	{
		free(w);
		return NULL;
	}

	x = (char *)calloc(reqSize + 1, sizeof (char));
	if (x == NULL)
	{
		free(w);
		return NULL;
	}

	x[reqSize] = '\0';

	retVal = WideCharToMultiByte(437, 0, w, srcLen, x, reqSize, 0, 0);
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
			ch = (int8_t)x[i];
			if (ch < 32 && ch != 0 && ch != -124 && ch != -108 &&
			    ch != -122 && ch != -114 && ch != -103 && ch != -113)
			{
				x[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return x;
}

char *unicharToCp437(UNICHAR *src, bool removeIllegalChars)
{
	char *x;
	int8_t ch;
	int32_t reqSize, retVal, srcLen, i;

	if (src == NULL)
		return NULL;

	srcLen = (int32_t)UNICHAR_STRLEN(src);
	if (srcLen <= 0)
		return NULL;

	reqSize = WideCharToMultiByte(437, 0, src, srcLen, 0, 0, 0, 0);
	if (reqSize <= 0)
		return NULL;

	x = (char *)malloc((reqSize + 1) * sizeof (char));
	if (x == NULL)
		return NULL;

	x[reqSize] = '\0';

	retVal = WideCharToMultiByte(437, 0, src, srcLen, x, reqSize, 0, 0);
	if (!retVal)
	{
		free(x);
		return NULL;
	}

	if (removeIllegalChars)
	{
		// remove illegal characters (only allow certain nordic ones)
		for (i = 0; i < reqSize; i++)
		{
			ch = (int8_t)x[i];
			if (ch < 32 && ch != 0 && ch != -124 && ch != -108 &&
			    ch != -122 && ch != -114 && ch != -103 && ch != -113)
			{
				x[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return x;
}

#else

// non-Windows routines
char *cp437ToUtf8(char *src)
{
	char *inPtr, *outPtr, *outBuf;
	int32_t rc;
	size_t srcLen, inLen, outLen;
	iconv_t cd;

	if (src == NULL)
		return NULL;

	srcLen = strlen(src);
	if (srcLen <= 0)
		return NULL;

	cd = iconv_open("UTF-8", "437");
	if (cd == (iconv_t)-1)
		return NULL;

	outLen = srcLen * 2; // should be sufficient

	outBuf = (char *)calloc(outLen + 2, sizeof (char));
	if (outBuf == NULL)
		return NULL;

	inPtr = src;
	inLen = srcLen;
	outPtr = outBuf;

#if defined(__NetBSD__) || defined(__sun) || defined(sun)
	rc = iconv(cd, (const char **)&inPtr, &inLen, &outPtr, &outLen);
#else
	rc = iconv(cd, &inPtr, &inLen, &outPtr, &outLen);
#endif
	iconv(cd, NULL, NULL, &outPtr, &outLen); // flush
	iconv_close(cd);

	if (rc == -1)
	{
		free(outBuf);
		return NULL;
	}

	return outBuf;
}

char *utf8ToCp437(char *src, bool removeIllegalChars)
{
	char *inPtr, *outPtr, *outBuf;
	int8_t ch;
	int32_t rc;
	size_t srcLen, inLen, outLen;
	iconv_t cd;

	if (src == NULL)
		return NULL;

	srcLen = strlen(src);
	if (srcLen <= 0)
		return NULL;

#ifdef __APPLE__
	cd = iconv_open("437//TRANSLIT//IGNORE", "UTF-8-MAC");
#elif defined(__NetBSD__) || defined(__sun) || defined(sun)
	cd = iconv_open("437", "UTF-8");
#else
	cd = iconv_open("437//TRANSLIT//IGNORE", "UTF-8");
#endif
	if (cd == (iconv_t)-1)
		return NULL;

	outLen = srcLen * 2; // should be sufficient

	outBuf = (char *)calloc(outLen + 1, sizeof (char));
	if (outBuf == NULL)
		return NULL;

	inPtr = src;
	inLen = srcLen;
	outPtr = outBuf;

#if defined(__NetBSD__) || defined(__sun) || defined(sun)
	rc = iconv(cd, (const char **)&inPtr, &inLen, &outPtr, &outLen);
#else
	rc = iconv(cd, &inPtr, &inLen, &outPtr, &outLen);
#endif
	iconv(cd, NULL, NULL, &outPtr, &outLen); // flush
	iconv_close(cd);

	if (rc == -1)
	{
		free(outBuf);
		return NULL;
	}

	if (removeIllegalChars)
	{
		// remove illegal characters (only allow certain nordic ones)
		for (size_t i = 0; i < outLen; i++)
		{
			ch = (int8_t)outBuf[i];
			if (ch < 32 && ch != 0 && ch != -124 && ch != -108 &&
			    ch != -122 && ch != -114 && ch != -103 && ch != -113)
			{
				outBuf[i] = ' '; // character not allowed, turn it into space
			}
		}
	}

	return outBuf;
}
#endif
