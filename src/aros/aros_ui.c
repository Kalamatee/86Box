/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/alib.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"
#include "../ui.h"
#include "../video/video.h"
#include "../mouse.h"

#include "aros.h"

extern void aros_dokeyevent(UWORD);
extern struct timerequest *secreq, *dispupdreq;

struct Task *uiTask = NULL;
struct Window *displayWindow = NULL;
struct BitMap *renderBitMap = NULL;
struct RastPort *BMRastPort = NULL;

ULONG timer_sigbit = 0;
ULONG uisignal_window = 0;
ULONG uisignal_hreset = 0;
ULONG uisignal_refresh = 0;
ULONG uisignal_blit = 0;

char uiTitle[256] = "";

wchar_t *
ui_window_title(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    if ((s) && (strcmp(uiTitle, s)))
    {
	strcpy(uiTitle, s);
	if (displayWindow && (uiTask == FindTask(NULL)))
	    SetWindowTitles(displayWindow, s, NULL);
	else
	    Signal(uiTask, (1 << uisignal_refresh));
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
		 y,
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
    BOOL winactive = FALSE, inside = TRUE;
    UWORD blankCursor = 0;
    /*
     * Everything has been configured, and all seems to work,
     * so now it is time to start the main thread to do some
     * real work, and we will hang in here, dealing with the
     * UI until we're done.
     */
    do_start();

    /* Run the message loop. */
    while (! quited) {
	ULONG signals  = Wait(((1 << timer_sigbit) |
					  (1 << uisignal_window) |
					  (1 << uisignal_hreset) |
					  (1 << uisignal_blit) |
					  SIGBREAKF_CTRL_C |
					  SIGBREAKF_CTRL_D));
	if ((signals & (1 << uisignal_hreset)))
	{
	    SetSignal(0, (1 << uisignal_hreset));
	    D(bug("86Box:%s - uisignal_hreset\n", __func__);)

	    pc_reset(1);
	}
	if ((signals & (1 << uisignal_blit)))
	{
	    SetSignal(0, (1 << uisignal_blit));
	    if (displayWindow && renderBitMap)
	    {
		BltBitMapRastPort(renderBitMap, 0, 0,
					    displayWindow->RPort, 0, 0, scrnsz_x, scrnsz_y, 0xC0);
	    }

	    D_VBLANK(
		bug("86Box:%s - vblank io_Command = %08x\n", __func__, dispupdreq->tr_node.io_Command);
		bug("86Box:%s - vblank mn_ReplyPort @ 0x%p\n", __func__, dispupdreq->tr_node.io_Message.mn_ReplyPort);
		bug("86Box:%s - vblank mn_Length = %d\n", __func__, dispupdreq->tr_node.io_Message.mn_Length);
		bug("86Box:%s - vblank io_Device @ 0x%p\n", __func__, dispupdreq->tr_node.io_Device);
		bug("86Box:%s - vblank io_Unit = %d\n", __func__, dispupdreq->tr_node.io_Unit);
		bug("86Box:%s -        tv_secs = %d\n", __func__, dispupdreq->tr_time.tv_secs);
		bug("86Box:%s -        tv_micro = %d\n", __func__, dispupdreq->tr_time.tv_micro);
	    )

#if !defined(ONESHOT_VBLANK)
	    // THIS SHOULDNT BE NECESSARY !!!
    	    dispupdreq->tr_time.tv_secs = 0;
	    dispupdreq->tr_time.tv_micro   = (1000000 / 60);

	    SendIO(&dispupdreq->tr_node);
#endif
	}
	if ((signals & (1 << uisignal_window)))
	{
	    struct IntuiMessage* wMsg;

	    D(bug("86Box:%s - uisignal_window\n", __func__);)

	    while (wMsg = (struct IntuiMessage *)GetMsg(displayWindow->UserPort))
	    {
		D(bug("86Box:%s - Msg @ 0x%p\n", __func__, wMsg);)
		if (!quited)
		{
		    switch(wMsg->Class)
		    {
		    case IDCMP_MOUSEBUTTONS:
			{
			    if (wMsg->Code == SELECTDOWN)
				mouse_buttons |= 1;
			    else if (wMsg->Code == SELECTUP)
				mouse_buttons &= ~1;
			    else if (wMsg->Code == MENUDOWN)
				mouse_buttons |= 2;
			    else if (wMsg->Code == MENUUP)
				mouse_buttons &= ~2;
			    else if (wMsg->Code == MIDDLEDOWN)
				mouse_buttons |= 4;
			    else if (wMsg->Code == MIDDLEUP)
				mouse_buttons &= ~4;
			}
			break;
		    case IDCMP_MOUSEMOVE:
			{
			    if (wMsg->MouseX < 0)
			    {
				mouse_x = 0;
				inside = FALSE;
			    }
			    else if (wMsg->MouseX > scrnsz_x)
			    {
				mouse_x = scrnsz_x;
				inside = FALSE;
			    }
			    else
			    {
				mouse_x = wMsg->MouseX;
			    }
			    if (wMsg->MouseY < 0)
			    {
				mouse_y = 0;
				inside = FALSE;
			    }
			    else if (wMsg->MouseY > scrnsz_y)
			    {
				mouse_y = scrnsz_y;
				inside = FALSE;
			    }
			    else
			    {
				mouse_y = wMsg->MouseX;
			    }
			    if (winactive && inside)
			    {
				mouse_capture = 1;
				if (displayWindow)
				    SetPointer(displayWindow, &blankCursor, 1, 1, 0, 0);
			    }
			    else
			    {
				mouse_capture = 0;
				if (displayWindow)
				    SetWindowPointer(displayWindow, WA_Pointer, NULL, TAG_DONE );
			    }
			}
			break;
		    case IDCMP_ACTIVEWINDOW:
			winactive = TRUE;
			if (inside)
			{
			    mouse_capture = 1;
			    if (displayWindow)
				SetPointer(displayWindow, &blankCursor, 1, 1, 0, 0);
			}
			else
			{
			    mouse_capture = 0;
			    if (displayWindow)
				SetWindowPointer(displayWindow, WA_Pointer, NULL, TAG_DONE );
			}
			break;
		    case IDCMP_INACTIVEWINDOW:
			winactive = FALSE;
			mouse_capture = 0;
			SetWindowPointer(displayWindow, WA_Pointer, NULL, TAG_DONE );
			break;
		    case IDCMP_CLOSEWINDOW:
			{
			    displayWindow->UserPort->mp_Flags = 0;
			    displayWindow->UserPort->mp_SigBit = 0;
			    displayWindow->UserPort->mp_SigTask = NULL;
			    quited = 1;
			}
			break;
		    case IDCMP_RAWKEY:
			{
			    aros_dokeyevent(wMsg->Code);
			}
			break;
		    }
		}
		ReplyMsg((struct Message *)wMsg);
	    }
	    SetSignal(0, (1 << uisignal_window));
	}
	if ((signals & (1 << timer_sigbit)))
	{
	    SetSignal(0, (1 << timer_sigbit));

	    D(bug("86Box:%s - timer_sigbit\n", __func__);)

	    pc_onesec();

	    D_TICK(
		bug("86Box:%s - 1sec timer io_Command = %08x\n", __func__, secreq->tr_node.io_Command);
		bug("86Box:%s - 1sec timer mn_ReplyPort @ 0x%p\n", __func__, secreq->tr_node.io_Message.mn_ReplyPort);
		bug("86Box:%s - 1sec timer mn_Length = %d\n", __func__, secreq->tr_node.io_Message.mn_Length);
		bug("86Box:%s - 1sec timer io_Device @ 0x%p\n", __func__, secreq->tr_node.io_Device);
		bug("86Box:%s - 1sec timer io_Unit = %d\n", __func__, secreq->tr_node.io_Unit);
		bug("86Box:%s -            tv_secs = %d\n", __func__, secreq->tr_time.tv_secs);
		bug("86Box:%s -            tv_micro = %d\n", __func__, secreq->tr_time.tv_micro);
	    )

#if !defined(ONESHOT_TICK)
	    // THIS SHOULDNT BE NECESSARY !!!
    	    secreq->tr_time.tv_secs = 1;
	    secreq->tr_time.tv_micro = 0;

	    // Send IORequest
	    SendIO(&secreq->tr_node);
#endif
	}
	if ((signals & (1 << uisignal_refresh)))
	{
	    SetSignal(0, (1 << uisignal_refresh));

	    D(bug("86Box:%s - uisignal_refresh\n", __func__);)
	    ui_window_title(uiTitle);
	}
	if ((signals & SIGBREAKF_CTRL_C))
	{
	    D(bug("86Box:%s - SIGBREAKF_CTRL_C\n", __func__);)
	    quited = 1;
	}
	if ((mouse_capture && keyboard_ismsexit())) {
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
    {
	plat_mouse_capture(0);
    }

    /* Close down the emulator. */
    do_stop();
}

BOOL ui_allocsignals(void)
{
    D(bug("86Box:%s()\n", __func__);)

    if ((uisignal_hreset = AllocSignal(-1)) != -1) {
	if ((uisignal_window = AllocSignal(-1)) != -1) {
	    if ((uisignal_refresh = AllocSignal(-1)) != -1) {
		if ((uisignal_blit = AllocSignal(-1)) != -1) {
		    D(bug("86Box:%s - reset signal = %d\n", __func__, uisignal_hreset);)
		    D(bug("86Box:%s - win signal = %d\n", __func__, uisignal_window);)
		    D(bug("86Box:%s - blit signal = %d\n", __func__, uisignal_blit);)
		    D(bug("86Box:%s - refresh signal = %d\n", __func__, uisignal_refresh);)
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

void ui_freesignals(void)
{
    D(bug("86Box:%s()\n", __func__);)

    FreeSignal(uisignal_hreset);
    FreeSignal(uisignal_window);
    FreeSignal(uisignal_refresh);
    FreeSignal(uisignal_blit);
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
