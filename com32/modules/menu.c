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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <consoles.h>
#include <getkey.h>
#include <minmax.h>
#ifdef __COM32__
#include <com32.h>
#endif

struct menu_entry {
  const char *displayname;
  const char *cmdline;
};

struct menu_entry menu_entries[] =
  {
    { "Test", "vmlinux root=/dev/zero" },
    { "this", "chain.c32 hd0 2" },
    { "entry 3", "runentry val=3" },
    { "entry 4", "runentry val=3" },
    { "entry 5", "runentry val=3" },
    { "entry 6", "runentry val=3" },
    { "entry 7", "runentry val=3" },
    { "entry 8", "runentry val=3" },
    { "entry 9", "runentry val=3" },
    { "entry 10", "runentry val=10" },
    { "entry 11", "runentry val=11" },
    { "entry 12", "runentry val=12" },
    { "entry 13", "runentry val=13" },
    { "entry 14", "runentry val=14" },
    { "entry 15", "runentry val=15" },
    { "entry 16", "runentry val=16" },
    { "entry 17", "runentry val=17" },
    { "entry 18", "runentry val=18" },
    { "entry 19", "runentry val=19" },
    { "entry 20", "runentry val=20" },
  };

int nentries  = sizeof menu_entries / sizeof(struct menu_entry);
int defentry  = 0;
int allowedit = 1;		/* Allow edits of the command line */

char *menu_title = "This is the menu title";

struct menu_attrib {
  const char *border;		/* Border area */
  const char *title;		/* Title bar */
  const char *unsel;		/* Unselected menu item */
  const char *sel;		/* Selected */
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

  if ( allowedit )
    printf("%s\033[%d;1H%s", menu_attrib->tabmsg, TABMSG_ROW,
	   pad_line("Press [Tab] to edit options", 1, WIDTH));

  printf("\033[%d;1H%s> %s%s\033[%d;1H",
	 CMDLINE_ROW, menu_attrib->cmdmark,
	 menu_attrib->cmdline,
	 pad_line(menu_entries[sel].cmdline, 0, 255),
	 END_ROW);
}

const char *run_menu(void)
{
  int key;
  int done = 0;
  int entry = defentry;
  int top = 0;

  /* Start with a clear screen */
  printf("%s\033[2J", menu_attrib->screen);

  while ( !done ) {
    if ( top < entry-MENU_ROWS+1 )
      top = max(entry-MENU_ROWS+1, 0);
    else if ( top > entry )
      top = min(entry, nentries-MENU_ROWS+1);

    draw_menu(entry, top);

  gkey:
    key = get_key(stdin);
    switch ( key ) {
    case '\r':
      done = 1;
      break;
    case KEY_UP:
      if ( entry > 0 )
	entry--;
      break;
    case KEY_DOWN:
      if ( entry < nentries-1 )
	entry++;
      break;
    case '\003':		/* Ctrl-C */
    case '\033':		/* Esc */
      exit(1);			/* FIX THIS... do something sane here */
    default:
      printf("[%04x]", key);
      goto gkey;
    }
  }

  printf("\033[%d;1H\033[0m", END_ROW);
  return menu_entries[defentry].cmdline;
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

int main(void)
{
  const char *cmdline;

  console_ansi_raw();

  fputs("\033%@\033(U", stdout); /* Enable CP 437 graphics on a real console */

  cmdline = run_menu();
  execute(cmdline);
}

  
