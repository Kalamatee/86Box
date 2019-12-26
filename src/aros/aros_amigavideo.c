/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/alib.h>
#include <proto/muimaster.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <cybergraphx/cybergraphics.h>
#include <libraries/mui.h>
#include <zune/customclasses.h>

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>

#include "../86box.h"
#include "../plat.h"
#include "../video/video.h"

#include "aros.h"
#include "aros_ui.h"
#include "aros_stbar.h"
#include "aros_mouse.h"

#if !defined(MIN)
#define MIN(a, b)       ((a) < (b)) ? (a) : (b)
#endif
extern void amiga_blit(int x, int y, int y1, int y2, int w, int h);

extern int sb_height;
extern MOUSESTATE mousestate;
extern Object *app;

static volatile int amigavid_enabled = 0;
Object *main_wnd = NULL;
Object *maingrpObj = NULL;
Object *statusbarObj = NULL;

/**
 ** *****************************************************************************
 ** **  Render Area Class  ******************************************************
 ** *****************************************************************************
 **/

// Private Attributes ..
#define MUIA_AmigaVideoRender_Activate          (TAG_USER + 0xf1f1)

#define MUIA_AmigaVideoWindow_ShowCursor        (TAG_USER + 0xf2f1)

// Private Methods ..
#define MUIM_AmigaVideoRender_Timer             (TAG_USER + 0xf1f1)
#define MUIM_AmigaVideoRender_VBlank            (TAG_USER + 0xf1f2)

struct AmigaVideoRender_DATA {
    struct MUI_EventHandlerNode   ehn;
    struct MUI_InputHandlerNode ihnvb;
    struct MUI_InputHandlerNode ihnos;
    UWORD LastMX, LastMY;
    BOOL winactive, inside;
};

Object *AmigaVideoRender__OM_NEW(Class *CLASS, Object *self, struct opSet *message)
{
    Object *o = NULL;
    D(bug("86Box:%s()\n", __func__);)

    o = (Object *) DoSuperNewTags
    (
        CLASS, self, NULL,
        MUIA_Frame,             MUIV_Frame_None,
        MUIA_InnerLeft,         0,
	MUIA_InnerTop,          0,
	MUIA_InnerRight,        0,
	MUIA_InnerBottom,       0,

        MUIA_FillArea,          FALSE,

        TAG_MORE, (IPTR) message->ops_AttrList
    );

    if (o)
    {
        struct AmigaVideoRender_DATA *data = INST_DATA(CLASS, o);

        data->winactive = FALSE;
        data->inside = FALSE;

        /* events we want to handle .. */
        data->ehn.ehn_Events   = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY;
        data->ehn.ehn_Priority = 1;
        data->ehn.ehn_Flags    = 0;
        data->ehn.ehn_Object   = o;
        data->ehn.ehn_Class    = CLASS;

        /* vblank update */
        data->ihnvb.ihn_Flags  = MUIIHNF_TIMER;
        data->ihnvb.ihn_Method = MUIM_AmigaVideoRender_VBlank;
        data->ihnvb.ihn_Object = o;
        data->ihnvb.ihn_Millis = 1000/60;

        /* 1sec timer */
        data->ihnos.ihn_Flags  = MUIIHNF_TIMER;
        data->ihnos.ihn_Method = MUIM_AmigaVideoRender_Timer;
        data->ihnos.ihn_Object = o;
        data->ihnos.ihn_Millis = 1000;
    }
    return(o);
}

IPTR AmigaVideoRender__OM_DISPOSE(Class *CLASS, Object *self, Msg message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaVideoRender__OM_SET(Class *CLASS, Object *self, struct opSet *message)
{
    struct AmigaVideoRender_DATA *data = INST_DATA(CLASS, self);
    struct TagItem       *tags  = message->ops_AttrList;
    struct TagItem   	 *tag;
    IPTR retval, activate = (IPTR)-1;

    D(bug("86Box:%s()\n", __func__);)

    while ((tag = NextTagItem(&tags)) != NULL)
    {
    	switch(tag->ti_Tag)
	{
            case MUIA_AmigaVideoRender_Activate:
                {
                    activate = tag->ti_Data;
                    D(bug("86Box:%s - Activate = %d\n", __func__, tag->ti_Data);)
                }
                break;
	    default:
	    	D(bug("86Box:%s -     %08x = %p\n", __func__, tag->ti_Tag, tag->ti_Data);)
		break;
	}
    }

    retval =  DoSuperMethodA(CLASS, self, message);

    if (activate != -1)
    {
        D_EVENT(bug("86Box:%s MUIA_AmigaVideoRender_Activate\n", __func__);)
        if (activate)
        {
            D_EVENT(bug("86Box:%s - window active\n", __func__);)
            data->winactive = TRUE;
            if (data->inside)
            {
                mousestate.capture = 1;
                SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, FALSE);
            }
            else
            {
                mousestate.capture = 0;
                SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, TRUE);
            }
        }
        else
        {
            D_EVENT(bug("86Box:%s - window inactive\n", __func__);)
            data->winactive = FALSE;
            mousestate.capture = 0;
            SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, TRUE);
        }
    }

    return(retval);
}

IPTR AmigaVideoRender__OM_GET(Class *CLASS, Object *self, struct opGet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaVideoRender__MUIM_AskMinMax(Class *CLASS, Object *self, struct MUIP_AskMinMax *message)
{
    D(bug("86Box:%s()\n", __func__);)

    DoSuperMethodA(CLASS, self, (Msg)message);

    message->MinMaxInfo->MinWidth  += 200;
    message->MinMaxInfo->MinHeight += 150;
    message->MinMaxInfo->DefWidth  += scrnsz_x;
    message->MinMaxInfo->DefHeight += scrnsz_y;
    message->MinMaxInfo->MaxWidth   = scrnsz_x;
    message->MinMaxInfo->MaxHeight  = scrnsz_y;

    return TRUE;
}

IPTR AmigaVideoRender__MUIM_Setup(Class *CLASS, Object *self, struct MUIP_Setup *message)
{
    struct AmigaVideoRender_DATA *data = INST_DATA(CLASS, self);

    D(bug("86Box:%s()\n", __func__);)
    
    if (!DoSuperMethodA(CLASS, self, (Msg)message)) return FALSE;

    DoMethod(_app(self), MUIM_Application_AddInputHandler, (IPTR) &data->ihnvb);
    DoMethod(_app(self), MUIM_Application_AddInputHandler, (IPTR) &data->ihnos);

    DoMethod(_win(self), MUIM_Window_AddEventHandler, (IPTR)&data->ehn);

    return TRUE;
}

IPTR AmigaVideoRender__MUIM_Cleanup(Class *CLASS, Object *self, struct MUIP_Cleanup *message)
{
    struct AmigaVideoRender_DATA *data = INST_DATA(CLASS, self);

    D(bug("86Box:%s()\n", __func__);)

    DoMethod(_win(self), MUIM_Window_RemEventHandler, (IPTR)&data->ehn);

    DoMethod(_app(self), MUIM_Application_RemInputHandler, (IPTR) &data->ihnos);
    DoMethod(_app(self), MUIM_Application_RemInputHandler, (IPTR) &data->ihnvb);

    return DoSuperMethodA(CLASS, self, (Msg)message);
}

IPTR AmigaVideoRender__MUIM_AmigaVideoRender_VBlank(Class *CLASS, Object *self, Msg message)
{
    Object *o = NULL;
    D(bug("86Box:%s()\n", __func__);)

    DoMethod(self, MUIM_Draw, MADF_DRAWOBJECT);

    return 0;
}

IPTR AmigaVideoRender__MUIM_AmigaVideoRender_Timer(Class *CLASS, Object *self, Msg message)
{
    Object *o = NULL;
    D(bug("86Box:%s()\n", __func__);)

    pc_onesec();

    return 0;
}

IPTR AmigaVideoRender__MUIM_HandleEvent(Class *CLASS, Object *self, struct MUIP_HandleEvent *message)
{
    struct AmigaVideoRender_DATA *data = INST_DATA(CLASS, self);

    D(bug("86Box:%s(0x%p)\n", __func__, message->imsg);)

    if (message->imsg)
    {
        switch (message->imsg->Class)
        {
        case IDCMP_MOUSEBUTTONS:
            {
                D_EVENT(bug("86Box:%s - button %d\n", __func__, message->imsg->Class);)

                if (message->imsg->Code == SELECTDOWN)
                    mousestate.buttons |= 1;
                else if (message->imsg->Code == SELECTUP)
                    mousestate.buttons &= ~1;
                else if (message->imsg->Code == MENUDOWN)
                    mousestate.buttons |= 2;
                else if (message->imsg->Code == MENUUP)
                    mousestate.buttons &= ~2;
                else if (message->imsg->Code == MIDDLEDOWN)
                    mousestate.buttons |= 4;
                else if (message->imsg->Code == MIDDLEUP)
                    mousestate.buttons &= ~4;
            }
            break;

        case IDCMP_MOUSEMOVE:
            {
                UWORD width = MIN(scrnsz_x, _right(self) - _left(self) + 1);
                UWORD height = MIN(scrnsz_y, _bottom(self) - _top(self) + 1);
                WORD new_x = 0, new_y = 0;
                BOOL inside = TRUE;

                D_EVENT(bug("86Box:%s - mousemove %d, %d\n", __func__, message->imsg->MouseX, message->imsg->MouseY);)

                if (message->imsg->MouseX < _left(self))
                {
                    inside = FALSE;
                }
                else if (message->imsg->MouseX > width)
                {
                    inside = FALSE;
                }
                else
                {
                    new_x = message->imsg->MouseX - data->LastMX;
                    data->LastMX = message->imsg->MouseX;
                }

                if (message->imsg->MouseY < _top(self))
                {
                    inside = FALSE;
                }
                else if (message->imsg->MouseY > height)
                {
                    inside = FALSE;
                }
                else
                {
                    new_y = message->imsg->MouseY - data->LastMY;
                    data->LastMY = message->imsg->MouseY;
                }

                if ((data->inside = inside) == TRUE)
                {
                    displayWindow->Flags  |= WFLG_RMBTRAP;
                    mousestate.dx += new_x;
                    mousestate.dy += new_y;
                    D_EVENT(bug("86Box:%s -         = %d, %d\n", __func__, mousestate.dx, mousestate.dy);)
                    if (data->winactive)
                    {
                        D_EVENT(bug("86Box:%s - mouse captured\n", __func__);)
                        mousestate.capture = 1;
                        SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, FALSE);
                    }
                    else
                    {
                        mousestate.capture = 0;
                        SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, TRUE);
                    }
                }
                else
                {
                    mousestate.capture = 0;
                    displayWindow->Flags  &= ~WFLG_RMBTRAP;
                    SET(_win(self), MUIA_AmigaVideoWindow_ShowCursor, TRUE);
                }
            }
            break;

        case IDCMP_RAWKEY:
            {
                aros_dokeyevent(message->imsg->Code);
            }
            break;
        }
    }

    return 0;
}

IPTR AmigaVideoRender__MUIM_Draw(Class *CLASS, Object *self, struct MUIP_Draw *message)
{
    D(bug("86Box:%s()\n", __func__);)

    if (!(message->flags & MADF_DRAWOBJECT))
        return 0;

    if (displayWindow && renderBitMap)
    {
        D(bug("86Box:%s - blitting display\n", __func__);)

        BltBitMapRastPort(renderBitMap, 0, 0,
                          _rp(self), _mleft(self), _mtop(self), _mright(self) - _mleft(self) + 1, _mbottom(self) - _mtop(self) + 1, 0xC0);
    }
    D(bug("86Box:%s - done\n", __func__);)
    return 1;
}

ZUNE_CUSTOMCLASS_11
(
  AmigaVideoRender, NULL, MUIC_Area, NULL,
  MUIM_AmigaVideoRender_VBlank,               Msg,
  MUIM_AmigaVideoRender_Timer,                Msg,
  MUIM_Draw,                                  struct MUIP_Draw *,
  MUIM_HandleEvent,                           struct MUIP_HandleEvent *,
  MUIM_AskMinMax,                             struct MUIP_AskMinMax *,
  OM_NEW,                                     struct opSet *,
  OM_DISPOSE,                                 Msg,
  OM_SET,                                     struct opSet *,
  OM_GET,                                     struct opGet *,
  MUIM_Setup,                                 struct MUIP_Setup *,
  MUIM_Cleanup,                               struct MUIP_Cleanup *
);

/**
 ** *****************************************************************************
 ** **  Main Display Window Class  **********************************************
 ** *****************************************************************************
 **/

struct AmigaVideoWindow_DATA {
    Object *rendObj;
    ULONG blankCursor;
    BOOL cursvis;
};

Object *AmigaVideoWindow__OM_NEW(Class *CLASS, Object *self, struct opSet *message)
{
    Object *o = NULL, *grp, *bar, *rend;

    D(bug("86Box:%s()\n", __func__);)

    o = (Object *) DoSuperNewTags
    (
        CLASS, self, NULL,
    
        MUIA_Window_Title,              (IPTR)emu_version,
        MUIA_Window_ID,                 MAKE_ID('8','6','R','W'),
        MUIA_Window_AppWindow,          TRUE,
        MUIA_Window_CloseGadget,        TRUE,
        MUIA_Window_SizeGadget,         FALSE,
#if (0)
        MUIA_Window_Screen,             _newIconWin__Screen,
        MUIA_Window_Backdrop,           fullscreen ? TRUE : FALSE,
        MUIA_Window_Borderless,         fullscreen ? TRUE : FALSE,
#endif
        MUIA_Window_Width,              scrnsz_x,
        MUIA_Window_Height,             scrnsz_y,
        MUIA_Window_LeftEdge,           o,
        MUIA_Window_TopEdge,            o,

        WindowContents, (IPTR)(grp = MUI_NewObject(MUIC_Group,
            MUIA_Frame,         MUIV_Frame_None,
            MUIA_InnerLeft,     0,
            MUIA_InnerTop,      0,
            MUIA_InnerRight,    0,
            MUIA_InnerBottom,   0,
            MUIA_Group_Spacing, 0,
            Child, (IPTR)(rend = NewObject(AmigaVideoRender_CLASS->mcc_Class, NULL,
            TAG_DONE)),
            Child, (IPTR)(bar = NewObject(AmigaStatusBar_CLASS->mcc_Class, NULL,
            TAG_DONE)),
        TAG_DONE)),

        TAG_MORE, (IPTR) message->ops_AttrList
    );

    if (o)
    {
        struct AmigaVideoWindow_DATA *data;

        D(bug("86Box:%s object @ 0x%p\n", __func__, o);)
        data = INST_DATA(CLASS, o);
        D(bug("86Box:%s object data @ 0x%p\n", __func__, data);)

        data->cursvis = TRUE;
        data->rendObj = rend;
        maingrpObj = grp;
        statusbarObj = bar;
    }

    D(bug("86Box:%s - returning %p\n", __func__, o);)

    return(o);
}

IPTR AmigaVideoWindow__OM_DISPOSE(Class *CLASS, Object *self, Msg message)
{
    IPTR retval;

    D(bug("86Box:%s()\n", __func__);)

    maingrpObj = NULL;
    statusbarObj = NULL;

    retval = DoSuperMethodA(CLASS, self, message);

    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

IPTR AmigaVideoWindow__OM_SET(Class *CLASS, Object *self, struct opSet *message)
{
    struct AmigaVideoWindow_DATA *data = INST_DATA(CLASS, self);
    struct TagItem       *tags  = message->ops_AttrList;
    struct TagItem   	 *tag;
    IPTR retval, show = (IPTR)-1;

    D(bug("86Box:%s()\n", __func__);)

    while ((tag = NextTagItem(&tags)) != NULL)
    {
    	switch(tag->ti_Tag)
	{
            case MUIA_Window_Activate:
                {
                    SET(data->rendObj, MUIA_AmigaVideoRender_Activate, tag->ti_Data);
                }
                break;
            case MUIA_Window_Open:
                {
                    show = tag->ti_Data;
                }
                break;
            case MUIA_AmigaVideoWindow_ShowCursor:
                {
                    if (tag->ti_Data)
                    {
                        if ((displayWindow) && (!data->cursvis))
                        {
                            D(bug("86Box:%s - showing cursor\n", __func__);)
                            SetWindowPointer(displayWindow, WA_Pointer, NULL, TAG_DONE );
                            data->cursvis = TRUE;
                        }
                    }
                    else
                    {
                        if ((displayWindow) && (data->cursvis))
                        {
                            D(bug("86Box:%s - hiding cursor\n", __func__);)
                            SetPointer(displayWindow, (UWORD *)&data->blankCursor, 1, 1, 0, 0);
                            data->cursvis = FALSE;
                        }                    
                    }
                }
                break;
	    default:
	    	D(bug("86Box:%s -     %08x = %p\n", __func__, tag->ti_Tag, tag->ti_Data);)
		break;
	}
    }

    retval = DoSuperMethodA(CLASS, self, (Msg)message);

    if (show != (IPTR)-1)
    {
        if (show)
        {
            GET(self, MUIA_Window_Window, &displayWindow);

            D(bug("86Box:%s - Amiga Window @ 0x%p\n", __func__, displayWindow);)
        }
        else
            displayWindow = NULL;
    }
    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

IPTR AmigaVideoWindow__OM_GET(Class *CLASS, Object *self, struct opGet *message)
{
    IPTR retval;

    D(bug("86Box:%s(%08x)\n", __func__, message->opg_AttrID);)

    retval = DoSuperMethodA(CLASS, self, (Msg)message);

    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

IPTR AmigaVideoWindow__MUIM_ConnectParent(Class *CLASS, Object *self, Msg message)
{
    struct AmigaVideoWindow_DATA *data = INST_DATA(CLASS, self);
    IPTR retval;

    D(bug("86Box:%s()\n", __func__);)

    retval = DoSuperMethodA(CLASS, self, message);
    if (retval)
    {
        DoMethod(self, MUIM_Notify,
          MUIA_Window_CloseRequest, TRUE,
          _app(self), 2,
          MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    }
    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

IPTR AmigaVideoWindow__MUIM_DisconnectParent(Class *CLASS, Object *self, Msg message)
{
    struct AmigaVideoWindow_DATA *data = INST_DATA(CLASS, self);
    IPTR retval;

    D(bug("86Box:%s()\n", __func__);)

    DoMethod(self, MUIM_KillNotify, MUIA_Window_CloseRequest);

    retval = DoSuperMethodA(CLASS, self, message);

    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

ZUNE_CUSTOMCLASS_6
(
  AmigaVideoWindow, NULL, MUIC_Window, NULL,
  OM_NEW,                                     struct opSet *,
  OM_DISPOSE,                                 Msg,
  OM_SET,                                     struct opSet *,
  OM_GET,                                     struct opGet *,
  MUIM_ConnectParent,                         Msg,
  MUIM_DisconnectParent,                      Msg
);

/**
 ** *****************************************************************************
 ** **  video entry points  *****************************************************
 ** *****************************************************************************
 **/


void amigavid_close(void)
{
    D(bug("86Box:%s()\n", __func__);)

    /* Unregister our renderer! */
    video_setblit(NULL);

    if (BMRastPort)
    {
        FreeRastPort(BMRastPort);
        BMRastPort = NULL;
    }

    if (main_wnd)
    {
        Object *disposeObj = main_wnd;
        DoMethod(app, OM_REMMEMBER, (IPTR) main_wnd);
        main_wnd = NULL;
        DisposeObject(disposeObj);
    }
    displayWindow = NULL;
}

int amigavid_pause(void)
{
    D(bug("86Box:%s()\n", __func__);)
    return(0);
}

void amigavid_resize(int x, int y)
{
    UWORD width;
    UWORD height;
    WORD diffx, diffy;

    D(bug("86Box:%s(%dx%d)\n", __func__, x, y);)

    if (displayWindow)
    {
        width = displayWindow->Width - (displayWindow->BorderLeft + displayWindow->BorderRight);
        height = displayWindow->Height - (displayWindow->BorderTop + displayWindow->BorderBottom + sb_height);
        
        if (width > x)
            diffx = -(width - x);
        else
            diffx = x - width;
        
        if (height > y)
            diffy = -(height - y);
        else
            diffy = y - height;

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

        if (main_wnd)
        {
            IPTR windowopen = FALSE;
            Object *disposeObj = main_wnd;
            GET(disposeObj, MUIA_Window_Open, &windowopen);
            DoMethod(app, OM_REMMEMBER, (IPTR) main_wnd);
            main_wnd = NULL;
            DisposeObject(disposeObj);
            main_wnd = NewObject(AmigaVideoWindow_CLASS->mcc_Class, NULL,
                            MUIA_Window_Activate, TRUE,
                            TAG_DONE);
            DoMethod(app, OM_ADDMEMBER, (IPTR) main_wnd);
            SET(main_wnd, MUIA_Window_Open, windowopen);
        }
#if (0)
        SizeWindow(displayWindow, diffx, diffy);
#endif
    }
}

void amigavid_enable(int enable)
{
    D(bug("86Box:%s()\n", __func__);)
    amigavid_enabled = enable;
}

int amigavid_init(APTR h)
{
    D(bug("86Box:%s()\n", __func__);)

    if (!main_wnd)
    {
        main_wnd = NewObject(AmigaVideoWindow_CLASS->mcc_Class, NULL,
                MUIA_Window_Activate, TRUE,
            TAG_DONE);

        if (main_wnd)
            DoMethod(app, OM_ADDMEMBER, (IPTR) main_wnd);
    }
    if (main_wnd)
    {
        D(bug("86Box:%s - window object @ 0x%p\n", __func__, main_wnd);)
        SET(main_wnd, MUIA_Window_Open, TRUE);

        /* Register our renderer! */
        video_setblit(amiga_blit);

        return(1);
    }

    return(0);
}
