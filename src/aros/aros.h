#ifndef AROS_H
# define AROS_H

struct ThreadLocalData
{
    struct timerequest *timerreq;
};

#if defined(DEBUG)
#define D_TICK(x)       x
#define D_VBLANK(x)     x
#define D_EVENT(x)      x
#else
#define D_TICK(x)
#define D_VBLANK(x)
#define D_EVENT(x)
#endif

#endif
