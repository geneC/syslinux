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
 * hello.c
 *
 * Simple COM32 image
 */

#include <com32.h>

#define NULL ((void *)0)

static inline void memset(void *buf, int ch, unsigned int len)
{
  asm volatile("cld; rep; stosb"
	       : "+D" (buf), "+c" (len) : "a" (ch) : "memory");
}

int _start(unsigned int nargs, char *cmdline,
	   void (*syscall)(unsigned char, com32sys_t *, com32sys_t *),
	   void *bounce_ptr, unsigned int bounce_len)
{
  const char *msg = "Hello, World!\r\n";
  com32sys_t inreg, outreg;
  const char *p;

  memset(&inreg, 0, sizeof inreg);

  for ( p = msg ; *p ; p++ ) {
    inreg.edx.b[0] = *p;
    inreg.eax.b[1] = 0x02;	/* Write Character */
    syscall(0x21, &inreg, NULL);
  }

  return 0;
}
