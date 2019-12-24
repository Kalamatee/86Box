#ifndef AROS_AMIGAVIDEO_H
# define AROS_AMIGAVIDEO_H

extern void     amigavid_close(void);
extern int      amigavid_init(APTR h);
extern int      amigavid_pause(void);
extern void	amigavid_resize(int x, int y);
extern void	amigavid_enable(int enable);

#endif /* !AROS_AMIGAVIDEO_H */
