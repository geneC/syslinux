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
 * msetup.c
 *
 * Initialization code for memory-based disk
 */

#include <stdint.h>
#include "memdisk.h"
#include "conio.h"
#include "e820.h"

static inline int get_e820(void)
{
  struct e820_info {
    uint64_t base;
    uint64_t len;
    uint32_t type;
  };
  struct e820_info buf;
  uint32_t lastptr = 0;
  uint32_t copied;
  int range_count = 0;
  uint32_t eax, edx;

  do {
    copied = sizeof(buf);
    eax = 0x0000e820;
    edx = 0x534d4150;
    
    asm volatile("int $0x15 ; "
		 "jnc 1f ; "
		 "xorl %0,%0\n"
		 "1:"
		 : "+c" (copied), "+b" (lastptr),
		 "+a" (eax), "+d" (edx)
		 : "D" (&buf)
		 : "esi", "ebp");
    
    if ( eax != 0x534d4150 || copied < 20 )
      break;
    
    insertrange(buf.base, buf.len, buf.type);
    range_count++;

  } while ( lastptr );

  return !range_count;
}

static inline void get_dos_mem(void)
{
  uint16_t dos_kb;

  asm volatile("int $0x12" : "=a" (dos_kb)
	       :: "ebx", "ecx", "edx", "esi", "edi", "ebp");

  insertrange(0, (uint64_t)((uint32_t)dos_kb << 10), 1);
}

static inline int get_e801(void)
{
  uint16_t low_mem;
  uint16_t high_mem;
  uint8_t err;

  asm volatile("movw $0xe801, %%ax ; "
	       "int $0x15 ; "
	       "setc %2"
	       : "=a" (low_mem), "=b" (high_mem), "=d" (err)
	       :: "ecx", "esi", "edi", "ebp");

  if ( !err ) {
    if ( low_mem ) {
      insertrange(0x100000, (uint64_t)((uint32_t)low_mem << 10), 1);
    }
    if ( high_mem ) {
      insertrange(0x1000000, (uint64_t)((uint32_t)high_mem << 16), 1);
    }
  }

  return err;
}

static inline int get_88(void)
{
  uint16_t low_mem;
  uint8_t err;

  asm volatile("movb $0x88,%%ah ; "
	       "int $0x15 ; "
	       "setc %1"
	       : "=a" (low_mem), "=d" (err)
	       :: "ebx", "ecx", "esi", "edi", "ebp");

  if ( !err ) {
    if ( low_mem ) {
      insertrange(0x100000, (uint64_t)((uint32_t)low_mem << 10), 1);
    }
  }

  return err;
}

uint32_t dos_mem  = 0;		/* 0-1MB */
uint32_t low_mem  = 0;		/* 1-16MB */
uint32_t high_mem = 0;		/* 16+ MB */

void get_mem(void)
{
  if ( get_e820() ) {
    get_dos_mem();
    if ( get_e801() ) {
      if ( get_88() ) {
	puts("MEMDISK: Unable to obtain memory map\n");
	die();
      }
    }
  }
}

#define PW(x) (1ULL << (x))

void parse_mem(void)
{
  struct e820range *ep;

  dos_mem = low_mem = high_mem = 0;

  /* Derive "dos mem", "high mem", and "low mem" from the range array */
  for ( ep = ranges ; ep->type != -1 ; ep++ ) {
    if ( ep->type == 1 ) {
      /* Only look at memory ranges */
      if ( ep->start == 0 ) {
	if ( ep[1].start > PW(20) )
	  dos_mem = PW(20);
	else
	  dos_mem = ep[1].start;
      }
      if ( ep->start <= PW(20) && ep[1].start > PW(20) ) {
	if ( ep[1].start > PW(24) )
	  low_mem = PW(24) - PW(20);
	else
	  low_mem = ep[1].start - PW(20);
      }
      if ( ep->start <= PW(24) && ep[1].start > PW(24) ) {
	if ( ep[1].start > PW(32) )
	  high_mem = PW(32) - PW(24);
	else
	  high_mem = ep[1].start - PW(24);
      }
    }
  }
}
