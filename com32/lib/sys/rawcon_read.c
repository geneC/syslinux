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
 * rawcon_read.c
 *
 * Character-oriented reading from the console without echo, and with
 * a timeout (50-100 ms).
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <sys/times.h>
#include "file.h"

/* Global, since it's used by stdcon_read */
ssize_t __rawcon_read(struct file_info *fp, void *buf, size_t count)
{
  com32sys_t ireg, oreg;
  char *bufp = buf;
  size_t n = 0;
  clock_t start;

  (void)fp;

  memset(&ireg, 0, sizeof ireg); 

  start = times(NULL);

  while ( count ) {
    if ( (clock_t)(times(NULL) - start) >= 2+CLK_TCK/20 )
      break;

    /* Poll */
    ireg.eax.b[1] = 0x0B;
    __intcall(0x21, &ireg, &oreg);
    if ( !oreg.eax.b[0] ) {
      if ( n )
	break;			/* We have data, deliver it */
      else
	continue;
    }

    /* We have data, go get it */
    ireg.eax.b[1]  = 0x08;
    __intcall(0x21, &ireg, &oreg);
    *bufp++ = oreg.eax.b[0];
    n++;
  }

  if ( n == 0 ) {
    errno = EAGAIN;
    return -1;
  } else {
    return n;
  }
}

const struct input_dev dev_rawcon_r = {
  .dev_magic  = __DEV_MAGIC,
  .flags      = __DEV_TTY | __DEV_INPUT,
  .fileflags  = O_RDONLY,
  .read       = __rawcon_read,
  .close      = NULL,
};
