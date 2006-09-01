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
 * xserial_write.c
 *
 * Raw writing to the serial port; no \n -> \r\n translation, but
 * convert \1# sequences.
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <colortbl.h>
#include "file.h"

static void emit(char ch)
{
  static com32sys_t ireg;	/* Zeroed with the BSS */

  ireg.eax.w[0] = 0x0400 | (unsigned char)ch;

  __intcall(0x21, &ireg, NULL);
}

ssize_t __xserial_write(struct file_info *fp, const void *buf, size_t count)
{
  const char *bufp = buf;
  size_t n = 0;
  static enum { st_0, st_1, st_2, st_3 } state = st_0;
  static int ncolor = 0;
  int num;
  const char *p;

  (void)fp;

  while ( count-- ) {
    unsigned char ch = *bufp++;

    switch (state) {
    case st_0:
      if (ch == '\1') {
	state = st_1;
      } else {
	emit(ch);
      }
      break;

    case st_1:
      if (ch == '#') {
	state = st_2;
	ncolor = 0;
      } else {
	state = st_0;
      }
      break;

    case st_2:
      num = ch-'0';
      if (num < 10) {
	ncolor = num*10;
	state = st_3;
      } else {
	state = st_0;
      }
      break;

    case st_3:
      num = ch-'0';
      if (num < 10) {
	ncolor += num;

	if (ncolor < console_color_table_size) {
	  emit('\033');
	  emit('[');
	  for (p = console_color_table[ncolor].ansi; *p; p++)
	    emit(*p);
	  emit('m');
	}
      }
      state = st_0;
      break;
    }

    n++;
  }

  return n;
}
