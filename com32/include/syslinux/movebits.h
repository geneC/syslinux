#ifndef _SYSLINUX_MOVEBITS_H
#define _SYSLINUX_MOVEBITS_H

#include <inttypes.h>
#include <setjmp.h>

typedef uint32_t addr_t;

struct syslinux_movelist {
  addr_t dst;
  addr_t src;
  addr_t len;
  struct syslinux_movelist *next;
};

struct syslinux_pm_regs {
  uint32_t eax, ecx, edx, ebx;
  uint32_t esp, ebp, esi, edi;
  uint32_t eip;
};

/*
 * moves is computed from "frags" and "freemem".  "space" lists
 * free memory areas at our disposal, and is (src, cnt) only.
 */

int syslinux_compute_movelist(struct syslinux_movelist **,
			      struct syslinux_movelist *,
			      struct syslinux_movelist *);

struct syslinux_movelist *syslinux_memory_map(void);
void syslinux_free_movelist(struct syslinux_movelist *);
int syslinux_add_movelist(struct syslinux_movelist **,
			  addr_t dst, addr_t src, addr_t len);
int syslinux_prepare_shuffle(struct syslinux_movelist *fraglist);
int syslinux_shuffle_boot_rm(struct syslinux_movelist *fraglist,
			     uint16_t bootflags,
			     uint32_t edx, uint32_t esi, uint16_t ds,
			     uint16_t cs, uint16_t ip);
int syslinux_shuffle_boot_pm(struct syslinux_movelist *fraglist,
			     uint16_t bootflags,
			     struct syslinux_pm_regs *regs);

#endif /* _SYSLINUX_MOVEBITS_H */
