/*
    $Id$
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/graphics.h>
#include <proto/alib.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#ifdef GLOBAL
#undef GLOBAL
#endif
#define GLOBAL
#include "../86box.h"
#include "../plat.h"

#include "aros.h"
#include "aros_amigavideo.h"
#if (0)
#include "aros_sdl.h"
#endif

extern ULONG timer_sigbit;
extern struct MinList ThreadList;
extern struct BitMap *renderBitMap;
extern struct Window *displayWindow;
extern struct RastPort *BMRastPort;

thread_t *thMain = NULL; /* The main PC emulation thread */
struct Task *mainTask = NULL;
struct timerequest *secreq = NULL;

static int	vid_api_inited = 0;

#define RENDERERS_NUM		1

static const struct {
    const char	*name;
    int		local;
    int		(*init)(void *);
    void	(*close)(void);
    void	(*resize)(int x, int y);
    int		(*pause)(void);
    void	(*enable)(int enable);
} vid_apis[2][RENDERERS_NUM] = {
    /* windowed ..*/
  {
#if defined AROS_AMIGAVIDEO_H
    {	"AmigaVideo", 1, (int(*)(void*))amigavid_init, amigavid_close, amigavid_resize, amigavid_pause, amigavid_enable		}
#endif
#if defined AROS_SDL_H
    {	"SDL", 1, (int(*)(void*))sdl_init, sdl_close, sdl_resize, sdl_pause, sdl_enable		}
#endif
  },
    /* Fullscreen ..*/
  {
#if defined AROS_AMIGAVIDEO_H
    {	"AmigaVideo", 1, (int(*)(void*))amigavid_init, amigavid_close, NULL, amigavid_pause, amigavid_enable		}
#endif
#if defined AROS_SDL_H
    {	"SDL", 1, (int(*)(void*))sdl_init, sdl_close, NULL, sdl_pause, sdl_enable		}
#endif
  }
};

CONST_STRPTR versionstr = "$VER: " EMU_NAME " " EMU_VERSION " (22.12.2019)";

int main(int argc, char **argv)
{
    struct MsgPort *timerport;   /* Message port pointer */
    struct timerequest *timerreq;
    struct ThreadLocalData *td;
    int retval = 0;

    D(bug("86Box:%s()\n", __func__);)

    mainTask = FindTask(NULL);
    NEWLIST(&ThreadList);

    D(bug("86Box:%s - MainTask @ 0x%p\n", __func__, mainTask);)
    if ((timer_sigbit = AllocSignal(-1L)) == -1)
        return(retval);

    /* Create port for timer device communications */
    timerport = CreatePort(0,0);
    if (!timerport)
    {
        return(retval);
    }

    timerport->mp_Flags = PA_SIGNAL;
    timerport->mp_SigBit = timer_sigbit;
    timerport->mp_SigTask = mainTask;

    td = AllocMem(sizeof(struct ThreadLocalData), MEMF_CLEAR);
    mainTask->tc_UserData = td;
    td->timerreq = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest));
    if (!mainTask->tc_UserData)
    {
        DeletePort(timerport);
        return(retval);
    }

    if ((OpenDevice("timer.device", UNIT_MICROHZ, &td->timerreq->tr_node, 0)) != 0)
    {
        DeleteIORequest(&td->timerreq->tr_node);
        DeletePort(timerport);
        return(retval);
    }

    D_TICK(
        bug("86Box:%s - mainTask mn_ReplyPort @ 0x%p\n", __func__, td->timerreq->tr_node.io_Message.mn_ReplyPort);
        bug("86Box:%s - mainTask mn_Length = %d\n", __func__, td->timerreq->tr_node.io_Message.mn_Length);
        bug("86Box:%s - mainTask io_Device @ 0x%p\n", __func__, td->timerreq->tr_node.io_Device);
        bug("86Box:%s - mainTask io_Unit = %d\n", __func__, td->timerreq->tr_node.io_Unit);
    )

    /* Set the application version ID string. */
    sprintf(emu_version, "%s v%s", EMU_NAME, EMU_VERSION);

    /* if there is a config file pre-open it and check if it is in widechar format */
    {
        char buff[4];
        FILE *f;
        f = plat_fopen(cfg_path, _S("rt"));
        if (f)
        {
            fgets(buff, sizeof(buff), f);
            if ((buff[0] == 0xFF && buff[1] == 0xFE) ||
                (buff[0] == 0xFE && buff[1] == 0xFF))
            {
                printf("Widechar prefs file detected\n");
            }
            fclose(f);
        }
    }

    /* Pre-initialize the system, this loads the config file. */
    if (! pc_init(argc, argv)) {
	retval = 1;
    }

    /* Handle our GUI. */
    retval = ui_init(10);

    D(bug("86Box:%s - exiting\n", __func__);)

    CloseDevice(&td->timerreq->tr_node);
    DeleteIORequest(&td->timerreq->tr_node);

    timerport->mp_Flags = 0;
    timerport->mp_SigBit = 0;
    timerport->mp_SigTask = NULL;
    FreeSignal(timer_sigbit);

    DeletePort(timerport);

    return(retval);
} /* main */

/*
 * We do this here since there is platform-specific stuff
 * going on here, and we do it in a function separate from
 * main() so we can call it from the UI module as well.
 */
void
do_start(void)
{
    struct Task *thisTask = FindTask(NULL);
    struct ThreadLocalData *td = (struct ThreadLocalData *)thisTask->tc_UserData;
    struct timerequest *timerreq = td->timerreq;
    struct Device* TimerBase = timerreq->tr_node.io_Device;

    D(bug("86Box:%s()\n", __func__);)

    secreq = AllocVec(sizeof(struct timerequest), MEMF_ANY);
    CopyMem(timerreq, secreq, sizeof(struct timerequest));

    /* We have not stopped yet. */
    quited = 0;

    secreq->tr_node.io_Command = TR_ADDREQUEST;
    secreq->tr_time.tv_secs    = 1;
    secreq->tr_time.tv_micro   = 0;

    /* Start the emulator, really. */
    thMain = thread_create(pc_thread, &quited);

    D(bug("86Box:%s - starting 1sec timer\n", __func__);)
    // Send IORequest
    SendIO(&secreq->tr_node);
}

/* Cleanly stop the emulator. */
void
do_stop(void)
{
    D(bug("86Box:%s()\n", __func__);)

    quited = 1;

    pc_close(thMain);

    vid_apis[0][vid_api].close();

    thMain = NULL;
}

void	/* plat_ */
startblit(void)
{
    D(bug("86Box:%s()\n", __func__);)
}

void	/* plat_ */
endblit(void)
{
    D(bug("86Box:%s()\n", __func__);)
}

wchar_t *
plat_get_string(int i)
{
    wchar_t *retval = NULL;

    D(bug("86Box:%s(%d)\n", __func__, i);)

    switch (i)
    {
    case 2077:
        retval = "Click to capture mouse";
        break;
    }
    return((wchar_t *)retval);
}

void
plat_get_exe_name(wchar_t *s, int size)
{
    struct Process *thisTask;

    D(bug("86Box:%s()\n", __func__);)

    thisTask = (struct Process *)FindTask(NULL);
    NameFromLock(thisTask->pr_CurrentDir, s, size);
    AddPart(s, thisTask->pr_Task.tc_Node.ln_Name, size);
}

void
plat_tempfile(wchar_t *bufp, wchar_t *prefix, wchar_t *suffix)
{
    D(bug("86Box:%s('%s', '%s')\n", __func__, prefix, suffix);)
}

int
plat_getcwd(wchar_t *bufp, int max)
{
    D(bug("86Box:%s()\n", __func__);)

    return getcwd(bufp, max);
}


int
plat_chdir(wchar_t *path)
{
    D(bug("86Box:%s('%s')\n", __func__, path);)

    return chdir(path);
}


FILE *
plat_fopen(wchar_t *path, wchar_t *mode)
{
    FILE *file;

    D(bug("86Box:%s('%s','%s')\n", __func__, path, mode);)

    file = fopen(path, mode);
    D(bug("86Box:%s - opened @ 0x%p\n", __func__, file);)

    return file;
}


/* Open a file, using Unicode pathname, with 64bit pointers. */
FILE *
plat_fopen64(const wchar_t *path, const wchar_t *mode)
{
    FILE *file;

    D(bug("86Box:%s('%s','%s')\n", __func__, path, mode);)

    file = fopen(path, mode);
    D(bug("86Box:%s - opened @ 0x%p\n", __func__, file);)

    return file;
}


void
plat_remove(wchar_t *path)
{
    D(bug("86Box:%s('%s')\n", __func__, path);)
}


/* Make sure a path ends with a trailing (back)slash. */
void
plat_path_slash(wchar_t *path)
{
    int i = strlen(path);

    D(bug("86Box:%s('%s')\n", __func__, path);)
    if (path[i - 1] != ':' && path[i - 1] != '/')
        path[i] = '/';
}


/* Check if the given path is absolute or not. */
int
plat_path_abs(wchar_t *path)
{
    char *match;

    D(bug("86Box:%s('%s')\n", __func__, path);)

    match = strstr(path, ":");
    if (match && match != path)
        return(1);
    return(0);
}


/* Return the last element of a pathname. */
wchar_t *
plat_get_basename(const wchar_t *path)
{
    D(bug("86Box:%s('%s')\n", __func__, path);)

    return((wchar_t *)NULL);
}


/* Return the 'directory' element of a pathname. */
void
plat_get_dirname(wchar_t *dest, const wchar_t *path)
{
    D(bug("86Box:%s('%s')\n", __func__, path);)
    strncpy(dest, path, (IPTR)PathPart(path) - (IPTR)path);
}

wchar_t *
plat_get_filename(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    return FilePart(s);
}

wchar_t *
plat_get_extension(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)

    return(&s[0]);
}

void
plat_append_filename(wchar_t *dest, wchar_t *s1, wchar_t *s2)
{
    D(bug("86Box:%s('%s', '%s')\n", __func__, s1, s2);)
    strcpy(dest, s1);
    AddPart(dest, s2, 1024);
}

void
plat_put_backslash(wchar_t *s)
{
    D(bug("86Box:%s('%s')\n", __func__, s);)
}


int
plat_dir_check(wchar_t *path)
{
    int retval = 0;
    BPTR lock;

    D(bug("86Box:%s('%s')\n", __func__, path);)
    if ((lock = Lock(path, ACCESS_READ)) != BNULL)
    {
        retval = 1;
        UnLock(lock);
    }
    return(retval);
}


int
plat_dir_create(wchar_t *path)
{
    int retval = 0;
    BPTR lock;

    D(bug("86Box:%s('%s')\n", __func__, path);)

    lock = CreateDir(path);
    if (lock)
    {
        retval = 1;
        UnLock(lock);
    }

    return(retval);
}


uint64_t
plat_timer_read(void)
{
    struct Task *thisTask = FindTask(NULL);
    struct ThreadLocalData *td = (struct ThreadLocalData *)thisTask->tc_UserData;
    struct Device* TimerBase = td->timerreq->tr_node.io_Device;
    struct timeval curtime;

    D(bug("86Box:%s()\n", __func__);)

    GetSysTime(&curtime);

    D(bug("86Box:%s - %d:%d\n", __func__, curtime.tv_secs, curtime.tv_micro);)

    return(((uint64_t)curtime.tv_secs << 32) | curtime.tv_micro);
}


uint32_t
plat_get_ticks(void)
{
    struct Task *thisTask = FindTask(NULL);
    struct ThreadLocalData *td = (struct ThreadLocalData *)thisTask->tc_UserData;
    struct Device* TimerBase = td->timerreq->tr_node.io_Device;
    struct timeval eltime;

    D(bug("86Box:%s()\n", __func__);)

    GetUpTime(&eltime);

    D(bug("86Box:%s - %d\n", __func__, (eltime.tv_secs * 1000 + eltime.tv_micro / 1000));)

    return(eltime.tv_secs * 1000 + eltime.tv_micro / 1000);
}


void
plat_delay_ms(uint32_t count)
{
    struct Task *thisTask = FindTask(NULL);
    struct ThreadLocalData *td = (struct ThreadLocalData *)thisTask->tc_UserData;
    struct timerequest *timerreq = td->timerreq;
    struct Device* TimerBase = timerreq->tr_node.io_Device;

    D(bug("86Box:%s(%d)\n", __func__, count);)

    timerreq->tr_node.io_Command = TR_ADDREQUEST;
    timerreq->tr_time.tv_secs    = 0;
    timerreq->tr_time.tv_micro   = count;

#if (0)
    // Send IORequest
    DoIO(&timerreq->tr_node);
#endif
}

/* Tell the UI about a new screen resolution. */
void
plat_resize(int x, int y)
{
    struct BitMap *tempBM;

    D(bug("86Box:%s(%dx%d)\n", __func__, x, y);)
    if (renderBitMap)
    {
	if ((x > GetBitMapAttr(renderBitMap, BMA_WIDTH)) ||
            (y > GetBitMapAttr(renderBitMap, BMA_HEIGHT)))
        {
            struct BitMap *disposebm = renderBitMap;
            renderBitMap = NULL;
            D(bug("86Box:%s - destroying old bitmap...\n", __func__);)
            FreeBitMap(disposebm);
        }
    }

    if (!renderBitMap)
    {
        D(bug("86Box:%s - allocating new bitmap...\n", __func__);)
        renderBitMap = AllocBitMap(x, y, 24, BMF_CLEAR, (displayWindow) ? displayWindow->RPort->BitMap : NULL);
        D(bug("86Box:%s - BitMap @ 0x%p\n", __func__, renderBitMap);)
    }
    if (vid_apis[0][vid_api].resize)
        vid_apis[0][vid_api].resize(x, y);

    D(bug("86Box:%s - done\n", __func__);)
}

/* Return the VIDAPI name for the given number. */
char *
plat_vidapi_name(int api)
{
    char *name = "default";

    D(bug("86Box:%s(%d)\n", __func__, api);)

    switch(api) {
#if defined AROS_AMIGAVIDEO_H
	case 1:
		name = "amigavideo";
		break;
#endif
    }

    return(name);
}

int
plat_vidapi(char *name)
{
    int i;

    D(bug("86Box:%s('%s')\n", __func__, name);)

    if (!strcasecmp(name, "default") || !strcasecmp(name, "system") || !strcasecmp(name, "amigavideo")) return(0);

    /* Default value. */
    return(0);
}

int
plat_setvid(int api)
{
    int i;

    D(bug("86Box:%s(%d)\n", __func__, api);)

    startblit();
    video_wait_for_blit();

    vid_api = api;

    /* Initialize the (new) API. */
    i = vid_apis[0][vid_api].init((void *)NULL);

    endblit();

    if (! i) return(0);

    device_force_redraw();

    D(bug("86Box:%s - done\n", __func__);)

    vid_api_inited = 1;

    return(1);
}
