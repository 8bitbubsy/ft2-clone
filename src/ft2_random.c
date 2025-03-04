#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>

static uint32_t randSeed;

void randomize(void)
{
	randSeed = (uint32_t)SDL_GetTicks();
}

int32_t randoml(int32_t limit)
{
	randSeed *= 134775813;
	randSeed++;
	return ((int64_t)randSeed * (int32_t)limit) >> 32;
}

int32_t random32(void)
{
	randSeed *= 134775813;
	randSeed++;

	return randSeed;
}
