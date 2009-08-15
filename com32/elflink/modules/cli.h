#ifndef CLI_H
#define CLI_H

extern void clear_screen(void);
extern int mygetkey(clock_t timeout);
extern const char *edit_cmdline(const char *input, int top /*, int width */,int (*pDraw_Menu)(int, int, int),void (*show_fkey)(int));

#endif
