#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TMP_FILENAME "TMP.HLP"
#define DATA_LINE_SIZE 12

static void delTmpPackFile(void)
{
	char command[256];

#ifdef _WIN32
	sprintf(command, "del %s 2>NUL", TMP_FILENAME);
#else
	sprintf(command, "rm %s &> /dev/null", TMP_FILENAME);
#endif

	system(command);
}

int main(int argc, char *argv[])
{
	char *helpData, *textPtr2, *textPtr;
	uint8_t textLenByte;
	uint32_t textLen, fileSize, currLine, totalCharLen;
	FILE *fIn, *fOut;

	printf("ft2hlp_to_h - by 8bitbubsy\n");

#ifndef _DEBUG
	if (argc != 2)
	{
		printf("Usage: ft2hlp_to_h <helpfile.hlp>\n");
		return -1;
	}

	fIn = fopen(argv[1], "rb");
#else
	fIn = fopen("FT2.HLP", "rb");
#endif

	if (fIn == NULL)
	{
		printf("Error: Could not open input file for reading!\n");
		return 1;
	}

	fOut = fopen(TMP_FILENAME, "wb");
	if (fOut == NULL)
	{
		printf("Error: Could not open temp file for writing!\n");
		fclose(fIn);

		return 1;
	}

	fseek(fIn, 0, SEEK_END);
	fileSize = ftell(fIn);
	rewind(fIn);

	helpData = (char *)malloc(fileSize);
	if (helpData == NULL)
	{
		printf("ERROR: Out of memory!\n");
		fclose(fIn);
		fclose(fOut);
		return -1;
	}

	fread(helpData, 1, fileSize, fIn);
	fclose(fIn);

	printf("Parsing...\n");

	totalCharLen = 0;
	currLine = 0;

	textPtr = helpData;
	while (totalCharLen < fileSize)
	{
		if (!strncmp(textPtr, "END", 3) && totalCharLen >= fileSize-(3+2))
		{
			/* we reached the final END */
			fputc(3, fOut);
			fprintf(fOut, "END");
			break;
		}

		textPtr2 = textPtr;

		textLen = 0;
		while (*textPtr2 != '\r')
		{
			textLen++;
			textPtr2++;
		}

		if (textLen > 255)
		{
			printf("ERROR: Line %d has more than 255 characters!\n", currLine);
			fclose(fOut);
			return 1;
		}

		textLenByte = textLen & 0xFF;
		fwrite(&textLenByte, 1, 1, fOut);
		fwrite(textPtr, 1, textLen, fOut);

		totalCharLen += textLen + 2;
		textPtr += textLen + 2;

		currLine++;
	}

	printf("Successfully parsed %d lines.\n", currLine - 1);
	fclose(fOut);

	fIn = fopen(TMP_FILENAME, "rb");
	if (fIn == NULL)
	{
		printf("Error: Could not open temp file for reading!\n");
		delTmpPackFile();
		return 1;
	}

	fOut = fopen("ft2_help_data.h", "w");
	if (fOut == NULL)
	{
		printf("Error: Could not open ft2_help_data.h for writing!\n");
		fclose(fIn);
		delTmpPackFile();
		return 1;
	}

	// fetch file size
	fseek(fIn, 0, SEEK_END);
	fileSize = ftell(fIn);
	rewind(fIn);

	printf("Converting data to header bytes..\n");

	// put header
	fprintf(fOut, "#ifndef __FT2_HELP_DATA_H\n");
	fprintf(fOut, "#define __FT2_HELP_DATA_H\n");
	fprintf(fOut, "\n");
	fprintf(fOut, "#include <stdint.h>\n");
	fprintf(fOut, "\n");
	fprintf(fOut, "#define HELP_DATA_LEN %d\n", fileSize);
	fprintf(fOut, "\n");
	fprintf(fOut, "const uint8_t helpData[%d] =\n", fileSize);
	fprintf(fOut, "{\n");

	// put data
	for (uint32_t i = 0; i < fileSize; i++)
	{
		if ((i % DATA_LINE_SIZE) == 0) fprintf(fOut, "\t"); // tab

		if (i == fileSize-1)
			fprintf(fOut, "0x%02X", fgetc(fIn));
		else
			fprintf(fOut, "0x%02X,", fgetc(fIn));

		if ((i % DATA_LINE_SIZE) == DATA_LINE_SIZE-1 || i == fileSize-1)
			fputc(0x0A, fOut); // linefeed
	}

	// put footer
	fprintf(fOut, "};\n");
	fprintf(fOut, "\n");
	fprintf(fOut, "#endif\n");

	fclose(fOut);
	fclose(fIn);

	printf("Done! %d bytes sucessfully written to ft2_help_data.h.\n", fileSize);
	delTmpPackFile();
	
	return 0;
}
