#pragma once

#include <stdbool.h>

#ifdef _WIN32
#include <wchar.h>
#endif

// unicode stuff for different platforms

#ifdef _WIN32

// Windows
typedef wchar_t UNICHAR;
#define UNICHAR_STRCPY(a, b) wcscpy(a, b)
#define UNICHAR_STRNCPY(a, b, c) wcsncpy(a, b, c)
#define UNICHAR_STRCMP(a, b) wcscmp(a, b)
#define UNICHAR_STRNCMP(a, b, c) wcsncmp(a, b, c)
#define UNICHAR_STRCAT(a, b) wcscat(a, b)
#define UNICHAR_STRDUP(a) _wcsdup(a)
#define UNICHAR_FOPEN(a, b) _wfopen(a, L ## b)
#define UNICHAR_CHDIR(a) _wchdir(a)
#define UNICHAR_GETCWD(a, b) _wgetcwd(a, b)
#define UNICHAR_RENAME(a, b) _wrename(a, b)
#define UNICHAR_REMOVE(a) _wremove(a)
#define UNICHAR_STRLEN(a) wcslen(a)
#else

// other OSes
typedef char UNICHAR;
#define UNICHAR_STRCPY(a, b) strcpy(a, b)
#define UNICHAR_STRNCPY(a, b, c) strncpy(a, b, c)
#define UNICHAR_STRCMP(a, b) strcmp(a, b)
#define UNICHAR_STRNCMP(a, b, c) strncmp(a, b, c)
#define UNICHAR_STRCAT(a, b) strcat(a, b)
#define UNICHAR_STRDUP(a) strdup(a)
#define UNICHAR_FOPEN(a, b) fopen(a, b)
#define UNICHAR_CHDIR(a) chdir(a)
#define UNICHAR_GETCWD(a, b) getcwd(a, b)
#define UNICHAR_RENAME(a, b) rename(a, b)
#define UNICHAR_REMOVE(a) remove(a)
#define UNICHAR_STRLEN(a) strlen(a)
#endif

char *cp437ToUtf8(char *src);
char *utf8ToCp437(char *src, bool removeIllegalChars);
#ifdef _WIN32
UNICHAR *cp437ToUnichar(char *src);
char *unicharToCp437(UNICHAR *src, bool removeIllegalChars);
#else
#define cp437ToUnichar(a) cp437ToUtf8(a)
#define unicharToCp437(a, b) utf8ToCp437(a, b)
#endif
