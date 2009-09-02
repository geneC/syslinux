#ifndef CLI_H
#define CLI_H

#define MAX_CMD_HISTORY 64

struct cli_command {
    struct list_head list;
    char *command;
};

struct list_head cli_history_head;

extern void clear_screen(void);
extern int mygetkey(clock_t timeout);
extern const char *edit_cmdline(const char *input, int top /*, int width */ ,
				int (*pDraw_Menu) (int, int, int),
				void (*show_fkey) (int));

#endif
