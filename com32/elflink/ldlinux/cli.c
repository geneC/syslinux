#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <setjmp.h>
#include <netinet/in.h>
#include <limits.h>
#include <minmax.h>
#include <linux/list.h>
#include <sys/exec.h>
#include <sys/module.h>
#include <core-elf.h>
#include <dprintf.h>

#include "getkey.h"
#include "menu.h"
#include "cli.h"

static jmp_buf timeout_jump;

static struct list_head cli_history_head;

void clear_screen(void)
{
    //dprintf("enter");
    fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
}

int mygetkey(clock_t timeout)
{
    clock_t t0, t;
    clock_t tto, to;
    int key;

    //dprintf("enter");
    if (!totaltimeout)
	return get_key(stdin, timeout);

    for (;;) {
	tto = min(totaltimeout, INT_MAX);
	to = timeout ? min(tto, timeout) : tto;

	t0 = 0;
	key = get_key(stdin, to);
	t = 0 - t0;

	if (totaltimeout <= t)
	    longjmp(timeout_jump, 1);

	totaltimeout -= t;

	if (key != KEY_NONE) {
		//dprintf("get key 0x%x", key);
	    return key;
	}

	if (timeout) {
	    if (timeout <= t) {
		//dprintf("timeout");
		return KEY_NONE;
		}

	    timeout -= t;
	}
    }
}

const char *edit_cmdline(const char *input, int top /*, int width */ ,
			 int (*pDraw_Menu) (int, int, int),
			 void (*show_fkey) (int))
{
    static char cmdline[MAX_CMDLINE_LEN];
    char temp_cmdline[MAX_CMDLINE_LEN] = { };
    int key, len, prev_len, cursor;
    int redraw = 1;		/* We enter with the menu already drawn */
    int x, y;
    bool done = false;
    const char *ret;
    int width = 0;
    struct cli_command *comm_counter;
    comm_counter =
	list_entry(cli_history_head.next->prev, typeof(*comm_counter), list);

    if (!width) {
	int height;
	if (getscreensize(1, &height, &width))
	    width = 80;
    }

    strncpy(cmdline, input, MAX_CMDLINE_LEN);
    cmdline[MAX_CMDLINE_LEN - 1] = '\0';

    len = cursor = strlen(cmdline);
    prev_len = 0;
    x = y = 0;

    while (!done) {
	if (redraw > 1) {
	    /* Clear and redraw whole screen */
	    /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	       to avoid confusing the Linux console */
	    clear_screen();
	    if (pDraw_Menu)
		    (*pDraw_Menu) (-1, top, 1);
	    prev_len = 0;
	    // printf("\033[0m\033[2J\033[H");
	}

	if (redraw > 0) {
	    int dy, at;

	    prev_len = max(len, prev_len);

	    /* Redraw the command line */
	    printf("\033[?7l\033[?25l");
	    if (y)
		printf("\033[%dA", y);
	    printf("\033[1G\033[1;36m> \033[0m");

	    x = 2;
	    y = 0;
	    at = 0;
	    while (at < prev_len) {
		putchar(at >= len ? ' ' : cmdline[at]);
		at++;
		x++;
		if (x >= width) {
		    printf("\r\n");
		    x = 0;
		    y++;
		}
	    }
	    printf("\033[K\r");

	    dy = y - (cursor + 2) / width;
	    x = (cursor + 2) % width;

	    if (dy) {
		printf("\033[%dA", dy);
		y -= dy;
	    }
	    if (x)
		printf("\033[%dC", x);
	    printf("\033[?25h");
	    prev_len = len;
	    redraw = 0;
	}

	key = mygetkey(0);

	switch (key) {
	case KEY_CTRL('L'):
	    redraw = 2;
	    break;

	case KEY_ENTER:
	case KEY_CTRL('J'):
	    ret = cmdline;
	    done = true;
	    break;

	case KEY_ESC:
	case KEY_CTRL('C'):
	    ret = NULL;
	    done = true;
	    break;

	case KEY_BACKSPACE:
	case KEY_DEL:
	    if (cursor) {
		memmove(cmdline + cursor - 1, cmdline + cursor,
			len - cursor + 1);
		len--;
		cursor--;
		redraw = 1;
	    }
	    break;

	case KEY_CTRL('D'):
	case KEY_DELETE:
	    if (cursor < len) {
		memmove(cmdline + cursor, cmdline + cursor + 1, len - cursor);
		len--;
		redraw = 1;
	    }
	    break;

	case KEY_CTRL('U'):
	    if (len) {
		len = cursor = 0;
		cmdline[len] = '\0';
		redraw = 1;
	    }
	    break;

	case KEY_CTRL('W'):
	    if (cursor) {
		int prevcursor = cursor;

		while (cursor && my_isspace(cmdline[cursor - 1]))
		    cursor--;

		while (cursor && !my_isspace(cmdline[cursor - 1]))
		    cursor--;

#if 0
		memmove(cmdline + cursor, cmdline + prevcursor,
			len - prevcursor + 1);
#else
		{
		    int i;
		    char *q = cmdline + cursor;
		    char *p = cmdline + prevcursor;
		    for (i = 0; i < len - prevcursor + 1; i++)
			*q++ = *p++;
		}
#endif
		len -= (prevcursor - cursor);
		redraw = 1;
	    }
	    break;

	case KEY_LEFT:
	case KEY_CTRL('B'):
	    if (cursor) {
		cursor--;
		redraw = 1;
	    }
	    break;

	case KEY_RIGHT:
	case KEY_CTRL('F'):
	    if (cursor < len) {
		putchar(cmdline[cursor]);
		cursor++;
		x++;
		if (x >= width) {
		    printf("\r\n");
		    y++;
		    x = 0;
		}
	    }
	    break;

	case KEY_CTRL('K'):
	    if (cursor < len) {
		cmdline[len = cursor] = '\0';
		redraw = 1;
	    }
	    break;

	case KEY_HOME:
	case KEY_CTRL('A'):
	    if (cursor) {
		cursor = 0;
		redraw = 1;
	    }
	    break;

	case KEY_END:
	case KEY_CTRL('E'):
	    if (cursor != len) {
		cursor = len;
		redraw = 1;
	    }
	    break;

	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
	    if (show_fkey != NULL) {
		(*show_fkey) (key);
		redraw = 1;
	    }
	    break;
	case KEY_CTRL('P'):
	case KEY_UP:
	    {
		if (!list_empty(&cli_history_head)) {
		    comm_counter =
			list_entry(comm_counter->list.next,
				   typeof(*comm_counter), list);
		    if (&comm_counter->list == &cli_history_head) {
			strcpy(cmdline, temp_cmdline);
		    } else {
			strcpy(cmdline, comm_counter->command);
		    }
		    cursor = len = strlen(cmdline);
		    redraw = 1;
		}
	    }
	    break;
	case KEY_CTRL('N'):
	case KEY_DOWN:
	    {
		if (!list_empty(&cli_history_head)) {
		    comm_counter =
			list_entry(comm_counter->list.prev,
				   typeof(*comm_counter), list);
		    if (&comm_counter->list == &cli_history_head) {
			strcpy(cmdline, temp_cmdline);
		    } else {
			strcpy(cmdline, comm_counter->command);
		    }
		    cursor = len = strlen(cmdline);
		    redraw = 1;
		}
	    }
	    break;
	default:
	    if (key >= ' ' && key <= 0xFF && len < MAX_CMDLINE_LEN - 1) {
		if (cursor == len) {
		    temp_cmdline[len] = key;
		    cmdline[len++] = key;
		    temp_cmdline[len] = cmdline[len] = '\0';
		    putchar(key);
		    cursor++;
		    x++;
		    if (x >= width) {
			printf("\r\n\033[K");
			y++;
			x = 0;
		    }
		    prev_len++;
		} else {
		    memmove(cmdline + cursor + 1, cmdline + cursor,
			    len - cursor + 1);
		    memmove(temp_cmdline + cursor + 1, temp_cmdline + cursor,
			    len - cursor + 1);
		    temp_cmdline[cursor] = key;
		    cmdline[cursor++] = key;
		    len++;
		    redraw = 1;
		}
	    }
	    break;
	}
    }

    printf("\033[?7h");
    return ret;
}

void process_command(const char *cmd, bool history)
{
	char **argv = malloc((MAX_COMMAND_ARGS + 1) * sizeof(char *));
	char *temp_cmd = (char *)malloc(sizeof(char) * (strlen(cmd) + 1));
	int argc = 1, len_mn;
	char *crt_arg, *module_name;

	/* return if user only press enter */
	if (cmd[0] == '\0') {
		printf("\n");
		return;
	}
	printf("\n");

	if (history) {
		struct cli_command  *comm;

		comm = malloc(sizeof(struct cli_command));
		comm->command = malloc(sizeof(char) * (strlen(cmd) + 1));
		strcpy(comm->command, cmd);
		list_add(&(comm->list), &cli_history_head);
	}

	//	dprintf("raw cmd = %s", cmd);
	strcpy(temp_cmd, cmd);
	module_name = strtok(cmd, COMMAND_DELIM);
	len_mn = strlen(module_name);

	if (!strcmp(module_name + len_mn - 4, ".c32")) {
		if (module_find(module_name) != NULL) {
			/* make module re-enterable */
		  //		dprintf("Module %s is already running");
		}
		do {
			argv[0] = module_name;
			crt_arg = strtok(NULL, COMMAND_DELIM);
			if (crt_arg != NULL && strlen(crt_arg) > 0) {
				argv[argc] = crt_arg;
				argc++;
			} else
				break;
		} while (argc < MAX_COMMAND_ARGS);
		argv[argc] = NULL;
		module_load_dependencies(module_name, MODULES_DEP);
		spawn_load(module_name, argv);
	} else if (!strcmp(module_name + len_mn - 2, ".0")) {
		execute(cmd, KT_PXE);
	} else if (!strcmp(module_name + len_mn - 3, ".bs")) {
	} else if (!strcmp(module_name + len_mn - 4, ".img")) {
		execute(cmd, KT_FDIMAGE);
	} else if (!strcmp(module_name + len_mn - 4, ".bin")) {
	} else if (!strcmp(module_name + len_mn - 4, ".bss")) {
		execute(cmd, KT_BSS);
	} else if (!strcmp(module_name + len_mn - 4, ".com")
	       || !strcmp(module_name + len_mn - 4, ".cbt")) {
		execute(cmd, KT_COMBOOT);
	} else if (!strcmp(module_name + len_mn - 4, ".cfg")
	       || !strcmp(module_name + len_mn - 5, ".conf")
	       || !strcmp(module_name + len_mn - 7, ".config")) {
		execute(module_name, KT_CONFIG);
	}
	/* use KT_KERNEL as default */
	else
		execute(temp_cmd, KT_KERNEL);

cleanup:
	free(argv);
	free(temp_cmd);
}

static int cli_init(void)
{
	INIT_LIST_HEAD(&cli_history_head);

	return 0;
}

static void cli_exit(void)
{
	/* Nothing to do */
}

MODULE_INIT(cli_init);
MODULE_EXIT(cli_exit);
