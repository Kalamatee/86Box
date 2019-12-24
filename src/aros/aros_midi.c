/*
    $Id$
*/

#include <aros/debug.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <stdint.h>
#include <stdio.h>

#include <stdint.h>

#include "../86box.h"
#include "../plat.h"

void
plat_midi_init(void)
{
    D(bug("86Box:%s()\n", __func__);)
}

void
plat_midi_close(void)
{
    D(bug("86Box:%s()\n", __func__);)
}

int
plat_midi_get_num_devs(void)
{
    D(bug("86Box:%s()\n", __func__);)
    return 0;
}

void
plat_midi_get_dev_name(int num, char *s)
{
    D(bug("86Box:%s()\n", __func__);)
}

void
plat_midi_play_msg(uint8_t *msg)
{
    D(bug("86Box:%s()\n", __func__);)
}

void
plat_midi_play_sysex(uint8_t *sysex, unsigned int len)
{
    D(bug("86Box:%s()\n", __func__);)
}

int
plat_midi_write(uint8_t val)
{
    D(bug("86Box:%s()\n", __func__);)
    return 0;
}
