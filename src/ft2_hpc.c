/*
** Hardware Performance Counter delay routines by 8bitbubsy.
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
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_hpc.h"

#define FRAC_BITS 63 /* leaves one bit for frac overflow test */
#define FRAC_SCALE (1ULL << FRAC_BITS)
#define FRAC_MASK (FRAC_SCALE-1)

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

// returns 64-bit fractional part of u64 divided by u32
static uint64_t getFrac64FromU64DivU32(uint64_t dividend, uint32_t divisor)
{
	if (dividend == 0 || divisor == 0 || divisor >= dividend)
		return 0;

	dividend %= divisor;

	if (dividend == 0)
		return 0;

	const uint32_t quotient  = (uint32_t)((dividend << 32) / divisor);
	const uint32_t remainder = (uint32_t)((dividend << 32) % divisor);

	const uint32_t resultHi = quotient;
	const uint32_t resultLo = (uint32_t)(((uint64_t)remainder << 32) / divisor);

	return ((uint64_t)resultHi << 32) | resultLo;
}

void hpc_SetDurationInHz(hpc_t *hpc, double dHz) // dHz = max 4095.999inf Hz (0.24ms)
{
#define BITS_IN_UINT32 32

	/* 20 = Good compensation between fraction bits and max integer size.
	** Most non-realtime OSes probably can't do a thread delay with such a
	** high precision ( 0.24ms, 1000/(2^(32-20)-1) ) anyway.
	*/
#define INPUT_FRAC_BITS 20
#define INPUT_FRAC_SCALE (1UL << INPUT_FRAC_BITS)
#define INPUT_INT_MAX ((1UL << (BITS_IN_UINT32-INPUT_FRAC_BITS))-1)

	if (dHz > INPUT_INT_MAX)
		dHz = INPUT_INT_MAX;

	const uint32_t fpHz = (uint32_t)((dHz * INPUT_FRAC_SCALE) + 0.5);

	// set 64:63fp value
	const uint64_t fpFreq64 = hpcFreq.freq64 << INPUT_FRAC_BITS;
	hpc->durationInt = fpFreq64 / fpHz;
	hpc->durationFrac = getFrac64FromU64DivU32(fpFreq64, fpHz) >> 1; // 64 -> 63 bits (1 bit for frac overflow test)

	hpc->resetFrame = ((uint64_t)fpHz * (60 * 30)) / INPUT_FRAC_SCALE; // reset counters every half an hour
}

void hpc_SetDurationInMs(hpc_t *hpc, double dMs) // dMs = minimum 0.2442002442 ms
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
#ifdef __EMSCRIPTEN__
			if (true)
			{
				// The right way
				emscripten_sleep(microSecsLeft / 1000);
			}
			else
			{
				// Just using emscripten_sleep() to yield to the main thread
				emscripten_sleep(1);
			}
#else
			usleep(microSecsLeft);
#endif
	}

	// set next end time

	hpc->endTimeInt += hpc->durationInt;

	// handle fractional part
	hpc->endTimeFrac += hpc->durationFrac;
	if (hpc->endTimeFrac >= FRAC_SCALE)
	{
		hpc->endTimeFrac &= FRAC_MASK;
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
