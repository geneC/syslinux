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
  
  do {
    puts("Calling INT 15 E820...\n");
    asm volatile("int $0x15 ; "
		 "jc 1f ; "
		 "cmpl $0x534d4150, %%eax ; "
		 "je 2f\n"
		 "1:\n\t"
		 "xorl %0,%0\n"
		 "2:"
		 : "=c" (copied), "+b" (lastptr)
		 : "a" (0x0000e820), "d" (0x534d4150),
		 "c" (sizeof(buf)), "D" (&buf)
		 : "eax", "edx", "esi", "edi", "ebp");

    if ( copied < 20 )
      break;

    printf("BIOS e820: %08x%08x %08x%08x %u\n",
	   (uint32_t)(buf.base >> 32),
	   (uint32_t)buf.base,
	   (uint32_t)(buf.len >> 32),
	   (uint32_t)buf.len,
	   buf.type);

    insertrange(buf.base, buf.len, buf.type);
    range_count++;

  } while ( lastptr );

  return !range_count;
}

static inline void get_dos_mem(void)
{
  uint16_t dos_kb;

  puts("Calling INT 12...\n");
  asm volatile("int $0x12" : "=a" (dos_kb)
	       :: "ebx", "ecx", "edx", "esi", "edi", "ebp");

  printf("BIOS 12:   %u K DOS memory\n", dos_kb);
  
  insertrange(0, (uint64_t)((uint32_t)dos_kb << 10), 1);
}

static inline int get_e801(void)
{
  uint16_t low_mem;
  uint16_t high_mem;
  uint8_t err;

  puts("Calling INT 15 E801...\n");
  asm volatile("movw $0xe801, %%ax ; "
	       "int $0x15 ; "
	       "setc %2"
	       : "=a" (low_mem), "=b" (high_mem), "=d" (err)
	       :: "ecx", "esi", "edi", "ebp");

  if ( !err ) {
    printf("BIOS e801: %u K low mem, %u K high mem\n",
	   low_mem, high_mem << 6);

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

  puts("Calling INT 15 88...\n");
  asm volatile("movb $0x88,%%ah ; "
	       "int $0x15 ; "
	       "setc %1"
	       : "=a" (low_mem), "=d" (err)
	       :: "ebx", "ecx", "esi", "edi", "ebp");

  if ( !err ) {
    printf("BIOS 88:    %u K extended memory\n", low_mem);

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

void parse_mem(void)
{
  struct e820range *ep;

  /* Derive "dos mem", "high mem", and "low mem" from the range array */
  for ( ep = ranges ; ep->type != -1 ; ep++ ) {
    if ( ep->type == 1 ) {
      /* Only look at memory ranges */
      if ( ep->start == 0 ) {
	if ( ep[1].start > 0x100000 )
	  dos_mem = 0x100000;
	else
	  dos_mem = ep[1].start;
      }
      if ( ep->start <= 0x100000 && ep[1].start > 0x100000 ) {
	if ( ep[1].start > 0x1000000 )
	  low_mem = 0x1000000 - ep->start;
	else
	  low_mem = ep[1].start - ep->start;
      }
      if ( ep->start <= 0x1000000 && ep[1].start > 0x1000000 ) {
	if ( ep[1].start > 0x100000000 )
	  high_mem = 0x100000000 - ep->start;
	else
	  high_mem = ep[1].start - ep->start;
      }
    }
  }
}
