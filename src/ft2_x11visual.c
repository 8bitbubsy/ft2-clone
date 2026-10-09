#ifdef __sgi

// kept in its own file, because the Xlib headers define names like Window, Font and Time

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <SDL2/SDL.h>
#include "ft2_x11visual.h"

static bool sdlHasOpenGLRenderDriver(void)
{
	const int numDrivers = SDL_GetNumRenderDrivers();
	for (int i = 0; i < numDrivers; i++)
	{
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(i, &info) == 0 && strcmp(info.name, "opengl") == 0)
			return true;
	}

	return false;
}

/* IRIX X servers often run with an 8-bit default depth, and SDL then uses an 8-bit visual for the
** window. SDL's software renderer can't draw to SGI's 8-bit TrueColor visual (BGR233), and fails
** with "Unknown window pixel format". Point SDL to a 24-bit TrueColor visual instead.
**
** This has to be done before SDL_Init(). SDL_VIDEO_X11_VISUALID also overrides the GLX visual SDL
** picks for OpenGL windows, so only do it when the OpenGL renderer won't be used.
*/
void selectX11VisualForSoftwareRenderer(bool forceSoftwareRenderer)
{
	if (getenv("SDL_VIDEO_X11_VISUALID") != NULL)
		return; // set by the user, leave it alone

	if (!forceSoftwareRenderer && sdlHasOpenGLRenderDriver())
		return;

	Display *display = XOpenDisplay(NULL);
	if (display == NULL)
		return;

	XVisualInfo visualInfo;
	if (XMatchVisualInfo(display, DefaultScreen(display), 24, TrueColor, &visualInfo))
	{
		char visualIdStr[32];
		sprintf(visualIdStr, "0x%lx", (unsigned long)visualInfo.visualid);
		SDL_setenv("SDL_VIDEO_X11_VISUALID", visualIdStr, 1);

		printf("Using X11 visual %s (24-bit TrueColor) for the software renderer\n", visualIdStr);
		fflush(stdout);
	}

	XCloseDisplay(display);
}

#endif
