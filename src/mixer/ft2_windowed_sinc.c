#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_windowed_sinc.h"

// globalized
double *gKaiserSinc = NULL;
double *gDownSample1 = NULL;
double *gDownSample2 = NULL;

/* Code taken from the OpenMPT project, which has a BSD
** license which is compatible with this this project.
**
** The code has been slightly modified.
*/

static double Izero(double y) // Compute Bessel function Izero(y) using a series approximation
{
	double s = 1.0, ds = 1.0, d = 0.0;

	const double epsilon = 1E-9; // 8bb: 1E-7 -> 1E-9 for added precision (still fast to calculate)

	do
	{
		d = d + 2.0;
		ds = ds * (y * y) / (d * d);
		s = s + ds;
	}
	while (ds > epsilon * s);

	return s;
}

static void getSinc(double *dLUTPtr, const double beta, const double cutoff)
{
	const double izeroBeta = Izero(beta);
	const double kPi = 4.0 * atan(1.0) * cutoff; // M_PI can't be trusted

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

		dLUTPtr[i] = dSinc * cutoff;
	}
}

bool calcWindowedSincTables(void)
{
	gKaiserSinc  = (double *)malloc(SINC_LUT_LEN * sizeof (double));
	gDownSample1 = (double *)malloc(SINC_LUT_LEN * sizeof (double));
	gDownSample2 = (double *)malloc(SINC_LUT_LEN * sizeof (double));

	if (gKaiserSinc == NULL || gDownSample1 == NULL || gDownSample2 == NULL)
		return false;

	// OpenMPT parameters
	getSinc(gKaiserSinc, 9.6377, 0.97);
	getSinc(gDownSample1, 8.5, 0.5);
	getSinc(gDownSample2, 2.7625, 0.425);

	return true;
}

void freeWindowedSincTables(void)
{
	if (gKaiserSinc != NULL)
	{
		free(gKaiserSinc);
		gKaiserSinc = NULL;
	}

	if (gDownSample1 != NULL)
	{
		free(gDownSample1);
		gDownSample1 = NULL;
	}

	if (gDownSample2 != NULL)
	{
		free(gDownSample2);
		gDownSample2 = NULL;
	}
}
