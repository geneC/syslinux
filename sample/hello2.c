#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2002 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * hello2.c
 *
 * Simple COM32 image
 *
 * This version shows how to use the bounce buffer for data transfer
 * to the BIOS or COMBOOT system calls.
 */

#include <com32.h>

#define NULL ((void *)0)

static inline void memset(void *buf, int ch, unsigned int len)
{
  asm volatile("cld; rep; stosb"
	       : "+D" (buf), "+c" (len) : "a" (ch) : "memory");
}

static inline void memcpy(void *dst, const void *src, unsigned int len)
{
  asm volatile("cld; rep; movsb"
	       : "+D" (dst), "+S" (src), "+c" (len) : : "memory");
}

int __start(void)
{
  const char msg[] = "Hello, World!\r\n";
  com32sys_t inreg, outreg;
  const char *p;

  memset(&inreg, 0, sizeof inreg);

  /* Bounce buffer is at least 64K in size */
  memcpy(__com32.cs_bounce, msg, sizeof msg);
  inreg.eax.w[0] = 0x0002;	/* Write string */
  inreg.ebx.w[0] = OFFS(__com32.cs_bounce);
  inreg.es       = SEG(__com32.cs_bounce);
  __com32.cs_syscall(0x22, &inreg, NULL);

  return 0;
}
