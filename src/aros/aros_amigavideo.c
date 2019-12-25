/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>

#include <stdint.h>
#include <stdio.h>

#include <stdint.h>

#include "../86box.h"
#include "../plat.h"
#include "../video/video.h"

#include "aros.h"

extern void amiga_blit(int x, int y, int y1, int y2, int w, int h);

extern int sb_height;

extern struct Task *uiTask;
extern ULONG uisignal_window;
extern ULONG uisignal_blit;
extern struct Window *displayWindow;
extern struct RastPort *BMRastPort;
extern struct BitMap *renderBitMap;

static volatile int amigavid_enabled = 0;
struct timerequest *dispupdreq;

void	amigavid_close(void)
{
    D(bug("86Box:%s()\n", __func__);)

    /* Unregister our renderer! */
    video_setblit(NULL);

    if (BMRastPort)
    {
        FreeRastPort(BMRastPort);
        BMRastPort = NULL;
    }

    if (displayWindow)
    {
        CloseWindow(displayWindow);
        displayWindow = NULL;
    }
}

int amigavid_init(APTR h)
{
    struct MsgPort *vbtimerport;   /* Message port pointer */
    struct timerequest *timerreq;

    D(bug("86Box:%s()\n", __func__);)

    vbtimerport = CreatePort(0,0);
    if (!vbtimerport)
    {
        D(bug("86Box:%s - failed to create a msg port\n", __func__);)
        return(0);
    }

    vbtimerport->mp_Flags = PA_SIGNAL;
    vbtimerport->mp_SigBit = uisignal_blit;
    vbtimerport->mp_SigTask = uiTask;

    dispupdreq = (struct timerequest *)CreateIORequest(vbtimerport, sizeof(struct timerequest));
    if (!dispupdreq)
    {
        D(bug("86Box:%s - failed to allocate an iorequest\n", __func__);)
        DeletePort(vbtimerport);
        vbtimerport = NULL;
        return(0);
    }

    if ((OpenDevice("timer.device", UNIT_MICROHZ, &dispupdreq->tr_node, 0)) != 0)
    {
        D(bug("86Box:%s - failed to open timer.device\n", __func__);)
        DeleteIORequest(&dispupdreq->tr_node);
        dispupdreq = NULL;
        DeletePort(vbtimerport);
        vbtimerport = NULL;
        return(0);
    }

    if (displayWindow = OpenWindowTags(NULL,
            WA_InnerWidth, scrnsz_x,
            WA_InnerHeight, scrnsz_y + sb_height,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_DepthGadget, TRUE,
            WA_Title, (IPTR)emu_version,
            WA_Flags, WFLG_GIMMEZEROZERO,
            WA_IDCMP, (IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW),
            TAG_END))
    {
        BMRastPort = CreateRastPort();

        if (displayWindow->UserPort)
        {
            displayWindow->UserPort->mp_Flags = PA_SIGNAL;
            displayWindow->UserPort->mp_SigBit = uisignal_window;
            displayWindow->UserPort->mp_SigTask = uiTask;
        }
        dispupdreq->tr_node.io_Command = TR_ADDREQUEST;
        dispupdreq->tr_time.tv_secs    = 0;
        dispupdreq->tr_time.tv_micro   = (1000000 / 60);

        D_VBLANK(bug("86Box:%s -        tv_micro = %d\n", __func__, dispupdreq->tr_time.tv_micro);)

        /* Register our renderer! */
        video_setblit(amiga_blit);

        SendIO(&dispupdreq->tr_node);

        return(1);
    }
    return(0);
}

int amigavid_pause(void)
{
    D(bug("86Box:%s()\n", __func__);)
    return(0);
}

void	amigavid_resize(int x, int y)
{
    UWORD width = displayWindow->Width - (displayWindow->BorderLeft + displayWindow->BorderRight);
    UWORD height = displayWindow->Height - (displayWindow->BorderTop + displayWindow->BorderBottom + sb_height);
    WORD diffx, diffy;

    D(bug("86Box:%s(%dx%d)\n", __func__, x, y);)
    
    if (width > x)
        diffx = -(width - x);
    else
        diffx = x - width;
    
    if (height > y)
        diffy = -(height - y);
    else
        diffy = y - height;

    if (displayWindow)
    {
        if (renderBitMap)
        {
	    APTR tmp = BMRastPort->BitMap;
	    BMRastPort->BitMap = renderBitMap;
	    WritePixelArray(
		 render_buffer->dat,
		 0,
		 0,
		 (render_buffer->w << 2),
		 BMRastPort,
		 0,
		 0,
		 x,
		 y,
		 RECTFMT_BGRA32);
	    BMRastPort->BitMap = tmp;
	    return;
        }

        SizeWindow(displayWindow, diffx, diffy);
    }
}

void	amigavid_enable(int enable)
{
    D(bug("86Box:%s()\n", __func__);)
    amigavid_enabled = enable;
}
