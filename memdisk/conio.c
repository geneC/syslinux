#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2001 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * conio.c
 *
 * Output to the screen
 */

#include <stdint.h>
#include "conio.h"

int putchar(int ch)
{
  if ( ch == '\n' )
    putchar('\r');		/* \n -> \r\n */

  asm volatile("int $0x10"
	       :: "a" ((uint16_t)(0x0e00|(ch&0xff))),
	       "b" ((uint16_t)0x07)
	       : "eax", "ebx", "ecx", "edx",
	       "esi", "edi", "ebp");

  return ch;
}

int puts(const char *s)
{
  int count = 0;

  while ( *s ) {
    putchar(*s);
    count++;
    s++;
  }

  return count;
}
