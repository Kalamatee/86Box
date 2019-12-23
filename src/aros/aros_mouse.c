/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <stdint.h>
#include <stdio.h>

#include <stdint.h>

#include "../86box.h"
#include "../plat.h"

int mouse_capture;

void
mouse_poll(void)
{
    D(bug("86Box:%s()\n", __func__);)
}