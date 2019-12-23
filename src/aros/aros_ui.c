/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <proto/exec.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <intuition/intuition.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"
#include "../ui.h"

#include "aros.h"

extern struct timerequest *secreq;
struct Task *uiTask = NULL;
struct Window *displayWindow = NULL;

ULONG timer_sigbit = 0;
ULONG uisignal_refresh = 0;
ULONG uisignal_window = 0;
ULONG uisignal_hreset = 0;

char uiTitle[256] = "";

wchar_t *
ui_window_title(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    if ((s) && (strcmp(uiTitle, s)))
    {
	strcpy(uiTitle, s);

	if (uiTask == FindTask(NULL))
	    plat_settitle(uiTitle);
	else
	{
	    Signal(uiTask, 1 << uisignal_refresh);
	}
    }

    return(uiTitle);
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

/* Tell the UI about a new screen resolution. */
void
plat_resize(int x, int y)
{
    D(bug("86Box:%s(%d, %d)\n", __func__, x, y);)
}

void ui_thread(void)
{
    /*
     * Everything has been configured, and all seems to work,
     * so now it is time to start the main thread to do some
     * real work, and we will hang in here, dealing with the
     * UI until we're done.
     */
    do_start();

    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (! quited) {
	ULONG signals  = Wait((1 << timer_sigbit) |
					  (1 << uisignal_window) |
					  (1 << uisignal_hreset) |
					  (1 << uisignal_refresh) |
					  SIGBREAKF_CTRL_C |
					  SIGBREAKF_CTRL_D);
	if (signals & (1 << uisignal_hreset))
	{
	    D(bug("86Box:%s - uisignal_hreset\n", __func__);)

	    SetSignal(0, (1 << uisignal_hreset));

	    pc_reset(1);
	}
	if (signals & (1 << uisignal_refresh))
	{
	    D(bug("86Box:%s - uisignal_refresh\n", __func__);)

	    SetSignal(0, (1 << uisignal_refresh));

	    plat_settitle(uiTitle);
	}
	if (signals & (1 << uisignal_window))
	{
	    struct IntuiMessage* wMsg;

	    D(bug("86Box:%s - uisignal_window\n", __func__);)

	    while (wMsg = (struct IntuiMessage *)GetMsg(displayWindow->UserPort))
	    {
		D(bug("86Box:%s - Msg @ 0x%p\n", __func__, wMsg);)
		if (wMsg->Class == IDCMP_CLOSEWINDOW)
		{
		    displayWindow->UserPort->mp_Flags = 0;
		    displayWindow->UserPort->mp_SigBit = 0;
		    displayWindow->UserPort->mp_SigTask = NULL;
		    quited = 1;
		}
		ReplyMsg((struct Message *)wMsg);
	    }
	    SetSignal(0, (1 << uisignal_window));
	    
	}
	if (signals & (1 << timer_sigbit))
	{
	    D(bug("86Box:%s - timer_sigbit\n", __func__);)

	    SetSignal(0, (1 << timer_sigbit));

	    pc_onesec();
	    // Send IORequest
	    SendIO(&secreq->tr_node);
	}
	if (signals & SIGBREAKF_CTRL_C)
	{
	    D(bug("86Box:%s - SIGBREAKF_CTRL_C\n", __func__);)
	    quited = 1;
	}
	if (mouse_capture && keyboard_ismsexit()) {
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

    if (mouse_capture)
	plat_mouse_capture(0);

    /* Close down the emulator. */
    do_stop();
}

void ui_freesignals(void)
{
    D(bug("86Box:%s()\n", __func__);)

    FreeSignal(uisignal_hreset);
    FreeSignal(uisignal_window);
    FreeSignal(uisignal_refresh);    
}

int
ui_init(int cmdShow)
{
    D(bug("86Box:%s(%d)\n", __func__, cmdShow);)

    uiTask = FindTask(NULL);
    uisignal_refresh = AllocSignal(-1);
    uisignal_window = AllocSignal(-1);
    uisignal_hreset = AllocSignal(-1);

    D(bug("86Box:%s - UI Task @ 0x%p\n", __func__, uiTask);)
    D(bug("86Box:%s - refresh signal = %d\n", __func__, uisignal_refresh);)
    D(bug("86Box:%s - reset signal = %d\n", __func__, uisignal_hreset);)

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
