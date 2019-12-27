/*
    $Id$
*/

#define DEBUG 0
#include <aros/debug.h>

#include <proto/alib.h>
#include <proto/muimaster.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/icon.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <zune/customclasses.h>
#include <utility/tagitem.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"

#include "../config.h"
#include "../cpu/cpu.h"
#include "../device.h"
#include "../machine/machine.h"
#include "../timer.h"
#include "../disk/hdd.h"
#include "../disk/hdc.h"
#include "../floppy/fdd.h"
#include "../floppy/fdd_86f.h"
#include "../scsi/scsi.h"
#include "../scsi/scsi_device.h"
#include "../cdrom/cdrom.h"
#include "../disk/zip.h"
#include "../cdrom/cdrom_image.h"
#include "../scsi/scsi_disk.h"
#include "../network/network.h"
#include "../video/video.h"
#include "../sound/sound.h"
#include "../ui.h"

#ifndef SB_ICON_HEIGHT
#define SB_ICON_HEIGHT SB_ICON_WIDTH
#endif

extern struct Screen *dispScreen;
static Object *sbarPrivate = NULL;

static int	sb_ready = 0;
int sb_height = 0;

/*************/

static int
hdd_count(int bus)
{
    int c = 0;
    int i;

    for (i=0; i<HDD_NUM; i++) {
	if (hdd[i].bus == bus)
		c++;
    }

    return(c);
}


/**
 ** *****************************************************************************
 ** **  StatusBar Area Icon Class  ******************************************************
 ** *****************************************************************************
 **/

// Private Attributes ..
#define MUIA_AmigaStatusBarIcon_List        (TAG_USER + 0xf3f1)

struct AmigaStatusBarIcon_DATA {
    struct Node iNode;
    Object *iSelf;
    int iState;
    struct DiskObject *iDiskObj;
};

Object *AmigaStatusBarIcon__OM_NEW(Class *CLASS, Object *self, struct opSet *message)
{
    struct List *iList = (struct List *)GetTagData(MUIA_AmigaStatusBarIcon_List, 0, message->ops_AttrList);
    Object *o = NULL;
    D(bug("86Box:%s()\n", __func__);)

    if (iList)
    {
        o = (Object *) DoSuperNewTags
        (
            CLASS, self, NULL,
            MUIA_FillArea, FALSE,
            TAG_MORE, (IPTR) message->ops_AttrList
        );

        if (o)
        {
            struct AmigaStatusBarIcon_DATA *data = INST_DATA(CLASS, o);
            IPTR geticon_error = 0, icon_id = 0;
            char icon_path[256] = "";
            char * icon_fname;

            GET(o, MUIA_UserData, &icon_id);
            D(bug("86Box:%s - ID = %08x\n", __func__, icon_id);)

            sprintf(icon_path, "PROGDIR:icons");
            if (icon_id & SB_FLOPPY)
                icon_fname = "floppe_35";
            else if (icon_id & SB_CDROM)
                icon_fname = "cdrom";
            else if (icon_id & SB_ZIP)
                icon_fname = "zip";
            else if (icon_id & SB_HDD)
                icon_fname = "hard_disk";
            else if (icon_id & SB_NETWORK)
                icon_fname = "network";
            AddPart(icon_path, icon_fname, sizeof(icon_path));

            bug("86Box:%s - file = '%s'\n", __func__, icon_path);
            data->iDiskObj = GetIconTags(icon_path,
                        (dispScreen) ? ICONGETA_Screen : TAG_IGNORE, dispScreen,
                        (dispScreen) ? ICONGETA_RemapIcon : TAG_IGNORE, TRUE,
                        ICONGETA_RemapIcon, TRUE,
                        ICONGETA_GenerateImageMasks, TRUE,
                        ICONGETA_FailIfUnavailable, TRUE,
                        ICONA_ErrorCode, &geticon_error,
                        TAG_DONE);
            bug("86Box:%s - dob @ 0x%p\n", __func__, data->iDiskObj);
            data->iSelf = o;
            AddTail(iList, (struct Node *)&data->iNode);
        }
    }
    return(o);
}

IPTR AmigaStatusBarIcon__OM_DISPOSE(Class *CLASS, Object *self, Msg message)
{
    struct AmigaStatusBarIcon_DATA *data = INST_DATA(CLASS, self);

    D(bug("86Box:%s()\n", __func__);)

    if (data->iDiskObj)
    {
        FreeDiskObject(data->iDiskObj);
    }
    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBarIcon__OM_SET(Class *CLASS, Object *self, struct opSet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBarIcon__OM_GET(Class *CLASS, Object *self, struct opGet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBarIcon__MUIM_AskMinMax(Class *CLASS, Object *self, struct MUIP_AskMinMax *message)
{
    D(bug("86Box:%s()\n", __func__);)

    DoSuperMethodA(CLASS, self, (Msg)message);

    message->MinMaxInfo->MinWidth  += SB_ICON_WIDTH;
    message->MinMaxInfo->MinHeight += SB_ICON_HEIGHT;
    message->MinMaxInfo->DefWidth  += SB_ICON_WIDTH;
    message->MinMaxInfo->DefHeight += SB_ICON_HEIGHT;
    message->MinMaxInfo->MaxWidth   = SB_ICON_WIDTH;
    message->MinMaxInfo->MaxHeight  = SB_ICON_HEIGHT;

    return TRUE;
}

IPTR AmigaStatusBarIcon__MUIM_Draw(Class *CLASS, Object *self, struct MUIP_Draw *message)
{
    D(bug("86Box:%s()\n", __func__);)

    if (!(message->flags & MADF_DRAWOBJECT))
        return 0;

    return 1;
}

ZUNE_CUSTOMCLASS_6
(
  AmigaStatusBarIcon, NULL, MUIC_Area, NULL,
  OM_NEW,                                     struct opSet *,
  OM_DISPOSE,                                 Msg,
  OM_SET,                                     struct opSet *,
  OM_GET,                                     struct opGet *,
  MUIM_AskMinMax,                             struct MUIP_AskMinMax *,
  MUIM_Draw,                                  struct MUIP_Draw *
);

/**
 ** *****************************************************************************
 ** **  StatusBar Area Class  ******************************************************
 ** *****************************************************************************
 **/

// Private Attributes ..
#define MUIA_AmigaStatusBar_UpdateIcon        (TAG_USER + 0xf4f1)
#define MUIA_AmigaStatusBar_UpdateState       (TAG_USER + 0xf4f2)

// Private Methods ..
#define MUIM_AmigaStatusBar_Update            (TAG_USER + 0xf4f2)
#define MUIM_AmigaStatusBar_IconUpdate        (TAG_USER + 0xf4f2)

struct AmigaStatusBar_DATA {
    struct List PanelEntries;
    IPTR        updateicon;
    IPTR        updatestate;
};

Object *AmigaStatusBar__OM_NEW(Class *CLASS, Object *self, struct opSet *message)
{
    Object *o = NULL;
    D(bug("86Box:%s()\n", __func__);)

    o = (Object *) DoSuperNewTags
    (
        CLASS, self, NULL,
        MUIA_FillArea, FALSE,
        TAG_MORE, (IPTR) message->ops_AttrList
    );

    if (o)
    {
        struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, o);
        D(bug("86Box:%s - data @ 0x%p\n", __func__, data);)
        NEWLIST(&data->PanelEntries);
        sbarPrivate = o;
    }

    return(o);
}

VOID AmigaStatusBar__DisposeIcons(struct AmigaStatusBar_DATA *data)
{
    struct AmigaStatusBarIcon_DATA *tmpNode, *tmpN2;
    D(bug("86Box:%s()\n", __func__);)
    ForeachNodeSafe(&data->PanelEntries, tmpNode, tmpN2)
    {
        Remove(&tmpNode->iNode);
        DisposeObject((Object *)tmpNode->iSelf);
    }
}

IPTR AmigaStatusBar__OM_DISPOSE(Class *CLASS, Object *self, Msg message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    D(bug("86Box:%s()\n", __func__);)

    sbarPrivate = NULL;
    AmigaStatusBar__DisposeIcons(data);

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBar__OM_SET(Class *CLASS, Object *self, struct opSet *message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    struct TagItem       *tags  = message->ops_AttrList;
    struct TagItem   	 *tag;
    IPTR retval;

    D(bug("86Box:%s()\n", __func__);)

    while ((tag = NextTagItem(&tags)) != NULL)
    {
    	switch(tag->ti_Tag)
	{
            case MUIA_AmigaStatusBar_UpdateIcon:
                data->updateicon = tag->ti_Data;
                break;
            case MUIA_AmigaStatusBar_UpdateState:
                data->updatestate = tag->ti_Data;
                break;
	    default:
	    	D(bug("86Box:%s -     %08x = %p\n", __func__, tag->ti_Tag, tag->ti_Data);)
		break;
	}
    }

    retval = DoSuperMethodA(CLASS, self, (Msg)message);

    D(bug("86Box:%s - returning %p\n", __func__, retval);)

    return retval;
}

IPTR AmigaStatusBar__OM_GET(Class *CLASS, Object *self, struct opGet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBar__MUIM_AskMinMax(Class *CLASS, Object *self, struct MUIP_AskMinMax *message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    UWORD width = 0;

    D(bug("86Box:%s()\n", __func__);)

    DoSuperMethodA(CLASS, self, (Msg)message);

    D(bug("86Box:%s - data @ 0x%p\n", __func__, data);)

    if (!IsListEmpty(&data->PanelEntries))
    {
        struct Node *tmpNode;
        ForeachNode(&data->PanelEntries, tmpNode)
        {
          width += SB_ICON_WIDTH + 1;
        }
    }

    D(bug("86Box:%s - width = %d\n", __func__, width);)

    message->MinMaxInfo->MinWidth  += (width + 50);
    message->MinMaxInfo->MinHeight += SB_ICON_HEIGHT + 2;
    message->MinMaxInfo->DefWidth  += (width + 50);
    message->MinMaxInfo->DefHeight += SB_ICON_HEIGHT + 2;
    message->MinMaxInfo->MaxWidth   = MUI_MAXMAX;
    message->MinMaxInfo->MaxHeight  = SB_ICON_HEIGHT + 2;

    return TRUE;
}

IPTR AmigaStatusBar__MUIM_AmigaStatusBar_Update(Class *CLASS, Object *self, Msg message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    int i, id, hdint;
    int edge = 0;
    int c_mfm, c_esdi, c_xta;
    int c_ide, c_scsi;
    int do_net;
    char *hdc_name;
    Object *objTmp;

    D(bug("86Box:%s()\n", __func__);)

    AmigaStatusBar__DisposeIcons(data);

    hdint = (machines[machine].flags & MACHINE_HDC) ? 1 : 0;
    c_mfm = hdd_count(HDD_BUS_MFM);
    c_esdi = hdd_count(HDD_BUS_ESDI);
    c_xta = hdd_count(HDD_BUS_XTA);
    c_ide = hdd_count(HDD_BUS_IDE);
    c_scsi = hdd_count(HDD_BUS_SCSI);
    do_net = network_available();

    for (i=0; i<FDD_NUM; i++) {
	if (fdd_get_type(i) != 0)
        {
            objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
                MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
                MUIA_UserData, SB_FLOPPY | i,
            TAG_DONE);
        }
    }

    hdc_name = hdc_get_internal_name(hdc_current);

    for (i=0; i < CDROM_NUM; i++) {
	/* Could be Internal or External IDE.. */
	if ((cdrom[i].bus_type == CDROM_BUS_ATAPI) &&
	    !(hdint || !memcmp(hdc_name, "ide", 3)))
            continue;

	if ((cdrom[i].bus_type == CDROM_BUS_SCSI) &&
	    (scsi_card_current == 0))
            continue;
	if (cdrom[i].bus_type != 0)
        {
            objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
                MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
                MUIA_UserData, SB_CDROM | i,
            TAG_DONE);
        }
    }
    for (i=0; i < ZIP_NUM; i++) {
	/* Could be Internal or External IDE.. */
	if ((zip_drives[i].bus_type == ZIP_BUS_ATAPI) &&
	    !(hdint || !memcmp(hdc_name, "ide", 3)))
            continue;

	if ((zip_drives[i].bus_type == ZIP_BUS_SCSI) &&
	    (scsi_card_current == 0))
            continue;
	if (zip_drives[i].bus_type != 0)
        {
            objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
                MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
                MUIA_UserData, SB_ZIP | i,
            TAG_DONE);
        }
    }
    if (c_mfm && (hdint || !memcmp(hdc_name, "st506", 5))) {
	/* MFM drives, and MFM or Internal controller. */
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
            MUIA_UserData, SB_HDD | HDD_BUS_MFM,
        TAG_DONE);
    }
    if (c_esdi && (hdint || !memcmp(hdc_name, "esdi", 4))) {
	/* ESDI drives, and ESDI or Internal controller. */
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
            MUIA_UserData, SB_HDD | HDD_BUS_ESDI,
        TAG_DONE);
    }
    if (c_xta && (hdint || !memcmp(hdc_name, "xta", 3)))
    {
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
            MUIA_UserData, SB_HDD | HDD_BUS_XTA,
        TAG_DONE);
    }
    if (c_ide && (hdint || !memcmp(hdc_name, "xtide", 5) || !memcmp(hdc_name, "ide", 3)))
    {
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
            MUIA_UserData, SB_HDD | HDD_BUS_IDE,
        TAG_DONE);
    }
    if (c_scsi && (scsi_card_current != 0))
    {
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,
            MUIA_UserData, SB_HDD | HDD_BUS_SCSI,
        TAG_DONE);
    }
    if (do_net)
    {
        objTmp = NewObject(AmigaStatusBarIcon_CLASS->mcc_Class, NULL,
            MUIA_AmigaStatusBarIcon_List, &data->PanelEntries,        
            MUIA_UserData, SB_NETWORK,
        TAG_DONE);
    }

    D(bug("86Box:%s - done\n", __func__);)

    return 0;
}

IPTR AmigaStatusBar__MUIM_Draw(Class *CLASS, Object *self, struct MUIP_Draw *message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    struct AmigaStatusBarIcon_DATA *tmpNode;
    static struct TagItem          drawIconStateTags[] = {
        { ICONDRAWA_Frameless, TRUE         },
        { ICONDRAWA_Borderless, TRUE        },
        { ICONDRAWA_EraseBackground, TRUE  },
        { TAG_DONE,                         }
    };

    WORD x;

    D(bug("86Box:%s()\n", __func__);)

    if (!(message->flags & (MADF_DRAWOBJECT|MADF_DRAWUPDATE)))
        return 0;

    if (!IsListEmpty(&data->PanelEntries))
    {
        IPTR iUserData;

        x = _mleft(self) + 1;
        ForeachNode(&data->PanelEntries, tmpNode)
        {
            iUserData = 0;
            GET((Object *)tmpNode->iSelf, MUIA_UserData, &iUserData);
            D(bug("86Box:%s - ID = %08x\n", __func__, iUserData);)

            if ((!(message->flags & MADF_DRAWUPDATE)) || (data->updateicon == iUserData))
            {
                int penno = FILLPEN;
                SetAPen(_rp(self), _dri(self)->dri_Pens[BACKGROUNDPEN]);
                Move(_rp(self), x - 1,
                   _mtop(self));
                Draw(_rp(self), x - 1,
                   _mbottom(self));
                Move(_rp(self), x - 1,
                   _mtop(self));
                Draw(_rp(self), x + SB_ICON_WIDTH,
                   _mtop(self));

                if ((message->flags & MADF_DRAWUPDATE) && (data->updateicon == iUserData))
                {
                    tmpNode->iState = data->updatestate;
                    if (!(tmpNode->iDiskObj) && (data->updatestate))
                        penno = HIGHLIGHTTEXTPEN;
                }

                if ((!(message->flags & MADF_DRAWUPDATE)) || (data->updateicon == iUserData))
                {
                    data->updateicon = 0;

                    if (tmpNode->iDiskObj)
                    {
                        struct Rectangle icrect;
                        UWORD adjustx, adjusty;

                        D(bug("86Box:%s - Rendering Icon 0x%p\n", __func__, tmpNode->iDiskObj);)

                        GetIconRectangleA(NULL, tmpNode->iDiskObj, NULL, &icrect, drawIconStateTags);
                        adjustx = (SB_ICON_WIDTH - ((icrect.MaxY - icrect.MinY) + 1) >> 1);
                        adjusty = (((_mbottom(self) - _mtop(self)) + 1) - ((icrect.MaxY - icrect.MinY) + 1) >> 1);
                        DrawIconStateA
                          (
                            _rp(self), tmpNode->iDiskObj, NULL,
                            x + adjustx, 
                            _mtop(self) + 1 + adjusty,
                            (tmpNode->iState) ? IDS_SELECTED : IDS_NORMAL,
                            drawIconStateTags
                          );
                    }
                    else
                    {
                        D(bug("86Box:%s - Filling Area\n", __func__);)
                        SetAPen(_rp(self), _dri(self)->dri_Pens[penno]);

                        RectFill(_rp(self), x,
                                   _mtop(self) + 1,
                                   x + SB_ICON_WIDTH,
                                   _mtop(self) + 1 + SB_ICON_HEIGHT);
                    }
                }
            }
            x += SB_ICON_WIDTH + 1;  
        }
    }
    else
        x = _mleft(self);

    SetAPen(_rp(self), _dri(self)->dri_Pens[BACKGROUNDPEN]);
    RectFill(_rp(self), x,
               _mtop(self),
               _mright(self),
               _mbottom(self));
    return 1;
}

ZUNE_CUSTOMCLASS_7
(
  AmigaStatusBar, NULL, MUIC_Area, NULL,
  OM_NEW,                                     struct opSet *,
  OM_DISPOSE,                                 Msg,
  OM_SET,                                     struct opSet *,
  OM_GET,                                     struct opGet *,
  MUIM_AskMinMax,                             struct MUIP_AskMinMax *,
  MUIM_Draw,                                  struct MUIP_Draw *,
  MUIM_AmigaStatusBar_Update,                 Msg
);

/*****************/

/* API: mark the status bar as not ready. */
void
ui_sb_set_ready(int ready)
{
    D(bug("86Box:%s(%d)\n", __func__, ready);)
    sb_ready = ready;
}

/* API: update the status bar panes. */
void
ui_sb_update_panes(void)
{
    D(bug("86Box:%s()\n", __func__);)
    if (sbarPrivate)
    {
        DoMethod(sbarPrivate, MUIM_AmigaStatusBar_Update);
    }
}

/* API */
/* API: update one of the icons after activity. */
void
ui_sb_update_icon(int tag, int active)
{
    D(bug("86Box:%s(%d, %d)\n", __func__, tag, active);)
    if (sbarPrivate)
    {
        SET(sbarPrivate, MUIA_AmigaStatusBar_UpdateIcon, tag);
        SET(sbarPrivate, MUIA_AmigaStatusBar_UpdateState, active);
        DoMethod(sbarPrivate, MUIM_Draw, MADF_DRAWUPDATE);
    }
}

/* API: This is for the drive state indicator. */
void
ui_sb_update_icon_state(int tag, int state)
{
    D(bug("86Box:%s(%d, %d)\n", __func__, tag, state);)
    if (sbarPrivate)
    {
#if (0)
        DoMethod(sbarPrivate, MUIM_AmigaStatusBar_SetIconState, tag, state);
#endif
        DoMethod(sbarPrivate, MUIM_Draw, MADF_DRAWUPDATE);
    }
}

/* API */
void
ui_sb_bugui(char *str)
{
    D(bug("86Box:%s('%s')\n", __func__, str);)
}

