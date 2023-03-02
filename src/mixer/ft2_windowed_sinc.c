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

float *fKaiserSinc = NULL, *fDownSample1 = NULL, *fDownSample2 = NULL; // globalized

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

static void getSinc(float *fLUTPtr, const double beta, const double cutoff)
{
	const double izeroBeta = Izero(beta);
	const double kPi = (4.0 * atan(1.0)) * cutoff; // M_PI can't be trusted

	for (int32_t i = 0; i < SINC_LUT_LEN; i++)
	{
		double dSinc;
		int32_t ix = (SINC_TAPS-1) - (i & (SINC_TAPS-1));

		ix = (ix * SINC_PHASES) + (i >> SINC_TAPS_BITS);
		if (ix == SINC_MID_TAP)
		{
			dSinc = 1.0;
		}
		else
		{
			const double x = (ix - SINC_MID_TAP) * (1.0 / SINC_PHASES);
			const double xPi = x * kPi;

			const double xMul = 1.0 / ((SINC_TAPS/2) * (SINC_TAPS/2));
			dSinc = sin(xPi) * Izero(beta * sqrt(1.0 - x * x * xMul)) / (izeroBeta * xPi); // Kaiser window
		}

		fLUTPtr[i] = (float)(dSinc * cutoff);
	}
}

bool calcWindowedSincTables(void)
{
	fKaiserSinc  = (float *)malloc(SINC_LUT_LEN * sizeof (float));
	fDownSample1 = (float *)malloc(SINC_LUT_LEN * sizeof (float));
	fDownSample2 = (float *)malloc(SINC_LUT_LEN * sizeof (float));

	if (fKaiserSinc == NULL || fDownSample1 == NULL || fDownSample2 == NULL)
		return false;

	getSinc(fKaiserSinc, 9.6377, 0.97);
	getSinc(fDownSample1, 8.5, 0.5);
	getSinc(fDownSample2, 7.3, 0.425); // 8bitbubsy: tweaked the beta value (was aliasing quite a bit)

	return true;
}

void freeWindowedSincTables(void)
{
	if (fKaiserSinc != NULL)
	{
		free(fKaiserSinc);
		fKaiserSinc = NULL;
	}

	if (fDownSample1 != NULL)
	{
		free(fDownSample1);
		fDownSample1 = NULL;
	}

	if (fDownSample2 != NULL)
	{
		free(fDownSample2);
		fDownSample2 = NULL;
	}
}
