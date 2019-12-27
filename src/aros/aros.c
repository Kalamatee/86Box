/*
    $Id$
*/

#include <aros/debug.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <libraries/asl.h>

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

extern struct MinList ThreadList;
extern struct BitMap *renderBitMap;
extern struct Window *displayWindow;
extern struct RastPort *BMRastPort;

Object *app = NULL;
Object *aroswin_about = NULL;
thread_t *thMain = NULL; /* The main PC emulation thread */
struct Task *mainTask = NULL;

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

CONST_STRPTR versionstr = "$VER: " EMU_NAME " " EMU_VERSION " (25.12.2019) © Sarah Walker, Miran Grca, Fred N. van Kempen & Others.";

AROS_UFH3
(
    void, aros_toggleabout,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(void *, dummy, A2),
    AROS_UFHA(void **, funcptr, A1)
)
{
    AROS_USERFUNC_INIT

    IPTR state = 0;
    GET(aroswin_about, MUIA_Window_Open, &state);
    state ^= 0x1;
    SET(aroswin_about, MUIA_Window_Open, state);

    AROS_USERFUNC_EXIT
}

int main(int argc, char **argv)
{
    Object *aboutwin_but_ok;
    Object *arosmenu_proj_hardreset;
    Object *arosmenu_proj_CAD;
    Object *arosmenu_proj_CAE;
    Object *arosmenu_proj_about;
    Object *arosmenu_proj_quit;
    Object *arosmenu_view_resize;
    Object *arosmenu_viev_remember;
    Object *arosmenu_view_render;
    Object *arosmenu_view_aspect;
    Object *arosmenu_view_scale;
    Object *arosmenu_view_xgasettings;
    Object *arosmenu_view_oversc;
    Object *arosmenu_view_contrast;
    Object *arosmenu_tools_settings;
    Object *arosmenu_tools_update;
#ifdef USE_DISCORD
    Object *arosmenu_tools_discord;
#endif
    Object *arosmenu_tools_screenshot;
#if defined(ENABLE_LOG_TOGGLES) || defined(ENABLE_LOG_COMMANDS)
# ifdef ENABLE_BUSLOGIC_LOG
    Object *arosmenu_log_buslogic;
# endif
# ifdef ENABLE_CDROM_LOG
    Object *arosmenu_log_cdrom;
# endif
# ifdef ENABLE_D86F_LOG
    Object *arosmenu_log_floppy;
# endif
# ifdef ENABLE_FDC_LOG
    Object *arosmenu_log_floppycon;
# endif
# ifdef ENABLE_IDE_LOG
    Object *arosmenu_log_ide;
# endif
# ifdef ENABLE_SERIAL_LOG
    Object *arosmenu_log_serp;
# endif
# ifdef ENABLE_NIC_LOG
    Object *arosmenu_log_net;
# endif
# ifdef ENABLE_LOG_COMMANDS
#  ifdef ENABLE_LOG_BREAKPOINT
    Object *arosmenu_log_bp;
#  endif
#  ifdef ENABLE_VRAM_DUMP
    Object *arosmenu_log_vram;
#  endif
# endif
#endif

    struct Process *thisProc;
    struct MsgPort *timerport;   /* Message port pointer */
    struct timerequest *timerreq;
    struct ThreadLocalData *td;
    struct Hook hook_AboutToggle;
    CONST_STRPTR taskname;
    int retval = 0;
    BPTR lock;

    D(bug("86Box:%s()\n", __func__);)

    mainTask = FindTask(NULL);
    thisProc = (struct Process *)mainTask;
    taskname = mainTask->tc_Node.ln_Name;

    D(bug("86Box:%s - MainTask @ 0x%p\n", __func__, mainTask);)

    /*
     * handle workbench startup if applicable... 
     */
    if (!thisProc->pr_CLI)
    {
        argv = (char **)&taskname;
        argc = 1;
    }

    NEWLIST(&ThreadList);

    /* Set the application version ID string. */
    sprintf(emu_version, "%s v%s", EMU_NAME, EMU_VERSION);
    D(bug("86Box:%s - '%s'\n", __func__, emu_version);)

    /* Create port for timer device communications */
    timerport = CreatePort(0,0);
    if (!timerport)
    {
        return(retval);
    }

    td = AllocMem(sizeof(struct ThreadLocalData), MEMF_CLEAR);
    mainTask->tc_UserData = td;
    if (!mainTask->tc_UserData)
    {
        DeletePort(timerport);
        return(retval);
    }

    td->timerreq = (struct timerequest *)CreateIORequest(timerport, sizeof(struct timerequest));
    if (!td->timerreq)
    {
        FreeMem(td, sizeof(struct ThreadLocalData));
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

#if (0)
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
#endif
    if ((lock = Lock("PROGDIR:86Box.png", ACCESS_READ)) != BNULL)
    {
        UnLock(lock);
    }

    app = ApplicationObject,
            MUIA_Application_Title, (IPTR) "" EMU_NAME,
            MUIA_Application_Version, (IPTR) versionstr,
            MUIA_Application_Copyright, (IPTR) "© 2019 Sarah Walker, Miran Grca, Fred N. van Kempen & Others",
            MUIA_Application_Description, (IPTR) "" EMU_NAME " " EMU_VERSION " - An emulator of old computers",
            MUIA_Application_SingleTask, TRUE,
            MUIA_Application_Base, EMU_NAME,
            MUIA_Application_Menustrip, MenustripObject,
                MUIA_Family_Child, MenuObject,
                    MUIA_Menu_Title, (IPTR)"Project",
                    MUIA_Family_Child, (IPTR)(arosmenu_proj_hardreset = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Hard Reset",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_proj_CAD = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Ctrl+Alt+Del",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_proj_CAE = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Ctrl+Alt+Esc",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_proj_about = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"About",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_proj_quit = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Quit",
                    End),
                End,
                MUIA_Family_Child, MenuObject,
                    MUIA_Menu_Title, (IPTR)"View",
                    MUIA_Family_Child, (IPTR)(arosmenu_view_resize = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Resizeable window",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_viev_remember = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Remember size/position",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_view_render = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Renderer",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_view_aspect = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Force 4:3 display ratio",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_view_scale = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Window scale factor",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_view_xgasettings = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"EGA/(S)VGA settings",
                    End),
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_view_oversc = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"CGA/PCjr/Tandy/EGA/(S)VGA overscan",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_view_contrast = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Change contrast for monochrome display",
                    End),
                End,
                MUIA_Family_Child, MenuObject,
                    MUIA_Menu_Title, (IPTR)"Tools",
                    MUIA_Family_Child, (IPTR)(arosmenu_tools_settings = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Settings...",
                    End),
                    MUIA_Family_Child, (IPTR)(arosmenu_tools_update = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Update status bar icons",
                    End),
#ifdef USE_DISCORD
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_tools_discord = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable Discord integration",
                    End),
#endif
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
                    MUIA_Family_Child, (IPTR)(arosmenu_tools_screenshot = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Take screenshot",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f11",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
                End,
#if defined(ENABLE_LOG_TOGGLES) || defined(ENABLE_LOG_COMMANDS)
                MUIA_Family_Child, MenuObject,
                    MUIA_Menu_Title, (IPTR)"Logging",
# ifdef ENABLE_BUSLOGIC_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_buslogic = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable BusLogic logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f4",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_CDROM_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_cdrom = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable CD-ROM logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f5",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_D86F_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_floppy = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable floppy (86F) logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f6",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_FDC_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_floppycon = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable floppy controller logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f7",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_IDE_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_ide = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable IDE logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f8",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_SERIAL_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_serp = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable Serial Port logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f3",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_NIC_LOG
                    MUIA_Family_Child, (IPTR)(arosmenu_log_net = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Enable Network logs",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f9",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
# endif
# ifdef ENABLE_LOG_COMMANDS
#  ifdef ENABLE_LOG_TOGGLES
                    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
#  endif
#  ifdef ENABLE_LOG_BREAKPOINT
                    MUIA_Family_Child, (IPTR)(arosmenu_log_bp = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Log breakpoint",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f10",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
#  endif
#  ifdef ENABLE_VRAM_DUMP
                    MUIA_Family_Child, (IPTR)(arosmenu_log_vram = MenuitemObject,
                        MUIA_Menuitem_Title, (IPTR)"Dump video RAM",
                        MUIA_Menuitem_Shortcut , (IPTR)"ctrl f1",
                        MUIA_Menuitem_CommandString, TRUE,
                    End),
#  endif
# endif
                End,
#endif
            End,
            SubWindow, (IPTR) (aroswin_about = (Object *)WindowObject,
                MUIA_Window_Title,              (IPTR) "About 86Box",
                MUIA_Window_Width,              MUIV_Window_Width_MinMax(0),
                MUIA_Window_CloseGadget,        FALSE,
                MUIA_Window_NoMenus,            TRUE,
                MUIA_Window_Activate,           TRUE,
                MUIA_Window_ID,                 MAKE_ID('A','B','W','N'),
                WindowContents, (IPTR) VGroup,
                    Child, HGroup,
                        Child, VGroup,
                            Child, ImageObject,
                                MUIA_Image_Spec, (IPTR)    "3:PROGDIR:86Box.png",
                            End,
                            Child, (IPTR) HVSpace,
                        End,
                        Child, VGroup,
                            Child, (IPTR) TextObject,
                                MUIA_Font,                 MUIV_Font_Big,
                                MUIA_Text_PreParse, (IPTR) "\033b",
                                MUIA_Text_Contents, (IPTR) "" EMU_NAME " " EMU_VERSION " - An emulator of old computers",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) HVSpace,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "Authors: Sarah Walker, Miran Grca, Fred N. van",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "Kempen (waltje), SA1988, MoochMcGee,",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "reenigne, leilei, JohnElliott, greatpsycho, and",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "others.",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) HVSpace,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "Released under the GNU General Public License",
                                MUIA_Weight,               0,
                            End,
                            Child, (IPTR) TextObject,
                                MUIA_Text_Contents, (IPTR) "2. See LICENSE for more information.",
                                MUIA_Weight,               0,
                            End,
                        End,
                    End,
                    Child, VGroup,
                        Child, (IPTR) (RectangleObject, MUIA_Rectangle_HBar, TRUE, End),
                        Child, HGroup,
                            Child, (IPTR) HVSpace,
                            Child, (IPTR) (aboutwin_but_ok = SimpleButton("OK")),
                        End,
                    End,
                End,
            End),
        End;

    if (!app)
    {
        D(bug("86Box:%s - failed to create app object\n", __func__);)
        return 1;
    }
    D(bug("86Box:%s - app object @ 0x%p\n", __func__, app);)

    hook_AboutToggle.h_Entry = (HOOKFUNC) aros_toggleabout;

    DoMethod(arosmenu_proj_quit, MUIM_Notify,
      MUIA_Menuitem_Trigger, MUIV_EveryTime,
      app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    DoMethod(arosmenu_proj_about, MUIM_Notify,
      MUIA_Menuitem_Trigger, MUIV_EveryTime,
      app, 4, MUIM_CallHook, (IPTR) &hook_AboutToggle,
            (IPTR) NULL, (IPTR) NULL);

    DoMethod(aboutwin_but_ok, MUIM_Notify,
      MUIA_Pressed, FALSE,
      aroswin_about, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    /* Pre-initialize the system, this loads the config file. */
    if (! pc_init(argc, argv)) {
	retval = 1;
    }

    /* Handle our GUI. */
    retval = ui_init(10);

    D(bug("86Box:%s - exiting\n", __func__);)

    DisposeObject(app);

    CloseDevice(&td->timerreq->tr_node);
    DeleteIORequest(&td->timerreq->tr_node);

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

    /* We have not stopped yet. */
    quited = 0;

    /* Start the emulator, really. */
    thMain = thread_create(pc_thread, &quited);
    D(bug("86Box:%s - Main PC emulation thread @ 0x%p\n", __func__, thMain);)
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
        if (!BMRastPort)
            BMRastPort = CreateRastPort();
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
