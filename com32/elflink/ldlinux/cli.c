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
#include <dprintf.h>

#include "getkey.h"
#include "menu.h"
#include "cli.h"
#include "config.h"

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

static const char * cmd_reverse_search(int *cursor)
{
    int key;
    int i = 0;
    char buf[MAX_CMDLINE_LEN];
    const char *p = NULL;
    struct cli_command *last_found;
    struct cli_command *last_good = NULL;

    last_found = list_entry(cli_history_head.next, typeof(*last_found), list);

    memset(buf, 0, MAX_CMDLINE_LEN);

    printf("\033[1G\033[1;36m(reverse-i-search)`': \033[0m");
    while (1) {
        key = mygetkey(0);

	if (key == KEY_CTRL('C')) {
	    return NULL;
	} else if (key == KEY_CTRL('R')) {
	    if (i == 0)
		continue; /* User typed nothing yet */
	    /* User typed 'CTRL-R' again, so try the next */
	    last_found = list_entry(last_found->list.next, typeof(*last_found), list);
	} else if (key >= ' ' && key <= 'z') {
	        buf[i++] = key;
	} else {
	    /* Treat other input chars as terminal */
	    break;
	}

	while (last_found != &cli_history_head) {
	    p = strstr(last_found->command, buf);
	    if (p)
	        break;
	    last_found = list_entry(last_found->list.next, typeof(*last_found), list);
	}

	if (!p && !last_good) {
	    return NULL;
	} else if (!p) {
	    continue;
	} else {
	    last_good = last_found;
            *cursor = p - last_good->command;
	}

	printf("\033[?7l\033[?25l");
	/* Didn't handle the line wrap case here */
	printf("\033[1G\033[1;36m(reverse-i-search)\033[0m`%s': %s", 
		buf, last_good->command ? : "");
	printf("\033[K\r");
    }

    return last_good ? last_good->command : NULL;
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

    if (!width) {
	int height;
	if (getscreensize(1, &height, &width))
	    width = 80;
    }

    cmdline[MAX_CMDLINE_LEN - 1] = '\0';

    len = cursor = 0;
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
	    printf("\033[1G\033[1;36m%s \033[0m", input);

	    x = strlen(input);
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

	    dy = y - (cursor + strlen(input) + 1) / width;
	    x = (cursor + strlen(input) + 1) % width;

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
	case KEY_CTRL('R'):
	    {
	         /* 
	          * Handle this case in another function, since it's 
	          * a kind of special.
	          */
	        const char *p = cmd_reverse_search(&cursor);
	        if (p) {
	            strcpy(cmdline, p);
		    len = strlen(cmdline);
	        } else {
	            cmdline[0] = '\0';
		    len = 0;
	        }
	        redraw = 1;
	    }
	    break;
	case KEY_TAB:
	    {
		const char *p;
		size_t len;

		/* Label completion enabled? */
		if (nocomplete)
	            break;

		p = cmdline;
		len = 0;
		while(*p && !my_isspace(*p)) {
		    p++;
		    len++;
		}

		print_labels(cmdline, len);
		redraw = 1;
		break;
	    }

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

    /* Add the command to the history */
    comm_counter = malloc(sizeof(struct cli_command));
    comm_counter->command = malloc(sizeof(char) * (strlen(ret) + 1));
    strcpy(comm_counter->command, ret);
    list_add(&(comm_counter->list), &cli_history_head);

    return ret;
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
