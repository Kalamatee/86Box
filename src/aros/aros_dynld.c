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
#include "../plat_dynld.h"

void *
dynld_module(const char *name, dllimp_t *table)
{
    D(bug("86Box:%s('%s')\n", __func__, name);)
    return((void *)NULL);
}

void
dynld_close(void *handle)
{
    D(bug("86Box:%s()\n", __func__);)
}
