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
#include "../game/gameport.h"

joystick_t joystick_state[MAX_JOYSTICKS];

void joystick_init()
{
    D(bug("86Box:%s()\n", __func__);)
}

void joystick_process(void)
{
    D(bug("86Box:%s()\n", __func__);)
}

