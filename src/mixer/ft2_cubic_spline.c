/*
** Cubic Hermite spline (Catmull-Rom) interpolation LUT generator
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_cubic_spline.h"
#include "../ft2_video.h" // showErrorMsgBox()

float *f4PointCubicSplineLUT = NULL, *f6PointCubicSplineLUT = NULL; // globalized

bool setupCubicSplineTables(void)
{
	float *fPtr;

	f4PointCubicSplineLUT = (float *)malloc(4 * CUBIC4P_SPLINE_PHASES * sizeof (float));
	f6PointCubicSplineLUT = (float *)malloc(6 * CUBIC6P_SPLINE_PHASES * sizeof (float));

	if (f4PointCubicSplineLUT == NULL || f6PointCubicSplineLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	// 4-point Cubic Hermite (Catmull-Rom)
	fPtr = f4PointCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC4P_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC4P_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		double t1 = (-(1.0/2.0) * x3) + ( (    1.0) * x2) + (-(1.0/2.0) * x1);
		double t2 = ( (3.0/2.0) * x3) + (-(5.0/2.0) * x2) + 1.0;
		double t3 = (-(3.0/2.0) * x3) + ( (    2.0) * x2) + ( (1.0/2.0) * x1);
		double t4 = ( (1.0/2.0) * x3) + (-(1.0/2.0) * x2);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
		*fPtr++ = (float)t4;
	}

	// 6-point Cubic Hermite (Catmull-Rom)
	fPtr = f6PointCubicSplineLUT;
	for (int32_t i = 0; i < CUBIC6P_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC6P_SPLINE_PHASES);
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

	return true;
}

void freeCubicSplineTables(void)
{
	if (f4PointCubicSplineLUT != NULL)
	{
		free(f4PointCubicSplineLUT);
		f4PointCubicSplineLUT = NULL;
	}

	if (f6PointCubicSplineLUT != NULL)
	{
		free(f6PointCubicSplineLUT);
		f6PointCubicSplineLUT = NULL;
	}
}
