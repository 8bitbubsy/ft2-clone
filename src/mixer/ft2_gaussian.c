/*
** Super Nintendo (SPC700) Gaussian interpolation LUT generator
**
** It was long believed that it uses a Gaussian curve, but it doesn't!
** We still call it Gaussian interpolation in the FT2 clone though, so
** that people recognize it.
**
** Based on code by Mednafen and nocash:
** https://forums.nesdev.org/viewtopic.php?t=10586
**
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_gaussian.h" // GAUSSIAN_TAPS, GAUSSIAN_PHASES
#include "../ft2_header.h" // PI
#include "../ft2_video.h" // showErrorMsgBox()

/*
**  1.28 = Super Nintendo
** 2.048 = Sony PlayStation (less aliasing on very low pitches)
*/
#define PI_MULTIPLIER 1.28

#define TAP_SUM_SCALE 1.0

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
		const double w1 = (0.42 + (0.50 * cos(2.0 * PI * x1)) + (0.08 * cos(4.0 * PI * x1))) / x1;
		const double w2 = (0.42 + (0.50 * cos(2.0 * PI * x2)) + (0.08 * cos(4.0 * PI * x2))) / x2;
		const double w3 = (0.42 + (0.50 * cos(2.0 * PI * x3)) + (0.08 * cos(4.0 * PI * x3))) / x3;
		const double w4 = (0.42 + (0.50 * cos(2.0 * PI * x4)) + (0.08 * cos(4.0 * PI * x4))) / x4;

		const double t1 = sin(PI_MULTIPLIER * PI * x1) * w1;
		const double t2 = sin(PI_MULTIPLIER * PI * x2) * w2;
		const double t3 = sin(PI_MULTIPLIER * PI * x3) * w3;
		const double t4 = sin(PI_MULTIPLIER * PI * x4) * w4;

		// calculate normalization value (also assures unity gain when summing taps)
		const double dScale = TAP_SUM_SCALE / (t1 + t2 + t3 + t4);

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
