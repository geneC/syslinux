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
 * stdcon_read.c
 *
 * Reading from the console, with echo
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include "file.h"

ssize_t __stdcon_read(struct file_info *fp, void *buf, size_t count)
{
  com32sys_t ireg, oreg;
  char *bufp = buf;
  size_t n = 0;

  (void)fp;

  memset(&ireg, 0, sizeof ireg); 

  while ( count ) {
    ireg.eax.b[1]  = 0x01;
    __intcall(0x21, &ireg, &oreg);
    *bufp++ = oreg.eax.b[0];
    n++;
    
    if ( ! --count )
      break;

    /* Only return more than one if there is stuff in the buffer */
    ireg.eax.b[1] = 0x0B;
    __intcall(0x21, &ireg, &oreg);
    if ( !oreg.eax.b[0] )
      break;
  }

  return n;
}
