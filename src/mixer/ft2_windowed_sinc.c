// 8-point/16-point windowed-sinc interpolation LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "ft2_windowed_sinc.h" // SINCx_WIDTH, SINCx_PHASES
#include "../ft2_header.h" // PI
#include "../ft2_video.h" // showErrorMsgBox()

// globalized
float *fSinc8_1 = NULL, *fSinc8_2 = NULL, *fSinc8_3 = NULL;
float *fSinc16_1 = NULL, *fSinc16_2 = NULL, *fSinc16_3 = NULL;
float *fSinc_1 = NULL, *fSinc_2 = NULL, *fSinc_3 = NULL;
uint64_t sincRatio1, sincRatio2;

// zeroth-order modified Bessel function of the first kind (series approximation)
static inline double besselI0(double z)
{
#define EPSILON (1E-12) /* verified: lower than this makes no change when LUT output is single-precision float */

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

static inline double sinc(double x)
{
	if (x == 0.0)
	{
		return 1.0;
	}
	else
	{
		x *= PI;
		return sin(x) / x;
	}
}

static inline double sincWithCutoff(double x, const double cutoff)
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

static void generateWindowedSinc(float *fOutput, const int32_t filterWidth, const int32_t filterPhases, const double beta, const double cutoff)
{
	const int32_t filterWidthBits = (int32_t)log2(filterWidth);
	const int32_t filterWidthMask = filterWidth - 1;
	const int32_t filterCenter = (filterWidth / 2) - 1;
	const double besselI0Beta = 1.0 / besselI0(beta);
	const double phaseMul = 1.0 / filterPhases;
	const double xMul = 1.0 / (filterWidth / 2);

	if (cutoff < 1.0)
	{
		// windowed-sinc with frequency cutoff
		for (int32_t i = 0; i < filterPhases * filterWidth; i++)
		{
			const double x = ((i & filterWidthMask) - filterCenter) - ((i >> filterWidthBits) * phaseMul);

			// Kaiser-Bessel window
			const double n = x * xMul;
			const double window = besselI0(beta * sqrt(1.0 - n * n)) * besselI0Beta;

			fOutput[i] = (float)(sincWithCutoff(x, cutoff) * window);
		}
	}
	else
	{
		// windowed-sinc with no frequency cutoff
		for (int32_t i = 0; i < filterPhases * filterWidth; i++)
		{
			const double x = ((i & filterWidthMask) - filterCenter) - ((i >> filterWidthBits) * phaseMul);

			// Kaiser-Bessel window
			const double n = x * xMul;
			const double window = besselI0(beta * sqrt(1.0 - n * n)) * besselI0Beta;

			fOutput[i] = (float)(sinc(x) * window);
		}
	}
}

bool setupWindowedSincTables(void)
{
	fSinc8_1  = (float *)malloc(SINC1_WIDTH*SINC1_PHASES * sizeof (float));
	fSinc8_2  = (float *)malloc(SINC1_WIDTH*SINC1_PHASES * sizeof (float));
	fSinc8_3  = (float *)malloc(SINC1_WIDTH*SINC1_PHASES * sizeof (float));
	fSinc16_1 = (float *)malloc(SINC2_WIDTH*SINC2_PHASES * sizeof (float));
	fSinc16_2 = (float *)malloc(SINC2_WIDTH*SINC2_PHASES * sizeof (float));
	fSinc16_3 = (float *)malloc(SINC2_WIDTH*SINC2_PHASES * sizeof (float));

	if (fSinc8_1  == NULL || fSinc8_2  == NULL || fSinc8_3  == NULL ||
		fSinc16_1 == NULL || fSinc16_2 == NULL || fSinc16_3 == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// LUT-select resampling ratios
	const double ratio1 = 1.1875; // fSinc_1 if <=
	const double ratio2 = 1.5; // fSinc_2 if <=, fSinc_3 if >

	// calculate mixer delta limits for LUT-selector
	sincRatio1 = (uint64_t)(ratio1 * MIXER_FRAC_SCALE);
	sincRatio2 = (uint64_t)(ratio2 * MIXER_FRAC_SCALE);

	/* Kaiser-Bessel beta parameter
	**
	** Basically;
	** Lower beta = less treble cut off, but more aliasing (shorter main lobe, more side lobe ripple)
	** Higher beta = more treble cut off, but less aliasing (wider main lobe, less side lobe ripple)
	**
	** There simply isn't any optimal value here, it has to be tweaked to personal preference.
	*/
	const double b1 = 3.0 * M_PI; // alpha = 3.00 (beta = ~9.425)
	const double b2 = 8.5;
	const double b3 = 7.3;

	// sinc low-pass cutoff (could maybe use some further tweaking)
	const double c1 = 1.000;
	const double c2 = 0.500;
	const double c3 = 0.425;

	// 8 point
	generateWindowedSinc(fSinc8_1,  SINC1_WIDTH, SINC1_PHASES, b1, c1);
	generateWindowedSinc(fSinc8_2,  SINC1_WIDTH, SINC1_PHASES, b2, c2);
	generateWindowedSinc(fSinc8_3,  SINC1_WIDTH, SINC1_PHASES, b3, c3);

	// 16 point
	generateWindowedSinc(fSinc16_1, SINC2_WIDTH, SINC2_PHASES, b1, c1);
	generateWindowedSinc(fSinc16_2, SINC2_WIDTH, SINC2_PHASES, b2, c2);
	generateWindowedSinc(fSinc16_3, SINC2_WIDTH, SINC2_PHASES, b3, c3);

	return true;
}

void freeWindowedSincTables(void)
{
	if (fSinc8_1 != NULL)
	{
		free(fSinc8_1);
		fSinc8_1 = NULL;
	}

	if (fSinc8_2 != NULL)
	{
		free(fSinc8_2);
		fSinc8_2 = NULL;
	}

	if (fSinc8_3 != NULL)
	{
		free(fSinc8_3);
		fSinc8_3 = NULL;
	}

	if (fSinc16_1 != NULL)
	{
		free(fSinc16_1);
		fSinc16_1 = NULL;
	}

	if (fSinc16_2 != NULL)
	{
		free(fSinc16_2);
		fSinc16_2 = NULL;
	}

	if (fSinc16_3 != NULL)
	{
		free(fSinc16_3);
		fSinc16_3 = NULL;
	}
}
