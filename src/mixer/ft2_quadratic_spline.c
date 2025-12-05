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

		const double t1 = (x1 * -1.5) + (x2 *  0.5) + 1.0;
		const double t2 = (x1 *  2.0) + (x2 * -1.0);
		const double t3 = (x1 * -0.5) + (x2 *  0.5);

		*fPtr++ = (float)t1; // tap #1 at sample offset 0 (center)
		*fPtr++ = (float)t2; // tap #2 at sample offset 1
		*fPtr++ = (float)t3; // tap #3 at sample offset 2
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
