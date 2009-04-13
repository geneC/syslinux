/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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
 * Shuffle and boot to protected mode code
 */

#include <stdlib.h>
#include <inttypes.h>
#include <com32.h>
#include <string.h>
#include <syslinux/movebits.h>
#include <syslinux/bootrm.h>

int syslinux_shuffle_boot_rm(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap,
			     uint16_t bootflags,
			     struct syslinux_rm_regs *regs)
{
  const struct syslinux_rm_regs_alt {
    uint16_t seg[6];
    uint32_t gpr[8];
    uint32_t csip;
    bool sti;
  } *rp;
  int i, rv;
  uint8_t handoff_code[5*5+8*6+1+5], *p;
  struct syslinux_memmap *tmap;
  addr_t regstub, stublen;

  tmap = syslinux_target_memmap(fraglist, memmap);
  if (!tmap)
    return -1;

  /* Search for a good place to put the real-mode register stub.
     We prefer it as low as possible above 0x800. */
  regstub = 0x800;
  stublen = sizeof handoff_code;
  rv = syslinux_memmap_find(tmap, SMT_FREE, &regstub, &stublen);

  if (rv || (regstub > 0x100000 - sizeof handoff_code)) {
    /*
     * Uh-oh.  This isn't real-mode accessible memory.
     * It might be possible to do something insane here like
     * putting the stub in the IRQ vectors, or in the 0x5xx segment.
     * This code tries the 0x510-0x7ff range and hopes for the best.
     */
    regstub = 0x510;		/* Try the 0x5xx segment... */
    stublen = sizeof handoff_code;
    rv = syslinux_memmap_find(tmap, SMT_FREE, &regstub, &stublen);

    if (!rv && (regstub > 0x100000 - sizeof handoff_code))
      rv = -1;			/* No acceptable memory found */
  }

  syslinux_free_memmap(tmap);
  if (rv)
    return -1;

  /* Build register-setting stub */
  p = handoff_code;
  rp = (const struct syslinux_rm_regs_alt *)regs;
  for (i = 0; i < 6; i++) {
    if (i != 1) {		/* Skip CS */
      p[0] = 0xb8;		/* MOV AX,imm16 */
      *(uint16_t *)(p+1) = rp->seg[i];
      *(uint16_t *)(p+3) = 0xc08e + (i << 11); /* MOV seg,AX */
      p += 5;
    }
  }
  for (i = 0; i < 8; i++) {
    p[0] = 0x66;		/* MOV exx,imm32 */
    p[1] = 0xb8 + i;
    *(uint32_t *)(p+2) = rp->gpr[i];
    p += 6;
  }
  *p++ = rp->sti ? 0xfb : 0xfa;	/* STI/CLI */

  *p++ = 0xea;			/* JMP FAR */
  *(uint32_t *)p = rp->csip;

  /* Add register-setting stub to shuffle list */
  if (syslinux_add_movelist(&fraglist, regstub, (addr_t)handoff_code,
			    sizeof handoff_code))
    return -1;

  /* Convert regstub to a CS:IP entrypoint pair */
  regstub = (SEG((void *)regstub) << 16) + OFFS((void *)regstub);

  return syslinux_do_shuffle(fraglist, memmap, regstub, 0, bootflags);
}
