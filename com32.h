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


/*
 * This structure defines the register frame used by the
 * system call interface.
 *
 * The syscall interface is:
 *
 * __syscall(<interrupt #>, <source regs>, <return regs>)
 */
typedef struct {
  unsigned short gs;		/* Offset  0 */
  unsigned short fs;		/* Offset  2 */
  unsigned short es;		/* Offset  4 */
  unsigned short ds;		/* Offset  6 */

  unsigned int edi;		/* Offset  8 */
  unsigned int esi;		/* Offset 12 */
  unsigned int ebp;		/* Offset 16 */
  unsigned int _unused;		/* Offset 20 */
  unsigned int ebx;		/* Offset 24 */
  unsigned int edx;		/* Offset 28 */
  unsigned int ecx;		/* Offset 32 */
  unsigned int eax;		/* Offset 36 */

  unsigned int eflags;		/* Offset 40 */
} com32sys_t;

/* The standard prototype for _start() */
int _start(unsigned int __nargs,
	   char *__cmdline,
	   void (*__syscall)(unsigned char, com32sys_t *, com32sys_t *),
	   void *__bounce_ptr,
	   unsigned int __bounce_len);

#endif /* _COM32_H */
