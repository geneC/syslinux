#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * menu.c
 *
 * Simple menu system which displays a list and allows the user to select
 * a command line and/or edit it.
 */

#define _GNU_SOURCE		/* Needed for asprintf() on Linux */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <consoles.h>
#include <getkey.h>
#include <minmax.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>
#ifdef __COM32__
#include <com32.h>
#endif

#include "menu.h"

#ifndef CLK_TCK
# define CLK_TCK sysconf(_SC_CLK_TCK)
#endif

struct menu_attrib {
  const char *border;		/* Border area */
  const char *title;		/* Title bar */
  const char *unsel;		/* Unselected menu item */
  const char *sel;		/* Selected */
  const char *more;		/* [More] tag */
  const char *tabmsg;		/* Press [Tab] message */
  const char *cmdmark;		/* Command line marker */
  const char *cmdline;		/* Command line */
  const char *screen;		/* Rest of the screen */
};

const struct menu_attrib default_attrib = {
  .border  = "\033[0;30;44m",
  .title   = "\033[1;36;44m",
  .unsel   = "\033[0;37;44m",
  .sel     = "\033[0;30;47m",
  .more    = "\033[0;37;44m",
  .tabmsg  = "\033[0;31;40m",
  .cmdmark = "\033[1;36;40m",
  .cmdline = "\033[0;37;40m",
  .screen  = "\033[0;37;40m",
};

const struct menu_attrib *menu_attrib = &default_attrib;

#define WIDTH		80
#define MARGIN		10
#define MENU_ROWS	12
#define TABMSG_ROW	18
#define CMDLINE_ROW	20
#define END_ROW		24

char *pad_line(const char *text, int align, int width)
{
  static char buffer[256];
  int n, p;

  if ( width >= (int) sizeof buffer )
    return NULL;		/* Can't do it */

  n = strlen(text);
  if ( n >= width )
    n = width;

  memset(buffer, ' ', width);
  buffer[width] = 0;
  p = ((width-n)*align)>>1;
  memcpy(buffer+p, text, n);

  return buffer;
}

void draw_menu(int sel, int top)
{
  int x, y;

  printf("\033[1;%dH%s\311", MARGIN+1, menu_attrib->border);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('\315');
  putchar('\273');

  printf("\033[2;%dH\272%s %s %s\272",
	 MARGIN+1,
	 menu_attrib->title,
	 pad_line(menu_title, 1, WIDTH-2*MARGIN-4),
	 menu_attrib->border);

  printf("\033[3;%dH\307", MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('\304');
  putchar('\266');

  if ( top != 0 )
    printf("%s\033[3;%dH[-]%s",
	   menu_attrib->more, WIDTH-MARGIN-5,
	   menu_attrib->border);

  for ( y = 4 ; y < 4+MENU_ROWS ; y++ ) {
    int i = (y-4)+top;
    const char *txt = (i >= nentries) ? "" : menu_entries[i].displayname;

    printf("\033[%d;%dH\272%s %s %s\272",
	   y, MARGIN+1,
	   (i == sel) ? menu_attrib->sel : menu_attrib->unsel,
	   pad_line(txt, 0, WIDTH-2*MARGIN-4),
	   menu_attrib->border);
  }

  printf("\033[%d;%dH\310", y, MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('\315');
  putchar('\274');

  if ( top < nentries-MENU_ROWS )
    printf("%s\033[%d;%dH[+]", menu_attrib->more, y, WIDTH-MARGIN-5);

  if ( allowedit )
    printf("%s\033[%d;1H%s", menu_attrib->tabmsg, TABMSG_ROW,
	   pad_line("Press [Tab] to edit options", 1, WIDTH));

  printf("%s\033[%d;1H", menu_attrib->screen, END_ROW);
}

const char *edit_cmdline(char *input, int top)
{
  static char cmdline[MAX_CMDLINE_LEN];
  int key, len;
  int redraw = 2;

  strncpy(cmdline, input, MAX_CMDLINE_LEN);
  cmdline[MAX_CMDLINE_LEN-1] = '\0';

  len = strlen(cmdline);

  for (;;) {
    if ( redraw > 1 ) {
      /* Clear and redraw whole screen */
      printf("%s\033[2J", menu_attrib->screen);
      draw_menu(-1, top);
    }

    if ( redraw > 0 ) {
      /* Redraw the command line */
      printf("\033[%d;1H%s> %s%s",
	     CMDLINE_ROW, menu_attrib->cmdmark,
	     menu_attrib->cmdline, pad_line(cmdline, 0, MAX_CMDLINE_LEN-1));
      printf("%s\033[%d;3H%s",
	     menu_attrib->cmdline, CMDLINE_ROW, cmdline);
      redraw = 0;
    }

    key = get_key(stdin, 0);

    /* FIX: should handle arrow keys and edit-in-middle */

    switch( key ) {
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
    case '\x7F':
      if ( len ) {
	cmdline[--len] = '\0';
	redraw = 1;
      }
      break;
    case KEY_CTRL('U'):
      if ( len ) {
	len = 0;
	cmdline[len] = '\0';
	redraw = 1;
      }
      break;
    case KEY_CTRL('W'):
      if ( len ) {
	int wasbs = (cmdline[len-1] <= ' ');
	while ( len && (cmdline[len-1] <= ' ' || !wasbs) ) {
	  len--;
	  wasbs = wasbs || (cmdline[len-1] <= ' ');
	}
	cmdline[len] = '\0';
	redraw = 1;
      }
      break;
    default:
      if ( key >= ' ' && key <= 0xFF && len < MAX_CMDLINE_LEN-1 ) {
	cmdline[len] = key;
	cmdline[++len] = '\0';
	putchar(key);
      }
      break;
    }
  }
}

const char *run_menu(void)
{
  int key;
  int done = 0;
  int entry = defentry;
  int top = 0;
  int clear = 1;
  const char *cmdline = NULL;
  clock_t key_timeout;

  /* Convert timeout from deciseconds to clock ticks */
  /* Note: for both key_timeout and timeout == 0 means no limit */
  key_timeout = (clock_t)(CLK_TCK*timeout+9)/10;

  printf("\033[?25l");		/* Hide cursor */

  while ( !done ) {
    if ( entry < 0 )
      entry = 0;
    else if ( entry >= nentries )
      entry = nentries-1;

    if ( top < 0 || top < entry-MENU_ROWS+1 )
      top = max(0, entry-MENU_ROWS+1);
    else if ( top > entry )
      top = entry;

    /* Start with a clear screen */
    if ( clear )
      printf("%s\033[2J", menu_attrib->screen);
    clear = 0;

    draw_menu(entry, top);

    key = get_key(stdin, key_timeout);
    switch ( key ) {
    case KEY_NONE:		/* Timeout */
      /* This is somewhat hacky, but this at least lets the user
	 know what's going on, and still deals with "phantom inputs"
	 e.g. on serial ports. */
      if ( entry != defentry )
	entry = defentry;
      else {
	cmdline = menu_entries[defentry].label;
	done = 1;
      }
      break;
    case KEY_CTRL('L'):
      clear = 1;
      break;
    case KEY_ENTER:
    case KEY_CTRL('J'):
      cmdline = menu_entries[entry].label;
      done = 1;
      break;
    case 'P':
    case 'p':
    case KEY_UP:
      entry--;
      break;
    case 'N':
    case 'n':
    case KEY_DOWN:
      entry++;
      break;
    case KEY_CTRL('P'):
    case KEY_PGUP:
      entry -= MENU_ROWS;
      top   -= MENU_ROWS;
      break;
    case KEY_CTRL('N'):
    case KEY_PGDN:
    case ' ':
      entry += MENU_ROWS;
      top   += MENU_ROWS;
      break;
    case '-':
      entry--;
      top--;
      break;
    case '+':
      entry++;
      top++;
      break;
    case KEY_TAB:
      if ( allowedit ) {
	printf("\033[?25h");		/* Show cursor */
	cmdline = edit_cmdline(menu_entries[entry].cmdline, top);
	printf("\033[?25l");		/* Hide cursor */
	done = !!cmdline;
	clear = 1;		/* In case we hit [Esc] and done is null */
      }
      break;
    case KEY_CTRL('C'):		/* Ctrl-C */
    case KEY_ESC:		/* Esc */
      if ( allowedit )
	done = 1;
      break;
    default:
      break;
    }
  }

  printf("\033[?25h");		/* Show cursor */

  /* Return the label name so localboot and ipappend work */
  return cmdline;
}


void __attribute__((noreturn)) execute(const char *cmdline)
{
#ifdef __COM32__
  static com32sys_t ireg;

  strcpy(__com32.cs_bounce, cmdline);
  ireg.eax.w[0] = 0x0003;	/* Run command */
  ireg.ebx.w[0] = OFFS(__com32.cs_bounce);
  ireg.es = SEG(__com32.cs_bounce);
  __intcall(0x22, &ireg, NULL);
  exit(255);  /* Shouldn't return */
#else
  /* For testing... */
  printf("\n>>> %s\n", cmdline);
  exit(0);
#endif
}

int main(int argc, char *argv[])
{
  const char *cmdline;

  (void)argc;

  console_ansi_raw();
  fputs("\033%@\033(U", stdout); /* Enable CP 437 graphics on a real console */

  parse_config(argv[1]);

  cmdline = run_menu();
  printf("\033[?25h\033[%d;1H\033[0m", END_ROW);
  if ( cmdline )
    execute(cmdline);
  else
    return 0;
}
