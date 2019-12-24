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

#include <stdint.h>
#include <stdio.h>

#include <stdint.h>

#include "../86box.h"
#include "../plat.h"
#include "../video/video.h"

#include "aros.h"

extern void amiga_blit(int x, int y, int y1, int y2, int w, int h);

extern struct Task *uiTask;
extern ULONG uisignal_window;
extern ULONG uisignal_blit;
extern struct Window *displayWindow;

static volatile int amigavid_enabled = 0;
struct timerequest *dispupdreq;

void	amigavid_close(void)
{
    D(bug("86Box:%s()\n", __func__);)

    /* Unregister our renderer! */
    video_setblit(NULL);
    
    if (displayWindow)
        CloseWindow(displayWindow);
}

int amigavid_init(APTR h)
{
    struct MsgPort *vbtimerport;   /* Message port pointer */
    struct timerequest *timerreq;

    D(bug("86Box:%s()\n", __func__);)

    vbtimerport = CreatePort(0,0);
    if (!vbtimerport)
    {
        return(0);
    }

    vbtimerport->mp_Flags = PA_SIGNAL;
    vbtimerport->mp_SigBit = uisignal_blit;
    vbtimerport->mp_SigTask = uiTask;

    dispupdreq = (struct timerequest *)CreateIORequest(vbtimerport, sizeof(struct timerequest));
    if (!dispupdreq)
    {
        DeletePort(vbtimerport);
        return(0);
    }


    if ((OpenDevice("timer.device", UNIT_MICROHZ, &dispupdreq->tr_node, 0)) != 0)
    {
        DeleteIORequest(&dispupdreq->tr_node);
        DeletePort(vbtimerport);
        return(0);
    }

    if (displayWindow = OpenWindowTags(NULL,
            WA_InnerWidth, scrnsz_x,
            WA_InnerHeight, scrnsz_y,
            WA_DragBar, TRUE,
            WA_CloseGadget, TRUE,
            WA_DepthGadget, TRUE,
            WA_Title, (IPTR)emu_version,
            WA_Flags, WFLG_GIMMEZEROZERO,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
            TAG_END))
    {
        displayWindow->UserPort->mp_Flags = PA_SIGNAL;
        displayWindow->UserPort->mp_SigBit = uisignal_window;
        displayWindow->UserPort->mp_SigTask = uiTask;

        dispupdreq->tr_node.io_Command = TR_ADDREQUEST;
        dispupdreq->tr_time.tv_secs    = 0;
        dispupdreq->tr_time.tv_micro   = 1000000 / 60;

        /* Register our renderer! */
        video_setblit(amiga_blit);
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
    UWORD height = displayWindow->Height - (displayWindow->BorderTop + displayWindow->BorderBottom);
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
        SizeWindow(displayWindow, diffx, diffy);
}

void	amigavid_enable(int enable)
{
    D(bug("86Box:%s()\n", __func__);)
    amigavid_enabled = enable;
}
