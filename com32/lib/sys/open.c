#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2003 H. Peter Anvin - All Rights Reserved
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

#include <errno.h>
#include <com32.h>
#include <string.h>
#include "file.h"

int open(const char *pathname, int flags, mode_t mode)
{
  com32sys_t regs;
  int fd;
  struct file_info *fp;
  
  if ( flags ) {
    errno = EINVAL;
    return -1;
  }

  for ( fd = 0, fp = __file_info ; fd < NFILES ; fd++, fp++ )
    if ( fp->blocklg2 == 0 )
      break;

  if ( fd >= NFILES ) {
    errno = EMFILE;
    return -1;
  }

  strlcpy(__com32.cs_bounce, pathname, __com32.cs_bounce_size);

  regs.eax.w[0] = 0x0006;
  regs.esi.w[0] = OFFS(__com32.cs_bounce);
  regs.es = SEG(__com32.cs_bounce);

  __com32.cs_intcall(0x22, &regs, &regs);
  
  if ( (regs.eflags.l & EFLAGS_CF) || regs.esi.w[0] == 0 ) {
    errno = ENOENT;
    return -1;
  }

  {
    uint16_t blklg2;
    asm("bsrw %1,%0" : "=r" (blklg2) : "rm" (regs.ecx.w[0]));
    fp->blocklg2 = blklg2;
  }
  fp->length    = regs.eax.l;
  fp->filedes   = regs.esi.w[0];
  fp->offset    = 0;
  fp->nbytes    = 0;
  fp->datap     = fp->buf;

  return fd;
} 
