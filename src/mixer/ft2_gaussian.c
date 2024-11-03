/*
** Super Nintendo SPC700 interpolation LUT generator.
** This has long believed to have a Gaussian curve, but it doesn't.
**
** Based on code by Mednafen and nocash:
** https://forums.nesdev.org/viewtopic.php?t=10586
**
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_gaussian.h"
#include "../ft2_video.h" // showErrorMsgBox()

#define MY_PI 3.14159265358979323846264338327950288

float *fGaussianLUT = NULL; // globalized

bool calcGaussianTable(void)
{
	fGaussianLUT = (float *)malloc(GAUSSIAN_TAPS*GAUSSIAN_PHASES * sizeof (float));
	if (fGaussianLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	float *fPtr = fGaussianLUT;
	for (int32_t i = 0; i < GAUSSIAN_PHASES; i++)
	{
		const int32_t i1 = GAUSSIAN_PHASES + i;
		const int32_t i2 = i;
		const int32_t i3 = (GAUSSIAN_PHASES-1) - i;
		const int32_t i4 = ((GAUSSIAN_PHASES*2)-1) - i;

		const double x1 = (0.5 + i1) * (1.0 / ((GAUSSIAN_PHASES*4)-1));
		const double x2 = (0.5 + i2) * (1.0 / ((GAUSSIAN_PHASES*4)-1));
		const double x3 = (0.5 + i3) * (1.0 / ((GAUSSIAN_PHASES*4)-1));
		const double x4 = (0.5 + i4) * (1.0 / ((GAUSSIAN_PHASES*4)-1));

		// Blackman window
		const double w1 = (0.42 + (0.50 * cos(2.0 * MY_PI * x1)) + (0.08 * cos(4.0 * MY_PI * x1))) / x1;
		const double w2 = (0.42 + (0.50 * cos(2.0 * MY_PI * x2)) + (0.08 * cos(4.0 * MY_PI * x2))) / x2;
		const double w3 = (0.42 + (0.50 * cos(2.0 * MY_PI * x3)) + (0.08 * cos(4.0 * MY_PI * x3))) / x3;
		const double w4 = (0.42 + (0.50 * cos(2.0 * MY_PI * x4)) + (0.08 * cos(4.0 * MY_PI * x4))) / x4;

		const double t1 = sin(1.28 * MY_PI * x1) * w1;
		const double t2 = sin(1.28 * MY_PI * x2) * w2;
		const double t3 = sin(1.28 * MY_PI * x3) * w3;
		const double t4 = sin(1.28 * MY_PI * x4) * w4;

		// normalization value (assures unity gain when summing taps)
		const double dScale = 1.0 / (t1 + t2 + t3 + t4);

		*fPtr++ = (float)(t1 * dScale);
		*fPtr++ = (float)(t2 * dScale);
		*fPtr++ = (float)(t3 * dScale);
		*fPtr++ = (float)(t4 * dScale);
	}

	return true;
}

void freeGaussianTable(void)
{
	if (fGaussianLUT != NULL)
	{
		free(fGaussianLUT);
		fGaussianLUT = NULL;
	}
}
