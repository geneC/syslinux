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

/* The standard prototype for _start() */
int _start(unsigned int __nargs,
	   char *__cmdline,
	   void (*__syscall)(uint8_t, com32sys_t *, com32sys_t *),
	   void *__bounce_ptr,
	   unsigned int __bounce_len);

#endif /* _COM32_H */
