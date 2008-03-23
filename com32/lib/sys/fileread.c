/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
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
 * read.c
 *
 * Reading from a file
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include "file.h"

int __file_get_block(struct file_info *fp)
{
  com32sys_t ireg, oreg;

  memset(&ireg, 0, sizeof ireg);
  ireg.eax.w[0] = 0x0007;	/* Read file */
  ireg.ebx.w[0] = OFFS(__com32.cs_bounce);
  ireg.es = SEG(__com32.cs_bounce);
  ireg.esi.w[0] = fp->i.filedes;
  ireg.ecx.w[0] = MAXBLOCK >> fp->i.blocklg2;

  __intcall(0x22, &ireg, &oreg);

  if ( oreg.eflags.l & EFLAGS_CF ) {
    errno = EIO;
    return -1;
  }

  fp->i.filedes = oreg.esi.w[0];
  fp->i.nbytes = oreg.ecx.l;
  fp->i.datap = fp->i.buf;
  memcpy(fp->i.buf, __com32.cs_bounce, fp->i.nbytes);

  return 0;
}

ssize_t __file_read(struct file_info *fp, void *buf, size_t count)
{
  char *bufp = buf;
  ssize_t n = 0;
  size_t ncopy;

  while ( count ) {
    if ( fp->i.nbytes == 0 ) {
      if ( fp->i.offset >= fp->i.length || !fp->i.filedes )
	return n;		/* As good as it gets... */

      if ( __file_get_block(fp) )
	return n ? n : -1;
    }

    ncopy = min(count, fp->i.nbytes);
    memcpy(bufp, fp->i.datap, ncopy);

    n += ncopy;
    bufp += ncopy;
    count -= ncopy;
    fp->i.datap += ncopy;
    fp->i.offset += ncopy;
    fp->i.nbytes -= ncopy;
  }

  return n;
}
