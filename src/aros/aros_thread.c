/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include <exec/types.h>
#include <dos/dos.h>

#include <pthread.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#include "../86box.h"
#include "../plat.h"

#include "aros.h"

extern struct Task *mainTask;
extern ULONG timer_sigbit;

struct MinList ThreadList;

typedef struct {
    struct MinNode node;
    pthread_t thread;
    void (*func)(void *);
    void *param;
    struct ThreadLocalData tld;
} aros_thread_t;

typedef struct {
    pthread_mutex_t mutex;
} aros_mutex_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signalled;
} aros_event_t;

void *thread_startup(void *arg)
{
    aros_thread_t *at = (aros_thread_t *)arg;
    struct Task *thisTask;
    struct MsgPort *timerport;   /* Message port pointer */

    D(bug("86Box:%s()\n", __func__);)

    thisTask = FindTask(NULL);

    D(bug("86Box:%s - Task @ 0x%p\n", __func__, thisTask);)

    /* create port for timer device communications */
    timerport = CreatePort(0,0);
    if (!timerport)
    {
        return NULL;
    }

    timerport->mp_Flags = PA_SIGNAL;
    timerport->mp_SigBit = timer_sigbit;
    timerport->mp_SigTask = mainTask;

    thisTask->tc_UserData = &at->tld;
    at->tld.timerreq = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest));
    if (!thisTask->tc_UserData)
    {
        DeletePort(timerport);
        return NULL;
    }

    if ((OpenDevice("timer.device", UNIT_MICROHZ, &at->tld.timerreq->tr_node, 0)) != 0)
    {
        DeleteIORequest(&at->tld.timerreq->tr_node);
        DeletePort(timerport);
        return NULL;
    }

    D(bug("86Box:%s - timerreq @ 0x%p\n", __func__, at->tld.timerreq);)
    D(bug("86Box:%s - thread mn_ReplyPort @ %p\n", __func__, at->tld.timerreq->tr_node.io_Message.mn_ReplyPort);)
    D(bug("86Box:%s - thread io_Device @ 0x%p\n", __func__, at->tld.timerreq->tr_node.io_Device);)
    D(bug("86Box:%s - thread io_Unit @ 0x%p\n", __func__, at->tld.timerreq->tr_node.io_Unit);)

    AddTail((struct List *)&ThreadList, (struct Node *)&at->node);
    /* call the thread .. */
    at->func(at->param);
    Remove((struct Node *)&at->node);

    D(bug("86Box:%s - Exiting Task @ 0x%p\n", __func__, thisTask);)

    CloseDevice(&at->tld.timerreq->tr_node);

    timerport->mp_Flags = 0;
    timerport->mp_SigBit = 0;
    timerport->mp_SigTask = NULL;
    
    DeletePort(timerport);

    return NULL;
}

thread_t *
thread_create(void (*func)(void *param), void *param)
{
    aros_thread_t *at;

    D(bug("86Box:%s()\n", __func__);)

    at = malloc(sizeof(aros_thread_t));
    at->func = func;
    at->param = param;

    if(!pthread_create(&at->thread, NULL, thread_startup, at)) {
        D(bug("86Box:%s: new thread created @ 0x%p\n", __func__, at);)
        return((thread_t *)at);
    }

    return((thread_t *)NULL);
}


void
thread_kill(void *arg)
{
    D(bug("86Box:%s(0x%p)\n", __func__, arg);)
    if (arg == NULL) return;
}


int
thread_wait(thread_t *arg, int timeout)
{
    D(bug("86Box:%s(0x%p, %d)\n", __func__, arg, timeout);)
    if (arg == NULL) return(0);

    return(1);
}


event_t *
thread_create_event(void)
{
    aros_event_t *ev;

    D(bug("86Box:%s()\n", __func__);)

    ev = malloc(sizeof(aros_event_t));

    pthread_mutex_init(&ev->mutex, NULL);
    pthread_cond_init(&ev->cond, NULL);
   
    return((event_t *)ev);
}


void
thread_set_event(event_t *arg)
{
    aros_event_t *ev = (aros_event_t *)arg;
    D(bug("86Box:%s(0x%p)\n", __func__, arg);)

    if (arg == NULL) return;

    pthread_mutex_lock(&ev->mutex);
    ev->signalled = true;
    pthread_mutex_unlock(&ev->mutex);
    pthread_cond_signal(&ev->cond);
}


void
thread_reset_event(event_t *arg)
{
    aros_event_t *ev = (aros_event_t *)arg;
    D(bug("86Box:%s(0x%p)\n", __func__, arg);)

    if (arg == NULL) return;

    pthread_mutex_lock(&ev->mutex);
    pthread_cond_init(&ev->cond, NULL);
    pthread_mutex_unlock(&ev->mutex);
}


int
thread_wait_event(event_t *arg, int timeout)
{
    aros_event_t *ev = (aros_event_t *)arg;
    D(bug("86Box:%s(0x%p, %d)\n", __func__, arg, timeout);)

    if (arg == NULL) return(0);

    pthread_mutex_lock(&ev->mutex);
    while (!ev->signalled)
    {
        pthread_cond_wait(&ev->cond, &ev->mutex);
    }
    ev->signalled = false;
    pthread_mutex_unlock(&ev->mutex);

    return(1);
}


void
thread_destroy_event(event_t *arg)
{
    aros_event_t *ev = (aros_event_t *)arg;
    D(bug("86Box:%s(0x%p)\n", __func__, arg);)

    if (arg == NULL) return;

    pthread_mutex_destroy(&ev->mutex);
    pthread_cond_destroy(&ev->cond);

    free(ev);
}


mutex_t *
thread_create_mutex(wchar_t *name)
{
    aros_mutex_t *mx;

    D(bug("86Box:%s('%s')\n", __func__, name);)

    mx = malloc(sizeof(aros_event_t));

    pthread_mutex_init(&mx->mutex, NULL);

    return((mutex_t*)mx);
}


void
thread_close_mutex(mutex_t *mutex)
{
    aros_mutex_t *mx = (aros_mutex_t *)mutex;

    D(bug("86Box:%s(0x%p)\n", __func__, mutex);)

    if (mutex == NULL) return;

    pthread_mutex_destroy(&mx->mutex);

    free(mx);
}


int
thread_wait_mutex(mutex_t *mutex)
{
    aros_mutex_t *mx = (aros_mutex_t *)mutex;

    D(bug("86Box:%s(0x%p)\n", __func__, mutex);)

    if (mutex == NULL) return(0);

    pthread_mutex_lock(&mx->mutex);

    return(1);
}


int
thread_release_mutex(mutex_t *mutex)
{
    aros_mutex_t *mx = (aros_mutex_t *)mutex;

    D(bug("86Box:%s(0x%p)\n", __func__, mutex);)

    if (mutex == NULL) return(0);

    pthread_mutex_unlock(&mx->mutex);

    return(1);
}
