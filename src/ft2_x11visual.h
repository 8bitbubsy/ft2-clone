#pragma once

#ifdef __sgi

#include <stdbool.h>

void selectX11VisualForSoftwareRenderer(bool forceSoftwareRenderer); // call before SDL_Init()

#endif
