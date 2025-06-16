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

		const double t1 = (x1 * -0.5) + (x2 *  1.0) + (x3 * -0.5);
		const double t2 =               (x2 * -2.5) + (x3 *  1.5) + 1.0;
		const double t3 = (x1 *  0.5) + (x2 *  2.0) + (x3 * -1.5);
		const double t4 =               (x2 * -0.5) + (x3 *  0.5);

		*fPtr++ = (float)t1; // tap #1 at sample offset -1
		*fPtr++ = (float)t2; // tap #2 at sample offset  0 (center)
		*fPtr++ = (float)t3; // tap #3 at sample offset  1
		*fPtr++ = (float)t4; // tap #4 at sample offset  2
	}

	/*
	** Code for 6-point cubic Hermite spline (Catmull-Rom).
	** May be useful for someone, so I keep it here despite not being used.
	*/

	/*
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const double x1 = i * (1.0 / CUBIC_SPLINE_PHASES);
		const double x2 = x1 * x1; // x^2
		const double x3 = x2 * x1; // x^3

		const double t1 = (x1 *  (1.0/12.0)) + (x2 * -(1.0/ 6.0)) + (x3 *  (1.0/12.0));
		const double t2 = (x1 * -(2.0/ 3.0)) + (x2 *  (5.0/ 4.0)) + (x3 * -(7.0/12.0));
		const double t3 =                      (x2 * -(7.0/ 3.0)) + (x3 *  (4.0/ 3.0)) + 1.0;
		const double t4 = (x1 *  (2.0/ 3.0)) + (x2 *  (5.0/ 3.0)) + (x3 * -(4.0/ 3.0));
		const double t5 = (x1 * -(1.0/12.0)) + (x2 * -(1.0/ 2.0)) + (x3 *  (7.0/12.0));
		const double t6 =                      (x2 *  (1.0/12.0)) + (x3 * -(1.0/12.0));

		*fPtr++ = (float)t1; // tap #1 at sample offset -2
		*fPtr++ = (float)t2; // tap #2 at sample offset -1
		*fPtr++ = (float)t3; // tap #3 at sample offset  0 (center)
		*fPtr++ = (float)t4; // tap #4 at sample offset  1
		*fPtr++ = (float)t5; // tap #5 at sample offset  2
		*fPtr++ = (float)t6; // tap #6 at sample offset  3
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
