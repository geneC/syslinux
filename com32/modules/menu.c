#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004-2005 H. Peter Anvin - All Rights Reserved
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
#include <sha1.h>
#include <base64.h>
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
  const char *hotkey;		/* Unselected hotkey */
  const char *sel;		/* Selected */
  const char *hotsel;		/* Selected hotkey */
  const char *scrollbar;	/* Scroll bar */
  const char *tabmsg;		/* Press [Tab] message */
  const char *cmdmark;		/* Command line marker */
  const char *cmdline;		/* Command line */
  const char *screen;		/* Rest of the screen */
  const char *pwdborder;	/* Password box border */
  const char *pwdheader;	/* Password box header */
  const char *pwdentry;		/* Password box contents */
};

static const struct menu_attrib default_attrib = {
  .border  	= "\033[0;30;44m",
  .title   	= "\033[1;36;44m",
  .unsel        = "\033[0;37;44m",
  .hotkey       = "\033[1;37;44m",
  .sel          = "\033[0;7;37;40m",
  .hotsel       = "\033[1;7;37;40m",
  .scrollbar    = "\033[0;30;44m",
  .tabmsg  	= "\033[0;31;40m",
  .cmdmark 	= "\033[1;36;40m",
  .cmdline 	= "\033[0;37;40m",
  .screen  	= "\033[0;37;40m",
  .pwdborder	= "\033[0;30;47m",
  .pwdheader    = "\033[0;31;47m",
  .pwdentry     = "\033[0;30;47m",
};

static const struct menu_attrib *menu_attrib = &default_attrib;

#define WIDTH		80
#define MARGIN		10
#define PASSWD_MARGIN	3
#define MENU_ROWS	12
#define TABMSG_ROW	18
#define CMDLINE_ROW	20
#define END_ROW		24
#define PASSWD_ROW	11

static char *
pad_line(const char *text, int align, int width)
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

/* Display an entry, with possible hotkey highlight.  Assumes
   that the current attribute is the non-hotkey one, and will
   guarantee that as an exit condition as well. */
static void
display_entry(const struct menu_entry *entry, const char *attrib,
	      const char *hotattrib, int width)
{
  const char *p = entry->displayname;

  while ( width ) {
    if ( *p ) {
      if ( *p == '^' ) {
	p++;
	if ( *p && (unsigned char)*p & ~0x20 == entry->hotkey ) {
	  fputs(hotattrib, stdout);
	  putchar(*p++);
	  fputs(attrib, stdout);
	  width--;
	}
      } else {
	putchar(*p++);
	width--;
      }
    } else {
      putchar(' ');
      width--;
    }
  }
}

static void
draw_row(int y, int sel, int top, int sbtop, int sbbot)
{
  int i = (y-4)+top;
  
  printf("\033[%d;%dH%s\016x\017%s ",
	 y, MARGIN+1, menu_attrib->border,
	 (i == sel) ? menu_attrib->sel : menu_attrib->unsel);
  
  if ( i >= nentries ) {
    fputs(pad_line("", 0, WIDTH-2*MARGIN-4), stdout);
  } else {
    display_entry(&menu_entries[i],
		  (i == sel) ? menu_attrib->sel : menu_attrib->unsel,
		  (i == sel) ? menu_attrib->hotsel : menu_attrib->hotkey,
		  WIDTH-2*MARGIN-4);
  }

  if ( nentries <= MENU_ROWS ) {
    printf(" %s\016x\017", menu_attrib->border);
  } else if ( sbtop > 0 ) {
    if ( y >= sbtop && y <= sbbot )
      printf(" %s\016a\017", menu_attrib->scrollbar);
    else
      printf(" %s\016x\017", menu_attrib->border);
  } else {
    putchar(' ');		/* Don't modify the scrollbar */
  }
}

static int
passwd_compare(const char *passwd, const char *entry)
{
  const char *p;
  SHA1_CTX ctx;
  unsigned char sha1[20], pwdsha1[20];

  if ( passwd[0] != '$' )	/* Plaintext passwd, yuck! */
    return !strcmp(entry, passwd);

  if ( strncmp(passwd, "$4$", 3) )
    return 0;			/* Only SHA-1 passwds supported */

  SHA1Init(&ctx);

  if ( (p = strchr(passwd+3, '$')) ) {
    SHA1Update(&ctx, passwd+3, p-(passwd+3));
    p++;
  } else {
    p = passwd+3;		/* Assume no salt */
  }

  SHA1Update(&ctx, entry, strlen(entry));
  SHA1Final(sha1, &ctx);

  memset(pwdsha1, 0, 20);
  unbase64(pwdsha1, 20, p);

  return !memcmp(sha1, pwdsha1, 20);
}

static int
ask_passwd(const char *menu_entry)
{
  static const char title[] = "Password required";
  char user_passwd[WIDTH], *p;
  int done;
  int key;
  int x;

  printf("\033[%d;%dH%s\016l", PASSWD_ROW, PASSWD_MARGIN+1,
	 menu_attrib->pwdborder);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar('q');
  
  printf("k\033[%d;%dHx", PASSWD_ROW+1, PASSWD_MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar(' ');

  printf("x\033[%d;%dHm", PASSWD_ROW+2, PASSWD_MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar('q');
  
  printf("j\017\033[%d;%dH%s %s \033[%d;%dH%s",
	 PASSWD_ROW, (WIDTH-((int)sizeof(title)+1))/2,
	 menu_attrib->pwdheader, title,
	 PASSWD_ROW+1, PASSWD_MARGIN+3, menu_attrib->pwdentry);

  /* Actually allow user to type a password, then compare to the SHA1 */
  done = 0;
  p = user_passwd;

  while ( !done ) {
    key = get_key(stdin, 0);

    switch ( key ) {
    case KEY_ENTER:
    case KEY_CTRL('J'):
      done = 1;
      break;

    case KEY_ESC:
    case KEY_CTRL('C'):
      p = user_passwd;		/* No password entered */
      done = 1;
      break;

    case KEY_BACKSPACE:
    case KEY_DEL:
    case KEY_DELETE:
      if ( p > user_passwd ) {
	printf("\b \b");
	p--;
      }
      break;

    case KEY_CTRL('U'):
      while ( p > user_passwd ) {
	printf("\b \b");
	p--;
      }
      break;

    default:
      if ( key >= ' ' && key <= 0xFF &&
	   (p-user_passwd) < WIDTH-2*PASSWD_MARGIN-5 ) {
	*p++ = key;
	putchar('*');
      }
      break;
    }
  }

  if ( p == user_passwd )
    return 0;			/* No password entered */

  *p = '\0';
      
  return (menu_master_passwd && passwd_compare(menu_master_passwd, user_passwd))
    || (menu_entry && passwd_compare(menu_entry, user_passwd));
}


static void
draw_menu(int sel, int top)
{
  int x, y;
  int sbtop = 0, sbbot = 0;
  
  if ( nentries > MENU_ROWS ) {
    int sblen = MENU_ROWS*MENU_ROWS/nentries;
    sbtop = (MENU_ROWS-sblen+1)*top/(nentries-MENU_ROWS+1);
    sbbot = sbtop + sblen - 1;
    
    sbtop += 4;  sbbot += 4;	/* Starting row of scrollbar */
  }
  
  printf("\033[1;%dH%s\016l", MARGIN+1, menu_attrib->border);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('q');
  
  printf("k\033[2;%dH%sx\017%s %s %s\016x",
	 MARGIN+1,
	 menu_attrib->border,
	 menu_attrib->title,
	 pad_line(menu_title, 1, WIDTH-2*MARGIN-4),
	 menu_attrib->border);
  
  printf("\033[3;%dH%st", MARGIN+1, menu_attrib->border);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('q');
  fputs("u\017", stdout);
  
  for ( y = 4 ; y < 4+MENU_ROWS ; y++ )
    draw_row(y, sel, top, sbtop, sbbot);

  printf("\033[%d;%dH%s\016m", y, MARGIN+1, menu_attrib->border);
  for ( x = 2 ; x <= WIDTH-2*MARGIN-1 ; x++ )
    putchar('q');
  fputs("j\017", stdout);

  if ( allowedit && !menu_master_passwd )
    printf("%s\033[%d;1H%s", menu_attrib->tabmsg, TABMSG_ROW,
	   pad_line("Press [Tab] to edit options", 1, WIDTH));

  printf("%s\033[%d;1H", menu_attrib->screen, END_ROW);
}

static const char *
edit_cmdline(char *input, int top)
{
  static char cmdline[MAX_CMDLINE_LEN];
  int key, len;
  int redraw = 1;		/* We enter with the menu already drawn */

  strncpy(cmdline, input, MAX_CMDLINE_LEN);
  cmdline[MAX_CMDLINE_LEN-1] = '\0';

  len = strlen(cmdline);

  for (;;) {
    if ( redraw > 1 ) {
      /* Clear and redraw whole screen */
      /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	 to avoid confusing the Linux console */
      printf("\033e\033%%@\033)0\033(B%s\033[?25l\033[2J", menu_attrib->screen);
      draw_menu(-1, top);
    }

    if ( redraw > 0 ) {
      /* Redraw the command line */
      printf("\033[?25h\033[%d;1H%s> %s%s",
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
    case KEY_DELETE:
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

static void
clear_screen(void)
{
  printf("\033e\033%%@\033)0\033(B%s\033[?25l\033[2J", menu_attrib->screen);
}

static const char *
run_menu(void)
{
  int key;
  int done = 0;
  int entry = defentry, prev_entry = -1;
  int top = 0, prev_top = -1;
  int clear = 1;
  const char *cmdline = NULL;
  clock_t key_timeout;

  /* Convert timeout from deciseconds to clock ticks */
  /* Note: for both key_timeout and timeout == 0 means no limit */
  key_timeout = (clock_t)(CLK_TCK*timeout+9)/10;

  while ( !done ) {
    if ( entry < 0 )
      entry = 0;
    else if ( entry >= nentries )
      entry = nentries-1;

    if ( top < 0 || top < entry-MENU_ROWS+1 )
      top = max(0, entry-MENU_ROWS+1);
    else if ( top > entry || top > max(0,nentries-MENU_ROWS) )
      top = min(entry, max(0,nentries-MENU_ROWS));

    /* Start with a clear screen */
    if ( clear ) {
      /* Clear and redraw whole screen */
      /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	 to avoid confusing the Linux console */
      clear_screen();
      clear = 0;
      prev_entry = prev_top = -1;
    }

    if ( top != prev_top ) {
      draw_menu(entry, top);
    } else if ( entry != prev_entry ) {
      draw_row(prev_entry-top+4, entry, top, 0, 0);
      draw_row(entry-top+4, entry, top, 0, 0);
    }

    prev_entry = entry;  prev_top = top;

    key = get_key(stdin, key_timeout);
    switch ( key ) {
    case KEY_NONE:		/* Timeout */
      /* This is somewhat hacky, but this at least lets the user
	 know what's going on, and still deals with "phantom inputs"
	 e.g. on serial ports.

	 Warning: a timeout will boot the default entry without any
	 password! */
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
      if ( menu_entries[entry].passwd ) {
	clear = 1;
	done = ask_passwd(menu_entries[entry].passwd);
      } else {
	done = 1;
      }
      cmdline = menu_entries[entry].label;
      break;
    case 'P':
    case 'p':
    case KEY_UP:
      if ( entry > 0 ) {
	entry--;
	if ( entry < top )
	  top -= MENU_ROWS;
      }
      break;
    case 'N':
    case 'n':
    case KEY_DOWN:
      if ( entry < nentries-1 ) {
	entry++;
	if ( entry >= top+MENU_ROWS )
	  top += MENU_ROWS;
      }
      break;
    case KEY_CTRL('P'):
    case KEY_PGUP:
    case KEY_LEFT:
      entry -= MENU_ROWS;
      top   -= MENU_ROWS;
      break;
    case KEY_CTRL('N'):
    case KEY_PGDN:
    case KEY_RIGHT:
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
    case KEY_CTRL('A'):
    case KEY_HOME:
      top = entry = 0;
      break;
    case KEY_CTRL('E'):
    case KEY_END:
      entry = nentries - 1;
      top = max(0, nentries-MENU_ROWS);
      break;
    case KEY_TAB:
      if ( allowedit ) {
	int ok = 1;

	draw_row(entry-top+4, -1, top, 0, 0);

	if ( menu_master_passwd ) {
	  ok = ask_passwd(NULL);
	  clear_screen();
	  draw_menu(-1, top);
	}
	
	if ( ok ) {
	  cmdline = edit_cmdline(menu_entries[entry].cmdline, top);
	  done = !!cmdline;
	  clear = 1;		/* In case we hit [Esc] and done is null */
	} else {
	  draw_row(entry-top+4, entry, top, 0, 0);
	}
      }
      break;
    case KEY_CTRL('C'):		/* Ctrl-C */
    case KEY_ESC:		/* Esc */
      if ( allowedit ) {
	done = 1;
	clear = 1;
	
	draw_row(entry-top+4, -1, top, 0, 0);

	if ( menu_master_passwd )
	  done = ask_passwd(NULL);
      }
      break;
    default:
      if ( key > 0 && key < 0xFF ) {
	key &= ~0x20;		/* Upper case */
	if ( menu_hotkeys[key] ) {
	  entry = menu_hotkeys[key] - menu_entries;
	  /* Should we commit at this point? */
	}
      }
      break;
    }
  }

  printf("\033[?25h");		/* Show cursor */

  /* Return the label name so localboot and ipappend work */
  return cmdline;
}


static void __attribute__((noreturn))
execute(const char *cmdline)
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
  const char *cmdline = NULL;
  int err = 0;

  (void)argc;

  console_ansi_raw();

  parse_config(argv[1]);

  if ( !nentries ) {
    fputs("No LABEL entries found in configuration file!\n", stdout);
    err = 1;
  } else {
    cmdline = run_menu();
  }

  printf("\033[?25h\033[%d;1H\033[0m", END_ROW);
  if ( cmdline )
    execute(cmdline);
  else
    return err;
}
