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
 * memmap.c
 *
 * Create initial memory map for "shuffle and boot" operation
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <setjmp.h>
#include <string.h>

#include <syslinux/movebits.h>

static struct movelist *
new_movelist(addr_t src, addr_t len)
{
  struct movelist *ml = malloc(sizeof(struct movelist));

  if ( !ml )
    return NULL;

  ml->dst = (addr_t)0;
  ml->src = src;
  ml->len = len;
  ml->next = NULL;

  return ml;
}

struct e820_entry {
  uint64_t start;
  uint64_t len;
  uint32_t type;
};

struct movelist *syslinux_memory_map(void)
{
  static com32sys_t ireg, zireg;
  com32sys_t oreg;
  struct movelist *ml = NULL;
  struct movelist **mlp = &ml;
  struct movelist *me;
  struct e820_entry *e820buf = __com32.cs_bounce;
  uint64_t start, len, maxlen;
  int memfound = 0;

  /* Use INT 12h to get DOS memory above 0x7c00 */
  __intcall(0x12, &zireg, &oreg);
  if (oreg.eax.w[0] > 31 && oreg.eax.w[0] <= 640) {
    addr_t dosmem = (oreg.eax.w[0] << 10) - 0x7c00;
    me = new_movelist(0, 0x7c00, dosmem);
    if (!me)
      goto bail;
    *mlp = me;
    mlp = &me->next;
  }

  /* First try INT 15h AX=E820h */
  ireg.eax.l    = 0xe820;
  ireg.edx.l    = 0x534d4150;
  ireg.ebx.l    = 0;
  ireg.ecx.l    = 20;
  ireg.es       = SEG(e820buf);
  ireg.edi.w[0] = OFFS(e820buf);

  do {
    __intcall(0x15, &ireg, &oreg);

    if ((oreg.eflags.l & EFLAGS_CF) ||
	(oreg.eax.l != 0x534d4150) ||
	(oreg.ecx.l < 20))
      break;

    if (e820buf.type == 1) {
      start = e820buf.start;
      len = e820buf.len;

      if (start < 0x100000000ULL) {
	/* Don't rely on E820 being valid for low memory.  Doing so
	   could mean stuff like overwriting the PXE stack even when
	   using "keeppxe", etc. */
	if (start < 0x100000ULL) {
	  if (len > 0x100000ULL-start)
	    len -= 0x100000ULL-start;
	  else
	    len = 0;
	  start = 0x100000ULL;
	}

	maxlen = 0x100000000ULL-start;
	if (len > maxlen)
	  len = maxlen;

	if (len) {
	  me = new_movelist(0, (addr_t)start, (addr_t)len);
	  if (!me)
	    goto bail;
	  *mlp = me;
	  mlp = &me->next;
	  memfound = 1;
	}
      }
    }

    ireg.ebx.l = oreg.ebx.l;
  } while (oreg.ebx.l);

  if (memfound)
    return ml;

  /* Next try INT 15h AX=E801h */
  ireg.eax.w[0] = 0xe801;
  __intcall(0x15, &ireg, &oreg);

  if (!(oreg.eflags.l & EFLAGS_CF) && oreg.ecx.w[0]) {
    me = new_movelist(0, (addr_t)1 << 20, oreg.ecx.w[0] << 10);
    if (!me)
      goto bail;
    *mlp = me;
    mlp = &me->next;

    if (oreg.edx.w[0]) {
      me = new_movelist(0, (addr_t)16 << 20, oreg.edx.w[0] << 16);
      if (!me)
	goto bail;
      *mlp = me;
      mlp = &me->next;
    }

    return ml;
  }

  /* Finally try INT 15h AH=88h */
  ireg.eax.w[0] = 0x8800;
  if (!(oreg.eflags.l & EFLAGS_CF) && oreg.eax.w[0]) {
    me = new_movelist(0, (addr_t)1 << 20, oreg.eax.w[0] << 10);
    if (!me)
      goto bail;
    *mlp = me;
    mlp = &me->next;
  }

  return ml;

 bail:
  while (ml) {
    me = ml;
    ml = ml->next;
    free(ml);
  }
  return NULL;
}
