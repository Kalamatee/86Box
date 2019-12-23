#ifndef AROS_SDL_H
# define AROS_SDL_H

extern void     sdl_close(void);
extern int      sdl_init(APTR h);
extern void     sdl_title(wchar_t *s);
extern int      sdl_pause(void);
extern void	sdl_resize(int x, int y);
extern void	sdl_enable(int enable);

#endif	/*AROS_SDL_H*/
