// mixer interpolation LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "../ft2_header.h"
#include "../ft2_video.h" // showErrorMsgBox()
#include "ft2_mix_interpolation.h"

typedef struct
{
	double kaiserBeta, sincCutoff;
} sincKernel_t;

static sincKernel_t sincKernelConfig[SINC_KERNELS] =
{
	/* Some notes on the Kaiser-Bessel beta parameter:
	** Lower beta = less treble cut off, more aliasing (narrower mainlobe, stronger sidelobe)
	** Higher beta = more treble cut off, less aliasing (wider mainlobe, weaker sidelobe)
	*/

	//   beta, cutoff
	{  9.6377, 1.000 }, // kernel #1 (lower beta results in audible ringing in some cases)
	{  8.5000, 0.750 }, // kernel #2
	{  7.3000, 0.425 }  // kernel #3
};

// globalized
float *fQuadraticSplineLUT, *fCubicSplineLUT, *fSinc[SINC_KERNELS], *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];
uint64_t sincRatio1, sincRatio2;
// ----------

static void makeSincKernel(float *fOut, int32_t numPoints, int32_t numPhases, double kaiserBeta, double cutoff);

bool setupMixerInterpolationTables(void)
{
	float *fPtr;

	// quadratic spline (3-point)

	fQuadraticSplineLUT = (float *)malloc(QUADRATIC_SPLINE_WIDTH * QUADRATIC_SPLINE_PHASES * sizeof (float));
	if (fQuadraticSplineLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	fPtr = fQuadraticSplineLUT;
	for (int32_t i = 0; i < QUADRATIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / QUADRATIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2

		const double t1 = (x1 * -1.5) + (x2 *  0.5) + 1.0;
		const double t2 = (x1 *  2.0) + (x2 * -1.0);
		const double t3 = (x1 * -0.5) + (x2 *  0.5);

		*fPtr++ = (float)t1; // tap #1 at sample offset 0 (center)
		*fPtr++ = (float)t2; // tap #2 at sample offset 1
		*fPtr++ = (float)t3; // tap #3 at sample offset 2
	}
	
	// cubic spline (4-point)

	fCubicSplineLUT = (float *)malloc(CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES * sizeof (float));
	if (fCubicSplineLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	fPtr = fCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		const double t1 = (x1 * -0.5) + (x2 *  1.0) + (x3 * -0.5);
		const double t2 =               (x2 * -2.5) + (x3 *  1.5) + 1.0;
		const double t3 = (x1 *  0.5) + (x2 *  2.0) + (x3 * -1.5);
		const double t4 =               (x2 * -0.5) + (x3 *  0.5);

		*fPtr++ = (float)t1; // tap #1 at sample offset -1
		*fPtr++ = (float)t2; // tap #2 at sample offset  0 (center)
		*fPtr++ = (float)t3; // tap #3 at sample offset  1
		*fPtr++ = (float)t4; // tap #4 at sample offset  2
	}

	// windowed-sinc (8-point/16-point)

	sincKernel_t *k = sincKernelConfig;
	for (int32_t i = 0; i < SINC_KERNELS; i++, k++)
	{
		 fSinc8[i] = (float *)malloc( 8 * SINC_PHASES * sizeof (float));
		fSinc16[i] = (float *)malloc(16 * SINC_PHASES * sizeof (float));

		if (fSinc8[i] == NULL || fSinc16[i] == NULL)
		{
			showErrorMsgBox("Not enough memory!");
			return false;
		}

		makeSincKernel( fSinc8[i],  8, SINC_PHASES, k->kaiserBeta, k->sincCutoff);
		makeSincKernel(fSinc16[i], 16, SINC_PHASES, k->kaiserBeta, k->sincCutoff);
	}

	// resampling ratios for sinc kernel selection
	sincRatio1 = (uint64_t)(1.1875 * MIXER_FRAC_SCALE); // fSinc[0] if <=
	sincRatio2 = (uint64_t)(1.5000 * MIXER_FRAC_SCALE); // fSinc[1] if <=, else fSinc[2] if >

	return true;
}

void freeMixerInterpolationTables(void)
{
	if (fQuadraticSplineLUT != NULL)
	{
		free(fQuadraticSplineLUT);
		fQuadraticSplineLUT = NULL;
	}

	if (fCubicSplineLUT != NULL)
	{
		free(fCubicSplineLUT);
		fCubicSplineLUT = NULL;
	}

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
	while (ds > s*(1E-12));

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
		x *= PI;
		return sin(cutoff * x) / x;
	}
}

// note: numPoints/numPhases must be 2^n!
static void makeSincKernel(float *fOut, int32_t numPoints, int32_t numPhases, double kaiserBeta, double cutoff)
{
	const int32_t kernelLen = numPhases * numPoints;
	const int32_t pointBits = (int32_t)log2(numPoints);
	const int32_t pointMask = numPoints - 1;
	const int32_t centerPoint = (numPoints / 2) - 1;
	const double besselI0Beta = 1.0 / besselI0(kaiserBeta);
	const double phaseMul = 1.0 / numPhases;
	const double xMul = 1.0 / (numPoints / 2);

	for (int32_t i = 0; i < kernelLen; i++)
	{
		const double x = ((i & pointMask) - centerPoint) - ((i >> pointBits) * phaseMul);

		// Kaiser-Bessel window
		const double n = x * xMul;
		const double window = besselI0(kaiserBeta * sqrt(1.0 - n * n)) * besselI0Beta;

		fOut[i] = (float)(sinc(x, cutoff) * window);
	}
}
