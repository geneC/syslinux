/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2007 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * menumain.c
 *
 * Simple menu system which displays a list and allows the user to select
 * a command line and/or edit it.
 */

#define _GNU_SOURCE		/* Needed for asprintf() on Linux */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <consoles.h>
#include <getkey.h>
#include <minmax.h>
#include <setjmp.h>
#include <limits.h>
#include <sha1.h>
#include <md5.h>
#include <base64.h>
#include <colortbl.h>
#include <com32.h>

#include "menu.h"

/*
 * The color/attribute indexes (\1#X, \2#XX, \3#XXX) are as follows
 *
 * 00 - screen		Rest of the screen
 * 01 - border		Border area
 * 02 - title		Title bar
 * 03 - unsel		Unselected menu item
 * 04 - hotkey		Unselected hotkey
 * 05 - sel		Selection bar
 * 06 - hotsel		Selected hotkey
 * 07 - scrollbar	Scroll bar
 * 08 - tabmsg		Press [Tab] message
 * 09 - cmdmark		Command line marker
 * 10 - cmdline		Command line
 * 11 - pwdborder	Password box border
 * 12 - pwdheader	Password box header
 * 13 - pwdentry	Password box contents
 * 14 - timeout_msg	Timeout message
 * 15 - timeout		Timeout counter
 * 16 - help		Current entry help text
 * 17 - disabled        Disabled menu item
 */

static const struct color_table default_color_table[] = {
  { "screen",      "37;40",     0x80ffffff, 0x00000000, SHADOW_NORMAL },
  { "border",      "30;44",     0x40000000, 0x00000000, SHADOW_NORMAL },
  { "title",       "1;36;44",   0xc00090f0, 0x00000000, SHADOW_NORMAL },
  { "unsel",       "37;44",     0x90ffffff, 0x00000000, SHADOW_NORMAL },
  { "hotkey",      "1;37;44",   0xffffffff, 0x00000000, SHADOW_NORMAL },
  { "sel",         "7;37;40",   0xe0000000, 0x20ff8000, SHADOW_ALL },
  { "hotsel",      "1;7;37;40", 0xe0400000, 0x20ff8000, SHADOW_ALL },
  { "scrollbar",   "30;44",     0x40000000, 0x00000000, SHADOW_NORMAL },
  { "tabmsg",      "31;40",     0x90ffff00, 0x00000000, SHADOW_NORMAL },
  { "cmdmark",     "1;36;40",   0xc000ffff, 0x00000000, SHADOW_NORMAL },
  { "cmdline",     "37;40",     0xc0ffffff, 0x00000000, SHADOW_NORMAL },
  { "pwdborder",   "30;47",     0x80ffffff, 0x20ffffff, SHADOW_NORMAL },
  { "pwdheader",   "31;47",     0x80ff8080, 0x20ffffff, SHADOW_NORMAL },
  { "pwdentry",    "30;47",     0x80ffffff, 0x20ffffff, SHADOW_NORMAL },
  { "timeout_msg", "37;40",     0x80ffffff, 0x00000000, SHADOW_NORMAL },
  { "timeout",     "1;37;40",   0xc0ffffff, 0x00000000, SHADOW_NORMAL },
  { "help",        "37;40",     0xc0ffffff, 0x00000000, SHADOW_NORMAL },
  { "disabled",    "1;30;44",   0x60cccccc, 0x00000000, SHADOW_NORMAL },
};

#define NCOLORS (sizeof default_color_table/sizeof(struct color_table))
const int message_base_color = NCOLORS;

struct menu_parameter mparm[] = {
  { "width", 80 },
  { "margin", 10 },
  { "passwordmargin", 3 },
  { "rows", 12 },
  { "tabmsgrow", 18 },
  { "cmdlinerow", 18 },
  { "endrow", -1 },
  { "passwordrow", 11 },
  { "timeoutrow", 20 },
  { "helpmsgrow", 22 },
  { "helpmsgendrow", -1 },
  { "hshift", 0 },
  { "vshift", 0 },
  { "hiddenrow", -2 },
  { NULL, 0 }
};

#define WIDTH		mparm[0].value
#define MARGIN		mparm[1].value
#define PASSWD_MARGIN	mparm[2].value
#define MENU_ROWS	mparm[3].value
#define TABMSG_ROW	(mparm[4].value+VSHIFT)
#define CMDLINE_ROW	(mparm[5].value+VSHIFT)
#define END_ROW		mparm[6].value
#define PASSWD_ROW	(mparm[7].value+VSHIFT)
#define TIMEOUT_ROW	(mparm[8].value+VSHIFT)
#define HELPMSG_ROW	(mparm[9].value+VSHIFT)
#define HELPMSGEND_ROW	mparm[10].value
#define HSHIFT		mparm[11].value
#define VSHIFT		mparm[12].value
#define HIDDEN_ROW	mparm[13].value

void set_msg_colors_global(unsigned int fg, unsigned int bg,
			   enum color_table_shadow shadow)
{
  struct color_table *cp = console_color_table+message_base_color;
  unsigned int i;
  unsigned int fga, bga;
  unsigned int fgh, bgh;
  unsigned int fg_idx, bg_idx;
  unsigned int fg_rgb, bg_rgb;

  static const unsigned int pc2rgb[8] =
    { 0x000000, 0x0000ff, 0x00ff00, 0x00ffff, 0xff0000, 0xff00ff, 0xffff00,
      0xffffff };

  /* Converting PC RGBI to sensible RGBA values is an "interesting"
     proposition.  This algorithm may need plenty of tweaking. */

  fga = fg & 0xff000000;
  fgh = ((fg >> 1) & 0xff000000) | 0x80000000;

  bga = bg & 0xff000000;
  bgh = ((bg >> 1) & 0xff000000) | 0x80000000;

  for (i = 0; i < 256; i++) {
    fg_idx = i & 15;
    bg_idx = i >> 4;

    fg_rgb = pc2rgb[fg_idx & 7] & fg;
    bg_rgb = pc2rgb[bg_idx & 7] & bg;

    if (fg_idx & 8) {
      /* High intensity foreground */
      fg_rgb |= fgh;
    } else {
      fg_rgb |= fga;
    }

    if (bg_idx == 0) {
      /* Default black background, assume transparent */
      bg_rgb = 0;
    } else if (bg_idx & 8) {
      bg_rgb |= bgh;
    } else {
      bg_rgb |= bga;
    }

    cp->argb_fg = fg_rgb;
    cp->argb_bg = bg_rgb;
    cp->shadow = shadow;
    cp++;
  }
}

static void
install_default_color_table(void)
{
  unsigned int i;
  const struct color_table *dp;
  struct color_table *cp;
  static struct color_table color_table[NCOLORS+256];
  static const int pc2ansi[8] = {0, 4, 2, 6, 1, 5, 3, 7};

  dp = default_color_table;
  cp = color_table;

  for (i = 0; i < NCOLORS; i++) {
    if (cp->ansi)
      free((void *)cp->ansi);

    *cp = *dp;
    cp->ansi = strdup(dp->ansi);

    cp++;
    dp++;
  }

  for (i = 0; i < 256; i++) {
    if (!cp->name)
      asprintf((char **)&cp->name, "msg%02x", i);

    if (cp->ansi)
      free((void *)cp->ansi);

    asprintf((char **)&cp->ansi, "%s3%d;4%d", (i & 8) ? "1;" : "",
	     pc2ansi[i & 7], pc2ansi[(i >> 4) & 7]);

    cp++;
  }

  console_color_table = color_table;
  console_color_table_size = NCOLORS+256;

  set_msg_colors_global(MSG_COLORS_DEF_FG, MSG_COLORS_DEF_BG,
			MSG_COLORS_DEF_SHADOW);
}

static char *
pad_line(const char *text, int align, int width)
{
  static char buffer[MAX_CMDLINE_LEN];
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
	if ( *p && ((unsigned char)*p & ~0x20) == entry->hotkey ) {
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
  int i = (y-4-VSHIFT)+top;
  int dis = (i < nentries) && menu_entries[i].disabled;

  printf("\033[%d;%dH\1#1\016x\017%s ",
	 y, MARGIN+1+HSHIFT,
	 (i == sel) ? "\1#5" : dis ? "\2#17" : "\1#3");

  if ( i >= nentries ) {
    fputs(pad_line("", 0, WIDTH-2*MARGIN-4), stdout);
  } else {
    display_entry(&menu_entries[i],
		  (i == sel) ? "\1#5" : dis ? "\2#17" : "\1#3",
		  (i == sel) ? "\1#6" : dis ? "\2#17" : "\1#4",
		  WIDTH-2*MARGIN-4);
  }

  if ( nentries <= MENU_ROWS ) {
    printf(" \1#1\016x\017");
  } else if ( sbtop > 0 ) {
    if ( y >= sbtop && y <= sbbot )
      printf(" \1#7\016a\017");
    else
      printf(" \1#1\016x\017");
  } else {
    putchar(' ');		/* Don't modify the scrollbar */
  }
}

static int passwd_compare_sha1(const char *passwd, const char *entry)
{
  const char *p;
  SHA1_CTX ctx;
  unsigned char sha1[20], pwdsha1[20];

  if ( (p = strchr(passwd+3, '$')) ) {
    SHA1Update(&ctx, (void *)passwd+3, p-(passwd+3));
    p++;
  } else {
    p = passwd+3;		/* Assume no salt */
  }

  SHA1Init(&ctx);

  SHA1Update(&ctx, (void *)entry, strlen(entry));
  SHA1Final(sha1, &ctx);

  memset(pwdsha1, 0, 20);
  unbase64(pwdsha1, 20, p);

  return !memcmp(sha1, pwdsha1, 20);
}

static int passwd_compare_md5(const char *passwd, const char *entry)
{
  const char *crypted = crypt_md5(entry, passwd+3);
  int len = strlen(crypted);

  return !strncmp(crypted, passwd, len) &&
    (passwd[len] == '\0' || passwd[len] == '$');
}

static int
passwd_compare(const char *passwd, const char *entry)
{
  if ( passwd[0] != '$' )	/* Plaintext passwd, yuck! */
    return !strcmp(entry, passwd);
  else if ( !strncmp(passwd, "$4$", 3) )
    return passwd_compare_sha1(passwd, entry);
  else if ( !strncmp(passwd, "$1$", 3) )
    return passwd_compare_md5(passwd, entry);
  else
    return 0;			/* Invalid encryption algorithm */
}

static jmp_buf timeout_jump;

int mygetkey(clock_t timeout)
{
  clock_t t0, t;
  clock_t tto, to;
  int key;

  if ( !totaltimeout )
    return get_key(stdin, timeout);

  for (;;) {
    tto = min(totaltimeout, INT_MAX);
    to = timeout ? min(tto, timeout) : tto;

    t0 = times(NULL);
    key = get_key(stdin, to);
    t = times(NULL) - t0;

    if ( totaltimeout <= t )
      longjmp(timeout_jump, 1);

    totaltimeout -= t;

    if ( key != KEY_NONE )
      return key;

    if ( timeout ) {
      if ( timeout <= t )
	return KEY_NONE;

      timeout -= t;
    }
  }
}

static int
ask_passwd(const char *menu_entry)
{
  char user_passwd[WIDTH], *p;
  int done;
  int key;
  int x;

  printf("\033[%d;%dH\2#11\016l", PASSWD_ROW, PASSWD_MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar('q');

  printf("k\033[%d;%dHx", PASSWD_ROW+1, PASSWD_MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar(' ');

  printf("x\033[%d;%dHm", PASSWD_ROW+2, PASSWD_MARGIN+1);
  for ( x = 2 ; x <= WIDTH-2*PASSWD_MARGIN-1 ; x++ )
    putchar('q');

  printf("j\017\033[%d;%dH\2#12 %s \033[%d;%dH\2#13",
	 PASSWD_ROW, (WIDTH-(strlen(messages[MSG_PASSPROMPT].msg)+2))/2,
	 messages[MSG_PASSPROMPT].msg, PASSWD_ROW+1, PASSWD_MARGIN+3);

  /* Actually allow user to type a password, then compare to the SHA1 */
  done = 0;
  p = user_passwd;

  while ( !done ) {
    key = mygetkey(0);

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
draw_menu(int sel, int top, int edit_line)
{
  int x, y;
  int sbtop = 0, sbbot = 0;
  const char *tabmsg;
  int tabmsg_len;

  if ( nentries > MENU_ROWS ) {
    int sblen = MENU_ROWS*MENU_ROWS/nentries;
    sbtop = (MENU_ROWS-sblen+1)*top/(nentries-MENU_ROWS+1);
    sbbot = sbtop + sblen - 1;

    sbtop += 4;  sbbot += 4;	/* Starting row of scrollbar */
  }

  printf("\033[%d;%dH\1#1\016l", VSHIFT+1, HSHIFT+MARGIN+1);
  for ( x = 2+HSHIFT ; x <= (WIDTH-2*MARGIN-1)+HSHIFT ; x++ )
    putchar('q');

  printf("k\033[%d;%dH\1#1x\017\1#2 %s \1#1\016x",
	 VSHIFT+2,
	 HSHIFT+MARGIN+1,
	 pad_line(messages[MSG_TITLE].msg, 1, WIDTH-2*MARGIN-4));

  printf("\033[%d;%dH\1#1t", VSHIFT+3, HSHIFT+MARGIN+1);
  for ( x = 2+HSHIFT ; x <= (WIDTH-2*MARGIN-1)+HSHIFT ; x++ )
    putchar('q');
  fputs("u\017", stdout);

  for ( y = 4+VSHIFT ; y < 4+VSHIFT+MENU_ROWS ; y++ )
    draw_row(y, sel, top, sbtop, sbbot);

  printf("\033[%d;%dH\1#1\016m", y, HSHIFT+MARGIN+1);
  for ( x = 2+HSHIFT ; x <= (WIDTH-2*MARGIN-1)+HSHIFT ; x++ )
    putchar('q');
  fputs("j\017", stdout);

  if ( edit_line && allowedit && !menu_master_passwd )
    tabmsg = messages[MSG_TAB].msg;
  else
    tabmsg = messages[MSG_NOTAB].msg;

  tabmsg_len = strlen(tabmsg);

  printf("\1#8\033[%d;%dH%s",
	 TABMSG_ROW, 1+HSHIFT+((WIDTH-tabmsg_len)>>1), tabmsg);
  printf("\1#0\033[%d;1H", END_ROW);
}

static void
clear_screen(void)
{
  fputs("\033e\033%@\033)0\033(B\1#0\033[?25l\033[2J", stdout);
}

static void
display_help(const char *text)
{
  int row;
  const char *p;

  if (!text) {
    text = "";
    printf("\1#0\033[%d;1H", HELPMSG_ROW);
  } else {
    printf("\2#16\033[%d;1H", HELPMSG_ROW);
  }

  for (p = text, row = HELPMSG_ROW; *p && row <= HELPMSGEND_ROW; p++) {
    switch (*p) {
    case '\r':
    case '\f':
    case '\v':
    case '\033':
      break;
    case '\n':
      printf("\033[K\033[%d;1H", ++row);
      break;
    default:
      putchar(*p);
    }
  }

  fputs("\033[K", stdout);

  while (row <= HELPMSGEND_ROW) {
    printf("\033[K\033[%d;1H", ++row);
  }
}

static void show_fkey(int key)
{
  int fkey;

  while (1) {
    switch (key) {
    case KEY_F1:  fkey =  0;  break;
    case KEY_F2:  fkey =  1;  break;
    case KEY_F3:  fkey =  2;  break;
    case KEY_F4:  fkey =  3;  break;
    case KEY_F5:  fkey =  4;  break;
    case KEY_F6:  fkey =  5;  break;
    case KEY_F7:  fkey =  6;  break;
    case KEY_F8:  fkey =  7;  break;
    case KEY_F9:  fkey =  8;  break;
    case KEY_F10: fkey =  9;  break;
    case KEY_F11: fkey = 10;  break;
    case KEY_F12: fkey = 11;  break;
    default: fkey = -1; break;
    }

    if (fkey == -1)
      break;

    if (fkeyhelp[fkey].textname)
      key = show_message_file(fkeyhelp[fkey].textname,
			      fkeyhelp[fkey].background);
    else
      break;
  }
}

static const char *
edit_cmdline(char *input, int top)
{
  static char cmdline[MAX_CMDLINE_LEN];
  int key, len, prev_len, cursor;
  int redraw = 1;		/* We enter with the menu already drawn */

  strncpy(cmdline, input, MAX_CMDLINE_LEN);
  cmdline[MAX_CMDLINE_LEN-1] = '\0';

  len = cursor = strlen(cmdline);
  prev_len = 0;

  for (;;) {
    if ( redraw > 1 ) {
      /* Clear and redraw whole screen */
      /* Enable ASCII on G0 and DEC VT on G1; do it in this order
	 to avoid confusing the Linux console */
      clear_screen();
      draw_menu(-1, top, 1);
      prev_len = 0;
    }

    if ( redraw > 0 ) {
      /* Redraw the command line */
      printf("\033[?25l\033[%d;1H\1#9> \2#10%s",
	     CMDLINE_ROW, pad_line(cmdline, 0, max(len, prev_len)));
      printf("\2#10\033[%d;3H%s\033[?25h",
	     CMDLINE_ROW, pad_line(cmdline, 0, cursor));
      prev_len = len;
      redraw = 0;
    }

    key = mygetkey(0);

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
      if ( cursor ) {
	memmove(cmdline+cursor-1, cmdline+cursor, len-cursor+1);
	len--;
	cursor--;
	redraw = 1;
      }
      break;

    case KEY_CTRL('D'):
    case KEY_DELETE:
      if ( cursor < len ) {
	memmove(cmdline+cursor, cmdline+cursor+1, len-cursor);
	len--;
	redraw = 1;
      }
      break;

    case KEY_CTRL('U'):
      if ( len ) {
	len = cursor = 0;
	cmdline[len] = '\0';
	redraw = 1;
      }
      break;

    case KEY_CTRL('W'):
      if ( cursor ) {
	int prevcursor = cursor;

	while ( cursor && my_isspace(cmdline[cursor-1]) )
	  cursor--;

	while ( cursor && !my_isspace(cmdline[cursor-1]) )
	  cursor--;

	memmove(cmdline+cursor, cmdline+prevcursor, len-prevcursor+1);
	len -= (cursor-prevcursor);
	redraw = 1;
      }
      break;

    case KEY_LEFT:
    case KEY_CTRL('B'):
      if ( cursor ) {
	cursor--;
	redraw = 1;
      }
      break;

    case KEY_RIGHT:
    case KEY_CTRL('F'):
      if ( cursor < len ) {
	putchar(cmdline[cursor++]);
      }
      break;

    case KEY_CTRL('K'):
      if ( cursor < len ) {
	cmdline[len = cursor] = '\0';
	redraw = 1;
      }
      break;

    case KEY_HOME:
    case KEY_CTRL('A'):
      if ( cursor ) {
	cursor = 0;
	redraw = 1;
      }
      break;

    case KEY_END:
    case KEY_CTRL('E'):
      if ( cursor != len ) {
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
      show_fkey(key);
      redraw = 1;
      break;

    default:
      if ( key >= ' ' && key <= 0xFF && len < MAX_CMDLINE_LEN-1 ) {
	if ( cursor == len ) {
	  cmdline[len] = key;
	  cmdline[++len] = '\0';
	  cursor++;
	  putchar(key);
	  prev_len++;
	} else {
	  memmove(cmdline+cursor+1, cmdline+cursor, len-cursor+1);
	  cmdline[cursor++] = key;
	  len++;
	  redraw = 1;
	}
      }
      break;
    }
  }
}

static inline int
shift_is_held(void)
{
  uint8_t shift_bits = *(uint8_t *)0x417;

  return !!(shift_bits & 0x5d);	/* Caps/Scroll/Alt/Shift */
}

static void
print_timeout_message(int tol, int row, const char *msg)
{
  char buf[256];
  int nc = 0, nnc;
  const char *tp = msg;
  char tc;
  char *tq = buf;

  while ((size_t)(tq-buf) < (sizeof buf-16) && (tc = *tp)) {
    tp++;
    if (tc == '#') {
      nnc = sprintf(tq, "\2#15%d\2#14", tol);
      tq += nnc;
      nc += nnc-8;		/* 8 formatting characters */
    } else if (tc == '{') {
      /* Deal with {singular[,dual],plural} constructs */
      struct {
	const char *s, *e;
      } tx[3];
      const char *tpp;
      int n = 0;

      memset(tx, 0, sizeof tx);

      tx[0].s = tp;

      while (*tp && *tp != '}') {
	if (*tp == ',' && n < 2) {
	  tx[n].e = tp;
	  n++;
	  tx[n].s = tp+1;
	}
	tp++;
      }
      tx[n].e = tp;

      if (*tp)
	tp++;			/* Skip final bracket */

      if (!tx[1].s)
	tx[1] = tx[0];
      if (!tx[2].s)
	tx[2] = tx[1];

      /* Now [0] is singular, [1] is dual, and [2] is plural,
	 even if the user only specified some of them. */

      switch (tol) {
      case 1: n = 0; break;
      case 2: n = 1; break;
      default: n = 2; break;
      }

      for (tpp = tx[n].s; tpp < tx[n].e; tpp++) {
	if ((size_t)(tq-buf) < (sizeof buf)) {
	  *tq++ = *tpp;
	  nc++;
	}
      }
    } else {
      *tq++ = tc;
      nc++;
    }
  }
  *tq = '\0';

  /* Let's hope 4 spaces on each side is enough... */
  printf("\033[%d;%dH\2#14    %s    ", row, HSHIFT+1+((WIDTH-nc-8)>>1), buf);
}

static const char *
do_hidden_menu(void)
{
  int key;
  int timeout_left, this_timeout;

  clear_screen();

  if ( !setjmp(timeout_jump) ) {
    timeout_left = timeout;

    while (!timeout || timeout_left) {
      int tol = timeout_left/CLK_TCK;

      print_timeout_message(tol, HIDDEN_ROW, messages[MSG_AUTOBOOT].msg);

      this_timeout = min(timeout_left, CLK_TCK);
      key = mygetkey(this_timeout);

      if (key != KEY_NONE)
	return NULL;		/* Key pressed */

      timeout_left -= this_timeout;
    }
  }

  return menu_entries[defentry].cmdline; /* Default entry */
}

static const char *
run_menu(void)
{
  int key;
  int done = 0;
  volatile int entry = defentry, prev_entry = -1;
  int top = 0, prev_top = -1;
  int clear = 1, to_clear;
  const char *cmdline = NULL;
  volatile clock_t key_timeout, timeout_left, this_timeout;

  /* Note: for both key_timeout and timeout == 0 means no limit */
  timeout_left = key_timeout = timeout;

  /* If we're in shiftkey mode, exit immediately unless a shift key is pressed */
  if ( shiftkey && !shift_is_held() ) {
    return menu_entries[defentry].cmdline;
  }

  /* Handle hiddenmenu */
  if ( hiddenmenu ) {
    cmdline = do_hidden_menu();
    if (cmdline)
      return cmdline;

    /* Otherwise display the menu now; the timeout has already been
       cancelled, since the user pressed a key. */
    hiddenmenu = 0;
    key_timeout = 0;
  }

  /* Handle both local and global timeout */
  if ( setjmp(timeout_jump) ) {
    entry = defentry;

    if ( top < 0 || top < entry-MENU_ROWS+1 )
      top = max(0, entry-MENU_ROWS+1);
    else if ( top > entry || top > max(0,nentries-MENU_ROWS) )
      top = min(entry, max(0,nentries-MENU_ROWS));

    draw_menu(ontimeout ? -1 : entry, top, 1);
    cmdline = ontimeout ? ontimeout : menu_entries[entry].cmdline;
    done = 1;
  }

  while ( !done ) {
    if ( entry <= 0 ) {
      entry = 0;
      while ( entry < nentries && menu_entries[entry].disabled ) entry++;
    }

    if ( entry >= nentries ) {
      entry = nentries-1;
      while ( entry > 0 && menu_entries[entry].disabled ) entry--;
    }

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
      draw_menu(entry, top, 1);
      display_help(menu_entries[entry].helptext);
    } else if ( entry != prev_entry ) {
      draw_row(prev_entry-top+4+VSHIFT, entry, top, 0, 0);
      draw_row(entry-top+4+VSHIFT, entry, top, 0, 0);
      display_help(menu_entries[entry].helptext);
    }

    prev_entry = entry;  prev_top = top;

    /* Cursor movement cancels timeout */
    if ( entry != defentry )
      key_timeout = 0;

    if ( key_timeout ) {
      int tol = timeout_left/CLK_TCK;
      print_timeout_message(tol, TIMEOUT_ROW, messages[MSG_AUTOBOOT].msg);
      to_clear = 1;
    } else {
      to_clear = 0;
    }

    this_timeout = min(min(key_timeout, timeout_left), CLK_TCK);
    key = mygetkey(this_timeout);

    if ( key != KEY_NONE ) {
      timeout_left = key_timeout;
      if ( to_clear )
	printf("\033[%d;1H\1#0\033[K", TIMEOUT_ROW);
    }

    switch ( key ) {
    case KEY_NONE:		/* Timeout */
      /* This is somewhat hacky, but this at least lets the user
	 know what's going on, and still deals with "phantom inputs"
	 e.g. on serial ports.

	 Warning: a timeout will boot the default entry without any
	 password! */
      if ( key_timeout ) {
	if ( timeout_left <= this_timeout )
	  longjmp(timeout_jump, 1);

	timeout_left -= this_timeout;
      }
      break;

    case KEY_CTRL('L'):
      clear = 1;
      break;

    case KEY_ENTER:
    case KEY_CTRL('J'):
      key_timeout = 0;		/* Cancels timeout */
      if ( menu_entries[entry].passwd ) {
	clear = 1;
	done = ask_passwd(menu_entries[entry].passwd);
      } else {
	done = 1;
      }
      cmdline = menu_entries[entry].cmdline;
      break;

    case KEY_UP:
    case KEY_CTRL('P'):
      while ( entry > 0 && entry-- && menu_entries[entry].disabled ) {
	if ( entry < top )
	  top -= MENU_ROWS;
      }

      if ( entry == 0 ) {
        while ( menu_entries[entry].disabled )
          entry++;
      }
      break;

    case KEY_DOWN:
    case KEY_CTRL('N'):
      while ( entry < nentries-1 && entry++ && menu_entries[entry].disabled ) {
	if ( entry >= top+MENU_ROWS )
	  top += MENU_ROWS;
      }

      if ( entry == nentries-1 ) {
        while ( menu_entries[entry].disabled )
          entry--;
      }
      break;

    case KEY_PGUP:
    case KEY_LEFT:
    case KEY_CTRL('B'):
    case '<':
      entry -= MENU_ROWS;
      top   -= MENU_ROWS;
      break;

    case KEY_PGDN:
    case KEY_RIGHT:
    case KEY_CTRL('F'):
    case '>':
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
      show_fkey(key);
      clear = 1;
      break;

    case KEY_TAB:
      if ( allowedit ) {
	int ok = 1;

	key_timeout = 0;	/* Cancels timeout */
	draw_row(entry-top+4+VSHIFT, -1, top, 0, 0);

	if ( menu_master_passwd ) {
	  ok = ask_passwd(NULL);
	  clear_screen();
	  draw_menu(-1, top, 0);
	} else {
	  /* Erase [Tab] message and help text*/
	  printf("\033[%d;1H\1#0\033[K", TABMSG_ROW);
	  display_help(NULL);
	}

	if ( ok ) {
	  cmdline = edit_cmdline(menu_entries[entry].cmdline, top);
	  done = !!cmdline;
	  clear = 1;		/* In case we hit [Esc] and done is null */
	} else {
	  draw_row(entry-top+4+VSHIFT, entry, top, 0, 0);
	}
      }
      break;
    case KEY_CTRL('C'):		/* Ctrl-C */
    case KEY_ESC:		/* Esc */
      if ( allowedit ) {
	done = 1;
	clear = 1;
	key_timeout = 0;

	draw_row(entry-top+4+VSHIFT, -1, top, 0, 0);

	if ( menu_master_passwd )
	  done = ask_passwd(NULL);
      }
      break;
    default:
      if ( key > 0 && key < 0xFF ) {
	key &= ~0x20;		/* Upper case */
	if ( menu_hotkeys[key] ) {
	  key_timeout = 0;
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


static void
execute(const char *cmdline, enum kernel_type type)
{
  com32sys_t ireg;
  const char *p, **pp;
  char *q = __com32.cs_bounce;
  const char *kernel, *args;

  memset(&ireg, 0, sizeof ireg);

  kernel = q;
  p = cmdline;
  while ( *p && !my_isspace(*p) ) {
    *q++ = *p++;
  }
  *q++ = '\0';

  args = q;
  while ( *p && my_isspace(*p) )
    p++;

  strcpy(q, p);

  if (kernel[0] == '.' && type == KT_NONE) {
    /* It might be a type specifier */
    enum kernel_type type = KT_NONE;
    for (pp = kernel_types; *pp; pp++, type++) {
      if (!strcmp(kernel+1, *pp)) {
	execute(p, type);	/* Strip the type specifier and retry */
      }
    }
  }

  if (type == KT_LOCALBOOT) {
    ireg.eax.w[0] = 0x0014;	/* Local boot */
    ireg.edx.w[0] = strtoul(kernel, NULL, 0);
  } else {
    if (type < KT_KERNEL)
      type = KT_KERNEL;

    ireg.eax.w[0] = 0x0016;	/* Run kernel image */
    ireg.esi.w[0] = OFFS(kernel);
    ireg.ds       = SEG(kernel);
    ireg.ebx.w[0] = OFFS(args);
    ireg.es       = SEG(args);
    ireg.edx.l    = type-KT_KERNEL;
    /* ireg.ecx.l    = 0; */		/* We do ipappend "manually" */
  }

  __intcall(0x22, &ireg, NULL);

  /* If this returns, something went bad; return to menu */
}

int menu_main(int argc, char *argv[])
{
  const char *cmdline;
  int rows, cols;
  int i;

  (void)argc;

  console_prepare();

  install_default_color_table();
  if (getscreensize(1, &rows, &cols)) {
    /* Unknown screen size? */
    rows = 24;
    cols = 80;
  }

  WIDTH = cols;
  parse_configs(argv+1);

  /* If anyone has specified negative parameters, consider them
     relative to the bottom row of the screen. */
  for (i = 0; mparm[i].name; i++)
    if (mparm[i].value < 0)
      mparm[i].value = max(mparm[i].value+rows, 0);

  draw_background(menu_background);

  if ( !nentries ) {
    fputs("No LABEL entries found in configuration file!\n", stdout);
    return 1;			/* Error! */
  }

  for(;;) {
    cmdline = run_menu();

    printf("\033[?25h\033[%d;1H\033[0m", END_ROW);
    console_cleanup();

    if ( cmdline ) {
      execute(cmdline, KT_NONE);
      if ( onerror )
	execute(onerror, KT_NONE);
    } else {
      return 0;			/* Exit */
    }

    console_prepare();		/* If we're looping... */
  }
}
