/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
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
 * shuffle_rm.c
 *
 * Shuffle and boot to real mode code
 */

#include <stdlib.h>
#include <inttypes.h>
#include <com32.h>
#include <string.h>
#include <syslinux/movebits.h>

int syslinux_shuffle_boot_rm(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap,
			     uint16_t bootflags,
			     uint32_t edx, uint32_t esi, uint16_t ds,
			     uint16_t cs, uint16_t ip)
{
  int nd;
  com32sys_t ireg;

  nd = syslinux_prepare_shuffle(fraglist, memmap);
  if (nd < 0)
    return -1;
  
  memset(&ireg, 0, sizeof ireg);
  
  ireg.eax.w[0] = 0x0012;
  ireg.edx.w[0] = bootflags;
  ireg.es       = SEG(__com32.cs_bounce);
  ireg.edi.l    = OFFS(__com32.cs_bounce);
  ireg.ecx.l    = nd;
  ireg.ebx.l    = edx;
  ireg.esi.l    = esi;
  ireg.ds       = ds;
  ireg.ebp.l    = (cs << 16) + ip;
  __intcall(0x22, &ireg, NULL);

  return -1;			/* Too many descriptors? */
}
