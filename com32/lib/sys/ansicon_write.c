#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *   
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * ansicon_write.c
 *
 * Write to the screen using ANSI control codes (about as capable as
 * DOS' ANSI.SYS.)
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <klibc/compiler.h>
#include "file.h"

static void __constructor ansicon_init(void)
{
  static com32sys_t ireg;	/* Auto-initalized to all zero */

  ireg.eax.w[0] = 0x0005;	/* Force text mode */
  __intcall(0x22, &ireg, NULL);
}

struct curxy {
  uint8_t x, y;
} __attribute__((packed));
#define BIOS_CURXY ((struct curxy *)0x450) /* Array for each page */
#define BIOS_ROWS (*(uint8_t *)0x449)      /* Minus one */
#define BIOS_COLS (*(uint16_t *)0x44A)
#define BIOS_PAGE (*(uint8_t *)0x462)
#define BIOS_FB   (*(uint16_t *)0x44E)     /* Current page fb address */

enum ansi_state {
  st_init,			/* Normal (no ESC seen) */
  st_esc,			/* ESC seen */
  st_empty,			/* CSI seen, empty position */
  st_digits,			/* CSI seen, digits seen in this position */
};

#define MAX_PARMS	16

static struct {
  int attr;			/* Current display attribute */
  int intensity;
  int underline;
  int blink;
  int reverse;
  int fg;
  int bg;
  struct curxy saved_xy;
  enum ansi_state state;
  int nparms;			/* Number of parameters seen */
  int parms[MAX_PARMS];
} st = {
  .attr = 0x07,			/* Grey on black */
  .intensity = 1,
  .underline = 0,
  .blink = 0,
  .reverse = 0,
  .fg = 7,
  .bg = 0,
  .saved_xy = { 0, 0 },
  .state = st_init,
  .nparms = 0,
  .parms = { 0, },
};

/* Erase a region of the screen */
static void ansicon_erase(int x0, int y0, int x1, int y1)
{
  static com32sys_t ireg;
  
  ireg.eax.w[0] = 0x0600;	/* Clear window */
  ireg.eax.b[1] = st.attr;	/* Fill with current attribute */
  ireg.ecx.b[0] = x0;
  ireg.ecx.b[1] = y0;
  ireg.edx.b[0] = x1;
  ireg.edx.b[1] = y1;
  __intcall(0x10, &ireg, NULL);
}

static void ansicon_putchar(int ch)
{
  static com32sys_t ireg;
  const int rows  = BIOS_ROWS ? BIOS_ROWS+1 : 25;
  const int cols  = BIOS_COLS;
  const int page  = BIOS_PAGE;
  struct curxy xy = BIOS_CURXY[page];
  int again;

  do {
    again = 0;

    switch ( st.state ) {
    case st_init:
      switch ( ch ) {
      case '\a':
	/* Ignore beep */
	break;
      case '\b':
	if ( xy.y > 0 ) xy.y--;
	break;
      case '\t':
	{
	  int nsp = 8 - (xy.y & 7);
	  while ( nsp-- )
	    ansicon_putchar(' ');
	}
	return;			/* Cursor already updated */
      case '\n':
      case '\v':
      case '\f':
	xy.y++;
	break;
      case '\r':
	xy.x = 0;
	break;
      case '\0':
      case 127:
	/* Ignore null or delete */
	break;
      case 27:
	st.state = st_esc;
	break;
      default:
	/* Print character */
	ireg.eax.b[1] = 0x09;
	ireg.eax.b[0] = ch;
	ireg.ebx.b[1] = page;
	ireg.ebx.b[0] = st.attr;
	ireg.ecx.w[0] = 1;
	__intcall(0x10, &ireg, NULL);
	xy.x++;
	break;
      }
      break;
      
    case st_esc:
      if ( ch == '[' ) {
	int i;
	st.state = st_empty;
	st.nparms = 0;
	for ( i = 0 ; i < MAX_PARMS ; i++ )
	  st.parms[i] = 1;	/* 1 is the default, not 0 */
      } else {
	st.state = st_init;
	again = 1;
      }
      break;
      
    case st_empty:
    case st_digits:
      switch ( ch ) {
	case '0' ... '9':
	if ( st.state == st_empty ) {
	  st.parms[st.nparms] = ch - '0';
	  st.state = st_digits;
	} else {
	  st.parms[st.nparms] = st.parms[st.nparms]*10 + (ch-'0');
	}
	break;
      case ';':
	st.nparms++;
	if ( st.nparms >= MAX_PARMS )
	  st.nparms = MAX_PARMS-1;
	st.state = st_empty;
	break;
      case 'A':
	{
	  int y = xy.y - st.parms[0];
	  xy.y = (y < 0) ? 0 : y;
	}
	break;
      case 'B':
	{
	  int y = xy.y + st.parms[0];
	  xy.y = (y >= rows) ? rows-1 : y;
	}
	break;
      case 'C':
	{
	  int x = xy.x + st.parms[0];
	  xy.x = (x >= cols) ? cols-1 : x;
	}
	break;
      case 'D':
	{
	  int x = xy.x - st.parms[0];
	  xy.x = (x < 0) ? 0 : x;
	}
	break;
      case 'E':
	{
	  int y = xy.y + st.parms[0];
	  xy.y = (y >= rows) ? rows-1 : y;
	  xy.x = 0;
	}
	break;
      case 'F':
	{
	  int y = xy.y - st.parms[0];
	  xy.y = (y < 0) ? 0 : y;
	  xy.x = 0;
	}
	break;
      case 'H':
      case 'f':
	{
	  int x = st.parms[0];
	  int y = st.parms[1];

	  xy.x = (x >= cols) ? cols-1 : x;
	  xy.y = (y >= rows) ? rows-1 : y;
	}
	break;
      case 'J':
	{
	  if ( st.parms[0] == 2 ) {
	    ansicon_erase(0, 0, cols-1, rows-1);
	  } else {
	    ansicon_erase(xy.x, xy.y, cols-1, xy.y);
	    if ( xy.y < rows-1 )
	      ansicon_erase(0, xy.y+1, cols-1, rows-1);
	  }
	}
	break;
      case 'K':
	{
	  if ( st.parms[0] == 2 )
	    ansicon_erase(0, xy.y, cols-1, xy.y);
	  else
	    ansicon_erase(xy.x, xy.y, cols-1, xy.y);
	}
	break;
      case 'm':
	{
	  static const int ansi2pc[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	  int i;
	  for ( i = 0 ; i < st.nparms ; i++ ) {
	    int a = st.parms[i];
	    switch ( a ) {
	    case 0:
	      st.fg = 7;
	      st.bg = 7;
	      st.intensity = 1;
	      st.underline = 0;
	      st.blink = 0;
	      st.reverse = 0;
	      break;
	    case 1:
	      st.intensity = 2;
	      break;
	    case 2:
	      st.intensity = 0;
	      break;
	    case 4:
	      st.underline = 1;
	      break;
	    case 5:
	      st.blink = 1;
	      break;
	    case 7:
	      st.reverse = 1;
	      break;
	    case 21:
	    case 22:
	      st.intensity = 1;
	      break;
	    case 24:
	      st.underline = 0;
	      break;
	    case 25:
	      st.blink = 0;
	      break;
	    case 27:
	      st.reverse = 0;
	      break;
	    case 30 ... 37:
	      st.fg = ansi2pc[a-30];
	      break;
	    case 38:
	      st.fg = 7;
	      st.underline = 1;
	      break;
	    case 39:
	      st.fg = 7;
	      st.underline = 0;
	      break;
	    case 40 ... 47:
	      st.bg = ansi2pc[a-40];
	      break;
	    case 49:
	      st.bg = 7;
	      break;
	    default:
	      /* Do nothing */
	      break;
	    }
	  }
	  
	  /* Turn into an attribute code */
	  {
	    int bg = st.bg;
	    int fg;

	    if ( st.underline )
	      fg = 0x01;
	    else if ( st.intensity == 0 )
	      fg = 0x08;
	    else
	      fg = st.fg;

	    if ( st.reverse ) {
	      bg = fg & 0x07;
	      fg &= 0x08;
	      fg |= st.bg;
	    }

	    if ( st.blink )
	      bg ^= 0x08;

	    if ( st.intensity == 2 )
	      fg ^= 0x08;

	    st.attr = (bg << 4) | fg;
	  }
	}
	break;
      case 's':
	st.saved_xy = xy;
	break;
      case 'u':
	xy = st.saved_xy;
	break;
      }
    }
  } while ( again );

  /* If we fell off the end of the screen, adjust */
  if ( xy.x >= cols ) {
    xy.x = 0;
    xy.y++;
    if ( xy.y >= rows ) {
      xy.y = 0;
      ireg.eax.w[0] = 0x0601;
      ireg.ebx.b[1] = st.attr;
      ireg.ecx.w[0] = 0;
      ireg.edx.b[1] = rows-1;
      ireg.edx.b[0] = cols-1;
      __intcall(0x10, &ireg, NULL); /* Scroll */
    }
  }

  /* Update cursor position */
  ireg.eax.b[1] = 0x04;
  ireg.ebx.b[1] = BIOS_PAGE;
  ireg.edx.b[1] = xy.y;
  ireg.edx.b[0] = xy.x;
  __intcall(0x10, &ireg, NULL);
}	
      

static ssize_t ansicon_write(struct file_info *fp, const void *buf, size_t count)
{
  com32sys_t ireg;
  const char *bufp = buf;
  size_t n = 0;

  (void)fp;

  memset(&ireg, 0, sizeof ireg); 
  ireg.eax.b[1] = 0x04;

  while ( count-- ) {
    ansicon_putchar(*bufp++);
    n++;
  }

  return n;
}

const struct output_dev dev_ansicon_w = {
  .dev_magic  = __DEV_MAGIC,
  .flags      = __DEV_TTY | __DEV_OUTPUT,
  .fileflags  = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND,
  .write      = ansicon_write,
  .close      = NULL,
};
