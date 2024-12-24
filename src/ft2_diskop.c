// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <shlwapi.h>
#include <windows.h>
#include <direct.h>
#include <shlobj.h> // SHGetFolderPathW()
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#ifndef __EMSCRIPTEN__
#include <fts.h> // for recursiveDelete()
#endif
#include <unistd.h>
#include <dirent.h>
#endif
#include <wchar.h>
#include <sys/stat.h>
#include "ft2_header.h"
#include "ft2_unicode.h"
#include "ft2_config.h"
#include "ft2_mouse.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_saver.h"
#include "ft2_diskop.h"
#include "ft2_wav_renderer.h"
#include "ft2_module_loader.h"
#include "ft2_module_saver.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_inst_ed.h"
#include "ft2_structs.h"

// hide POSIX warnings for chdir()
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#define FILENAME_TEXT_X 170
#define FILESIZE_TEXT_X 295
#define DISKOP_MAX_DRIVE_BUTTONS 8

#ifdef _WIN32
#define PARENT_DIR_STR L".."
static HANDLE hFind;
#else
#define PARENT_DIR_STR ".."
static DIR *hFind;
#endif

// "look for file" flags
enum
{
	LFF_DONE = 0,
	LFF_SKIP = 1,
	LFF_OK = 2
};

typedef struct DirRec
{
	UNICHAR *nameU;
	bool isDir;
	int32_t filesize;
} DirRec;

static char FReq_SysReqText[256], *FReq_FileName, *FReq_NameTemp;
static char *modTmpFName, *insTmpFName, *smpTmpFName, *patTmpFName, *trkTmpFName;
static char *modTmpFNameUTF8; // for window title
static uint8_t FReq_Item;
static bool FReq_ShowAllFiles, insPathSet, smpPathSet, patPathSet, trkPathSet, firstTimeOpeningDiskOp = true;
static int32_t FReq_EntrySelected = -1, FReq_FileCount, FReq_DirPos, lastMouseY;
static UNICHAR *FReq_CurPathU, *FReq_ModCurPathU, *FReq_InsCurPathU, *FReq_SmpCurPathU, *FReq_PatCurPathU, *FReq_TrkCurPathU;
static DirRec *FReq_Buffer;
static SDL_Thread *thread;

static void setDiskOpItem(uint8_t item);

bool setupExecutablePath(void)
{
	editor.binaryPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	if (editor.binaryPathU == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	editor.binaryPathU[0] = 0;
	UNICHAR_GETCWD(editor.binaryPathU, PATH_MAX);

	return true;
}

int32_t getFileSize(UNICHAR *fileNameU) // returning -1 = filesize over 2GB
{
	int64_t fSize;

#ifdef _WIN32
	FILE *f = UNICHAR_FOPEN(fileNameU, "rb");
	if (f == NULL)
		return 0;

	_fseeki64(f, 0, SEEK_END);
	fSize = _ftelli64(f);
	fclose(f);
#else
	struct stat st;
	if (stat(fileNameU, &st) != 0)
		return 0;

	fSize = (int64_t)st.st_size;
#endif
	if (fSize < 0)
		fSize = 0;

	if (fSize > INT32_MAX)
		return -1; // -1 = ">2GB" flag

	return (int32_t)fSize;
}

uint8_t getDiskOpItem(void)
{
	return FReq_Item;
}

char *getCurrSongFilename(void) // for window title
{
	return modTmpFNameUTF8;
}

void updateCurrSongFilename(void) // for window title
{
	if (modTmpFNameUTF8 != NULL)
	{
		free(modTmpFNameUTF8);
		modTmpFNameUTF8 = NULL;
	}

	if (modTmpFName == NULL)
		return;

	modTmpFNameUTF8 = cp850ToUtf8(modTmpFName);
}

// drive buttons for Windows
#ifdef _WIN32
static char logicalDriveNames[26][3] = 
{
	"A:", "B:", "C:", "D:", "E:", "F:", "G:", "H:", "I:", "J:", "K:", "L:", "M:",
	"N:", "O:", "P:", "Q:", "R:", "S:", "T:", "U:", "V:", "W:", "X:", "Y:", "Z:"
};
static uint32_t numLogicalDrives;
static uint32_t driveIndexes[DISKOP_MAX_DRIVE_BUTTONS];
#endif

char *getDiskOpFilename(void)
{
	return FReq_FileName;
}

const UNICHAR *getDiskOpCurPath(void)
{
	return FReq_CurPathU;
}

const UNICHAR *getDiskOpModPath(void)
{
	return FReq_ModCurPathU;
}

const UNICHAR *getDiskOpSmpPath(void)
{
	return FReq_SmpCurPathU;
}

static void setupInitialPaths(void)
{
	// the UNICHAR paths are already zeroed out

#ifdef _WIN32
	if (config.modulesPath[0] != '\0')
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, config.modulesPath, -1, FReq_ModCurPathU, 80);
		FReq_ModCurPathU[80] = 0;
	}

	if (config.instrPath[0] != '\0')
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, config.instrPath, -1, FReq_InsCurPathU, 80);
		FReq_InsCurPathU[80] = 0;
		insPathSet = true;
	}

	if (config.samplesPath[0] != '\0')
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, config.samplesPath, -1, FReq_SmpCurPathU, 80);
		FReq_SmpCurPathU[80] = 0;
		smpPathSet = true;
	}

	if (config.patternsPath[0] != '\0')
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, config.patternsPath, -1, FReq_PatCurPathU, 80);
		FReq_PatCurPathU[80] = 0;
		patPathSet = true;
	}

	if (config.tracksPath[0] != '\0')
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, config.tracksPath, -1, FReq_TrkCurPathU, 80);
		FReq_TrkCurPathU[80] = 0;
		trkPathSet = true;
	}
#else
	if (config.modulesPath[0] != '\0')
	{
		strncpy(FReq_ModCurPathU, config.modulesPath, 80);
		FReq_ModCurPathU[80] = 0;
	}

	if (config.instrPath[0] != '\0')
	{
		strncpy(FReq_InsCurPathU, config.instrPath, 80);
		FReq_InsCurPathU[80] = 0;
		insPathSet = true;
	}

	if (config.samplesPath[0] != '\0')
	{
		strncpy(FReq_SmpCurPathU, config.samplesPath, 80);
		FReq_SmpCurPathU[80] = 0;
		smpPathSet = true;
	}

	if (config.patternsPath[0] != '\0')
	{
		strncpy(FReq_PatCurPathU, config.patternsPath, 80);
		FReq_PatCurPathU[80] = 0;
		patPathSet = true;
	}

	if (config.tracksPath[0] != '\0')
	{
		strncpy(FReq_TrkCurPathU, config.tracksPath, 80);
		FReq_TrkCurPathU[80] = 0;
		trkPathSet = true;
	}
#endif
}

static void freeDirRecBuffer(void)
{
	if (FReq_Buffer != NULL)
	{
		for (int32_t i = 0; i < FReq_FileCount; i++)
		{
			if (FReq_Buffer[i].nameU != NULL)
				free(FReq_Buffer[i].nameU);
		}

		free(FReq_Buffer);
		FReq_Buffer = NULL;
	}

	FReq_FileCount = 0;
}

void freeDiskOp(void)
{
	if (editor.tmpFilenameU != NULL)
	{
		free(editor.tmpFilenameU);
		editor.tmpFilenameU = NULL;
	}

	if (editor.tmpInstrFilenameU != NULL)
	{
		free(editor.tmpInstrFilenameU);
		editor.tmpInstrFilenameU = NULL;
	}

	if (modTmpFName != NULL) { free(modTmpFName); modTmpFName = NULL; }
	if (insTmpFName != NULL) { free(insTmpFName); insTmpFName = NULL; }
	if (smpTmpFName != NULL) { free(smpTmpFName); smpTmpFName = NULL; }
	if (patTmpFName != NULL) { free(patTmpFName); patTmpFName = NULL; }
	if (trkTmpFName != NULL) { free(trkTmpFName); trkTmpFName = NULL; }
	if (FReq_NameTemp != NULL) { free(FReq_NameTemp); FReq_NameTemp = NULL; }
	if (FReq_ModCurPathU != NULL) { free(FReq_ModCurPathU); FReq_ModCurPathU = NULL; }
	if (FReq_InsCurPathU != NULL) { free(FReq_InsCurPathU); FReq_InsCurPathU = NULL; }
	if (FReq_SmpCurPathU != NULL) { free(FReq_SmpCurPathU); FReq_SmpCurPathU = NULL; }
	if (FReq_PatCurPathU != NULL) { free(FReq_PatCurPathU); FReq_PatCurPathU = NULL; }
	if (FReq_TrkCurPathU != NULL) { free(FReq_TrkCurPathU); FReq_TrkCurPathU = NULL; }
	if (modTmpFNameUTF8 != NULL) { free(modTmpFNameUTF8); modTmpFNameUTF8 = NULL; }

	freeDirRecBuffer();
}

bool setupDiskOp(void)
{
	modTmpFName = (char *)malloc((PATH_MAX + 1) * sizeof (char));
	insTmpFName = (char *)malloc((PATH_MAX + 1) * sizeof (char));
	smpTmpFName = (char *)malloc((PATH_MAX + 1) * sizeof (char));
	patTmpFName = (char *)malloc((PATH_MAX + 1) * sizeof (char));
	trkTmpFName = (char *)malloc((PATH_MAX + 1) * sizeof (char));
	FReq_NameTemp = (char *)malloc((PATH_MAX + 1) * sizeof (char));

	FReq_ModCurPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	FReq_InsCurPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	FReq_SmpCurPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	FReq_PatCurPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));
	FReq_TrkCurPathU = (UNICHAR *)malloc((PATH_MAX + 1) * sizeof (UNICHAR));

	if (modTmpFName      == NULL || insTmpFName      == NULL || smpTmpFName      == NULL ||
		patTmpFName      == NULL || trkTmpFName      == NULL || FReq_NameTemp    == NULL ||
		FReq_ModCurPathU == NULL || FReq_InsCurPathU == NULL || FReq_SmpCurPathU == NULL ||
		FReq_PatCurPathU == NULL || FReq_TrkCurPathU == NULL)
	{
		// allocated memory is free'd lateron
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// clear first entry of strings
	modTmpFName[0] = '\0';
	insTmpFName[0] = '\0';
	smpTmpFName[0] = '\0';
	patTmpFName[0] = '\0';
	trkTmpFName[0] = '\0';
	FReq_NameTemp[0] = '\0';
	FReq_ModCurPathU[0] = 0;
	FReq_InsCurPathU[0] = 0;
	FReq_SmpCurPathU[0] = 0;
	FReq_PatCurPathU[0] = 0;
	FReq_TrkCurPathU[0] = 0;

	strcpy(modTmpFName, "untitled.xm");
	strcpy(insTmpFName, "untitled.xi");
	strcpy(smpTmpFName, "untitled.wav");
	strcpy(patTmpFName, "untitled.xp");
	strcpy(trkTmpFName, "untitled.xt");

	setupInitialPaths();
	setDiskOpItem(0);

	updateCurrSongFilename(); // for window title
	updateWindowTitle(true);

	return true;
}

int32_t getExtOffset(char *s, int32_t stringLen) // get byte offset of file extension (last '.')
{
	if (s == NULL || stringLen < 1)
		return -1;

	for (int32_t i = stringLen - 1; i >= 0; i--)
	{
		if (i != 0 && s[i] == '.')
			return i;
	}

	return -1;
}

static void removeQuestionmarksFromString(char *s)
{
	if (s == NULL || *s == '\0')
		return;

	const int32_t len = (int32_t)strlen(s);
	for (int32_t i = 0; i < len; i++)
	{
		if (s[i] == '?')
			s[i] = ' ' ;
	}
}

#ifdef _WIN32 // WINDOWS SPECIFIC FILE OPERATION ROUTINES

bool fileExistsAnsi(char *str)
{
	UNICHAR *strU = cp850ToUnichar(str);
	if (strU == NULL)
		return false;

	bool retVal = PathFileExistsW(strU);
	free(strU);

	return retVal;
}

static bool deleteDirRecursive(UNICHAR *strU)
{
	SHFILEOPSTRUCTW shfo;

	memset(&shfo, 0, sizeof (shfo));
	shfo.wFunc = FO_DELETE;
	shfo.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
	shfo.pFrom = strU;

	return (SHFileOperationW(&shfo) == 0);
}

static bool makeDirAnsi(char *str)
{
	UNICHAR *strU = cp850ToUnichar(str);
	if (strU == NULL)
		return false;

	int32_t retVal = _wmkdir(strU);
	free(strU);

	return (retVal == 0);
}

static bool renameAnsi(UNICHAR *oldNameU, char *newName)
{
	UNICHAR *newNameU = cp850ToUnichar(newName);
	if (newNameU == NULL)
		return false;

	int32_t retVal = UNICHAR_RENAME(oldNameU, newNameU);
	free(newNameU);

	return (retVal == 0);
}

static void setupDiskOpDrives(void) // Windows only
{
	fillRect(134, 29, 31, 111, PAL_DESKTOP);
	numLogicalDrives = 0;

	// get number of drives and drive names
	const uint32_t drivesBitmask = GetLogicalDrives();
	for (int32_t i = 0; i < 8*sizeof (uint32_t); i++)
	{
		if ((drivesBitmask & (1 << i)) != 0)
		{
			driveIndexes[numLogicalDrives++] = i;
			if (numLogicalDrives == DISKOP_MAX_DRIVE_BUTTONS)
				break;
		}
	}

	// hide all buttons
	for (uint16_t i = 0; i < DISKOP_MAX_DRIVE_BUTTONS; i++)
		hidePushButton(PB_DISKOP_DRIVE1 + i);

	// set button names and show buttons
	for (uint16_t i = 0; i < numLogicalDrives; i++)
	{
		pushButtons[PB_DISKOP_DRIVE1 + i].caption = logicalDriveNames[driveIndexes[i]];
		showPushButton(PB_DISKOP_DRIVE1 + i);
	}
}

static void openDrive(char *str) // Windows only
{
	if (mouse.mode == MOUSE_MODE_DELETE)
	{
		okBox(0, "System message", "Drive deletion is not implemented!", NULL);
		return;
	}

	if (str == NULL || *str == '\0')
	{
		okBox(0, "System message", "Couldn't open drive!", NULL);
		return;
	}

	if (chdir(str) != 0)
		okBox(0, "System message", "Couldn't open drive! Please make sure there's a disk in it.", NULL);
	else
		editor.diskOpReadDir = true;
}

#else // NON-WINDOWS SPECIFIC FILE OPERATION ROUTINES

bool fileExistsAnsi(char *str)
{
	UNICHAR *strU = cp850ToUnichar(str);
	if (strU == NULL)
		return false;

	int32_t retVal = access(strU, F_OK);
	free(strU);

	return (retVal != -1);
}

static bool deleteDirRecursive(UNICHAR *strU)
{
#ifdef __EMSCRIPTEN__
	bool ret = false;
	pid_t child;

	child = fork();
	if (child == 0) {
		// child
		static const char *EXEC_RM = "/bin/rm";
		const char *argv[] = { "-rf", "--", (const char*)strU, NULL };

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		execv(EXEC_RM, (char *const*)argv);

		perror(EXEC_RM);
		exit(errno == ENOENT ? 126 : 127);
	}
	else if (child > 0) {
		// parent
		int wstatus;

		waitpid(child, &wstatus, 0);
		ret = WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
	}
	else {
		// error
	}

	return ret;
#else
	FTSENT *curr;
	char *files[] = { (char *)(strU), NULL };

	FTS *ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
	if (!ftsp)
		return false;

	bool ret = true;
	while ((curr = fts_read(ftsp)))
	{
		switch (curr->fts_info)
		{
			default:
			case FTS_NS:
			case FTS_DNR:
			case FTS_ERR:
				ret = false;
			break;

			case FTS_D:
			case FTS_DC:
			case FTS_DOT:
			case FTS_NSOK:
				break;

			case FTS_DP:
			case FTS_F:
			case FTS_SL:
			case FTS_SLNONE:
			case FTS_DEFAULT:
			{
				if (remove(curr->fts_accpath) < 0)
					ret = false;
			}
			break;
		}
	}

	if (ftsp != NULL)
		fts_close(ftsp);

	return ret;
#endif
}

static bool makeDirAnsi(char *str)
{
	UNICHAR *strU = cp850ToUnichar(str);
	if (strU == NULL)
		return false;

	int32_t retVal = mkdir(str, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	free(strU);

	return (retVal == 0);
}

static bool renameAnsi(UNICHAR *oldNameU, char *newName)
{
	int32_t retVal;
	UNICHAR *newNameU;

	newNameU = cp850ToUnichar(newName);
	if (newNameU == NULL)
		return false;

	retVal = UNICHAR_RENAME(oldNameU, newNameU);
	free(newNameU);

	return (retVal == 0);
}
#endif

static void openDirectory(UNICHAR *strU)
{
	if (strU == NULL || UNICHAR_STRLEN(strU) == 0)
	{
		okBox(0, "System message", "Couldn't open directory! No permission or in use?", NULL);
		return;
	}

	if (UNICHAR_CHDIR(strU) != 0)
		okBox(0, "System message", "Couldn't open directory! No permission or in use?", NULL);
	else
		editor.diskOpReadDir = true;
}

bool diskOpGoParent(void)
{
	if (chdir("..") == 0)
	{
		editor.diskOpReadDir = true;
		FReq_EntrySelected = -1;

		return true;
	}

	return false;
}

static char *getFilenameFromPath(char *p)
{
	int32_t i;

	if (p == NULL || p[0] == '\0')
		return p;

	const int32_t len = (int32_t)strlen(p);
	if (len < 2 || p[len-1] == DIR_DELIMITER)
		return p;

	// search for last directory delimiter
	for (i = len - 1; i >= 0; i--)
	{
		if (p[i] == DIR_DELIMITER)
			break;
	}

	if (i != 0)
		p += i+1; // we found a directory delimiter - skip to the last one

	return p;
}

void sanitizeFilename(const char *src)
{
	// some of these are legal on GNU/Linux and macOS, but whatever...
	const char illegalChars[] = "\\/:*?\"<>|";
	char *ptr;

	if (src == NULL || src[0] == '\0')
		return;

	// convert illegal characters to space (for making a filename the OS will accept)
	while ((ptr = (char *)strpbrk(src, illegalChars)) != NULL)
		*ptr = ' ';
}

void diskOpSetFilename(uint8_t type, UNICHAR *pathU)
{
	char *ansiPath = unicharToCp850(pathU, true);
	if (ansiPath == NULL)
		return;

	char *filename = getFilenameFromPath(ansiPath);
	uint32_t filenameLen = (uint32_t)strlen(filename);

	if (filenameLen > PATH_MAX)
	{
		free(ansiPath);
		return; // filename is too long, don't bother to copy it over
	}

	sanitizeFilename(filename);

	switch (type)
	{
		default:
		case DISKOP_ITEM_MODULE:
		{
			strcpy(modTmpFName, filename);
			updateCurrSongFilename(); // for window title

			if (editor.moduleSaveMode == MOD_SAVE_MODE_MOD)
				changeFilenameExt(modTmpFName, ".mod", PATH_MAX);
			else if (editor.moduleSaveMode == MOD_SAVE_MODE_XM)
				changeFilenameExt(modTmpFName, ".xm", PATH_MAX);
			else if (editor.moduleSaveMode == MOD_SAVE_MODE_WAV)
				changeFilenameExt(modTmpFName, ".wav", PATH_MAX);

			updateWindowTitle(true);
		}
		break;

		case DISKOP_ITEM_INSTR:
			strcpy(insTmpFName, filename);
		break;

		case DISKOP_ITEM_SAMPLE:
		{
			strcpy(smpTmpFName, filename);

			if (editor.sampleSaveMode == SMP_SAVE_MODE_RAW)
				changeFilenameExt(smpTmpFName, ".raw", PATH_MAX);
			else if (editor.sampleSaveMode == SMP_SAVE_MODE_IFF)
				changeFilenameExt(smpTmpFName, ".iff", PATH_MAX);
			else if (editor.sampleSaveMode == SMP_SAVE_MODE_WAV)
				changeFilenameExt(smpTmpFName, ".wav", PATH_MAX);
		}
		break;

		case DISKOP_ITEM_PATTERN:
			strcpy(patTmpFName, filename);
		break;

		case DISKOP_ITEM_TRACK:
			strcpy(trkTmpFName, filename);
		break;
	}

	free(ansiPath);

	if (ui.diskOpShown)
		drawTextBox(TB_DISKOP_FILENAME);
}

static void openFile(UNICHAR *filenameU, bool songModifiedCheck)
{
	// first check if we can actually open the requested file
	FILE *f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
	{
		okBox(0, "System message", "Couldn't open file/directory! No permission or in use?", NULL);
		return;
	}
	fclose(f);

	const int32_t filesize = getFileSize(filenameU);
	if (filesize == -1) // >2GB
	{
		okBox(0, "System message", "The file is too big and can't be loaded (over 2GB).", NULL);
		return;
	}

	if (filesize >= 128L*1024*1024) // 128MB
	{
		if (okBox(2, "System request", "Are you sure you want to load such a big file?", NULL) != 1)
			return;
	}

	// file is readable, handle file...
	switch (FReq_Item)
	{
		default:
		case DISKOP_ITEM_MODULE:
		{
			if (songModifiedCheck && song.isModified)
			{
				// remove file selection before okBox() opens up
				FReq_EntrySelected = -1;
				diskOp_DrawFilelist();

				if (okBox(2, "System request", "You have unsaved changes in your song. Load new song and lose all changes?", NULL) != 1)
					return;
			}

			editor.loadMusicEvent = EVENT_LOADMUSIC_DISKOP;
			loadMusic(filenameU);
		}
		break;

		case DISKOP_ITEM_INSTR:
			loadInstr(filenameU);
		break;

		case DISKOP_ITEM_SAMPLE:
			loadSample(filenameU, editor.curSmp, false);
		break;

		case DISKOP_ITEM_PATTERN:
			loadPattern(filenameU);
		break;

		case DISKOP_ITEM_TRACK:
			loadTrack(filenameU);
		break;
	}
}

static void removeFilenameExt(char *name)
{
	if (name == NULL || *name == '\0')
		return;

	const int32_t len = (int32_t)strlen(name);

	const int32_t extOffset = getExtOffset(name, len);
	if (extOffset != -1)
		name[extOffset] = '\0';
}

void changeFilenameExt(char *name, char *ext, int32_t nameMaxLen)
{
	if (name == NULL || name[0] == '\0' || ext == NULL)
		return;

	removeFilenameExt(name);

	const int32_t len = (int32_t)strlen(name);
	int32_t extLen = (int32_t)strlen(ext);

	if (len+extLen > nameMaxLen)
		extLen = nameMaxLen-len;

	strncat(name, ext, extLen);

	if (ui.diskOpShown)
		diskOp_DrawDirectory();
}

void diskOpChangeFilenameExt(char *ext)
{
	changeFilenameExt(FReq_FileName, ext, PATH_MAX);
}

void trimEntryName(char *name, bool isDir)
{
	char extBuffer[24];

	int32_t j = (int32_t)strlen(name);
	const int32_t extOffset = getExtOffset(name, j);
	int32_t extLen = (int32_t)strlen(&name[extOffset]);
	j--;

	if (isDir)
	{
		// directory
		while (textWidth(name) > 160-8 && j >= 2)
		{
			name[j-2] = '.';
			name[j-1] = '.';
			name[j-0] = '\0';
			j--;
		}

		return;
	}

	if (extOffset != -1 && extLen <= 4)
	{
		// has extension
		sprintf(extBuffer, ".. %s", &name[extOffset]); // "testtestte... .xm"

		extLen = (int32_t)strlen(extBuffer);
		while (textWidth(name) >= FILESIZE_TEXT_X-FILENAME_TEXT_X && j >= extLen+1)
		{
			memcpy(&name[j - extLen], extBuffer, extLen + 1);
			j--;
		}
	}
	else
	{
		// no extension
		while (textWidth(name) >= FILESIZE_TEXT_X-FILENAME_TEXT_X && j >= 2)
		{
			name[j-2] = '.';
			name[j-1] = '.';
			name[j-0] = '\0';
			j--;
		}
	}
}

void createFileOverwriteText(char *filename, char *buffer)
{
	char nameTmp[128];

	// read entry name to a small buffer
	const uint32_t nameLen = (uint32_t)strlen(filename);
	memcpy(nameTmp, filename, (nameLen >= sizeof (nameTmp)) ? sizeof (nameTmp) : (nameLen + 1));
	nameTmp[sizeof (nameTmp) - 1] = '\0';

	trimEntryName(nameTmp, false);

	sprintf(buffer, "Overwrite file \"%s\"?", nameTmp);
}

static void diskOpSave(bool checkOverwrite)
{
	UNICHAR *fileNameU;

	if (FReq_FileName[0] == '\0')
	{
		okBox(0, "System message", "Filename can't be empty!", NULL);
		return;
	}

	// test if the very first character has a dot...
	if (FReq_FileName[0] == '.')
	{
		okBox(0, "System message", "The very first character in the filename can't be '.' (dot)!", NULL);
		return;
	}

	// test for illegal file name
	if (FReq_FileName[0] == '\0' || strpbrk(FReq_FileName, "\\/:*?\"<>|") != NULL)
	{
		okBox(0, "System message", "The filename can't contain the following characters: \\ / : * ? \" < > |", NULL);
		return;
	}

	switch (FReq_Item)
	{
		default:
		case DISKOP_ITEM_MODULE:
		{
			switch (editor.moduleSaveMode)
			{
				         case MOD_SAVE_MODE_MOD: diskOpChangeFilenameExt(".mod"); break;
				default: case MOD_SAVE_MODE_XM:  diskOpChangeFilenameExt(".xm");  break;
				         case MOD_SAVE_MODE_WAV: diskOpChangeFilenameExt(".wav"); break;
			}

			// enter WAV renderer if needed
			if (editor.moduleSaveMode == MOD_SAVE_MODE_WAV)
			{
				exitDiskOpScreen();
				showWavRenderer();
				return;
			}

			if (checkOverwrite && fileExistsAnsi(FReq_FileName))
			{
				createFileOverwriteText(FReq_FileName, FReq_SysReqText);
				if (okBox(2, "System request", FReq_SysReqText, NULL) != 1)
					return;
			}

			fileNameU = cp850ToUnichar(FReq_FileName);
			if (fileNameU == NULL)
			{
				okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
				return;
			}

			saveMusic(fileNameU);
			free(fileNameU);
			// sets editor.diskOpReadDir after thread is done
		}
		break;

		case DISKOP_ITEM_INSTR:
		{
			diskOpChangeFilenameExt(".xi");

			if (checkOverwrite && fileExistsAnsi(FReq_FileName))
			{
				createFileOverwriteText(FReq_FileName, FReq_SysReqText);
				if (okBox(2, "System request", FReq_SysReqText, NULL) != 1)
					return;
			}

			fileNameU = cp850ToUnichar(FReq_FileName);
			if (fileNameU == NULL)
			{
				okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
				return;
			}

			saveInstr(fileNameU, editor.curInstr);
			free(fileNameU);
			// editor.diskOpReadDir is set after thread is done
		}
		break;

		case DISKOP_ITEM_SAMPLE:
		{
			switch (editor.sampleSaveMode)
			{
				         case SMP_SAVE_MODE_RAW: diskOpChangeFilenameExt(".raw"); break;
				         case SMP_SAVE_MODE_IFF: diskOpChangeFilenameExt(".iff"); break;
				default: case SMP_SAVE_MODE_WAV: diskOpChangeFilenameExt(".wav"); break;
			}

			if (checkOverwrite && fileExistsAnsi(FReq_FileName))
			{
				createFileOverwriteText(FReq_FileName, FReq_SysReqText);
				if (okBox(2, "System request", FReq_SysReqText, NULL) != 1)
					return;
			}

			fileNameU = cp850ToUnichar(FReq_FileName);
			if (fileNameU == NULL)
			{
				okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
				return;
			}

			saveSample(fileNameU, SAVE_NORMAL);
			free(fileNameU);
			// editor.diskOpReadDir is set after thread is done
		}
		break;

		case DISKOP_ITEM_PATTERN:
		{
			diskOpChangeFilenameExt(".xp");

			if (checkOverwrite && fileExistsAnsi(FReq_FileName))
			{
				createFileOverwriteText(FReq_FileName, FReq_SysReqText);
				if (okBox(2, "System request", FReq_SysReqText, NULL) != 1)
					return;
			}

			fileNameU = cp850ToUnichar(FReq_FileName);
			if (fileNameU == NULL)
			{
				okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
				return;
			}

			editor.diskOpReadDir = savePattern(fileNameU);
			free(fileNameU);
		}
		break;

		case DISKOP_ITEM_TRACK:
		{
			diskOpChangeFilenameExt(".xt");

			if (checkOverwrite && fileExistsAnsi(FReq_FileName))
			{
				createFileOverwriteText(FReq_FileName, FReq_SysReqText);
				if (okBox(2, "System request", FReq_SysReqText, NULL) != 1)
					return;
			}

			fileNameU = cp850ToUnichar(FReq_FileName);
			if (fileNameU == NULL)
			{
				okBox(0, "System message", "General I/O error during saving! Is the file in use?", NULL);
				return;
			}

			editor.diskOpReadDir = saveTrack(fileNameU);
			free(fileNameU);
		}
		break;
	}
}

void pbDiskOpSave(void)
{
	diskOpSave(config.cfg_OverwriteWarning ? true : false); // check if about to overwrite
}

static void fileListPressed(int32_t index)
{
	char *nameTmp;
	int32_t result;

	const int32_t entryIndex = FReq_DirPos + index;
	if (entryIndex >= FReq_FileCount || FReq_FileCount == 0)
		return; // illegal entry

	const int8_t mode = mouse.mode;

	// set normal mouse cursor
	if (mouse.mode != MOUSE_MODE_NORMAL)
		setMouseMode(MOUSE_MODE_NORMAL);

	// remove file selection
	FReq_EntrySelected = -1;
	diskOp_DrawFilelist();

	DirRec *dirEntry = &FReq_Buffer[entryIndex];
	switch (mode)
	{
		// open file/folder
		default:
		case MOUSE_MODE_NORMAL:
		{
			if (dirEntry->isDir)
				openDirectory(dirEntry->nameU);
			else
				openFile(dirEntry->nameU, true);
		}
		break;

		// delete file/folder
		case MOUSE_MODE_DELETE:
		{
			if (!dirEntry->isDir || UNICHAR_STRCMP(dirEntry->nameU, PARENT_DIR_STR)) // don't handle ".." dir
			{
				nameTmp = unicharToCp850(dirEntry->nameU, true);
				if (nameTmp == NULL)
					break;

				trimEntryName(nameTmp, dirEntry->isDir);

				if (dirEntry->isDir)
					sprintf(FReq_SysReqText, "Delete directory \"%s\"?", nameTmp);
				else
					sprintf(FReq_SysReqText, "Delete file \"%s\"?", nameTmp);

				free(nameTmp);

				if (okBox(2, "System request", FReq_SysReqText, NULL) == 1)
				{
					if (dirEntry->isDir)
					{
						result = deleteDirRecursive(dirEntry->nameU);
						if (!result)
							okBox(0, "System message", "Couldn't delete folder: Access denied!", NULL);
						else
							editor.diskOpReadDir = true;
					}
					else
					{
						result = (UNICHAR_REMOVE(dirEntry->nameU) == 0);
						if (!result)
							okBox(0, "System message", "Couldn't delete file: Access denied!", NULL);
						else
							editor.diskOpReadDir = true;
					}
				}
			}
		}
		break;

		// rename file/folder
		case MOUSE_MODE_RENAME:
		{
			if (dirEntry->isDir || UNICHAR_STRCMP(dirEntry->nameU, PARENT_DIR_STR)) // don't handle ".." dir
			{
				nameTmp = unicharToCp850(dirEntry->nameU, true);
				if (nameTmp == NULL)
					break;

				strncpy(FReq_NameTemp, nameTmp, PATH_MAX);
				FReq_NameTemp[PATH_MAX] = '\0';
				free(nameTmp);

				// in case of UTF8 -> CP437 encoding failure, there can be question marks. Remove them...
				removeQuestionmarksFromString(FReq_NameTemp);

				if (inputBox(1, dirEntry->isDir ? "Enter new directory name:" : "Enter new filename:", FReq_NameTemp, PATH_MAX) == 1)
				{
					if (FReq_NameTemp == NULL || FReq_NameTemp[0] == '\0')
					{
						okBox(0, "System message", "New name can't be empty!", NULL);
						break;
					}

					if (!renameAnsi(dirEntry->nameU, FReq_NameTemp))
					{
						if (dirEntry->isDir)
							okBox(0, "System message", "Couldn't rename directory: Access denied, or dir already exists!", NULL);
						else
							okBox(0, "System message", "Couldn't rename file: Access denied, or file already exists!", NULL);
					}
					else
					{
						editor.diskOpReadDir = true;
					}
				}
			}
		}
		break;
	}
}

bool testDiskOpMouseDown(bool mouseHeldDlown)
{
	int32_t tmpEntry;

	if (!ui.diskOpShown || FReq_FileCount == 0)
		return false;

	int32_t max = FReq_FileCount - FReq_DirPos;
	if (max > DISKOP_ENTRY_NUM) // needed kludge when mouse-scrolling
		max = DISKOP_ENTRY_NUM;

	if (!mouseHeldDlown) // select file
	{
		FReq_EntrySelected = -1;

		if (mouse.x >= 169 && mouse.x <= 331 && mouse.y >= 4 && mouse.y <= 168)
		{
			tmpEntry = (mouse.y - 4) / (FONT1_CHAR_H + 1);
			if (tmpEntry >= 0 && tmpEntry < max)
			{
				FReq_EntrySelected = tmpEntry;
				diskOp_DrawFilelist();
			}

			mouse.lastUsedObjectType = OBJECT_DISKOPLIST;
			return true;
		}

		return false;
	}

	// handle scrolling if outside of disk op. list area
	if (mouse.y < 4)
	{
		scrollBarScrollUp(SB_DISKOP_LIST, 1);
		FReq_EntrySelected = -1;
	}
	else if (mouse.y > 168)
	{
		scrollBarScrollDown(SB_DISKOP_LIST, 1);
		FReq_EntrySelected = -1;
	}

	if (mouse.y == lastMouseY)
		return true;

	lastMouseY = mouse.y;

	tmpEntry = (mouse.y - 4) / (FONT1_CHAR_H + 1);
	if (mouse.x < 169 || mouse.x > 331 || mouse.y < 4 || tmpEntry < 0 || tmpEntry >= max)
	{
		FReq_EntrySelected = -1;
		diskOp_DrawFilelist();

		return true;
	}

	if (tmpEntry != FReq_EntrySelected)
	{
		FReq_EntrySelected = tmpEntry;
		diskOp_DrawFilelist();
	}

	return true;
}

void testDiskOpMouseRelease(void)
{
	if (ui.diskOpShown && FReq_EntrySelected != -1)
	{
		if (mouse.x >= 169 && mouse.x <= 329 && mouse.y >= 4 && mouse.y <= 168)
			fileListPressed((mouse.y - 4) / (FONT1_CHAR_H + 1));

		FReq_EntrySelected = -1;
		diskOp_DrawFilelist();
	}
}

static bool moduleExtensionAccepted(char *extPtr)
{
	int32_t i = 0;
	while (true)
	{
		const char *str = supportedModExtensions[i++];
		if (!_stricmp(str, "END_OF_LIST"))
			return false;

		if (!_stricmp(str, extPtr))
			break;
	}

	return true;
}

static bool sampleExtensionAccepted(char *extPtr)
{
	int32_t i = 0;
	while (true)
	{
		const char *str = supportedSmpExtensions[i++];
		if (!_stricmp(str, "END_OF_LIST"))
			return false;

		if (!_stricmp(str, extPtr))
			break;
	}

	return true;
}

static uint8_t handleEntrySkip(UNICHAR *nameU, bool isDir)
{
	// skip if illegal name or filesize >32-bit
	if (nameU == NULL)
		return true;

	char *name = unicharToCp850(nameU, false);
	if (name == NULL)
		return true;

	if (name[0] == '\0')
		goto skipEntry;

	const int32_t nameLen = (int32_t)strlen(name);

	// skip ".name" dirs/files
	if (nameLen >= 2 && name[0] == '.' && name[1] != '.')
		goto skipEntry;

	if (isDir)
	{
		// skip '.' directory
		if (nameLen == 1 && name[0] == '.')
			goto skipEntry;

		// macOS/Linux: skip '..' directory if we're in root
#ifndef _WIN32
		if (nameLen == 2 && name[0] == '.' && name[1] == '.')
		{
			if (FReq_CurPathU[0] == '/' && FReq_CurPathU[1] == '\0')
				goto skipEntry;
		}
#endif
	}
	else if (!FReq_ShowAllFiles)
	{
		int32_t extOffset = getExtOffset(name, nameLen);
		if (extOffset == -1)
			goto skipEntry;
		extOffset++; // skip '.'

		const int32_t extLen = (int32_t)strlen(&name[extOffset]);
		if (extLen < 2 || extLen > 6)
			goto skipEntry; // no possibly known extensions to filter out

		char *extPtr = &name[extOffset];

		// decide what entries to keep based on file extension
		switch (FReq_Item)
		{
			default:
			case DISKOP_ITEM_MODULE:
			{
				if (editor.moduleSaveMode == MOD_SAVE_MODE_WAV && !_stricmp("wav", extPtr))
					break; // show .wav files when save mode is "WAV"

				if (!moduleExtensionAccepted(extPtr))
					goto skipEntry;
			}
			break;

			case DISKOP_ITEM_INSTR:
			{
				if (!_stricmp("xi", extPtr))
					break;

				if (!sampleExtensionAccepted(extPtr))
					goto skipEntry;
			}
			break;

			case DISKOP_ITEM_SAMPLE:
			{
				if (!sampleExtensionAccepted(extPtr))
					goto skipEntry;
			}
			break;

			case DISKOP_ITEM_PATTERN:
			{
				if (!_stricmp("xp", extPtr))
					break;

				goto skipEntry;
			}
			break;

			case DISKOP_ITEM_TRACK:
			{
				if (!_stricmp("xt", extPtr))
					break;

				goto skipEntry;
			}
			break;
		}
	}

	free(name);
	return false; // "Show All Files" mode is enabled, don't skip entry

skipEntry:
	free(name);
	return true;
}

static int8_t findFirst(DirRec *searchRec)
{
#ifdef _WIN32
	WIN32_FIND_DATAW fData;
#else
	struct dirent *fData;
	struct stat st;
	int64_t fSize;
#endif

#if defined(__sun) || defined(sun)
	struct stat s;
#endif

	searchRec->nameU = NULL; // this one must be initialized

#ifdef _WIN32
	hFind = FindFirstFileW(L"*", &fData);
	if (hFind == NULL || hFind == INVALID_HANDLE_VALUE)
		return LFF_DONE;

	searchRec->nameU = UNICHAR_STRDUP(fData.cFileName);
	if (searchRec->nameU == NULL)
		return LFF_SKIP;

	searchRec->filesize = (fData.nFileSizeHigh > 0) ? -1 : fData.nFileSizeLow;
	searchRec->isDir = (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
#else
	hFind = opendir(".");
	if (hFind == NULL)
		return LFF_DONE;

	fData = readdir(hFind);
	if (fData == NULL)
		return LFF_DONE;

	searchRec->nameU = UNICHAR_STRDUP(fData->d_name);
	if (searchRec->nameU == NULL)
		return LFF_SKIP;

	searchRec->filesize = 0;

#if defined(__sun) || defined(sun)
	stat(fData->d_name, &s);
	searchRec->isDir = (s.st_mode != S_IFDIR) ? true : false;
#else
	searchRec->isDir = (fData->d_type == DT_DIR) ? true : false;
#endif

#if defined(__sun) || defined(sun)
	if (s.st_mode == S_IFLNK)
#else
	if (fData->d_type == DT_UNKNOWN || fData->d_type == DT_LNK)
#endif
	{
		if (stat(fData->d_name, &st) == 0)
		{
			fSize = (int64_t)st.st_size;
			searchRec->filesize = (fSize > INT32_MAX) ? -1 : (fSize & 0xFFFFFFFF);

			if ((st.st_mode & S_IFMT) == S_IFDIR)
				searchRec->isDir = true;
		}
	}
	else if (!searchRec->isDir)
	{
		if (stat(fData->d_name, &st) == 0)
		{
			fSize = (int64_t)st.st_size;
			searchRec->filesize = (fSize > INT32_MAX) ? -1 : (fSize & 0xFFFFFFFF);
		}
	}
#endif

	if (searchRec->filesize < -1)
		searchRec->filesize = -1;

	if (handleEntrySkip(searchRec->nameU, searchRec->isDir))
	{
		// skip file
		free(searchRec->nameU);
		searchRec->nameU = NULL;

		return LFF_SKIP;
	}

	return LFF_OK;
}

static int8_t findNext(DirRec *searchRec)
{
#ifdef _WIN32
	WIN32_FIND_DATAW fData;
#else
	struct dirent *fData;
	struct stat st;
	int64_t fSize;
#endif

#if defined(__sun) || defined(sun)
	struct stat s;
#endif

	searchRec->nameU = NULL; // important

#ifdef _WIN32
	if (hFind == NULL || FindNextFileW(hFind, &fData) == 0)
		return LFF_DONE;

	searchRec->nameU = UNICHAR_STRDUP(fData.cFileName);
	if (searchRec->nameU == NULL)
		return LFF_SKIP;

	searchRec->filesize = (fData.nFileSizeHigh > 0) ? -1 : fData.nFileSizeLow;
	searchRec->isDir = (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
#else
	if (hFind == NULL || (fData = readdir(hFind)) == NULL)
		return LFF_DONE;

	searchRec->nameU = UNICHAR_STRDUP(fData->d_name);
	if (searchRec->nameU == NULL)
		return LFF_SKIP;

	searchRec->filesize = 0;

#if defined(__sun) || defined(sun)
	stat(fData->d_name, &s);
	searchRec->isDir = (s.st_mode != S_IFDIR) ? true : false;
#else
	searchRec->isDir = (fData->d_type == DT_DIR) ? true : false;
#endif

#if defined(__sun) || defined(sun)
	if (s.st_mode == S_IFLNK)
#else
	if (fData->d_type == DT_UNKNOWN || fData->d_type == DT_LNK)
#endif
	{
		if (stat(fData->d_name, &st) == 0)
		{
			fSize = (int64_t)st.st_size;
			searchRec->filesize = (fSize > INT32_MAX) ? -1 : (fSize & 0xFFFFFFFF);

			if ((st.st_mode & S_IFMT) == S_IFDIR)
				searchRec->isDir = true;
		}
	}
	else if (!searchRec->isDir)
	{
		if (stat(fData->d_name, &st) == 0)
		{
			fSize = (int64_t)st.st_size;
			searchRec->filesize = (fSize > INT32_MAX) ? -1 : (fSize & 0xFFFFFFFF);
		}
	}
#endif

	if (searchRec->filesize < -1)
		searchRec->filesize = -1;

	if (handleEntrySkip(searchRec->nameU, searchRec->isDir))
	{
		// skip file
		free(searchRec->nameU);
		searchRec->nameU = NULL;

		return LFF_SKIP;
	}

	return LFF_OK;
}

static void findClose(void)
{
	if (hFind != NULL)
	{
#ifdef _WIN32
		FindClose(hFind);
#else
		closedir(hFind);
#endif
		hFind = NULL;
	}
}

static bool swapBufferEntry(int32_t a, int32_t b) // used for sorting
{
	if (a >= FReq_FileCount || b >= FReq_FileCount)
		return false;

	DirRec tmpBuffer = FReq_Buffer[a];
	FReq_Buffer[a] = FReq_Buffer[b];
	FReq_Buffer[b] = tmpBuffer;

	return true;
}

static char *ach(int32_t rad) // used for sortDirectory()
{
	DirRec *dirEntry = &FReq_Buffer[rad];

	char *name = unicharToCp850(dirEntry->nameU, true);
	if (name == NULL)
		return NULL;

	const int32_t nameLen = (int32_t)strlen(name);
	if (nameLen == 0)
	{
		free(name);
		return NULL;
	}

	char *p = (char *)malloc(nameLen+1+1);
	if (p == NULL)
	{
		free(name);
		return NULL;
	}

	if (dirEntry->isDir)
	{
		// directory

		if (nameLen == 2 && name[0] == '.' && name[1] == '.')
			p[0] = 0x01; // make ".." directory first priority
		else
			p[0] = 0x02; // make second priority

		strcpy(&p[1], name);

		free(name);
		return p;
	}
	else
	{
		// file

		const int32_t i = getExtOffset(name, nameLen);
		if (config.cfg_SortPriority == 1 || i == -1)
		{
			// sort by filename
			strcpy(p, name);
			free(name);
			return p;
		}
		else
		{
			// sort by filename extension
			const int32_t extLen = nameLen - i;
			if (extLen <= 1)
			{
				strcpy(p, name);
				free(name);
				return p;
			}

			// FILENAME.EXT -> EXT.FILENAME (for sorting)
			memcpy(p, &name[i+1], extLen - 1);
			memcpy(&p[extLen-1], name, i);
			p[nameLen-1] = '\0';

			free(name);
			return p;
		}
	}
}

static void sortDirectory(void)
{
	bool didSwap;

	if (FReq_FileCount < 2)
		return; // no need to sort

	uint32_t offset = FReq_FileCount >> 1;
	while (offset > 0)
	{
		const uint32_t limit = FReq_FileCount - offset;
		do
		{
			didSwap = false;
			for (uint32_t i = 0; i < limit; i++)
			{
				char *p1 = ach(i);
				char *p2 = ach(offset+i);

				if (p1 == NULL || p2 == NULL)
				{
					if (p1 != NULL) free(p1);
					if (p2 != NULL) free(p2);
					okBox(0, "System message", "Not enough memory!", NULL);
					return;
				}

				if (_stricmp(p1, p2) > 0)
				{
					if (!swapBufferEntry(i, offset + i))
					{
						free(p1);
						free(p2);
						return;
					}

					didSwap = true;
				}

				free(p1);
				free(p2);
			}
		}
		while (didSwap);

		offset >>= 1;
	}
}

static uint8_t numDigits32(uint32_t x)
{
	if (x >= 1000000000) return 10;
	if (x >=  100000000) return  9;
	if (x >=   10000000) return  8;
	if (x >=    1000000) return  7;
	if (x >=     100000) return  6;
	if (x >=      10000) return  5;
	if (x >=       1000) return  4;
	if (x >=        100) return  3;
	if (x >=         10) return  2;

	return 1;
}

static void printFormattedFilesize(uint16_t x, uint16_t y, uint32_t bufEntry)
{
	char sizeStrBuffer[16];
	int32_t printFilesize;

	const int32_t filesize = FReq_Buffer[bufEntry].filesize;
	if (filesize == -1)
	{
		x += 6;
		textOut(x, y, PAL_BLCKTXT, ">2GB");
		return;
	}

	assert(filesize >= 0);

	if (filesize >= 1024*1024*10) // >= 10MB?
	{
forceMB:
		printFilesize = (int32_t)ceil(filesize / (1024.0*1024.0));
		x += (4 - numDigits32(printFilesize)) * (FONT1_CHAR_W - 1);
		sprintf(sizeStrBuffer, "%dM", printFilesize);
	}
	else if (filesize >= 1024*10) // >= 10kB?
	{
		printFilesize = (int32_t)ceil(filesize / 1024.0);
		if (printFilesize > 9999)
			goto forceMB; // ceil visual overflow kludge

		x += (4 - numDigits32(printFilesize)) * (FONT1_CHAR_W - 1);
		sprintf(sizeStrBuffer, "%dk", printFilesize);
	}
	else // bytes
	{
		printFilesize = filesize;
		x += (5 - numDigits32(printFilesize)) * (FONT1_CHAR_W - 1);
		sprintf(sizeStrBuffer, "%d", printFilesize);
	}

	textOut(x, y, PAL_BLCKTXT, sizeStrBuffer);
}

static void displayCurrPath(void)
{
	fillRect(4, 145, 162, 10, PAL_DESKTOP);

	if (FReq_CurPathU == NULL)
		return;

	const uint32_t pathLen = (uint32_t)UNICHAR_STRLEN(FReq_CurPathU);
	if (pathLen == 0)
		return;

	char *asciiPath = unicharToCp850(FReq_CurPathU, true);
	if (asciiPath == NULL)
	{
		okBox(0, "System message", "Not enough memory!", NULL);
		return;
	}

	char *p = asciiPath;
	if (textWidth(p) <= 162)
	{
		// path fits, print it all
		textOut(4, 145, PAL_FORGRND, p);
	}
	else
	{
		// path doesn't fit, print drive + ".." + last directory

#ifdef _WIN32
		memcpy(FReq_NameTemp, p, 3); // get drive (f.ex. C:\)
		FReq_NameTemp[3] = '\0';

		strcat(FReq_NameTemp, ".\001"); // special character in font
		FReq_NameTemp[5] = '\0';
#else
		FReq_NameTemp[0] = '\0';
		strcpy(FReq_NameTemp, "/");
		strcat(FReq_NameTemp, "..");
#endif

		char *delimiter = strrchr(p, DIR_DELIMITER);
		if (delimiter != NULL)
		{
#ifdef _WIN32
			strcat(FReq_NameTemp, delimiter+1);
#else
			strcat(FReq_NameTemp, delimiter);
#endif
		}

		int32_t j = (int32_t)strlen(FReq_NameTemp);
		if (j > 6)
		{
			j--;

			p = FReq_NameTemp;
			while (j >= 6 && textWidth(p) >= 162)
			{
				p[j-2] = '.';
				p[j-1] = '.';
				p[j-0] = '\0';
				j--;
			}
		}

		textOutClipX(4, 145, PAL_FORGRND, FReq_NameTemp, 165);
	}

	free(asciiPath);
}

void diskOp_DrawFilelist(void)
{
	clearRect(FILENAME_TEXT_X-1, 4, 162, 164);

	if (FReq_FileCount == 0)
		return;

	// draw "selected file" rectangle
	if (FReq_EntrySelected != -1)
	{
		const uint16_t y = 4 + (uint16_t)((FONT1_CHAR_H + 1) * FReq_EntrySelected);
		fillRect(FILENAME_TEXT_X - 1, y, 162, FONT1_CHAR_H, PAL_PATTEXT);
	}

	for (uint16_t i = 0; i < DISKOP_ENTRY_NUM; i++)
	{
		const int32_t bufEntry = FReq_DirPos + i;
		if (bufEntry >= FReq_FileCount)
			break;

		if (FReq_Buffer[bufEntry].nameU == NULL)
			continue;

		// convert unichar name to codepage 437
		char *readName = unicharToCp850(FReq_Buffer[bufEntry].nameU, true);
		if (readName == NULL)
			continue;

		const uint16_t y = 4 + (i * (FONT1_CHAR_H + 1));

		// shrink entry name and add ".." if it doesn't fit on screen
		trimEntryName(readName, FReq_Buffer[bufEntry].isDir);

		if (FReq_Buffer[bufEntry].isDir)
		{
			// directory
			charOut(FILENAME_TEXT_X, y, PAL_BLCKTXT, DIR_DELIMITER);
			textOut(FILENAME_TEXT_X + FONT1_CHAR_W, y, PAL_BLCKTXT, readName);
		}
		else
		{
			// filename
			textOut(FILENAME_TEXT_X, y, PAL_BLCKTXT, readName);
		}

		free(readName);

		if (!FReq_Buffer[bufEntry].isDir)
			printFormattedFilesize(FILESIZE_TEXT_X, y, bufEntry);
	}
}

void diskOp_DrawDirectory(void)
{
	drawTextBox(TB_DISKOP_FILENAME);

	displayCurrPath();
#ifdef _WIN32
	setupDiskOpDrives();
#endif

	setScrollBarEnd(SB_DISKOP_LIST, FReq_FileCount);
	setScrollBarPos(SB_DISKOP_LIST, FReq_DirPos, false);

	diskOp_DrawFilelist();
}

static DirRec *bufferCreateEmptyDir(void) // special case: creates a dir entry with a ".." directory
{
	DirRec *dirEntry = (DirRec *)malloc(sizeof (DirRec));
	if (dirEntry == NULL)
		return NULL;

	dirEntry->nameU = UNICHAR_STRDUP(PARENT_DIR_STR);
	if (dirEntry->nameU == NULL)
	{
		free(dirEntry);
		return NULL;
	}

	dirEntry->isDir = true;
	dirEntry->filesize = 0;

	return dirEntry;
}

static int32_t SDLCALL diskOp_ReadDirectoryThread(void *ptr)
{
	DirRec tmpBuffer;

	FReq_DirPos = 0;

	// free old buffer
	freeDirRecBuffer();

	UNICHAR_GETCWD(FReq_CurPathU, PATH_MAX);

	// read first file
	int8_t lastFindFileFlag = findFirst(&tmpBuffer);
	if (lastFindFileFlag != LFF_DONE && lastFindFileFlag != LFF_SKIP)
	{
		FReq_Buffer = (DirRec *)malloc(sizeof (DirRec) * (FReq_FileCount+1));
		if (FReq_Buffer == NULL)
		{
			findClose();

			okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);

			FReq_Buffer = bufferCreateEmptyDir();
			if (FReq_Buffer != NULL)
				FReq_FileCount = 1;
			else
				okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);

			setMouseBusy(false);
			return false;
		}

		memcpy(&FReq_Buffer[FReq_FileCount], &tmpBuffer, sizeof (DirRec));
		FReq_FileCount++;
	}

	// read remaining files
	while (lastFindFileFlag != LFF_DONE)
	{
		lastFindFileFlag = findNext(&tmpBuffer);
		if (lastFindFileFlag != LFF_DONE && lastFindFileFlag != LFF_SKIP)
		{
			DirRec *newPtr = (DirRec *)realloc(FReq_Buffer, sizeof (DirRec) * (FReq_FileCount + 1));
			if (newPtr == NULL)
			{
				freeDirRecBuffer();
				okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
				break;
			}

			FReq_Buffer = newPtr;

			memcpy(&FReq_Buffer[FReq_FileCount], &tmpBuffer, sizeof (DirRec));
			FReq_FileCount++;
		}
	}

	findClose();

	if (FReq_FileCount > 0)
	{
		sortDirectory();
	}
	else
	{
		// access denied or out of memory - create parent directory link
		FReq_Buffer = bufferCreateEmptyDir();
		if (FReq_Buffer != NULL)
			FReq_FileCount = 1;
		else
			okBoxThreadSafe(0, "System message", "Not enough memory!", NULL);
	}

	editor.diskOpReadDone = true;
	setMouseBusy(false);

	return true;

	(void)ptr;
}

void diskOp_StartDirReadThread(void)
{
	editor.diskOpReadDone = false;

	mouseAnimOn();
	thread = SDL_CreateThread(diskOp_ReadDirectoryThread, NULL, NULL);
	if (thread == NULL)
	{
		editor.diskOpReadDone = true;
		okBox(0, "System message", "Couldn't create thread!", NULL);
		return;
	}

	SDL_DetachThread(thread);
}

static void drawSaveAsElements(void)
{
	switch (FReq_Item)
	{
		default:
		case DISKOP_ITEM_MODULE:
		{
			textOutShadow(19, 101, PAL_FORGRND, PAL_DSKTOP2, "MOD");
			textOutShadow(19, 115, PAL_FORGRND, PAL_DSKTOP2, "XM");
			textOutShadow(19, 129, PAL_FORGRND, PAL_DSKTOP2, "WAV");
		}
		break;

		case DISKOP_ITEM_INSTR:
			textOutShadow(19, 101, PAL_FORGRND, PAL_DSKTOP2, "XI");
		break;

		case DISKOP_ITEM_SAMPLE:
		{
			textOutShadow(19, 101, PAL_FORGRND, PAL_DSKTOP2, "RAW");
			textOutShadow(19, 115, PAL_FORGRND, PAL_DSKTOP2, "IFF");
			textOutShadow(19, 129, PAL_FORGRND, PAL_DSKTOP2, "WAV");
		}
		break;

		case DISKOP_ITEM_PATTERN:
			textOutShadow(19, 101, PAL_FORGRND, PAL_DSKTOP2, "XP");
		break;

		case DISKOP_ITEM_TRACK:
			textOutShadow(19, 101, PAL_FORGRND, PAL_DSKTOP2, "XT");
		break;
	}
}

static void setDiskOpItemRadioButtons(void)
{
	uncheckRadioButtonGroup(RB_GROUP_DISKOP_MOD_SAVEAS);
	uncheckRadioButtonGroup(RB_GROUP_DISKOP_INS_SAVEAS);
	uncheckRadioButtonGroup(RB_GROUP_DISKOP_SMP_SAVEAS);
	uncheckRadioButtonGroup(RB_GROUP_DISKOP_PAT_SAVEAS);
	uncheckRadioButtonGroup(RB_GROUP_DISKOP_TRK_SAVEAS);

	hideRadioButtonGroup(RB_GROUP_DISKOP_MOD_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_INS_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_SMP_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_PAT_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_TRK_SAVEAS);

	if (editor.moduleSaveMode > 3)
		editor.moduleSaveMode = 3;

	if (editor.sampleSaveMode > 3)
		editor.sampleSaveMode = 3;

	radioButtons[RB_DISKOP_MOD_SAVEAS_MOD + editor.moduleSaveMode].state = RADIOBUTTON_CHECKED;
	radioButtons[RB_DISKOP_SMP_SAVEAS_RAW + editor.sampleSaveMode].state = RADIOBUTTON_CHECKED;

	if (FReq_Item == DISKOP_ITEM_INSTR)   radioButtons[RB_DISKOP_INS_SAVEAS_XI].state = RADIOBUTTON_CHECKED;
	if (FReq_Item == DISKOP_ITEM_PATTERN) radioButtons[RB_DISKOP_PAT_SAVEAS_XP].state = RADIOBUTTON_CHECKED;
	if (FReq_Item == DISKOP_ITEM_TRACK)   radioButtons[RB_DISKOP_TRK_SAVEAS_XT].state = RADIOBUTTON_CHECKED;

	if (ui.diskOpShown)
	{
		switch (FReq_Item)
		{
			default: case DISKOP_ITEM_MODULE:  showRadioButtonGroup(RB_GROUP_DISKOP_MOD_SAVEAS); break;
			         case DISKOP_ITEM_INSTR:   showRadioButtonGroup(RB_GROUP_DISKOP_INS_SAVEAS); break;
			         case DISKOP_ITEM_SAMPLE:  showRadioButtonGroup(RB_GROUP_DISKOP_SMP_SAVEAS); break;
			         case DISKOP_ITEM_PATTERN: showRadioButtonGroup(RB_GROUP_DISKOP_PAT_SAVEAS); break;
			         case DISKOP_ITEM_TRACK:   showRadioButtonGroup(RB_GROUP_DISKOP_TRK_SAVEAS); break;
		}
	}
}

static void setDiskOpItem(uint8_t item)
{
	hideRadioButtonGroup(RB_GROUP_DISKOP_MOD_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_INS_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_SMP_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_PAT_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_TRK_SAVEAS);

	if (item > 4)
		item = 4;

	FReq_Item = item;
	switch (FReq_Item)
	{
		default:
		case DISKOP_ITEM_MODULE:
		{
			FReq_FileName = modTmpFName;

			// FReq_ModCurPathU is always set at this point

			FReq_CurPathU = FReq_ModCurPathU;
			if (FReq_CurPathU != NULL && FReq_CurPathU[0] != '\0')
				UNICHAR_CHDIR(FReq_CurPathU);
		}
		break;

		case DISKOP_ITEM_INSTR:
		{
			FReq_FileName = insTmpFName;

			if (!insPathSet && FReq_CurPathU != NULL && FReq_CurPathU[0] != '\0')
			{
				UNICHAR_STRCPY(FReq_InsCurPathU, FReq_CurPathU);
				insPathSet = true;
			}

			FReq_CurPathU = FReq_InsCurPathU;
			if (FReq_CurPathU != NULL)
				UNICHAR_CHDIR(FReq_CurPathU);
		}
		break;

		case DISKOP_ITEM_SAMPLE:
		{
			FReq_FileName = smpTmpFName;

			if (!smpPathSet && FReq_CurPathU != NULL && FReq_CurPathU[0] != '\0')
			{
				UNICHAR_STRCPY(FReq_SmpCurPathU, FReq_CurPathU);
				smpPathSet = true;
			}

			FReq_CurPathU = FReq_SmpCurPathU;
			if (FReq_CurPathU != NULL)
				UNICHAR_CHDIR(FReq_CurPathU);
		}
		break;

		case DISKOP_ITEM_PATTERN:
		{
			FReq_FileName = patTmpFName;

			if (!patPathSet && FReq_CurPathU != NULL && FReq_CurPathU[0] != '\0')
			{
				UNICHAR_STRCPY(FReq_PatCurPathU, FReq_CurPathU);
				patPathSet = true;
			}

			FReq_CurPathU = FReq_PatCurPathU;
			if (FReq_CurPathU != NULL)
				UNICHAR_CHDIR(FReq_CurPathU);
		}
		break;

		case DISKOP_ITEM_TRACK:
		{
			FReq_FileName = trkTmpFName;

			if (!trkPathSet && FReq_CurPathU != NULL && FReq_CurPathU[0] != '\0')
			{
				UNICHAR_STRCPY(FReq_TrkCurPathU, FReq_CurPathU);
				trkPathSet = true;
			}

			FReq_CurPathU = FReq_TrkCurPathU;
			if (FReq_CurPathU != NULL)
				UNICHAR_CHDIR(FReq_CurPathU);
		}
		break;
	}

	if (FReq_CurPathU != NULL && FReq_ModCurPathU != NULL)
	{
		if (FReq_CurPathU[0] == '\0' && FReq_ModCurPathU[0] != '\0')
			UNICHAR_STRCPY(FReq_CurPathU, FReq_ModCurPathU);
	}

	textBoxes[TB_DISKOP_FILENAME].textPtr = FReq_FileName;
	FReq_ShowAllFiles = false;

	if (ui.diskOpShown)
	{
		editor.diskOpReadDir = true;

		fillRect(4, 101, 40, 38, PAL_DESKTOP);
		drawSaveAsElements();
		setDiskOpItemRadioButtons();

		diskOp_DrawDirectory();
		drawTextBox(TB_DISKOP_FILENAME);
	}
	else
	{
		editor.diskOpReadOnOpen = true;
	}
}

static void drawDiskOpScreen(void)
{
	drawFramework(0,     0,  67,  86, FRAMEWORK_TYPE1);
	drawFramework(67,    0,  64, 142, FRAMEWORK_TYPE1);
	drawFramework(131,   0,  37, 142, FRAMEWORK_TYPE1);
	drawFramework(0,    86,  67,  56, FRAMEWORK_TYPE1);
	drawFramework(0,   142, 168,  31, FRAMEWORK_TYPE1);
	drawFramework(168,   0, 164,   3, FRAMEWORK_TYPE1);
	drawFramework(168, 170, 164,   3, FRAMEWORK_TYPE1);
	drawFramework(332,   0,  24, 173, FRAMEWORK_TYPE1);
	drawFramework(30,  157, 136,  14, FRAMEWORK_TYPE2);

	clearRect(168, 2, 164, 168);

	showPushButton(PB_DISKOP_SAVE);
	showPushButton(PB_DISKOP_DELETE);
	showPushButton(PB_DISKOP_RENAME);
	showPushButton(PB_DISKOP_MAKEDIR);
	showPushButton(PB_DISKOP_REFRESH);
	showPushButton(PB_DISKOP_EXIT);
	showPushButton(PB_DISKOP_PARENT);
	showPushButton(PB_DISKOP_ROOT);
	showPushButton(PB_DISKOP_SHOW_ALL);
	showPushButton(PB_DISKOP_SET_PATH);
	showPushButton(PB_DISKOP_LIST_UP);
	showPushButton(PB_DISKOP_LIST_DOWN);

	showScrollBar(SB_DISKOP_LIST);
	showTextBox(TB_DISKOP_FILENAME);

	textBoxes[TB_DISKOP_FILENAME].textPtr = FReq_FileName;

	if (FReq_Item > 4)
		FReq_Item = 4;

	uncheckRadioButtonGroup(RB_GROUP_DISKOP_ITEM);
	radioButtons[RB_DISKOP_MODULE + FReq_Item].state = RADIOBUTTON_CHECKED;
	showRadioButtonGroup(RB_GROUP_DISKOP_ITEM);

	// item selector
	textOutShadow(5,   3, PAL_FORGRND, PAL_DSKTOP2, "Item:");
	textOutShadow(19, 17, PAL_FORGRND, PAL_DSKTOP2, "Module");
	textOutShadow(19, 31, PAL_FORGRND, PAL_DSKTOP2, "Instr.");
	textOutShadow(19, 45, PAL_FORGRND, PAL_DSKTOP2, "Sample");
	textOutShadow(19, 59, PAL_FORGRND, PAL_DSKTOP2, "Pattern");
	textOutShadow(19, 73, PAL_FORGRND, PAL_DSKTOP2, "Track");

	// file format
	textOutShadow(5,  89, PAL_FORGRND, PAL_DSKTOP2, "Save as:");
	drawSaveAsElements();
	setDiskOpItemRadioButtons();

	// filename
	textOutShadow(4, 159, PAL_FORGRND, PAL_DSKTOP2, "File:");

	diskOp_DrawDirectory();
}

void showDiskOpScreen(void)
{
	// if first time opening Disk Op., set initial directory
	if (firstTimeOpeningDiskOp)
	{
		assert(FReq_ModCurPathU != NULL);

		// first test if we can change the dir to the one stored in the config (if present)
		if (FReq_ModCurPathU[0] == '\0' || UNICHAR_CHDIR(FReq_ModCurPathU) != 0)
		{
			// nope, couldn't do that, set Disk Op. path to the user's desktop directory
#ifdef _WIN32
			SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, FReq_ModCurPathU);
#else
			char *home = getenv("HOME");
			if (home != NULL)
			{
				UNICHAR_CHDIR(home);
				UNICHAR_STRCPY(FReq_ModCurPathU, home);

				UNICHAR_STRCAT(FReq_ModCurPathU, "/Desktop");
			}
#endif
			UNICHAR_CHDIR(FReq_ModCurPathU);
		}

		UNICHAR_GETCWD(FReq_ModCurPathU, PATH_MAX);
		firstTimeOpeningDiskOp = false;
	}

	if (ui.extendedPatternEditor)
		exitPatternEditorExtended();

	hideTopScreen();
	ui.diskOpShown = true;
	ui.scopesShown = false;

	showTopRightMainScreen();
	drawDiskOpScreen();

	// a flag that says if we need to update the filelist after disk op. was shown
	if (editor.diskOpReadOnOpen)
	{
		editor.diskOpReadOnOpen = false;
		diskOp_StartDirReadThread();
	}
}

void hideDiskOpScreen(void)
{
#ifdef _WIN32
	for (uint16_t i = 0; i < DISKOP_MAX_DRIVE_BUTTONS; i++)
		hidePushButton(PB_DISKOP_DRIVE1 + i);
#endif

	hidePushButton(PB_DISKOP_SAVE);
	hidePushButton(PB_DISKOP_DELETE);
	hidePushButton(PB_DISKOP_RENAME);
	hidePushButton(PB_DISKOP_MAKEDIR);
	hidePushButton(PB_DISKOP_REFRESH);
	hidePushButton(PB_DISKOP_EXIT);
	hidePushButton(PB_DISKOP_PARENT);
	hidePushButton(PB_DISKOP_ROOT);
	hidePushButton(PB_DISKOP_SHOW_ALL);
	hidePushButton(PB_DISKOP_SET_PATH);
	hidePushButton(PB_DISKOP_LIST_UP);
	hidePushButton(PB_DISKOP_LIST_DOWN);

	hideScrollBar(SB_DISKOP_LIST);
	hideTextBox(TB_DISKOP_FILENAME);
	hideRadioButtonGroup(RB_GROUP_DISKOP_ITEM);
	hideRadioButtonGroup(RB_GROUP_DISKOP_MOD_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_INS_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_SMP_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_PAT_SAVEAS);
	hideRadioButtonGroup(RB_GROUP_DISKOP_TRK_SAVEAS);

	ui.diskOpShown = false;
}

void exitDiskOpScreen(void)
{
	hideDiskOpScreen();
	ui.oldTopLeftScreen = 0; // disk op. ignores previously opened top screens
	showTopScreen(true);
}

void toggleDiskOpScreen(void)
{
	if (ui.diskOpShown)
		exitDiskOpScreen();
	else
		showDiskOpScreen();
}

void sbDiskOpSetPos(uint32_t pos)
{
	if ((int32_t)pos != FReq_DirPos && FReq_FileCount > DISKOP_ENTRY_NUM)
	{
		FReq_DirPos = (int32_t)pos;
		diskOp_DrawFilelist();
	}
}

void pbDiskOpListUp(void)
{
	if (FReq_DirPos > 0 && FReq_FileCount > DISKOP_ENTRY_NUM)
		scrollBarScrollUp(SB_DISKOP_LIST, 1);
}

void pbDiskOpListDown(void)
{
	if (FReq_DirPos < FReq_FileCount-DISKOP_ENTRY_NUM && FReq_FileCount > DISKOP_ENTRY_NUM)
		scrollBarScrollDown(SB_DISKOP_LIST, 1);
}

void pbDiskOpParent(void)
{
	diskOpGoParent();
}

void pbDiskOpRoot(void)
{
#ifdef _WIN32
	openDirectory(L"\\");
#else
	openDirectory("/");
#endif
}

void pbDiskOpShowAll(void)
{
	FReq_ShowAllFiles = true;
	editor.diskOpReadDir = true; // refresh dir
}

#ifdef _WIN32
void pbDiskOpDrive1(void) { openDrive(logicalDriveNames[driveIndexes[0]]); }
void pbDiskOpDrive2(void) { openDrive(logicalDriveNames[driveIndexes[1]]); }
void pbDiskOpDrive3(void) { openDrive(logicalDriveNames[driveIndexes[2]]); }
void pbDiskOpDrive4(void) { openDrive(logicalDriveNames[driveIndexes[3]]); }
void pbDiskOpDrive5(void) { openDrive(logicalDriveNames[driveIndexes[4]]); }
void pbDiskOpDrive6(void) { openDrive(logicalDriveNames[driveIndexes[5]]); }
void pbDiskOpDrive7(void) { openDrive(logicalDriveNames[driveIndexes[6]]); }
void pbDiskOpDrive8(void) { openDrive(logicalDriveNames[driveIndexes[7]]); }
#endif

void pbDiskOpDelete(void)
{
	setMouseMode(MOUSE_MODE_DELETE);
}

void pbDiskOpRename(void)
{
	setMouseMode(MOUSE_MODE_RENAME);
}

void pbDiskOpMakeDir(void)
{
	FReq_NameTemp[0] = '\0';
	if (inputBox(1, "Enter directory name:", FReq_NameTemp, PATH_MAX) == 1)
	{
		if (FReq_NameTemp[0] == '\0')
		{
			okBox(0, "System message", "Name can't be empty!", NULL);
			return;
		}

		if (makeDirAnsi(FReq_NameTemp))
			editor.diskOpReadDir = true;
		else
			okBox(0, "System message", "Couldn't create directory: Access denied, or a dir with the same name already exists!", NULL);
	}
}

void pbDiskOpRefresh(void)
{
	editor.diskOpReadDir = true; // refresh dir
#ifdef _WIN32
	setupDiskOpDrives();
#endif
}

void pbDiskOpSetPath(void)
{
	FReq_NameTemp[0] = '\0';
	if (inputBox(1, "Enter new directory path:", FReq_NameTemp, PATH_MAX) == 1)
	{
		if (FReq_NameTemp[0] == '\0')
		{
			okBox(0, "System message", "Name can't be empty!", NULL);
			return;
		}

		if (chdir(FReq_NameTemp) == 0)
			editor.diskOpReadDir = true;
		else
			okBox(0, "System message", "Couldn't set directory path!", NULL);
	}
}

void pbDiskOpExit(void)
{
	exitDiskOpScreen();
}

void rbDiskOpModule(void)
{
	checkRadioButton(RB_DISKOP_MODULE);
	setDiskOpItem(DISKOP_ITEM_MODULE);
}

void rbDiskOpInstr(void)
{
	checkRadioButton(RB_DISKOP_INSTR);
	setDiskOpItem(DISKOP_ITEM_INSTR);
}

void rbDiskOpSample(void)
{
	checkRadioButton(RB_DISKOP_SAMPLE);
	setDiskOpItem(DISKOP_ITEM_SAMPLE);
}

void rbDiskOpPattern(void)
{
	checkRadioButton(RB_DISKOP_PATTERN);
	setDiskOpItem(DISKOP_ITEM_PATTERN);
}

void rbDiskOpTrack(void)
{
	checkRadioButton(RB_DISKOP_TRACK);
	setDiskOpItem(DISKOP_ITEM_TRACK);
}

void rbDiskOpModSaveMod(void)
{
	if (editor.moduleSaveMode == MOD_SAVE_MODE_WAV)
		editor.diskOpReadDir = true;

	editor.moduleSaveMode = MOD_SAVE_MODE_MOD;
	checkRadioButton(RB_DISKOP_MOD_SAVEAS_MOD);
	diskOpChangeFilenameExt(".mod");

	updateCurrSongFilename(); // for window title
	updateWindowTitle(true);
}

void rbDiskOpModSaveXm(void)
{
	if (editor.moduleSaveMode == MOD_SAVE_MODE_WAV)
		editor.diskOpReadDir = true;

	editor.moduleSaveMode = MOD_SAVE_MODE_XM;
	checkRadioButton(RB_DISKOP_MOD_SAVEAS_XM);
	diskOpChangeFilenameExt(".xm");

	updateCurrSongFilename(); // for window title
	updateWindowTitle(true);
}

void rbDiskOpModSaveWav(void)
{
	editor.moduleSaveMode = MOD_SAVE_MODE_WAV;
	checkRadioButton(RB_DISKOP_MOD_SAVEAS_WAV);
	diskOpChangeFilenameExt(".wav");

	updateCurrSongFilename(); // for window title
	updateWindowTitle(true);

	editor.diskOpReadDir = true;
}

void rbDiskOpSmpSaveRaw(void)
{
	editor.sampleSaveMode = SMP_SAVE_MODE_RAW;
	checkRadioButton(RB_DISKOP_SMP_SAVEAS_RAW);
	diskOpChangeFilenameExt(".raw");
}

void rbDiskOpSmpSaveIff(void)
{
	editor.sampleSaveMode = SMP_SAVE_MODE_IFF;
	checkRadioButton(RB_DISKOP_SMP_SAVEAS_IFF);
	diskOpChangeFilenameExt(".iff");
}

void rbDiskOpSmpSaveWav(void)
{
	editor.sampleSaveMode = SMP_SAVE_MODE_WAV;
	checkRadioButton(RB_DISKOP_SMP_SAVEAS_WAV);
	diskOpChangeFilenameExt(".wav");
}
