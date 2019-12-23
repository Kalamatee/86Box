/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "../86box.h"
#include "../plat.h"

static int	sb_ready = 0;

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
