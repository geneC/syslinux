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

const char *edit_cmdline(const char *input, int top)
{
    static char cmdline[MAX_CMDLINE_LEN];
    int key, len, prev_len, cursor;
    int redraw = 1;		/* We enter with the menu already drawn */

    strncpy(cmdline, input, MAX_CMDLINE_LEN);
    cmdline[MAX_CMDLINE_LEN - 1] = '\0';

    len = cursor = strlen(cmdline);
    prev_len = 0;

    for (;;) {
	if (redraw > 1) {
	    /* Clear and redraw whole screen */
	    /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	       to avoid confusing the Linux console */
	   /* clear_screen();
	    draw_menu(-1, top, 1);
	    prev_len = 0;*/
	}

	if (redraw > 0) {
	    /* Redraw the command line */
	  /*  printf("\033[?25l\033[%d;1H\1#9> \2#10%s",
		   CMDLINE_ROW, pad_line(cmdline, 0, max(len, prev_len)));
	    printf("\2#10\033[%d;3H%s\033[?25h",
		   CMDLINE_ROW, pad_line(cmdline, 0, cursor));
	    prev_len = len;
	    redraw = 0;*/
	}

	key = mygetkey(0);

	switch (key) {
	case KEY_CTRL('L'):
	    redraw = 2;
	    break;

	case KEY_ENTER:
	case KEY_CTRL('J'):
	    return cmdline;

	case KEY_ESC:
	case KEY_CTRL('C'):
	    return NULL;

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

		memmove(cmdline + cursor, cmdline + prevcursor,
			len - prevcursor + 1);
		len -= (cursor - prevcursor);
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
		putchar(cmdline[cursor++]);
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
	    //show_fkey(key);
	    redraw = 1;
	    break;

	default:
	    if (key >= ' ' && key <= 0xFF && len < MAX_CMDLINE_LEN - 1) {
		if (cursor == len) {
		    cmdline[len] = key;
		    cmdline[++len] = '\0';
		    cursor++;
		    putchar(key);
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
}

