/*
** Hardware Performance Counter delay routines
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

#define FRAC_BITS 53
#define FRAC_SCALE (1ULL << FRAC_BITS)
#define FRAC_MASK (FRAC_SCALE-1)

hpcFreq_t hpcFreq;

#ifdef _WIN32 // Windows usleep() implementation

static NTSTATUS (__stdcall *NtDelayExecution)(BOOL Alertable, PLARGE_INTEGER DelayInterval);
static NTSTATUS (__stdcall *NtQueryTimerResolution)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG ActualResolution);
static NTSTATUS (__stdcall *NtSetTimerResolution)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

static void (*usleep)(int32_t usec);

static void usleepGood(int32_t usec)
{
	LARGE_INTEGER delayInterval;

	// NtDelayExecution() delays in 100ns-units, and negative value = delay from current time
	usec *= -10;

	delayInterval.HighPart = 0xFFFFFFFF;
	delayInterval.LowPart = usec;
	NtDelayExecution(false, &delayInterval);
}

static void usleepWeak(int32_t usec) // fallback if no NtDelayExecution()
{
	Sleep((usec + 500) / 1000);
}

static void windowsSetupUsleep(void)
{
	NtDelayExecution = (NTSTATUS (__stdcall *)(BOOL, PLARGE_INTEGER))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDelayExecution");
	NtQueryTimerResolution = (NTSTATUS (__stdcall *)(PULONG, PULONG, PULONG))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryTimerResolution");
	NtSetTimerResolution = (NTSTATUS (__stdcall *)(ULONG, BOOLEAN, PULONG))GetProcAddress(GetModuleHandle("ntdll.dll"), "NtSetTimerResolution");

	usleep = (NtDelayExecution != NULL) ? usleepGood : usleepWeak;
}
#endif

void hpc_Init(void)
{
#ifdef _WIN32
	windowsSetupUsleep();
#endif
	hpcFreq.freq64 = SDL_GetPerformanceFrequency();
	hpcFreq.dFreq = (double)hpcFreq.freq64;
	hpcFreq.dFreqMulMicro = (1000.0 * 1000.0) / hpcFreq.dFreq;
}

void hpc_SetDurationInHz(hpc_t *hpc, const double dHz)
{
	const double dDuration = hpcFreq.dFreq / dHz;

	// break down duration into integer and frac parts
	double dDurationInt;
	double dDurationFrac = modf(dDuration, &dDurationInt);

	// set 64:53fp values
	hpc->duration64Int = (uint64_t)dDurationInt;
	hpc->duration64Frac = (uint64_t)round(dDurationFrac * FRAC_SCALE);
}

void hpc_ResetEndTime(hpc_t *hpc)
{
	hpc->endTime64Int = SDL_GetPerformanceCounter() + hpc->duration64Int;
	hpc->endTime64Frac = hpc->duration64Frac;
}

void hpc_Wait(hpc_t *hpc)
{
#ifdef _WIN32 // set resolution to 0.5ms (safest minium) - this is confirmed to improve NtDelayExecution() and Sleep()
	ULONG originalTimerResolution, minRes, maxRes, curRes;

	if (NtQueryTimerResolution != NULL && NtSetTimerResolution != NULL)
	{
		if (!NtQueryTimerResolution(&minRes, &maxRes, &originalTimerResolution))
		{
			if (originalTimerResolution != 5000 && maxRes <= 5000)
				NtSetTimerResolution(5000, TRUE, &curRes); // set to 0.5ms (safest minimum)
		}
	}
#endif

	const uint64_t currTime64 = SDL_GetPerformanceCounter();
	if (currTime64 < hpc->endTime64Int)
	{
		uint64_t timeLeft64 = hpc->endTime64Int - currTime64;

		// convert to int32_t for fast SSE2 SIMD usage lateron
		if (timeLeft64 > INT32_MAX)
			timeLeft64 = INT32_MAX;

		const int32_t timeLeft32 = (int32_t)timeLeft64;

		int32_t microSecsLeft = (int32_t)((timeLeft32 * hpcFreq.dFreqMulMicro) + 0.5); // rounded
		if (microSecsLeft > 0)
			usleep(microSecsLeft);
	}

	// set next end time

	hpc->endTime64Int += hpc->duration64Int;

	hpc->endTime64Frac += hpc->duration64Frac;
	if (hpc->endTime64Frac >= FRAC_SCALE)
	{
		hpc->endTime64Frac &= FRAC_MASK;
		hpc->endTime64Int++;
	}
}
