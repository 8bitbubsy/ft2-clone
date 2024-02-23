/* The code in this file is based on code from the OpenMPT project,
** which shares the same coding license as this project.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_windowed_sinc.h"
#include "../ft2_video.h" // showErrorMsgBox()

#define MY_PI 3.14159265358979323846264338327950288

// globalized
float *fKaiserSinc_8 = NULL, *fDownSample1_8 = NULL, *fDownSample2_8 = NULL;
float *fKaiserSinc_32 = NULL, *fDownSample1_32 = NULL, *fDownSample2_32 = NULL;

// set based on selected sinc interpolator (8 point or 32 point)
float *fKaiserSinc = NULL, *fDownSample1 = NULL, *fDownSample2 = NULL;

// zeroth-order modified Bessel function of the first kind (series approximation)
static double besselI0(double z)
{
	double s = 1.0, ds = 1.0, d = 2.0;

	do
	{
		ds *= (z * z) / (d * d);
		s += ds;
		d += 2.0;
	}
	while (ds > s*1E-15);

	return s;
}

static void getSinc(uint32_t numTaps, float *fLUTPtr, const double beta, const double cutoff)
{
	const double I0Beta = besselI0(beta);
	const double kPi = MY_PI * cutoff;

	const uint32_t length = numTaps * SINC_PHASES;
	const uint32_t tapBits = (int32_t)log2(numTaps);
	const uint32_t tapsMinus1 = numTaps - 1;
	const double xMul = 1.0 / ((numTaps / 2) * (numTaps / 2));
	const int32_t midTap = (numTaps / 2) * SINC_PHASES;

	for (uint32_t i = 0; i < length; i++)
	{
		const int32_t ix = ((tapsMinus1 - (i & tapsMinus1)) << SINC_PHASES_BITS) + (i >> tapBits);

		double dSinc = 1.0;
		if (ix != midTap)
		{
			const double x = (ix - midTap) * (1.0 / SINC_PHASES);
			const double xPi = x * kPi;

			// sinc with Kaiser window
			dSinc = (sin(xPi) * besselI0(beta * sqrt(1.0 - (x * x * xMul)))) / (I0Beta * xPi);
		}

		fLUTPtr[i] = (float)(dSinc * cutoff);
	}
}

static double dBToKaiserBeta(double dB)
{
	if (dB < 21.0)
		return 0.0;
	else if (dB <= 50.0)
		return 0.5842 * pow(dB - 21.0, 0.4) + 0.07886 * (dB - 21.0);
	else
		return 0.1102 * (dB - 8.7);
}

bool calcWindowedSincTables(void)
{
	fKaiserSinc_8  = (float *)malloc(SINC1_TAPS*SINC_PHASES * sizeof (float));
	fDownSample1_8 = (float *)malloc(SINC1_TAPS*SINC_PHASES * sizeof (float));
	fDownSample2_8 = (float *)malloc(SINC1_TAPS*SINC_PHASES * sizeof (float));

	fKaiserSinc_32  = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));
	fDownSample1_32 = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));
	fDownSample2_32 = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));

	if (fKaiserSinc_8  == NULL || fDownSample1_8  == NULL || fDownSample2_8  == NULL ||
		fKaiserSinc_32 == NULL || fDownSample1_32 == NULL || fDownSample2_32 == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// 8 point (modelled after OpenMPT)
	getSinc(SINC1_TAPS, fKaiserSinc_8,  dBToKaiserBeta(96.15645), 1.000);
	getSinc(SINC1_TAPS, fDownSample1_8, dBToKaiserBeta(85.83249), 0.500);
	getSinc(SINC1_TAPS, fDownSample2_8, dBToKaiserBeta(72.22088), 0.425);

	// 32 point
	getSinc(SINC2_TAPS, fKaiserSinc_32,  dBToKaiserBeta(96.0), 1.000);
	getSinc(SINC2_TAPS, fDownSample1_32, dBToKaiserBeta(86.0), 0.500);
	getSinc(SINC2_TAPS, fDownSample2_32, dBToKaiserBeta(74.0), 0.425);

	return true;
}

void freeWindowedSincTables(void)
{
	if (fKaiserSinc_8 != NULL)
	{
		free(fKaiserSinc_8);
		fKaiserSinc_8 = NULL;
	}

	if (fDownSample1_8 != NULL)
	{
		free(fDownSample1_8);
		fDownSample1_8 = NULL;
	}

	if (fDownSample2_8 != NULL)
	{
		free(fDownSample2_8);
		fDownSample2_8 = NULL;
	}

	if (fKaiserSinc_32 != NULL)
	{
		free(fKaiserSinc_32);
		fKaiserSinc_32 = NULL;
	}

	if (fDownSample1_32 != NULL)
	{
		free(fDownSample1_32);
		fDownSample1_32 = NULL;
	}

	if (fDownSample2_32 != NULL)
	{
		free(fDownSample2_32);
		fDownSample2_32 = NULL;
	}
}
