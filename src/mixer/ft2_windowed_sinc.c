/*
** Windowed-sinc (Kaiser window) w/ low-pass interpolation LUT generator
**
** This was originally based on OpenMPT's source code,
** but it has been heavily modified.
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

static void generateSincLUT(float *fOutput, uint32_t filterWidth, uint32_t numPhases, const double beta, const double lpCutoff)
{
	const double I0Beta = besselI0(beta);
	const double kPi = MY_PI * lpCutoff;
	const double iMul = 1.0 / numPhases;
	const double xMul = 1.0 / ((filterWidth / 2) * (filterWidth / 2));

	const uint32_t length = filterWidth * numPhases;
	const uint32_t tapBits = (uint32_t)log2(filterWidth);
	const uint32_t phasesBits = (uint32_t)log2(numPhases);
	const uint32_t tapsMinus1 = filterWidth - 1;
	const int32_t midPoint = (filterWidth / 2) * numPhases;

	for (uint32_t i = 0; i < length; i++)
	{
		const int32_t ix = ((tapsMinus1 - (i & tapsMinus1)) << phasesBits) + (i >> tapBits);

		double dSinc = 1.0;
		if (ix != midPoint)
		{
			const double x = (ix - midPoint) * iMul;
			const double xPi = x * kPi;

			// Kaiser window
			dSinc = (sin(xPi) * besselI0(beta * sqrt(1.0 - (x * x * xMul)))) / (I0Beta * xPi);
		}

		fOutput[i] = (float)(dSinc * lpCutoff);
	}
}

static double decibelsToKaiserBeta(double dB)
{
	if (dB < 21.0)
		return 0.0;
	else if (dB <= 50.0)
		return 0.5842 * pow(dB - 21.0, 0.4) + 0.07886 * (dB - 21.0);
	else
		return 0.1102 * (dB - 8.7);
}

static double rippleToDecibels(double ripple)
{
	return 20.0 * log10(ripple);
}

bool calcWindowedSincTables(void)
{
	fKaiserSinc_8   = (float *)malloc(SINC1_TAPS*SINC1_PHASES * sizeof (float));
	fDownSample1_8  = (float *)malloc(SINC1_TAPS*SINC1_PHASES * sizeof (float));
	fDownSample2_8  = (float *)malloc(SINC1_TAPS*SINC1_PHASES * sizeof (float));
	fKaiserSinc_16  = (float *)malloc(SINC2_TAPS*SINC2_PHASES * sizeof (float));
	fDownSample1_16 = (float *)malloc(SINC2_TAPS*SINC2_PHASES * sizeof (float));
	fDownSample2_16 = (float *)malloc(SINC2_TAPS*SINC2_PHASES * sizeof (float));

	if (fKaiserSinc_8  == NULL || fDownSample1_8  == NULL || fDownSample2_8  == NULL ||
		fKaiserSinc_16 == NULL || fDownSample1_16 == NULL || fDownSample2_16 == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// resampling ratios for picking suitable downsample LUT
	const double ratio1 = 1.1875; // fKaiserSinc if <=
	const double ratio2 = 1.5; // fDownSample1 if <=, fDownSample2 if >

	sincDownsample1Ratio = (uint64_t)(ratio1 * MIXER_FRAC_SCALE);
	sincDownsample2Ratio = (uint64_t)(ratio2 * MIXER_FRAC_SCALE);

	/* Max ripple (to be converted into Kaiser beta parameter)
	**
	** NOTE:
	**  These may not be the correct values. Proper calculation
	**  is needed, but is beyond my knowledge.
	*/
	const double maxRipple1 = 1 << 16;
	const double maxRipple2 = 1 << 14;
	const double maxRipple3 = 1 << 12;

	// convert max ripple into sidelode attenuation (dB)
	double db1 = rippleToDecibels(maxRipple1);
	double db2 = rippleToDecibels(maxRipple2);
	double db3 = rippleToDecibels(maxRipple3);

	// convert sidelobe attenuation (dB) into Kaiser beta
	const double b1 = decibelsToKaiserBeta(db1);
	const double b2 = decibelsToKaiserBeta(db2);
	const double b3 = decibelsToKaiserBeta(db3);

	// low-pass cutoffs (could maybe use some further tweaking)
	const double c1 = 1.000;
	const double c2 = 0.500;
	const double c3 = 0.425;

	// 8 point
	generateSincLUT(fKaiserSinc_8,   SINC1_TAPS, SINC1_PHASES, b1, c1);
	generateSincLUT(fDownSample1_8,  SINC1_TAPS, SINC1_PHASES, b2, c2);
	generateSincLUT(fDownSample2_8,  SINC1_TAPS, SINC1_PHASES, b3, c3);

	// 16 point
	generateSincLUT(fKaiserSinc_16,  SINC2_TAPS, SINC2_PHASES, b1, c1);
	generateSincLUT(fDownSample1_16, SINC2_TAPS, SINC2_PHASES, b2, c2);
	generateSincLUT(fDownSample2_16, SINC2_TAPS, SINC2_PHASES, b3, c3);

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
