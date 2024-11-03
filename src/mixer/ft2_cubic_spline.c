/*
** Cubic Hermite spline (Catmull-Rom) interpolation LUT generator
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_cubic_spline.h"
#include "../ft2_video.h" // showErrorMsgBox()

float *fCubicSplineLUT = NULL; // globalized

bool calcCubicSplineTable(void)
{
	fCubicSplineLUT = (float *)malloc(CUBIC_SPLINE_TAPS*CUBIC_SPLINE_PHASES * sizeof (float));
	if (fCubicSplineLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	float *fPtr = fCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		const double t1 = -0.5 * x3 +       x2 - 0.5 * x1;
		const double t2 =  1.5 * x3 - 2.5 * x2 + 1.0;
		const double t3 = -1.5 * x3 + 2.0 * x2 + 0.5 * x1;
		const double t4 =  0.5 * x3 - 0.5 * x2;

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
	}

	return true;
}

void freeCubicSplineTable(void)
{
	if (fCubicSplineLUT != NULL)
	{
		free(fCubicSplineLUT);
		fCubicSplineLUT = NULL;
	}
}
