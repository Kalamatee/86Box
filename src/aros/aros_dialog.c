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

int
ui_msgbox(int flags, void *arg)
{
    int fl = 0;
    D(bug("86Box:%s()\n", __func__);)
    return(fl);
}

