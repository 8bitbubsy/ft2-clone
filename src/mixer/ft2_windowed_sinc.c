/* Code taken from the OpenMPT project, which has a BSD license which is
** compatible with this project.
**
** The code has been slightly modified.
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

// set based on selected sinc interpolator (8 point or 16 point)
float *fKaiserSinc = NULL, *fDownSample1 = NULL, *fDownSample2 = NULL;

static double Izero(double y) // Compute Bessel function Izero(y) using a series approximation
{
	double s = 1.0, ds = 1.0, d = 0.0;

	const double epsilon = 1E-9; // 8bitbubsy: 1E-7 -> 1E-9 for added precision (still fast to calculate)

	do
	{
		d = d + 2.0;
		ds = ds * (y * y) / (d * d);
		s = s + ds;
	}
	while (ds > epsilon * s);

	return s;
}

static void getSinc(uint32_t numTaps, float *fLUTPtr, const double beta, const double cutoff)
{
	const double izeroBeta = Izero(beta);
	const double kPi = MY_PI * cutoff;

	const uint32_t length = numTaps * SINC_PHASES;
	const uint32_t tapBits = (int32_t)log2(numTaps);
	const uint32_t tapsMinus1 = numTaps - 1;
	const double xMul = 1.0 / ((numTaps / 2) * (numTaps / 2));
	const int32_t midTap = (numTaps / 2) * SINC_PHASES;

	for (uint32_t i = 0; i < length; i++)
	{
		const int32_t ix = ((tapsMinus1 - (i & tapsMinus1)) * SINC_PHASES) + (i >> tapBits);

		double dSinc;
		if (ix == midTap)
		{
			dSinc = 1.0;
		}
		else
		{
			const double x = (ix - midTap) * (1.0 / SINC_PHASES);
			const double xPi = x * kPi;
			dSinc = sin(xPi) * Izero(beta * sqrt(1.0 - x * x * xMul)) / (izeroBeta * xPi); // Kaiser window
		}

		fLUTPtr[i] = (float)(dSinc * cutoff);
	}
}

bool calcWindowedSincTables(void)
{ 
	fKaiserSinc_8  = (float *)malloc(8*SINC_PHASES * sizeof (float));
	fDownSample1_8 = (float *)malloc(8*SINC_PHASES * sizeof (float));
	fDownSample2_8 = (float *)malloc(8*SINC_PHASES * sizeof (float));

	fKaiserSinc_16  = (float *)malloc(16*SINC_PHASES * sizeof (float));
	fDownSample1_16 = (float *)malloc(16*SINC_PHASES * sizeof (float));
	fDownSample2_16 = (float *)malloc(16*SINC_PHASES * sizeof (float));

	if (fKaiserSinc_8  == NULL || fDownSample1_8  == NULL || fDownSample2_8  == NULL ||
		fKaiserSinc_16 == NULL || fDownSample1_16 == NULL || fDownSample2_16 == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	getSinc(8, fKaiserSinc_8, 9.6377, 1.0);
	getSinc(8, fDownSample1_8, 8.5, 0.5);
	getSinc(8, fDownSample2_8, 7.3, 0.425);

	getSinc(16, fKaiserSinc_16, 9.6377, 1.0);
	getSinc(16, fDownSample1_16, 8.5, 0.5);
	getSinc(16, fDownSample2_16, 7.3, 0.425);

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
