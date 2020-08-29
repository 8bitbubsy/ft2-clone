#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_cubic.h"

float *fCubicLUT8 = NULL;
float *fCubicLUT16 = NULL;

/* 4-tap cubic spline interpolation (Horner's method)
**
** This simplified LUT formula has been used in several trackers, but
** I think it originates from ModPlug Tracker (not sure about this).
*/

static void calcCubicLUT(float *fLUTPtr, const double dSale)
{
	for (int32_t i = 0; i < CUBIC_PHASES; i++)
	{
		const double x  = i * (1.0 / CUBIC_PHASES);
		const double x2 = x * x;  // x^2
		const double x3 = x2 * x; // x^3

		*fLUTPtr++ = (float)((-0.5 * x3 + 1.0 * x2 - 0.5 * x) * dSale);
		*fLUTPtr++ = (float)(( 1.5 * x3 - 2.5 * x2 + 1.0)     * dSale);
		*fLUTPtr++ = (float)((-1.5 * x3 + 2.0 * x2 + 0.5 * x) * dSale);
		*fLUTPtr++ = (float)(( 0.5 * x3 - 0.5 * x2)           * dSale);
	}
}

bool calcCubicTable(void)
{
	fCubicLUT8 = (float *)malloc(CUBIC_LUT_LEN * sizeof (float));
	if (fCubicLUT8 == NULL)
		return false;

	fCubicLUT16 = (float *)malloc(CUBIC_LUT_LEN * sizeof (float));
	if (fCubicLUT16 == NULL)
		return false;

	calcCubicLUT(fCubicLUT8, 1.0 / 128.0); // for 8-bit samples
	calcCubicLUT(fCubicLUT16, 1.0 / 32768.0); // for 16-bit samples

	return true;
}

void freeCubicTable(void)
{
	if (fCubicLUT8 != NULL)
	{
		free(fCubicLUT8);
		fCubicLUT8 = NULL;
	}

	if (fCubicLUT16 != NULL)
	{
		free(fCubicLUT16);
		fCubicLUT16 = NULL;
	}
}
