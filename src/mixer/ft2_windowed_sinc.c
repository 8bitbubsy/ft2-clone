// 8-point/16-point windowed-sinc interpolation LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_windowed_sinc.h"
#include "../ft2_header.h" // PI
#include "../ft2_video.h" // showErrorMsgBox()

typedef struct
{
	float kaiserBeta, sincCutoff;
} sincKernel_t;

// globalized
float *fSinc[SINC_KERNELS], *fSinc8[SINC_KERNELS], *fSinc16[SINC_KERNELS];
uint64_t sincRatio1, sincRatio2;

static sincKernel_t sincKernelConfig[2][SINC_KERNELS] =
{
	/* Some notes on the Kaiser-Bessel beta parameter:
	** Lower beta = less treble cut off, more aliasing (narrower mainlobe, stronger sidelobe)
	** Higher beta = more treble cut off, less aliasing (wider mainlobe, weaker sidelobe)
	**
	** The first 8-point kernel should not have a beta lower than around 9.2, as it
	** results in audible ringing at very low resampling ratios (well below 1.0, that is).
	*/

	{ // -- settings for 8-point sinc --
		// beta, cutoff
		{  9.2f, 1.000f }, // kernel #1
		{  8.5f, 0.750f }, // kernel #2
		{  7.3f, 0.425f }  // kernel #3
	},

	{ // -- settings for 16-point sinc --
		// beta, cutoff
		{  8.6f, 1.000f }, // kernel #1
		{  8.5f, 0.750f }, // kernel #2
		{  7.3f, 0.425f }  // kernel #3
	}
};

// zeroth-order modified Bessel function of the first kind (series approximation)
static inline float besselI0(float z)
{
	float s = 1.0f, ds = 1.0f, d = 2.0f;
	const float zz = z * z;

	do
	{
		ds *= zz / (d * d);
		s += ds;
		d += 2.0f;
	}
	while (ds > s*(1E-7f));

	return s;
}

static inline float sinc(float x, const float cutoff)
{
	if (x == 0.0f)
	{
		return cutoff;
	}
	else
	{
		x *= (float)PI;
		return sinf(cutoff * x) / x;
	}
}

// note: numPoints/numPhases must be 2^n!
static void makeSincKernel(float *out, int32_t numPoints, int32_t numPhases, float beta, float cutoff)
{
	const int32_t kernelLen = numPhases * numPoints;
	const int32_t pointBits = (int32_t)log2(numPoints);
	const int32_t pointMask = numPoints - 1;
	const int32_t centerPoint = (numPoints / 2) - 1;
	const float besselI0Beta = 1.0f / besselI0(beta);
	const float phaseMul = 1.0f / numPhases;
	const float xMul = 1.0f / (numPoints / 2);

	for (int32_t i = 0; i < kernelLen; i++)
	{
		const float x = (float)((i & pointMask) - centerPoint) - ((float)(i >> pointBits) * phaseMul);

		// Kaiser-Bessel window
		const float n = x * xMul;
		const float window = besselI0(beta * sqrtf(1.0f - n * n)) * besselI0Beta;

		out[i] = sinc(x, cutoff) * window;
	}
}

bool setupWindowedSincTables(void)
{
	sincKernel_t *k;
	for (int32_t i = 0; i < SINC_KERNELS; i++, k++)
	{
		fSinc8[i]  = (float *)malloc( 8 * SINC_PHASES * sizeof (float));
		fSinc16[i] = (float *)malloc(16 * SINC_PHASES * sizeof (float));

		if (fSinc8[i] == NULL || fSinc16[i] == NULL)
		{
			showErrorMsgBox("Not enough memory!");
			return false;
		}

		k = &sincKernelConfig[0][i];
		makeSincKernel(fSinc8[i], 8, SINC_PHASES, k->kaiserBeta, k->sincCutoff);

		k = &sincKernelConfig[1][i];
		makeSincKernel(fSinc16[i], 16, SINC_PHASES, k->kaiserBeta, k->sincCutoff);
	}

	// resampling ratios for sinc kernel selection
	sincRatio1 = (uint64_t)(1.1875 * MIXER_FRAC_SCALE); // fSinc[0] if <=
	sincRatio2 = (uint64_t)(1.5000 * MIXER_FRAC_SCALE); // fSinc[1] if <=, else fSinc[2] if >

	return true;
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
