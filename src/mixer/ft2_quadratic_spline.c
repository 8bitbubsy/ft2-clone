// 3-point quadratic spline interpolation LUT generator

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_quadratic_spline.h"
#include "../ft2_video.h" // showErrorMsgBox()

float *fQuadraticSplineLUT = NULL; // globalized

bool setupQuadraticSplineTable(void)
{
	fQuadraticSplineLUT = (float *)malloc(QUADRATIC_SPLINE_WIDTH * QUADRATIC_SPLINE_PHASES * sizeof (float));
	if (fQuadraticSplineLUT == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	float *fPtr = fQuadraticSplineLUT;
	for (int32_t i = 0; i < QUADRATIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / QUADRATIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2

		double t1 = ( 0.5 * x2) + (-1.5 * x1) + 1.0;
		double t2 = (-1.0 * x2) + ( 2.0 * x1);
		double t3 = ( 0.5 * x2) + (-0.5 * x1);

		*fPtr++ = (float)t1;
		*fPtr++ = (float)t2;
		*fPtr++ = (float)t3;
	}

	return true;
}

void freeQuadraticSplineTable(void)
{
	if (fQuadraticSplineLUT != NULL)
	{
		free(fQuadraticSplineLUT);
		fQuadraticSplineLUT = NULL;
	}
}
