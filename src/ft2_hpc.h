#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint64_t freq64;
	double dFreq, dFreqMulMicro, dFreqMulMs;
} hpcFreq_t;

typedef struct
{
	uint64_t duration64Int, duration64Frac;
	uint64_t endTime64Int, endTime64Frac;
} hpc_t;

extern hpcFreq_t hpcFreq;

void hpc_Init(void);
void hpc_SetDurationInHz(hpc_t *hpc, double dHz);
void hpc_ResetEndTime(hpc_t *hpc);
void hpc_Wait(hpc_t *hpc);
