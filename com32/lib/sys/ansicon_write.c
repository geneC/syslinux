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

static struct {
  uint8_t attr;			/* Current display attribute */
} st = {
  0x0007,			/* Grey on black */
};

static void ansicon_putchar(int ch)
{
  static com32sys_t ireg;
  int rows = BIOS_ROWS ? BIOS_ROWS+1 : 25;
  int cols = BIOS_COLS;
  struct curxy xy = BIOS_CURXY[BIOS_PAGE];
  
  ireg.eax.b[1] = 0x09;
  ireg.eax.b[0] = ch;
  ireg.ebx.b[1] = BIOS_PAGE;
  ireg.ebx.b[0] = st.attr;
  ireg.ecx.w[0] = 1;
  __intcall(0x10, &ireg, NULL);

  xy.x++;
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
  
  ireg.eax.b[1] = 0x04;
  ireg.ebx.b[1] = BIOS_PAGE;
  ireg.edx.b[1] = curxy.y;
  ireg.edx.b[0] = curxy.x;
  __intcall(0x10, &ireg, NULL);	/* Update cursor position */
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
  .write      = __ansicon_write,
  .close      = NULL,
};
