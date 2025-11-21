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
		const float x1 = (float)i * (1.0f / CUBIC_SPLINE_PHASES);
		const float x2 = x1 * x1; // x^2
		const float x3 = x2 * x1; // x^3

		const float t1 = (x1 * -0.5f) + (x2 *  1.0f) + (x3 * -0.5f);
		const float t2 =                (x2 * -2.5f) + (x3 *  1.5f) + 1.0f;
		const float t3 = (x1 *  0.5f) + (x2 *  2.0f) + (x3 * -1.5f);
		const float t4 =                (x2 * -0.5f) + (x3 *  0.5f);

		*fPtr++ = t1; // tap #1 at sample offset -1
		*fPtr++ = t2; // tap #2 at sample offset  0 (center)
		*fPtr++ = t3; // tap #3 at sample offset  1
		*fPtr++ = t4; // tap #4 at sample offset  2
	}

	/*
	** Code for 6-point cubic Hermite spline (Catmull-Rom).
	** May be useful for someone, so I keep it here despite not being used.
	*/

	/*
	for (int32_t i = 0; i < CUBIC_SPLINE_PHASES; i++)
	{
		const float x1 = i * (1.0f / CUBIC_SPLINE_PHASES);
		const float x2 = x1 * x1; // x^2
		const float x3 = x2 * x1; // x^3

		const float t1 = (x1 *  (1.0f/12.0f)) + (x2 * -(1.0f/ 6.0f)) + (x3 *  (1.0f/12.0f));
		const float t2 = (x1 * -(2.0f/ 3.0f)) + (x2 *  (5.0f/ 4.0f)) + (x3 * -(7.0f/12.0f));
		const float t3 =                        (x2 * -(7.0f/ 3.0f)) + (x3 *  (4.0f/ 3.0f)) + 1.0f;
		const float t4 = (x1 *  (2.0f/ 3.0f)) + (x2 *  (5.0f/ 3.0f)) + (x3 * -(4.0f/ 3.0f));
		const float t5 = (x1 * -(1.0f/12.0f)) + (x2 * -(1.0f/ 2.0f)) + (x3 *  (7.0f/12.0f));
		const float t6 =                        (x2 *  (1.0f/12.0f)) + (x3 * -(1.0f/12.0f));

		*fPtr++ = t1; // tap #1 at sample offset -2
		*fPtr++ = t2; // tap #2 at sample offset -1
		*fPtr++ = t3; // tap #3 at sample offset  0 (center)
		*fPtr++ = t4; // tap #4 at sample offset  1
		*fPtr++ = t5; // tap #5 at sample offset  2
		*fPtr++ = t6; // tap #6 at sample offset  3
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
