/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
  int nd;
  com32sys_t ireg;
  char *regbuf;
  const struct syslinux_rm_regs_alt {
    uint16_t seg[6];
    uint32_t gpr[8];
    uint32_t csip;
  } *rp;
  int i, rv;
  uint8_t handoff_code[5*5+8*6+5], *p;
  struct syslinux_memmap *tmap, *tp;
  addr_t regstub;

  tmap = syslinux_target_memmap(fraglist, memmap);
  if (!tmap)
    return -1;

  /* Search for a good place to put the real-mode register stub.
     We prefer to put it as high as possible in the low 640K. */
  regstub = 0;
  for (tp = tmap; tp->type != SMT_END; tp = tp->next) {
    addr_t xend, xlen;
    if (tp->start >= 640*1024)
      continue;
    if (tp->type != SMT_FREE)
      continue;
    xend = tp->next->start;
    if (xend > 640*1024)
      xend = 640*1024;
    xlen = xend - tp->start;
    if (xlen < sizeof handoff_code)
      continue;
    regstub = xend - sizeof handoff_code; /* Best alternative so far */
  }

  syslinux_free_memmap(tmap);

  /* XXX: it might be possible to do something insane here like
     putting the stub in the IRQ vectors... */
  if (!regstub)
    return -1;			/* No space at all */

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
