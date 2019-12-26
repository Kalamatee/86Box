/*
    $Id$
*/

#include <aros/debug.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <libraries/mui.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"
#include "../ui.h"
#include "../video/video.h"

#include "aros.h"

#include "aros_mouse.h"

extern void aros_dokeyevent(UWORD);
extern MOUSESTATE mousestate;
extern Object *app, *main_wnd;

struct Task *uiTask = NULL;
struct Window *displayWindow = NULL;
struct BitMap *renderBitMap = NULL;
struct RastPort *BMRastPort = NULL;

ULONG uisignal_hreset = 0;

char uiTitle[256] = "";

wchar_t *
ui_window_title(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    if ((s) && (strcmp(uiTitle, s)))
    {
	strcpy(uiTitle, s);
	
	if (main_wnd)
	    DoMethod(_app(main_wnd), MUIM_Application_PushMethod, (IPTR)main_wnd, 3,
			 MUIM_Set, MUIA_Window_Title, uiTitle);
    }
    return(uiTitle);
}

void
amiga_blit(int x, int y, int y1, int y2, int w, int h)
{
    void *pixeldata;
    int pitch;
    int yy, ret;

    D(bug("86Box:%s(%d -> %d)\n", __func__, y1, y2);)

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
    D(bug("86Box:%s - render_buffer->dat = 0x%p\n", __func__, render_buffer->dat);)

    if (displayWindow)
    {
        if ((BMRastPort) && (renderBitMap))
        {
	    APTR tmp = BMRastPort->BitMap;
	    BMRastPort->BitMap = renderBitMap;
	    WritePixelArray(
		 render_buffer->dat,
		 x,
		 (y + y1),
		 (render_buffer->w << 2),
		 BMRastPort,
		 0,
		 y1,
		 w,
		 h,
		 RECTFMT_BGRA32);
	    BMRastPort->BitMap = tmp;
        }
    }
    video_blit_complete();
}

void
plat_mouse_capture(int on)
{
    D(bug("86Box:%s(%d)\n", __func__, on);)
}

/* We should have the language ID as a parameter. */
void
plat_pause(int p)
{
    D(bug("86Box:%s(%d)\n", __func__, p);)

    dopause = p;
}

void ui_thread(void)
{
    ULONG signals = 0;

   /*
     * Everything has been configured, and all seems to work,
     * so now it is time to start the main thread to do some
     * real work, and we will hang in here, dealing with the
     * UI until we're done.
     */
    do_start();

    while
    ( 
	(! quited) &&
	(DoMethod(app, MUIM_Application_NewInput, (IPTR) &signals)  != MUIV_Application_ReturnID_Quit)
    )
    {
	if (signals)
	{
	    signals = Wait(signals | (1 << uisignal_hreset) | SIGBREAKF_CTRL_C);
	    if (signals & SIGBREAKF_CTRL_C)
	    {
		D(bug("86Box:%s - SIGBREAKF_CTRL_C\n", __func__);)
		break;
	    }
	    if ((signals & (1 << uisignal_hreset)))
	    {
		SetSignal(0, (1 << uisignal_hreset));
		D(bug("86Box:%s - uisignal_hreset\n", __func__);)

		pc_reset(1);
	    }
	}
	if ((mousestate.capture && keyboard_ismsexit())) {
		/* Release the in-app mouse. */
		plat_mouse_capture(0);
	}
#if (0)
	if (video_fullscreen && keyboard_isfsexit()) {
		/* Signal "exit fullscreen mode". */
		plat_setfullscreen(0);
	}
#endif
    }

    quited = 1;

    D(bug("86Box:%s - finished event loop\n", __func__);)

    if (mousestate.capture)
    {
	plat_mouse_capture(0);
    }

    /* Close down the emulator. */
    do_stop();

    D(bug("86Box:%s - exiting\n", __func__);)
}

BOOL ui_allocsignals(void)
{
    D(bug("86Box:%s()\n", __func__);)

    if ((uisignal_hreset = AllocSignal(-1)) != -1) {
	    D(bug("86Box:%s - reset signal = %d\n", __func__, uisignal_hreset);)
	    return TRUE;
    }
    return FALSE;
}

void ui_freesignals(void)
{
    D(bug("86Box:%s()\n", __func__);)

    FreeSignal(uisignal_hreset);
}

int
ui_init(int cmdShow)
{
    D(bug("86Box:%s(%d)\n", __func__, cmdShow);)

    uiTask = FindTask(NULL);
    if (!ui_allocsignals())
	return (6);

    D(bug("86Box:%s - UI Task @ 0x%p\n", __func__, uiTask);)

    if (settings_only) {
	if (! pc_init_modules()) {
                D(bug("86Box:%s - no ROMs found!\n", __func__);)
		/* Dang, no ROMs found at all! */
#if (0)
		MessageBox(hwnd,
			   plat_get_string(IDS_2056), // "No usable ROM images found!"
			   plat_get_string(IDS_2050), // "86Box Fatal Error"
			   MB_OK | MB_ICONERROR);
#endif
		ui_freesignals();
		return(6);
	}

#if (0)
	win_settings_open(NULL);
#endif
	ui_freesignals();
	return(0);
    }

    /* All done, fire up the actual emulated machine. */
    if (! pc_init_modules()) {
	/* Dang, no ROMs found at all! */
        D(bug("86Box:%s - no ROMs found!\n", __func__);)
#if (0)
	MessageBox(hwnd,
		   plat_get_string(IDS_2056), // "No usable ROM images found!"
		   plat_get_string(IDS_2050), // "86Box Fatal Error"
		   MB_OK | MB_ICONERROR);
#endif
	ui_freesignals();
	return(6);
    }

    /* Initialize the configured Video API. */
    if (! plat_setvid(vid_api)) {
#if (0)
	MessageBox(hwnd,
		   plat_get_string(IDS_2095), // "Neither DirectDraw nor Direct3D available !"
		   plat_get_string(IDS_2050), // "86Box Fatal Error"
		   MB_OK | MB_ICONERROR);
#endif
	ui_freesignals();
	return(5);
    }

    ui_window_title(emu_version);

#if (0)
    /* Initialize the rendering window, or fullscreen. */
    if (start_in_fullscreen)
	plat_setfullscreen(1);
#endif

    /* Set up the current window size. */
    plat_resize(scrnsz_x, scrnsz_y);

    /* Fire up the machine. */
    pc_reset_hard_init();

    /* Set the PAUSE mode depending on the renderer. */
    plat_pause(0);

    ui_thread();

    ui_freesignals();
}
