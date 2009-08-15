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
#include "getkey.h"

#include "common.h"
#include "menu.h"
#include "cli.h"

void clear_screen(void)
{
    fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
}

int mygetkey(clock_t timeout)
{
    clock_t t0, t;
    clock_t tto, to;
    int key;

    if (!totaltimeout)
	return get_key(stdin, timeout);

    for (;;) {
	tto = min(totaltimeout, INT_MAX);
	to = timeout ? min(tto, timeout) : tto;

	t0 = times(NULL);
	key = get_key(stdin, to);
	t = times(NULL) - t0;

	if (totaltimeout <= t)
	    longjmp(timeout_jump, 1);

	totaltimeout -= t;

	if (key != KEY_NONE)
	    return key;

	if (timeout) {
	    if (timeout <= t)
		return KEY_NONE;

	    timeout -= t;
	}
    }
}

const char *edit_cmdline(const char *input, int top /*, int width */,int (*pDraw_Menu)(int, int, int),void (*show_fkey)(int))
{
    static char cmdline[MAX_CMDLINE_LEN];
    int key, len, prev_len, cursor;
    int redraw = 1;		/* We enter with the menu already drawn */
    int x, y;
    bool done = false;
    const char *ret;

    int width = 0;

    if (!width) {
	int height;
	if (getscreensize(1, &height, &width))
	    width = 80;
    }

    printf("width = %d\n", width);

    strncpy(cmdline, input, MAX_CMDLINE_LEN);
    cmdline[MAX_CMDLINE_LEN - 1] = '\0';

    len = cursor = strlen(cmdline);
    prev_len = 0;
    x = y = 0;

    while (!done) {
	if (redraw > 1 && pDraw_Menu!=NULL) {
	    /* Clear and redraw whole screen */
	    /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	       to avoid confusing the Linux console */
	   /* clear_screen();
	    draw_menu(-1, top, 1);
	    prev_len = 0;*/
	    clear_screen();
	    (*pDraw_Menu)(-1, top, 1);
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

	    dy = y - (cursor+2) / width;
	    x = (cursor+2) % width;

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
	    if(show_fkey!=NULL)
	    {	
	        (*show_fkey)(key);
	    	redraw = 1;
	    }
	    break;

	default:
	    if (key >= ' ' && key <= 0xFF && len < MAX_CMDLINE_LEN - 1) {
		if (cursor == len) {
		    cmdline[len++] = key;
		    cmdline[len] = '\0';
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

