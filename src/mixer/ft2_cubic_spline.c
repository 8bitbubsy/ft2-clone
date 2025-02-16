// 4-point cubic Hermite spline (Catmull-Rom) interpolation LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_cubic_spline.h"
#include "../ft2_video.h" // showErrorMsgBox()

float *fCubicSplineLUT = NULL; // globalized

bool setupCubicSplineTable(void)
{
	fCubicSplineLUT = (float *)malloc(CUBIC_SPLINE_WIDTH * CUBIC_SPLINE_PHASES * sizeof (float));
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

		double t1 = (-0.5 * x3) + ( 1.0 * x2) + (-0.5 * x1);
		double t2 = ( 1.5 * x3) + (-2.5 * x2) + 1.0;
		double t3 = (-1.5 * x3) + ( 2.0 * x2) + ( 0.5 * x1);
		double t4 = ( 0.5 * x3) + (-0.5 * x2);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
	}

	/*
	// 6-point Cubic Hermite (Catmull-Rom)
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		double t1 = ( (1.0/12.0) * x3) + (-(1.0/ 6.0) * x2) + ( (1.0/12.0) * x1);
		double t2 = (-(7.0/12.0) * x3) + ( (5.0/ 4.0) * x2) + (-(2.0/ 3.0) * x1);
		double t3 = ( (4.0/ 3.0) * x3) + (-(7.0/ 3.0) * x2) + 1.0;
		double t4 = (-(4.0/ 3.0) * x3) + ( (5.0/ 3.0) * x2) + ( (2.0/ 3.0) * x1);
		double t5 = ( (7.0/12.0) * x3) + (-(1.0/ 2.0) * x2) + (-(1.0/12.0) * x1);
		double t6 = (-(1.0/12.0) * x3) + ( (1.0/12.0) * x2);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
		*fPtr++ = (float)t5;
		*fPtr++ = (float)t6;
	}
	*/

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
