// by 8bitbubsy

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#endif
#include <stdio.h>
#include <string.h> // stricmp()
#include <stdlib.h> // atoi()
#include <ctype.h> // toupper()
#include <stdint.h>
#include <stdbool.h>

// if compiling for Windows, you need to link shlwapi.lib (for PathRemoveFileSpecW())

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define IO_BUF_SIZE 4096
#define MAX_NAME_LEN 64
#define DATA_NAME  "hdata" // this has to be a legal C variable name
#define BYTES_PER_LINE 24
#define MAX_MEGABYTES_IN 8
#define MIN_BYTES_PER_LINE 1
#define MAX_BYTES_PER_LINE 64

static const char hex2ch[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
static char fileNameOut[MAX_NAME_LEN + 8], dataType[32];
static char dataName[MAX_NAME_LEN + 1], dataNameUpper[MAX_NAME_LEN + 1];
static bool useConstFlag = true, addZeroFlag;
static size_t fileSize, bytesPerLine = BYTES_PER_LINE, dataNameLen;

#ifdef _WIN32
static void setDirToExecutablePath(void);
#endif

static void setNewDataName(const char *name);
static bool handleArguments(int argc, char *argv[]);
static bool compareFileNames(const char *path1, const char *path2);
static bool setAndTestFileSize(FILE *f);
static bool writeHeader(FILE *f);

int main(int argc, char *argv[])
{
	uint8_t byte, readBuffer[IO_BUF_SIZE], writeBuffer[IO_BUF_SIZE * 6]; // don't mess with the sizes here
	size_t lineEntry, readLength, writeLength, bytesWritten;
	FILE *fIn, *fOut;

#ifdef _WIN32 // path is not set to cwd in XP (with VS2017 runtime) for some reason
	setDirToExecutablePath();
#endif

	strcpy(dataType, "uint8_t");
	setNewDataName(DATA_NAME);

	printf("bin2h v0.3 - by 8bitbubsy 2018-2020 - Converts a binary to a C include file\n");
	printf("===========================================================================\n");

	// handle input arguments
	if (!handleArguments(argc, argv))
		return -1;

	// test if the output file is the same as the input file (illegal)
	if (compareFileNames(argv[1], fileNameOut))
	{
		printf("Error: Output file can't be the same as input file!\n");
		return 1;
	}

	// open input file
	fIn = fopen(argv[1], "rb");
	if (fIn == NULL)
	{
		printf("Error: Could not open input file for reading!\n");
		return 1;
	}

	// get filesize and test if it's 0 or too high
	if (!setAndTestFileSize(fIn))
	{
		fclose(fIn);
		return 1;
	}

	// open output file
	fOut = fopen(fileNameOut, "w");
	if (fOut == NULL)
	{
		fclose(fIn);

		printf("Error: Could not open data.h for writing!\n");
		return 1;
	}

	// write .h header
	if (!writeHeader(fOut))
		goto IOError;

	// write main data

	printf("Reading/writing from/to files...\n");

	bytesWritten = 0;
	while (!feof(fIn))
	{
		readLength = fread(readBuffer, 1, IO_BUF_SIZE, fIn);

		writeLength = 0;
		for (size_t i = 0; i < readLength; i++)
		{
			lineEntry = bytesWritten % bytesPerLine;

			// put tab if we're at the start of a line
			if (lineEntry == 0)
				writeBuffer[writeLength++] = '\t';

			// write table entry

			byte = readBuffer[i];
			if (bytesWritten == fileSize-1)
			{
				// last byte in table

				if (addZeroFlag) // fileSize is increased by one if addZeroFlag = true
				{
					writeBuffer[writeLength++] = '\0';
				}
				else
				{
					// "0xXX"
					writeBuffer[writeLength++] = '0';
					writeBuffer[writeLength++] = 'x';
					writeBuffer[writeLength++] = hex2ch[byte >> 4];
					writeBuffer[writeLength++] = hex2ch[byte & 15];
				}
			}
			else
			{
				// "0xXX,"
				writeBuffer[writeLength++] = '0';
				writeBuffer[writeLength++] = 'x';
				writeBuffer[writeLength++] = hex2ch[byte >> 4];
				writeBuffer[writeLength++] = hex2ch[byte & 15];
				writeBuffer[writeLength++] = ',';
			}

			// put linefeed if we're at the end of a line (or at the last byte entry)
			if (lineEntry == bytesPerLine-1 || bytesWritten == fileSize-1)
				writeBuffer[writeLength++] = '\n';

			bytesWritten++;
		}

		if (fwrite(writeBuffer, 1, writeLength, fOut) != writeLength)
			goto IOError;
	}

	if (bytesWritten != fileSize)
		goto IOError;

	// write .h footer
	if (fprintf(fOut, "};\n") < 0) goto IOError;

	fclose(fOut);
	fclose(fIn);

	printf("%d bytes sucessfully written to \"%s\".\n", fileSize, fileNameOut);
	return 0;

IOError:
	fclose(fOut);
	fclose(fIn);

	printf("Error: General I/O fault during reading/writing!\n");
	return 1;
}

#ifdef _WIN32
static void setDirToExecutablePath(void)
{
	WCHAR path[MAX_PATH];

	GetModuleFileNameW(GetModuleHandleW(NULL), path, MAX_PATH);
	PathRemoveFileSpecW(path);
	SetCurrentDirectoryW(path);

	// we don't care if this fails or not, so no error checking for now
}
#endif

static bool dataNameIsValid(const char *s, size_t len)
{
	bool charIsValid;

	for (size_t i = 0; i < len; i++)
	{
		if (i == 0 && (s[i] >= '0' && s[i] <= '9'))
			return false; // a C variable can't start with a digit

		charIsValid = (s[i] >= '0' && s[i] <= '9') ||
		              (s[i] >= 'a' && s[i] <= 'z') ||
		              (s[i] >= 'A' && s[i] <= 'Z');

		if (!charIsValid)
			return false;
	}

	return true;
}

static void setNewDataName(const char *name)
{
	size_t i;

	strncpy(dataName, name, sizeof (dataName)-1);
	dataNameLen = strlen(dataName);
	sprintf(fileNameOut, "%s.h", dataName);

	// maker upper case version of dataName
	for (i = 0; i < dataNameLen; i++)
		dataNameUpper[i] = (char)toupper(dataName[i]);
	dataNameUpper[i] = '\0';

	// test if dataName is valid for use in a C header
	if (!dataNameIsValid(dataName, dataNameLen))
	{
		strcpy(dataName, DATA_NAME);
		printf("Warning: Data name was illegal and was set to default (%s).\n", DATA_NAME);
	}
}

static bool handleArguments(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Usage: bin2h <binary> [options]\n");
		printf("\n");
		printf("Options:\n");
		printf(" --no-const   Don't declare data as const\n");
		printf(" --signed     Declare data as signed instead of unsigned\n");
		printf(" --terminate  Insert a zero after the data (also increases length constant by 1)\n");
		printf(" -n <name>    Set data name (max %d chars, a-zA-Z0-9_, default = %s)\n", MAX_NAME_LEN, DATA_NAME);
		printf(" -b <num>     Set amount of bytes per line in data table (%d..%d, default = %d)\n",
			MIN_BYTES_PER_LINE, MAX_BYTES_PER_LINE, BYTES_PER_LINE);

		return false;
	}
	else
	{
		for (int32_t i = 0; i < argc; i++)
		{
			if (!_stricmp(argv[i], "--no-const"))
			{
				useConstFlag = false;
			}
			if (!_stricmp(argv[i], "--terminate"))
			{
				addZeroFlag = true;
			}
			if (!_stricmp(argv[i], "--signed"))
			{
				strcpy(dataType, "int8_t");
			}
			else if (!_stricmp(argv[i], "-n"))
			{
				if (i < argc-1)
					setNewDataName(argv[i+1]);
			}
			else if (!_stricmp(argv[i], "-b"))
			{
				if (i < argc-1)
					bytesPerLine = CLAMP(atoi(argv[i+1]), MIN_BYTES_PER_LINE, MAX_BYTES_PER_LINE);
			}
		}
	}

	return true;
}

// compares two filenames (mixed full path or not, case insensitive)
static bool compareFileNames(const char *path1, const char *path2)
{
	char sep;
	const char *p1, *p2;
	size_t l1, l2;

	if (path1 == NULL || path2 == NULL)
		return false; // error

#ifdef _WIN32
	sep = '\\';
#else
	sep = '/';
#endif

	// set pointers to first occurrences of separator in paths
	p1 = strrchr(path1, sep);
	p2 = strrchr(path2, sep);

	if (p1 == NULL)
	{
		// separator not found, point to start of path
		p1 = path1;
		l1 = strlen(p1);
	}
	else
	{
		// separator found, increase pointer by 1 if we have room
		l1 = strlen(p1);
		if (l1 > 0)
		{
			p1++;
			l1--;
		}
	}

	if (p2 == NULL)
	{
		// separator not found, point to start of path
		p2 = path2;
		l2 = strlen(p2);
	}
	else
	{
		// separator found, increase pointer by 1 if we have room
		l2 = strlen(p2);
		if (l2 > 0)
		{
			p2++;
			l2--;
		}
	}

	if (l1 != l2)
		return false; // definitely not the same

	// compare strings
	for (size_t i = 0; i < l1; i++)
	{
		if (tolower(p1[i]) != tolower(p2[i]))
			return false; // not the same
	}

	return true; // both filenames are the same
}

static bool setAndTestFileSize(FILE *f)
{
	fseek(f, 0, SEEK_END);
	fileSize = ftell(f);
	rewind(f);

	if (fileSize == 0)
	{
		printf("Error: Input file is empty!\n");
		return false;
	}

	if (fileSize > MAX_MEGABYTES_IN*(1024UL*1024))
	{
		printf("Error: The input filesize is over %dMB. This program is meant for small files!\n", MAX_MEGABYTES_IN);
		return false;
	}

	if (addZeroFlag)
		fileSize++; // make room for zero

	return true;
}

static bool writeHeader(FILE *f)
{
	if (fprintf(f, "#pragma once\n") < 0) return false;
	if (fprintf(f, "\n") < 0) return false;
	if (fprintf(f, "#include <stdint.h>\n") < 0) return false;
	if (fprintf(f, "\n") < 0) return false;
	if (fprintf(f, "#define %s_LENGTH %d\n", dataNameUpper, fileSize) < 0) return false;
	if (fprintf(f, "\n") < 0) return false;

	if (useConstFlag)
	{
		if (fprintf(f, "const %s %s[%s_LENGTH] =\n", dataType, dataName, dataNameUpper) < 0)
			return false;
	}
	else
	{
		if (fprintf(f, "%s %s[%s_LENGTH] =\n", dataType, dataName, dataNameUpper) < 0)
			return false;
	}

	if (fprintf(f, "{\n") < 0)
		return false;

	return true;
}
