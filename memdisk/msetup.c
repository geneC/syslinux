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
#include "e820.h"

static inline int get_e820(void)
{
  struct e820_info {
    uint64_t base;
    uint64_t len;
    uint32_t type;
  } __attribute__((packed));
  struct e820_info buf;
  uint32_t lastptr = 0;
  int copied;
  int range_count;
  
  do {
    asm volatile("int $0x15 ; "
		 "jc 1f ; "
		 "cmpl $0x534d4150, %%eax ; "
		 "je 2f\n"
		 "1:\n\t"
		 "xorl %%ecx, %%ecx\n"
		 "2:"
		 : "=c" (copied), "=&b" (lastptr)
		 : "a" (0x0000e820), "d" (0x534d4150),
		 "c" (20), "D" (&buf)
		 : "esi", "ebp");

    if ( copied < 20 )
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

static inline int get_e881(void)
{
  uint32_t low_mem;
  uint32_t high_mem;
  uint8_t err;

  asm volatile("movw $0xe881, %%ax ; "
	       "int $0x15 ; "
	       "setc %2"
	       : "=a" (low_mem), "=b" (high_mem), "=d" (err)
	       :: "ecx", "esi", "edi", "ebp");

  if ( !err ) {
    if ( low_mem ) {
      insertrange(0x100000, (uint64_t)low_mem << 10, 1);
    }
    if ( high_mem ) {
      insertrange(0x1000000, (uint64_t)high_mem << 16, 1);
    }
  }

  return err;
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
    if ( get_e881() ) {
      if ( get_e801() ) {
	if ( get_88() ) {
	  /* Running out of ideas here... */
	}
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

extern const char _binary_memdisk_bin_start[], _binary_memdisk_bin_end[];
extern const char _binary_memdisk_bin_size[]; /* Weird, I know */
struct memdisk_header {
  uint16_t int13_offs;
  uint16_t int15_offs;
  uint16_t patch_offs;
  uint16_t total_size;
};
struct patch_area {
  uint8_t  driveno;
  uint8_t  drivetype;
  uint8_t  laststatus;
  uint8_t  _pad1;

  uint16_t cylinders;
  uint16_t heads;
  uint32_t sectors;
  uint32_t disksize;
  uint32_t diskbuf;

  uint32_t e820table;
  uint32_t mem1mb;
  uint32_t mem16mb;
  uint32_t memint1588;

  uint32_t oldint13;
  uint32_t oldint15;
  uint16_t olddosmem;
};

/* Access to objects in the zero page */
static inline void
wrz_8(uint32_t addr, uint8_t data)
{
  asm volatile("movb %0,%%fs:%1" :: "ri" (data), "m" (*(uint8_t *)addr));
}
static inline void
wrz_16(uint32_t addr, uint16_t data)
{
  asm volatile("movw %0,%%fs:%1" :: "ri" (data), "m" (*(uint16_t *)addr));
}
static inline void
wrz_32(uint32_t addr, uint16_t data)
{
  asm volatile("movl %0,%%fs:%1" :: "ri" (data), "m" (*(uint32_t *)addr));
}
static inline uint8_t
rdz_8(uint32_t addr)
{
  uint8_t data;
  asm volatile("movb %%fs:%1,%0" : "=r" (data) : "m" (*(uint8_t *)addr));
  return data;
}
static inline uint16_t
rdz_16(uint32_t addr)
{
  uint16_t data;
  asm volatile("movw %%fs:%1,%0" : "=r" (data) : "m" (*(uint16_t *)addr));
  return data;
}
static inline uint8_t
rdz_32(uint32_t addr)
{
  uint32_t data;
  asm volatile("movl %%fs:%1,%0" : "=r" (data) : "m" (*(uint32_t *)addr));
  return data;
}

/* Addresses in the zero page */
#define BIOS_BASEMEM	0x413	/* Amount of DOS memory */

void setup(void)
{
  unsigned int size = (int) &_binary_memdisk_bin_size;
  struct memdisk_header *hptr;
  struct patch_area *pptr;
  uint32_t old_dos_mem;

  /* Point %fs to the zero page */
  asm volatile("movw %0,%%fs" :: "r" (0));

  get_mem();
  parse_mem();

  /* Figure out where it needs to go */
  hptr = (struct memdisk_header *) &_binary_memdisk_bin_start;
  pptr = (struct patch_area *)(_binary_memdisk_bin_start + hptr->patch_offs);

  if ( hptr->total_size > dos_mem ) {
    /* Badness... */
  }

  old_dos_mem = dos_mem;

  dos_mem -= hptr->total_size;
  dos_mem &= ~0x3FF;

  /* Reserve this range of memory */
  insertrange(dos_mem, old_dos_mem-dos_mem, 2);
  parse_mem();

  pptr->mem1mb     = low_mem  >> 10;
  pptr->mem16mb    = high_mem >> 16;
  pptr->memint1588 = (low_mem == 0xf00000)
    ? ((high_mem > 0x30ffc00) ? 0xffff : (high_mem >> 10)+0x3c00)
    : (low_mem >> 10);
  pptr->olddosmem = rdz_16(BIOS_BASEMEM);
  wrz_16(BIOS_BASEMEM, dos_mem >> 10);

  /* ... patch other things ... */

  /* Copy the driver into place */
  asm volatile("pushw %%es ; "
	       "movw %0,%%es ; "
	       "rep ; movsl %%ds:(%%si), %%es:(%%di) ; "
	       "popw %%es"
	       :: "r" ((uint16_t)(dos_mem >> 4)),
	       "c" (size >> 2),
	       "S" (&_binary_memdisk_bin_start),
	       "D" (0));
}
