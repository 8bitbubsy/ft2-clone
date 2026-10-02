// polyphase windowed-sinc LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "../ft2_header.h" // MY_PI
#include "../ft2_audio.h" // voice_t
#include "../ft2_config.h" // config.interpolation
#include "../ft2_video.h" // showErrorMsgBox()
#include "ft2_windowed_sinc.h"

typedef struct
{
	double kaiserBeta, sincCutoff;
} sincKernel_t;

// what sinc kernel to use based on resampling ratio
#define KERNEL1_RATIO_LIMIT 1.1875 /*      if ratio < x, kernel[0] */
#define KERNEL2_RATIO_LIMIT 1.5002 /* else if ratio < x, kernel[1] */
#define KERNEL3_RATIO_LIMIT 2.1000 /* else if ratio < x, kernel[2], else kernel[3] */

static sincKernel_t sincKernelConfig[SINC_KERNELS] =
{
	// kaiser-beta   cutoff
	{    9.6568,      1.00  }, // kernel #1
	{    8.1933,      0.77  }, // kernel #2
	{    7.1939,      0.52  }, // kernel #3
	{    3.6517,      0.33  }  // kernel #4
};

static float *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];

/* Polyphase sinc LUT generator w/ Kaiser-Bessel window.
**
** - The 'sincCutoff' parameter ranges from 0.0 to 1.0, where 1.0 is no cutoff.
** - The 'numTaps' parameter *must* be an even number.
*/
static bool calcPolyphaseSincLUT(float *fOut, int32_t numTaps, int32_t numPhases, double kaiserBeta, double sincCutoff);

bool setupWindowedSincTables(void)
{
	sincKernel_t *k = sincKernelConfig;
	for (int32_t i = 0; i < SINC_KERNELS; i++, k++)
	{
		// (+1 comes from the need for an additional phase for interpolation)
		 fSinc8[i] = (float *)malloc((SINC_OVERSAMPLING+1) *  SINC8_TAPS * sizeof (float));
		fSinc16[i] = (float *)malloc((SINC_OVERSAMPLING+1) * SINC16_TAPS * sizeof (float));

		if (fSinc8[i] == NULL || fSinc16[i] == NULL)
			goto outOfMemory;

		if (!calcPolyphaseSincLUT(fSinc8[i], SINC8_TAPS, SINC_OVERSAMPLING, k->kaiserBeta, k->sincCutoff))
			goto outOfMemory;

		if (!calcPolyphaseSincLUT(fSinc16[i], SINC16_TAPS, SINC_OVERSAMPLING, k->kaiserBeta, k->sincCutoff))
			goto outOfMemory;
	}

	return true;

outOfMemory:
	showErrorMsgBox("Not enough memory!");
	return false; // potentially allocated memory is free'd up later
}

void freeWindowedSincTables(void)
{
	for (int32_t i = 0; i < SINC_KERNELS; i++)
	{
		if (fSinc8[i] != NULL)
		{
			free(fSinc8[i]);
			fSinc8[i] = NULL;
		}

		if (fSinc16[i] != NULL)
		{
			free(fSinc16[i]);
			fSinc16[i] = NULL;
		}
	}
}

void setWindowedSincIntrpTable(voice_t *v)
{
	const float **fSincLUT = (config.interpolation == INTERPOLATION_SINC16) ? fSinc16 : fSinc8;

	if (v->delta < (uint64_t)(KERNEL1_RATIO_LIMIT * MIXER_FRAC_SCALE))
		v->fSincLUT = fSincLUT[0];
	else if (v->delta < (uint64_t)(KERNEL2_RATIO_LIMIT * MIXER_FRAC_SCALE))
		v->fSincLUT = fSincLUT[1];
	else if (v->delta < (uint64_t)(KERNEL3_RATIO_LIMIT * MIXER_FRAC_SCALE))
		v->fSincLUT = fSincLUT[2];
	else
		v->fSincLUT = fSincLUT[3];
}

// zeroth-order modified Bessel function of the first kind (series approximation)
static inline double besselI0(double z)
{
	double s = 1.0, ds = 1.0, d = 2.0;
	const double zz = z * z;

	do
	{
		ds *= zz / (d * d);
		s += ds;
		d += 2.0;
	}
	while (ds > s*(1E-10));

	return s;
}

static inline double sinc(double x, double cutoff)
{
	if (x == 0.0)
	{
		return cutoff;
	}
	else
	{
		x *= MY_PI;
		return sin(cutoff * x) / x;
	}
}

static bool calcPolyphaseSincLUT(float *fOut, int32_t numTaps, int32_t numPhases, double kaiserBeta, double sincCutoff)
{
	double *tapBuffer = (double *)malloc(numTaps * sizeof (double));
	if (tapBuffer == NULL)
		return false;

	const int32_t centerPoint = (numTaps / 2) - 1;
	const double besselI0BetaMul = 1.0 / besselI0(kaiserBeta);
	const double phaseMul = 1.0 / numPhases;
	const double kaiserXMul = 1.0 / (numTaps / 2);

	float *fOutPtr = fOut;
	for (int32_t i = 0; i < numPhases; i++)
	{
		const double phase = i * phaseMul;

		double tapSum = 0.0;
		for (int32_t j = 0; j < numTaps; j++)
		{
			const double x = (j - centerPoint) - phase;

			// Kaiser-Bessel window
			const double n = x * kaiserXMul;
			const double window = besselI0(kaiserBeta * sqrt(1.0 - n * n)) * besselI0BetaMul;

			const double wsinc = sinc(x, sincCutoff) * window;
			tapBuffer[j] = wsinc;
			tapSum += wsinc;
		}

		// normalize for unity gain before storing taps
		const double tapMul = 1.0 / tapSum;
		for (int32_t j = 0; j < numTaps; j++)
			*fOutPtr++ = (float)(tapBuffer[j] * tapMul);
	}

	free(tapBuffer);

	// store inverted copy of first phase after end of LUT (for interpolation look-up)
	for (int32_t i = 0; i < numTaps; i++)
		*fOutPtr++ = fOut[(numTaps-1) - i];

	return true;
}
