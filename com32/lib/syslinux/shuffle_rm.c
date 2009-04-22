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

enum gpr_index { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum seg_index { R_ES, R_CS, R_SS, R_DS, R_FS, R_GS };

#define MOV_TO_SEG(S,R) (0xc08e + ((R) << 8) + ((S) << 11))
#define MOV_TO_R32(R)   (0xb866 + ((R) << 8))

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
  uint8_t handoff_code[8+5*5+8*6+1+5], *p;
  struct syslinux_memmap *tmap;
  addr_t regstub, stublen;
  /* Assign GPRs for each sreg, don't use AX and SP */
  static const uint8_t gpr_for_seg[6] = { R_CX, R_DX, R_BX, R_BP, R_SI, R_DI };

  tmap = syslinux_target_memmap(fraglist, memmap);
  if (!tmap)
    return -1;

  /*
   * Search for a good place to put the real-mode register stub.
   * We prefer it as low as possible above 0x800.  KVM barfs horribly
   * if we're not aligned to a paragraph boundary, so set the alignment
   * appropriately.
   */
  regstub = 0x800;
  stublen = sizeof handoff_code;
  rv = syslinux_memmap_find(tmap, SMT_FREE, &regstub, &stublen, 16);

  if (rv || (regstub > 0x100000 - sizeof handoff_code)) {
    /*
     * Uh-oh.  This isn't real-mode accessible memory.
     * It might be possible to do something insane here like
     * putting the stub in the IRQ vectors, or in the 0x5xx segment.
     * This code tries the 0x510-0x7ff range and hopes for the best.
     */
    regstub = 0x510;		/* Try the 0x5xx segment... */
    stublen = sizeof handoff_code;
    rv = syslinux_memmap_find(tmap, SMT_FREE, &regstub, &stublen, 16);

    if (!rv && (regstub > 0x100000 - sizeof handoff_code))
      rv = -1;			/* No acceptable memory found */
  }

  syslinux_free_memmap(tmap);
  if (rv)
    return -1;

  /* Build register-setting stub */
  p = handoff_code;
  rp = (const struct syslinux_rm_regs_alt *)regs;

  /* Set up GPRs with segment registers - don't use AX */
  for (i = 0; i < 6; i++) {
    if (i != R_CS) {
      p[0] = 0xb8 + gpr_for_seg[i];	/* MOV reg,imm16 */
      *(uint16_t *)(p+1) = rp->seg[i];
      p += 3;
    }
  }

  /* Actual transition to real mode */
  *(uint32_t *)p     = 0xeac0220f;	/* MOV CR0,EAX; JMP FAR */
  *(uint16_t *)(p+4) = (p-handoff_code)+8;	/* Offset */
  *(uint16_t *)(p+6) = regstub >> 4;		/* Segment */
  p += 8;

  /* Load SS and ESP immediately */
  *(uint16_t *)p     = MOV_TO_SEG(R_SS, R_BX);
  *(uint16_t *)(p+2) = MOV_TO_R32(R_SP);
  *(uint32_t *)(p+4) = rp->seg[R_SP];
  p += 8;

  /* Load the other segments */
  *(uint16_t *)p = MOV_TO_SEG(R_ES, R_CX);
  p += 2;
  *(uint16_t *)p = MOV_TO_SEG(R_DS, R_BP);
  p += 2;
  *(uint16_t *)p = MOV_TO_SEG(R_FS, R_SI);
  p += 2;
  *(uint16_t *)p = MOV_TO_SEG(R_GS, R_DI);
  p += 2;

  for (i = 0; i < 8; i++) {
    if (i != R_SP) {
      *(uint16_t *)p = MOV_TO_R32(i);	/* MOV r32,imm32 */
      *(uint32_t *)(p+2) = rp->gpr[i];
      p += 6;
    }
  }
  *p++ = rp->sti ? 0xfb : 0xfa;		/* STI/CLI */

  *p++ = 0xea;				/* JMP FAR */
  *(uint32_t *)p = rp->csip;

  /* Add register-setting stub to shuffle list */
  if (syslinux_add_movelist(&fraglist, regstub, (addr_t)handoff_code,
			    sizeof handoff_code))
    return -1;

  return syslinux_do_shuffle(fraglist, memmap, regstub, 0, bootflags);
}
