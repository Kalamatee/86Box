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
#include "../mouse.h"

#include "aros_mouse.h"

MOUSESTATE mousestate;

void
mouse_poll(void)
{
    static int b = 0;

    D(bug("86Box:%s()\n", __func__);)

    if (mousestate.capture || video_fullscreen) {
        mouse_x += mousestate.dx;
        mouse_y += mousestate.dy;
        mouse_z = mousestate.dwheel;
        bug("86Box:%s - %d, %d\n", __func__, mousestate.dx, mousestate.dy);
        mousestate.dx = 0;
        mousestate.dy = 0;
        if (b != mousestate.buttons) {
            mouse_buttons = mousestate.buttons;
            b = mousestate.buttons;
        }
    }
}
