/* ----------------------------------------------------------------------- *
 *   Not Copyright 2002 H. Peter Anvin
 *   This file is in the public domain.
 * ----------------------------------------------------------------------- */

/*
 * com32.h
 *
 * Common declarations for com32 programs.
 */

#ifndef _COM32_H
#define _COM32_H

#include <stdint.h>

/*
 * This structure defines the register frame used by the
 * system call interface.
 *
 * The syscall interface is:
 *
 * __syscall(<interrupt #>, <source regs>, <return regs>)
 */
typedef union {
  uint32_t l;
  uint16_t w[2];
  uint8_t  b[4];
} reg32_t;

typedef struct {
  uint16_t gs;			/* Offset  0 */
  uint16_t fs;			/* Offset  2 */
  uint16_t es;			/* Offset  4 */
  uint16_t ds;			/* Offset  6 */

  reg32_t edi;			/* Offset  8 */
  reg32_t esi;			/* Offset 12 */
  reg32_t ebp;			/* Offset 16 */
  reg32_t _unused;		/* Offset 20 */
  reg32_t ebx;			/* Offset 24 */
  reg32_t edx;			/* Offset 28 */
  reg32_t ecx;			/* Offset 32 */
  reg32_t eax;			/* Offset 36 */

  reg32_t eflags;		/* Offset 40 */
} com32sys_t;

extern struct com32_sys_args {
  uint32_t cs_sysargs;
  char *cs_cmdline;
  void (*cs_intcall)(uint8_t, com32sys_t *, com32sys_t *);
  void *cs_bounce;
  uint32_t cs_bounce_size;
  void (*cs_farcall)(uint32_t, com32sys_t *, com32sys_t *);
} __com32;

/*
 * These functions convert between linear pointers in the range
 * 0..0xFFFFF and real-mode style SEG:OFFS pointers.  Note that a
 * 32-bit linear pointer is not compatible with a SEG:OFFS pointer
 * stored in two consecutive 16-bit words.
 */
static inline uint16_t SEG(void *__p)
{
  return (uint16_t)(((uint32_t)__p) >> 4);
}

static inline uint16_t OFFS(void *__p)
{
  /* The double cast here is to shut up gcc */
  return (uint16_t)(uint32_t)__p & 0x000F;
}

static inline void *MK_PTR(uint16_t __seg, uint16_t __offs)
{
  return (void *)( ((uint32_t)__seg << 4) + (uint32_t)__offs );
}

#endif /* _COM32_H */
