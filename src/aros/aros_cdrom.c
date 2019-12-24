/*
    $Id$
*/

#include <aros/debug.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <stdint.h>
#include <stdio.h>

#include "../86box.h"
#include "../plat.h"

void
plat_cdrom_ui_update(uint8_t id, uint8_t reload)
{
    D(bug("86Box:%s(%d)\n", __func__, id);)
}

void
zip_eject(uint8_t id)
{
    D(bug("86Box:%s(%d)\n", __func__, id);)
}

void
zip_reload(uint8_t id)
{
    D(bug("86Box:%s(%d)\n", __func__, id);)
}
