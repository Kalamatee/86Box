/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#include <stdint.h>
#include <stdio.h>

#include <stdint.h>

#include "../86box.h"
#include "../plat.h"
#include "../video/video.h"

extern struct Task *uiTask;
extern ULONG uisignal_window;
extern struct Window *displayWindow;

static volatile int sdl_enabled = 0;
struct Library *SDLBase = NULL;
SDL_Surface *dispScreen = NULL;

static void
sdl_blit(int x, int y, int y1, int y2, int w, int h)
{
    SDL_Rect r_src;
    void *pixeldata;
    int pitch;
    int yy, ret;

    D(bug("86Box:%s()\n", __func__);)

#if (0)
    if (!sdl_enabled) {
        D(bug("86Box:%s - sdl not enabled\n", __func__);)
	video_blit_complete();
	return;
    }
#endif

    if ((y1 == y2) || (h <= 0)) {
	video_blit_complete();
	return;
    }

    if (buffer32 == NULL) {
        D(bug("86Box:%s - nothing to render\n", __func__);)
	video_blit_complete();
	return;
    }

    D(bug("86Box:%s - rendering display\n", __func__);)

    SDL_LockSurface(dispScreen);
    pixeldata = dispScreen->pixels;
    pitch = dispScreen->pitch;

    for (yy = y1; yy < y2; yy++) {
       	if ((y + yy) >= 0 && (y + yy) < render_buffer->h)
		memcpy((uint32_t *) &(((uint8_t *)pixeldata)[yy * pitch]), &(render_buffer->line[y + yy][x]), w * 4);
    }

    video_blit_complete();

    SDL_UnlockSurface(dispScreen);
}

void	sdl_close(void)
{
    D(bug("86Box:%s()\n", __func__);)

    /* Unregister our renderer! */
    video_setblit(NULL);

    if (SDLBase)
    {
        SDL_Quit();
        CloseLibrary(SDLBase);
    }
    SDLBase = NULL;
}

int sdl_init(APTR h)
{
    SDL_SysWMinfo wminfo;
    wminfo.version.major = SDL_MAJOR_VERSION;

    D(bug("86Box:%s()\n", __func__);)
    SDLBase = OpenLibrary("SDL.library",0);
    D(bug("86Box:%s - SDLBase = 0x%p\n", __func__, SDLBase);)

    /* Initialize the SDL system. */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	D(bug("86Box:%s - SDL initialization failed (%s)\n", __func__, SDL_GetError()));
	return(0);
    }

    dispScreen = SDL_SetVideoMode(scrnsz_x, scrnsz_y, 24, SDL_SWSURFACE);
    if ( dispScreen == NULL )
        return 0;

    if (SDL_GetWMInfo(&wminfo) > 0)
    {
        D(bug("86Box:%s - SDL_WM Window @ 0x%p\n", __func__, wminfo.window);)
        displayWindow = wminfo.window;

        if (displayWindow->UserPort)
        {
            D(bug("86Box:%s - setting windows port to signal uitask...\n", __func__);)
            displayWindow->UserPort->mp_Flags = PA_SIGNAL;
            displayWindow->UserPort->mp_SigBit = uisignal_window;
            displayWindow->UserPort->mp_SigTask = uiTask;
        }
    }

    /* Register our renderer! */
    video_setblit(sdl_blit);

    return(1);
}

void sdl_title(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    SDL_WM_SetCaption(s, NULL);
}

int sdl_pause(void)
{
    D(bug("86Box:%s()\n", __func__);)
    return(0);
}

void	sdl_resize(int x, int y)
{
    D(bug("86Box:%s()\n", __func__);)
}

void	sdl_enable(int enable)
{
    D(bug("86Box:%s()\n", __func__);)
    sdl_enabled = enable;
}
