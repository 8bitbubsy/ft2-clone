/*
** Hardware Performance Counter delay routines.
**
** These are by no means well written, and are made for specific
** usage cases. There may be some hackish design choices here.
**
** NOTE: hpc_SetDurationInHz() has quite a bit of overhead, so it's
**       recommended to have one hpcFreq_t counter for each delay value,
**       then call hpc_SetDurationInHz()/hpc_SetDurationInMs() on program
**       init instead of every time you want to delay.
*/

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_hpc.h"

#define DURATION_FRAC_BITS 52 /* more makes little sense */
#define DURATION_FRAC_SCALE (1ULL << DURATION_FRAC_BITS)
#define DURATION_FRAC_MASK (DURATION_FRAC_SCALE-1)

hpcFreq_t hpcFreq;

#ifdef _WIN32 // Windows usleep() implementation

#define STATUS_SUCCESS 0

static bool canAdjustTimerResolution;

static NTSTATUS (__stdcall *NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval);
static NTSTATUS (__stdcall *NtQueryTimerResolution)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG ActualResolution);
static NTSTATUS (__stdcall *NtSetTimerResolution)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

static void (*usleep)(int32_t usec);

static void usleepAcceptable(int32_t usec)
{
	LARGE_INTEGER delayInterval;

	// NtDelayExecution() delays in 100ns-units, and a negative value means to delay from current time
	usec *= -10;

	delayInterval.HighPart = 0xFFFFFFFF; // negative 64-bit value, we only set the lower dword
	delayInterval.LowPart = usec;
	NtDelayExecution(false, &delayInterval);
}

static void usleepPoor(int32_t usec) // fallback if no NtDelayExecution()
{
	Sleep((usec + 500) / 1000);
}

static void windowsSetupUsleep(void)
{
	NtDelayExecution = (NTSTATUS (__stdcall *)(BOOL, PLARGE_INTEGER))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
	usleep = (NtDelayExecution != NULL) ? usleepAcceptable : usleepPoor;

	NtQueryTimerResolution = (NTSTATUS (__stdcall *)(PULONG, PULONG, PULONG))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryTimerResolution");
	NtSetTimerResolution = (NTSTATUS (__stdcall *)(ULONG, BOOLEAN, PULONG))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtSetTimerResolution");
	canAdjustTimerResolution = (NtQueryTimerResolution != NULL && NtSetTimerResolution != NULL);
}
#endif

void hpc_Init(void)
{
#ifdef _WIN32
	windowsSetupUsleep();
#endif
	hpcFreq.freq64 = SDL_GetPerformanceFrequency();

	double dFreq = (double)hpcFreq.freq64;

	hpcFreq.dFreqMulMs = 1000.0 / dFreq;
	hpcFreq.dFreqMulMicro = (1000.0 * 1000.0) / dFreq;
}

void hpc_SetDurationInHz(hpc_t *hpc, double dHz)
{
	const double dTickTime = (double)hpcFreq.freq64 / dHz;
	double dTimeInt, dTimeFrac = modf(dTickTime, &dTimeInt);

	hpc->durationInt = (uint32_t)dTimeInt;
	hpc->durationFrac = (uint64_t)(dTimeFrac * DURATION_FRAC_SCALE);
	hpc->resetFrame = (uint64_t)(dHz * (60*30)); // reset counters every half an hour
}

void hpc_SetDurationInMs(hpc_t *hpc, double dMs)
{
	hpc_SetDurationInHz(hpc, 1000.0 / dMs);
}

void hpc_ResetCounters(hpc_t *hpc)
{
	hpc->endTimeInt = SDL_GetPerformanceCounter() + hpc->durationInt;
	hpc->endTimeFrac = hpc->durationFrac;
}

void hpc_Wait(hpc_t *hpc)
{
#ifdef _WIN32
	/* Make sure resolution is set to 0.5ms (safest minimum) - this is confirmed to improve
	** NtDelayExecution() and Sleep(). This will only be changed when needed, not per frame.
	*/
	ULONG curRes, minRes, maxRes, junk;
	if (canAdjustTimerResolution && NtQueryTimerResolution(&minRes, &maxRes, &curRes) == STATUS_SUCCESS)
	{
		if (curRes != 5000 && maxRes <= 5000)
			NtSetTimerResolution(5000, TRUE, &junk); // 0.5ms
	}
#endif

	const uint64_t currTime64 = SDL_GetPerformanceCounter();
	if (currTime64 < hpc->endTimeInt)
	{
		uint64_t timeLeft64 = hpc->endTimeInt - currTime64;

		// convert to int32_t for fast SSE2 SIMD usage lateron
		if (timeLeft64 > INT32_MAX)
			timeLeft64 = INT32_MAX;

		const int32_t timeLeft32 = (int32_t)timeLeft64;

		int32_t microSecsLeft = (int32_t)((timeLeft32 * hpcFreq.dFreqMulMicro) + 0.5); // rounded
		if (microSecsLeft > 0)
			usleep(microSecsLeft);
	}

	// set next end time

	hpc->endTimeInt += hpc->durationInt;

	// handle fractional part
	hpc->endTimeFrac += hpc->durationFrac;
	if (hpc->endTimeFrac >= DURATION_FRAC_SCALE)
	{
		hpc->endTimeFrac &= DURATION_FRAC_MASK;
		hpc->endTimeInt++;
	}

	/* The counter ("endTimeInt") can accumulate major errors after a couple of hours,
	** since each frame is not happening at perfect intervals.
	** To fix this, reset the counter's int & frac once every half an hour. We should only
	** get up to one frame of jitter while they are resetting, then it's back to normal.
	*/
	hpc->frameCounter++;
	if (hpc->frameCounter >= hpc->resetFrame)
	{
		hpc->frameCounter = 0;
		hpc_ResetCounters(hpc);
	}
}
