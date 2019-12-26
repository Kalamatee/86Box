/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/alib.h>
#include <proto/muimaster.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <zune/customclasses.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"
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

static int	sb_ready = 0;
int sb_height = 0;

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
}

/* API */
/* API: update one of the icons after activity. */
void
ui_sb_update_icon(int tag, int active)
{
    D(bug("86Box:%s(%d, %d)\n", __func__, tag, active);)
}

/* API: This is for the drive state indicator. */
void
ui_sb_update_icon_state(int tag, int state)
{
    D(bug("86Box:%s(%d, %d)\n", __func__, tag, state);)
}

/* API */
void
ui_sb_bugui(char *str)
{
    D(bug("86Box:%s('%s')\n", __func__, str);)
}

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
 ** **  StatusBar Area Class  ******************************************************
 ** *****************************************************************************
 **/

struct AmigaStatusBar_DATA {
    int sb_parts;
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

    return(o);
}

IPTR AmigaStatusBar__OM_DISPOSE(Class *CLASS, Object *self, Msg message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBar__OM_SET(Class *CLASS, Object *self, struct opSet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBar__OM_GET(Class *CLASS, Object *self, struct opGet *message)
{
    D(bug("86Box:%s()\n", __func__);)

    return DoSuperMethodA(CLASS, self, message);
}

IPTR AmigaStatusBar__MUIM_AskMinMax(Class *CLASS, Object *self, struct MUIP_AskMinMax *message)
{
    D(bug("86Box:%s()\n", __func__);)

    DoSuperMethodA(CLASS, self, (Msg)message);

    message->MinMaxInfo->MinWidth  += 200;
    message->MinMaxInfo->MinHeight += 15;
    message->MinMaxInfo->DefWidth  += 200;
    message->MinMaxInfo->DefHeight += 15;
    message->MinMaxInfo->MaxWidth   = MUI_MAXMAX;
    message->MinMaxInfo->MaxHeight  = 15;

    return TRUE;
}

IPTR AmigaStatusBar__xxxxxxxxxxxxxx(Class *CLASS, Object *self, Msg message)
{
    struct AmigaStatusBar_DATA *data = INST_DATA(CLASS, self);
    int i, id, hdint;
    int edge = 0;
    int c_mfm, c_esdi, c_xta;
    int c_ide, c_scsi;
    int do_net;
    char *hdc_name;
    
    D(bug("86Box:%s()\n", __func__);)

    hdint = (machines[machine].flags & MACHINE_HDC) ? 1 : 0;
    c_mfm = hdd_count(HDD_BUS_MFM);
    c_esdi = hdd_count(HDD_BUS_ESDI);
    c_xta = hdd_count(HDD_BUS_XTA);
    c_ide = hdd_count(HDD_BUS_IDE);
    c_scsi = hdd_count(HDD_BUS_SCSI);
    do_net = network_available();

    data->sb_parts = 0;

    for (i=0; i<FDD_NUM; i++) {
	if (fdd_get_type(i) != 0)
		data->sb_parts++;
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
		data->sb_parts++;
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
		data->sb_parts++;
    }
    if (c_mfm && (hdint || !memcmp(hdc_name, "st506", 5))) {
	/* MFM drives, and MFM or Internal controller. */
	data->sb_parts++;
    }
    if (c_esdi && (hdint || !memcmp(hdc_name, "esdi", 4))) {
	/* ESDI drives, and ESDI or Internal controller. */
	data->sb_parts++;
    }
    if (c_xta && (hdint || !memcmp(hdc_name, "xta", 3)))
	data->sb_parts++;
    if (c_ide && (hdint || !memcmp(hdc_name, "xtide", 5) || !memcmp(hdc_name, "ide", 3)))
	data->sb_parts++;
    if (c_scsi && (scsi_card_current != 0))
	data->sb_parts++;
    if (do_net)
	data->sb_parts++;
    data->sb_parts += 2;

    D(bug("86Box:%s - sb_parts = &d\n", __func__, data->sb_parts);)

    return 0;
}


IPTR AmigaStatusBar__MUIM_Draw(Class *CLASS, Object *self, struct MUIP_Draw *message)
{
    D(bug("86Box:%s()\n", __func__);)

    if (!(message->flags & MADF_DRAWOBJECT))
        return 0;

    return 1;
}

ZUNE_CUSTOMCLASS_6
(
  AmigaStatusBar, NULL, MUIC_Area, NULL,
  OM_NEW,                                     struct opSet *,
  OM_DISPOSE,                                 Msg,
  OM_SET,                                     struct opSet *,
  OM_GET,                                     struct opGet *,
  MUIM_AskMinMax,                             struct MUIP_AskMinMax *,
  MUIM_Draw,                                  struct MUIP_Draw *
);
