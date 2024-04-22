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
float *fKaiserSinc_16 = NULL, *fDownSample1_16 = NULL, *fDownSample2_16 = NULL;
uint64_t sincDownsample1Ratio, sincDownsample2Ratio;

// set based on selected sinc interpolator (8 point or 16 point)
float *fKaiserSinc = NULL, *fDownSample1 = NULL, *fDownSample2 = NULL;

// zeroth-order modified Bessel function of the first kind (series approximation)
static double besselI0(double z)
{
#define EPSILON (1E-15)

	double s = 1.0, ds = 1.0, d = 2.0;
	const double zz = z * z;

	do
	{
		ds *= zz / (d * d);
		s += ds;
		d += 2.0;
	}
	while (ds > s*EPSILON);

	return s;
}

static void getSinc(uint32_t numTaps, float *fLUTPtr, const double beta, const double cutoff)
{
	const double I0Beta = besselI0(beta);
	const double kPi = MY_PI * cutoff;
	const double xMul = 1.0 / ((numTaps / 2) * (numTaps / 2));

	const uint32_t length = numTaps * SINC_PHASES;
	const uint32_t tapBits = (uint32_t)log2(numTaps);
	const uint32_t tapsMinus1 = numTaps - 1;
	const int32_t midPoint = (numTaps / 2) * SINC_PHASES;

	for (uint32_t i = 0; i < length; i++)
	{
		const int32_t ix = ((tapsMinus1 - (i & tapsMinus1)) << SINC_PHASES_BITS) + (i >> tapBits);

		double dSinc = 1.0;
		if (ix != midPoint)
		{
			const double x = (ix - midPoint) * (1.0 / SINC_PHASES);
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

	fKaiserSinc_16  = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));
	fDownSample1_16 = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));
	fDownSample2_16 = (float *)malloc(SINC2_TAPS*SINC_PHASES * sizeof (float));

	if (fKaiserSinc_8  == NULL || fDownSample1_8  == NULL || fDownSample2_8  == NULL ||
		fKaiserSinc_16 == NULL || fDownSample1_16 == NULL || fDownSample2_16 == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	sincDownsample1Ratio = (uint64_t)(1.1875 * MIXER_FRAC_SCALE);
	sincDownsample2Ratio = (uint64_t)(1.5    * MIXER_FRAC_SCALE);

	// sidelobe attenuation (Kaiser beta)
	const double b0 = dBToKaiserBeta(96.15645);
	const double b1 = dBToKaiserBeta(85.83249);
	const double b2 = dBToKaiserBeta(72.22088);

	// 8 point
	getSinc(SINC1_TAPS, fKaiserSinc_8,  b0, 1.0);
	getSinc(SINC1_TAPS, fDownSample1_8, b1, 0.5);
	getSinc(SINC1_TAPS, fDownSample2_8, b2, 0.425);

	// 16 point
	getSinc(SINC2_TAPS, fKaiserSinc_16,  b0, 1.0);
	getSinc(SINC2_TAPS, fDownSample1_16, b1, 0.5);
	getSinc(SINC2_TAPS, fDownSample2_16, b2, 0.425);

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

	if (fKaiserSinc_16 != NULL)
	{
		free(fKaiserSinc_16);
		fKaiserSinc_16 = NULL;
	}

	if (fDownSample1_16 != NULL)
	{
		free(fDownSample1_16);
		fDownSample1_16 = NULL;
	}

	if (fDownSample2_16 != NULL)
	{
		free(fDownSample2_16);
		fDownSample2_16 = NULL;
	}
}
