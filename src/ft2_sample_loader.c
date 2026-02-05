// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_unicode.h"
#include "ft2_audio.h"
#include "ft2_sample_ed.h"
#include "ft2_mouse.h"
#include "ft2_diskop.h"
#include "ft2_structs.h"

#ifdef HAS_LIBFLAC
bool loadFLAC(FILE *f, uint32_t filesize);
#endif

bool detectBRR(FILE *f);
bool loadBRR(FILE *f, uint32_t filesize);

bool loadAIFF(FILE *f, uint32_t filesize);
bool loadIFF(FILE *f, uint32_t filesize);
bool loadRAW(FILE *f, uint32_t filesize);
bool loadWAV(FILE *f, uint32_t filesize);

enum
{
	FORMAT_UNKNOWN = 0,
	FORMAT_IFF = 1,
	FORMAT_WAV = 2,
	FORMAT_AIFF = 3,
	FORMAT_FLAC = 4,
	FORMAT_BRR = 5
};

// file extensions accepted by Disk Op. in sample mode
char *supportedSmpExtensions[] =
{
	"iff", "raw", "wav", "snd", "smp", "sam", "aif", "pat",
	"aiff","flac","brr", // IMPORTANT: Remember comma after last entry!!!

	"END_OF_LIST" // do NOT move, remove or edit this line!
};

// globals for sample loaders
bool loadAsInstrFlag, smpFilenameSet;
char *smpFilename;
uint8_t sampleSlot;
sample_t tmpSmp;
// --------------------------

static volatile bool sampleIsLoading;
static SDL_Thread *thread;

static void freeTmpSample(sample_t *s);

// Crude sample detection routine. These aren't always accurate detections!
static int8_t detectSample(FILE *f)
{
	uint8_t D[512];

	uint32_t oldPos = ftell(f);
	rewind(f);
	memset(D, 0, sizeof (D));
	fread(D, 1, sizeof (D), f);
	fseek(f, oldPos, SEEK_SET);

	if (!memcmp("fLaC", &D[0], 4)) // XXX: Kinda lousy detection...
		return FORMAT_FLAC;

	if (!memcmp("FORM", &D[0], 4) && (!memcmp("8SVX", &D[8], 4) || !memcmp("16SV", &D[8], 4)))
		return FORMAT_IFF;

	if (!memcmp("RIFF", &D[0], 4) && !memcmp("WAVE", &D[8], 4))
		return FORMAT_WAV;

	if (!memcmp("FORM", &D[0], 4) && (!memcmp("AIFF", &D[8], 4) || !memcmp("AIFC", &D[8], 4)))
		return FORMAT_AIFF;

	if (detectBRR(f))
		return FORMAT_BRR;

	return FORMAT_UNKNOWN;
}

static int32_t SDLCALL loadSampleThread(void *ptr)
{
	if (editor.tmpFilenameU == NULL)
	{
		loaderMsgBox("General I/O error during loading!");
		goto loadError;
	}

	FILE *f = UNICHAR_FOPEN(editor.tmpFilenameU, "rb");
	if (f == NULL)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		goto loadError;
	}

	int8_t format = detectSample(f);
	fseek(f, 0, SEEK_END);
	uint32_t filesize = ftell(f);

	if (filesize == 0)
	{
		fclose(f);
		loaderMsgBox("Error loading sample: The file is empty!");
		goto loadError;
	}

	bool sampleLoaded = false;

	rewind(f);
	switch (format)
	{
		case FORMAT_FLAC:
		{
#ifdef HAS_LIBFLAC
			sampleLoaded = loadFLAC(f, filesize);
#else
			loaderMsgBox("Can't load sample: Program is not compiled with FLAC support!");
#endif
		}
		break;

		case FORMAT_IFF: sampleLoaded = loadIFF(f, filesize); break;
		case FORMAT_WAV: sampleLoaded = loadWAV(f, filesize); break;
		case FORMAT_AIFF: sampleLoaded = loadAIFF(f, filesize); break;
		case FORMAT_BRR: sampleLoaded = loadBRR(f, filesize); break;
		default: sampleLoaded = loadRAW(f, filesize); break;
	}
	fclose(f);

	if (!sampleLoaded)
		goto loadError;

	// sample loaded successfully!

	if (!smpFilenameSet) // if we didn't set a custom sample name in the loader, set it to its filename
	{
		char *tmpFilename = unicharToCp850(editor.tmpFilenameU, true);
		if (tmpFilename != NULL)
		{
			int32_t i = (int32_t)strlen(tmpFilename);
			while (i--)
			{
				if (tmpFilename[i] == DIR_DELIMITER)
					break;
			}

			char *tmpPtr = tmpFilename;
			if (i > 0)
				tmpPtr += i+1;

			sanitizeFilename(tmpPtr);

			int32_t filenameLen = (int32_t)strlen(tmpPtr);
			for (i = 0; i < 22; i++)
			{
				if (i < filenameLen)
					tmpSmp.name[i] = tmpPtr[i];
				else
					tmpSmp.name[i] = '\0';
			}

			free(tmpFilename);
		}
	}

	fixString(tmpSmp.name, 21); // remove leading spaces from sample filename

	lockMixerCallback();
	if (loadAsInstrFlag) // if loaded in instrument mode
	{
		freeInstr(editor.curInstr);
		memset(song.instrName[editor.curInstr], 0, 23);
	}

	if (instr[editor.curInstr] == NULL)
		allocateInstr(editor.curInstr);

	if (instr[editor.curInstr] == NULL)
	{
		loaderMsgBox("Not enough memory!");
		goto loadError;
	}

	sample_t *s = &instr[editor.curInstr]->smp[sampleSlot];

	freeSample(editor.curInstr, sampleSlot);
	memcpy(s, &tmpSmp, sizeof (sample_t));

	sanitizeSample(s);

	fixSample(s); // prepares sample for branchless resampling interpolation
	fixInstrAndSampleNames(editor.curInstr);

	unlockMixerCallback();

	setSongModifiedFlag();

	// when caught in main/video thread, it disables busy mouse and sets sampleIsLoading to true
	editor.updateCurSmp = true;

	return true;

loadError:
	setMouseBusy(false);
	freeTmpSample(&tmpSmp);
	sampleIsLoading = false;
	return false;

	(void)ptr;
}

static void freeTmpSample(sample_t *s)
{
	freeSmpData(s);
}

void removeSampleIsLoadingFlag(void)
{
	sampleIsLoading = false;
}

bool loadSample(UNICHAR *filenameU, uint8_t smpNr, bool instrFlag)
{
	if (sampleIsLoading || filenameU == NULL)
		return false;

	// setup message box functions
	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;

	if (editor.curInstr == 0)
	{
		loaderMsgBox("The zero-instrument cannot hold instrument data!");
		return false;
	}

	sampleSlot = smpNr;
	loadAsInstrFlag = instrFlag;
	sampleIsLoading = true;
	smpFilenameSet = false;

	memset(&tmpSmp, 0, sizeof (tmpSmp));
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(loadSampleThread, "sample load thread", NULL);
	if (thread == NULL)
	{
		sampleIsLoading = false;
		loaderMsgBox("Couldn't create thread!");
		return false;
	}

	SDL_DetachThread(thread);
	return true;
}

void normalizeSigned32Bit(int32_t *sampleData, uint32_t sampleLength)
{
	uint32_t i;

	uint32_t sampleVolPeak = 0;
	for (i = 0; i < sampleLength; i++)
	{
		const uint32_t sample = ABS(sampleData[i]);
		if (sampleVolPeak < sample)
			sampleVolPeak = sample;
	}

	if (sampleVolPeak <= 0)
		return;

	const double dGain = (double)INT32_MAX / sampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		sampleData[i] = (int32_t)(sampleData[i] * dGain);
}

void normalize32BitFloatToSigned16Bit(float *fSampleData, uint32_t sampleLength)
{
	uint32_t i;

	float fSampleVolPeak = 0.0f;
	for (i = 0; i < sampleLength; i++)
	{
		const float fSample = fabsf(fSampleData[i]);
		if (fSampleVolPeak < fSample)
			fSampleVolPeak = fSample;
	}

	if (fSampleVolPeak <= 0.0f)
		return;

	const float fGain = (float)INT16_MAX / fSampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		fSampleData[i] *= fGain;
}

void normalize64BitFloatToSigned16Bit(double *dSampleData, uint32_t sampleLength)
{
	uint32_t i;

	double dSampleVolPeak = 0.0;
	for (i = 0; i < sampleLength; i++)
	{
		const double dSample = fabs(dSampleData[i]);
		if (dSampleVolPeak < dSample)
			dSampleVolPeak = dSample;
	}

	if (dSampleVolPeak <= 0.0)
		return;

	const double dGain = (double)INT16_MAX / dSampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		dSampleData[i] *= dGain;
}
