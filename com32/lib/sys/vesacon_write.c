/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2006 H. Peter Anvin - All Rights Reserved
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
 * vesacon_write.c
 *
 * Write to the screen mapped using VESA BIOS extensions (VBE)
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <klibc/compiler.h>
#include "file.h"

struct curxy {
  uint8_t x, y;
} __attribute__((packed));
#define BIOS_CURXY ((struct curxy *)0x450) /* Array for each page */
#define BIOS_ROWS (*(uint8_t *)0x484)      /* Minus one; if zero use 24 (= 25 lines) */
#define BIOS_COLS (*(uint16_t *)0x44A)
#define BIOS_PAGE (*(uint8_t *)0x462)

enum ansi_state {
  st_init,			/* Normal (no ESC seen) */
  st_esc,			/* ESC seen */
  st_csi,			/* CSI seen */
};

#define MAX_PARMS	16

struct term_state {
  int disabled;
  int attr;			/* Current display attribute */
  int vtgraphics;		/* VT graphics on/off */
  int intensity;
  int underline;
  int blink;
  int reverse;
  int fg;
  int bg;
  int autocr;
  struct curxy saved_xy;
  uint16_t cursor_type;
  enum ansi_state state;
  int pvt;			/* Private code? */
  int nparms;			/* Number of parameters seen */
  int parms[MAX_PARMS];
};

static const struct term_state default_state =
{
  .disabled = 0,
  .attr = 0x07,			/* Grey on black */
  .vtgraphics = 0,
  .intensity = 1,
  .underline = 0,
  .blink = 0,
  .reverse = 0,
  .fg = 7,
  .bg = 0,
  .autocr = 0,
  .saved_xy = { 0, 0 },
  .cursor_type = 0x0607,
  .state = st_init,
  .pvt = 0,
  .nparms = 0,
};

static struct term_state st;

/* DEC VT graphics to codepage 437 table (characters 0x60-0x7F only) */
static const char decvt_to_cp437[] =
  { 0004, 0261, 0007, 0007, 0007, 0007, 0370, 0361, 0007, 0007, 0331, 0277, 0332, 0300, 0305, 0304,
    0304, 0304, 0137, 0137, 0303, 0264, 0301, 0302, 0263, 0363, 0362, 0343, 0330, 0234, 0007, 00 };

/* Common setup */
static void __constructor vesacon_init(void)
{
  static com32sys_t ireg;	/* Auto-initalized to all zero */
  com32sys_t oreg;

  /* Initial state */
  memcpy(&st, &default_state, sizeof st);

  /* Are we disabled? */
  ireg.eax.w[0] = 0x000b;
  __intcall(0x22, &ireg, &oreg);

  if ( (signed char)oreg.ebx.b[1] < 0 ) {
    st.disabled = 1;
    return;
  }

  /* Force text mode */
  ireg.eax.w[0] = 0x0005;
  __intcall(0x22, &ireg, NULL);

  /* Get cursor shape */
  ireg.eax.b[1] = 0x03;
  ireg.ebx.b[1] = BIOS_PAGE;
  __intcall(0x10, &ireg, &oreg);
  st.cursor_type = oreg.ecx.w[0];
}

/* Erase a region of the screen */
static void vesacon_erase(int x0, int y0, int x1, int y1)
{
  static com32sys_t ireg;

  ireg.eax.w[0] = 0x0600;	/* Clear window */
  ireg.ebx.b[1] = st.attr;	/* Fill with current attribute */
  ireg.ecx.b[0] = x0;
  ireg.ecx.b[1] = y0;
  ireg.edx.b[0] = x1;
  ireg.edx.b[1] = y1;
  __intcall(0x10, &ireg, NULL);
}

/* Show or hide the cursor */
static void showcursor(int yes)
{
  static com32sys_t ireg;

  ireg.eax.b[1] = 0x01;
  ireg.ecx.w[0] = yes ? st.cursor_type : 0x2020;
  __intcall(0x10, &ireg, NULL);
}

static void vesacon_putchar(int ch)
{
  static com32sys_t ireg;
  const int rows  = BIOS_ROWS ? BIOS_ROWS+1 : 25;
  const int cols  = BIOS_COLS;
  const int page  = BIOS_PAGE;
  struct curxy xy = BIOS_CURXY[page];

  switch ( st.state ) {
  case st_init:
    switch ( ch ) {
    case '\b':
      if ( xy.x > 0 ) xy.x--;
      break;
    case '\t':
      {
	int nsp = 8 - (xy.x & 7);
	while ( nsp-- )
	  vesacon_putchar(' ');
      }
      return;			/* Cursor already updated */
    case '\n':
    case '\v':
    case '\f':
      xy.y++;
      if ( st.autocr )
	xy.x = 0;
      break;
    case '\r':
      xy.x = 0;
      break;
    case 127:
      /* Ignore delete */
      break;
    case 14:
      st.vtgraphics = 1;
      break;
    case 15:
      st.vtgraphics = 0;
      break;
    case 27:
      st.state = st_esc;
      break;
    default:
      /* Print character */
      if ( ch >= 32 ) {
	if ( st.vtgraphics && (ch & 0xe0) == 0x60 )
	  ch = decvt_to_cp437[ch - 0x60];

	ireg.eax.b[1] = 0x09;
	ireg.eax.b[0] = ch;
	ireg.ebx.b[1] = page;
	ireg.ebx.b[0] = st.attr;
	ireg.ecx.w[0] = 1;
	__intcall(0x10, &ireg, NULL);
	xy.x++;
      }
      break;
    }
    break;

  case st_esc:
    switch ( ch ) {
    case '%':
    case '(':
    case ')':
    case '#':
      /* Ignore this plus the subsequent character, allows
	 compatibility with Linux sequence to set charset */
      break;
    case '[':
      st.state = st_csi;
      st.nparms = st.pvt = 0;
      memset(st.parms, 0, sizeof st.parms);
      break;
    case 'c':
      /* Reset terminal */
      memcpy(&st, &default_state, sizeof st);
      vesacon_erase(0, 0, cols-1, rows-1);
      xy.x = xy.y = 1;
      break;
    default:
      /* Ignore sequence */
      st.state = st_init;
      break;
    }
    break;

  case st_csi:
    {
      int p0 = st.parms[0] ? st.parms[0] : 1;

      if ( ch >= '0' && ch <= '9' ) {
	st.parms[st.nparms] = st.parms[st.nparms]*10 + (ch-'0');
      } else if ( ch == ';' ) {
	st.nparms++;
	if ( st.nparms >= MAX_PARMS )
	  st.nparms = MAX_PARMS-1;
	break;
      } else if ( ch == '?' ) {
	st.pvt = 1;
      } else {
	switch ( ch ) {
	case 'A':
	  {
	    int y = xy.y - p0;
	    xy.y = (y < 0) ? 0 : y;
	  }
	  break;
	case 'B':
	  {
	    int y = xy.y + p0;
	    xy.y = (y >= rows) ? rows-1 : y;
	  }
	  break;
	case 'C':
	  {
	    int x = xy.x + p0;
	    xy.x = (x >= cols) ? cols-1 : x;
	  }
	  break;
	case 'D':
	  {
	    int x = xy.x - p0;
	    xy.x = (x < 0) ? 0 : x;
	  }
	  break;
	case 'E':
	  {
	    int y = xy.y + p0;
	    xy.y = (y >= rows) ? rows-1 : y;
	    xy.x = 0;
	  }
	  break;
	case 'F':
	  {
	    int y = xy.y - p0;
	    xy.y = (y < 0) ? 0 : y;
	    xy.x = 0;
	  }
	  break;
	case 'G':
	case '\'':
	  {
	    int x = st.parms[0] - 1;
	    xy.x = (x >= cols) ? cols-1 : (x < 0) ? 0 : x;
	  }
	  break;
	case 'H':
	case 'f':
	  {
	    int y = st.parms[0] - 1;
	    int x = st.parms[1] - 1;

	    xy.x = (x >= cols) ? cols-1 : (x < 0) ? 0 : x;
	    xy.y = (y >= rows) ? rows-1 : (y < 0) ? 0 : y;
	  }
	  break;
	case 'J':
	  {
	    switch ( st.parms[0] ) {
	    case 0:
	      vesacon_erase(xy.x, xy.y, cols-1, xy.y);
	      if ( xy.y < rows-1 )
		vesacon_erase(0, xy.y+1, cols-1, rows-1);
	      break;

	    case 1:
	      if ( xy.y > 0 )
		vesacon_erase(0, 0, cols-1, xy.y-1);
	      if ( xy.y > 0 )
		vesacon_erase(0, xy.y, xy.x-1, xy.y);
	      break;

	    case 2:
	      vesacon_erase(0, 0, cols-1, rows-1);
	      break;

	    default:
	      /* Ignore */
	      break;
	    }
	  }
	  break;
	case 'K':
	  {
	    switch ( st.parms[0] ) {
	    case 0:
	      vesacon_erase(xy.x, xy.y, cols-1, xy.y);
	      break;

	    case 1:
	      if ( xy.x > 0 )
		vesacon_erase(0, xy.y, xy.x-1, xy.y);
	      break;

	    case 2:
	      vesacon_erase(0, xy.y, cols-1, xy.y);
	      break;

	    default:
	      /* Ignore */
	      break;
	    }
	  }
	  break;
	case 'h':
	case 'l':
	  {
	    int set = (ch == 'h');
	    switch ( st.parms[0] ) {
	    case 20:
	      st.autocr = set;
	      break;
	    case 25:
	      showcursor(set);
	      break;
	    default:
	      /* Ignore */
	      break;
	    }
	  }
	  break;
	case 'm':
	  {
	    static const int ansi2pc[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	    int i;
	    for ( i = 0 ; i <= st.nparms ; i++ ) {
	      int a = st.parms[i];
	      switch ( a ) {
	      case 0:
		st.fg = 7;
		st.bg = 0;
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
	default:		/* Includes CAN and SUB */
	  break;		/* Drop unknown sequence */
	}
	st.state = st_init;
      }
    }
    break;
  }

  /* If we fell off the end of the screen, adjust */
  if ( xy.x >= cols ) {
    xy.x = 0;
    xy.y++;
  }
  while ( xy.y >= rows ) {
    xy.y--;
    ireg.eax.w[0] = 0x0601;
    ireg.ebx.b[1] = st.attr;
    ireg.ecx.w[0] = 0;
    ireg.edx.b[1] = rows-1;
    ireg.edx.b[0] = cols-1;
    __intcall(0x10, &ireg, NULL); /* Scroll */
  }

  /* Update cursor position */
  ireg.eax.b[1] = 0x02;
  ireg.ebx.b[1] = page;
  ireg.edx.b[1] = xy.y;
  ireg.edx.b[0] = xy.x;
  __intcall(0x10, &ireg, NULL);
}


ssize_t __vesacon_write(struct file_info *fp, const void *buf, size_t count)
{
  const unsigned char *bufp = buf;
  size_t n = 0;

  (void)fp;

  if ( st.disabled )
    return n;			/* Nothing to do */

  while ( count-- ) {
    vesacon_putchar(*bufp++);
    n++;
  }

  return n;
}

const struct output_dev dev_vesacon_w = {
  .dev_magic  = __DEV_MAGIC,
  .flags      = __DEV_TTY | __DEV_OUTPUT,
  .fileflags  = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND,
  .write      = __vesacon_write,
  .close      = NULL,
};
