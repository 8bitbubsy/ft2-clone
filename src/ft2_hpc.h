#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint64_t freq64;
	double dFreqMulMicro, dFreqMulMs;
} hpcFreq_t;

typedef struct
{
	uint64_t durationInt, durationFrac;
	uint64_t endTimeInt, endTimeFrac;
	uint64_t frameCounter, resetFrame;
} hpc_t;

extern hpcFreq_t hpcFreq;

void hpc_Init(void);
void hpc_SetDurationInHz(hpc_t *hpc, uint32_t dHz);
void hpc_ResetCounters(hpc_t *hpc);
void hpc_Wait(hpc_t *hpc);
