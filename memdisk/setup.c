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

#include <stdint.h>
#include "e820.h"
#include "conio.h"

extern const char _binary_memdisk_bin_start[], _binary_memdisk_bin_end[];
extern const char _binary_memdisk_bin_size[]; /* Weird, I know */

struct memdisk_header {
  uint16_t int13_offs;
  uint16_t int15_offs;
  uint16_t patch_offs;
  uint16_t total_size;
};

struct patch_area {
  uint16_t cylinders;
  uint16_t heads;
  uint32_t sectors;
  uint32_t disksize;
  uint32_t diskbuf;

  uint32_t mem1mb;
  uint32_t mem16mb;

  uint32_t oldint13;
  uint32_t oldint15;

  uint16_t memint1588;
  uint16_t olddosmem;

  uint8_t  driveno;
  uint8_t  drivetype;

  uint16_t mystack;
};

/* This is the header in the boot sector/setup area */
struct setup_header {
  uint8_t dummy[0x1f1];		/* Boot sector */
  uint8_t setup_secs;
  uint16_t syssize;
  uint16_t swap_dev;
  uint16_t ram_size;
  uint16_t vid_mode;
  uint16_t root_dev;
  uint16_t boot_flag;
  uint16_t jump;
  char header[4];
  uint16_t version;
  uint32_t realmode_swtch;
  uint32_t start_sys;
  uint8_t type_of_loader;
  uint8_t loadflags;
  uint16_t setup_move_size;
  uint32_t code32_start;
  uint32_t ramdisk_image;
  uint32_t ramdisk_size;
  uint32_t bootsect_kludge;
  uint16_t head_end_ptr;
  uint16_t pad1;
  uint32_t cmd_line_ptr;
  uint32_t initrd_addr_max;
};

const struct setup_header * const shdr = (struct setup_header *)0;

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
wrz_32(uint32_t addr, uint32_t data)
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
static inline uint32_t
rdz_32(uint32_t addr)
{
  uint32_t data;
  asm volatile("movl %%fs:%1,%0" : "=r" (data) : "m" (*(uint32_t *)addr));
  return data;
}

/* Addresses in the zero page */
#define BIOS_INT13	(0x13*4) /* INT 13h vector */
#define BIOS_INT15	(0x15*4) /* INT 13h vector */
#define BIOS_BASEMEM	0x413	 /* Amount of DOS memory */

/*
 * Figure out the "geometry" of the disk in question
 */
struct geometry {
  uint32_t size;		/* Size in bytes */
  uint32_t sectors;		/* 512-byte sector limit */
  uint32_t c, h, s;		/* C/H/S geometry */
  uint8_t type;		        /* Type byte for INT 13h AH=08h */
  uint8_t driveno;		/* Drive no */
};

static const uint32_t image_sizes[] =
{  360*1024,
   720*1024,
  1200*1024,
  1440*1024,
  2880*1024,
  0 };
static const struct geometry geometries[] =
{ {  360*1024,  720, 40,  2,  9, 0x01, 0 },
  {  720*1024, 1440, 80,  2,  9, 0x03, 0 },
  { 1200*1024, 2400, 80,  2, 15, 0x02, 0 },
  { 1440*1024, 2880, 80,  2, 18, 0x04, 0 },
  { 2880*1024, 5760, 80,  2, 36, 0x06, 0 } };
#define known_geometries (sizeof(geometries)/sizeof(struct geometry))

const struct geometry *get_disk_image_geometry(uint32_t where, uint32_t size)
{
  static struct geometry hd_geometry;
  int i;

  for ( i = 0 ; i < known_geometries ; i++ ) {
    if ( size == geometries[i].size ) {
      return &geometries[i];
    }
  }

  /* No match, must be a hard disk image */
  /* Need to examine the partition table for geometry */

  /* For now, though, just use a simple standard geometry CHS = Nx16x63 */

  hd_geometry.size    = size & ~0x1FF;
  hd_geometry.sectors = size >> 9;
  hd_geometry.c       = size/(16*63*512);
  hd_geometry.h       = 16;
  hd_geometry.s       = 63;
  hd_geometry.type    = 0;
  hd_geometry.driveno = 0x80;	/* Hard drive */

  return &hd_geometry;
}

/*
 * Jump here if all hope is gone...
 */
void __attribute__((noreturn)) die(void)
{
  asm volatile("sti");
  for(;;)
    asm volatile("hlt");
}

#define STACK_NEEDED	128	/* Number of bytes of stack */

/*
 * Actual setup routine
 * Returns the drive number (which is then passed in %dl to the
 * called routine.)
 */
uint32_t setup(void)
{
  unsigned int bin_size = (int) &_binary_memdisk_bin_size;
  struct memdisk_header *hptr;
  struct patch_area *pptr;
  uint16_t driverseg;
  uint32_t driverptr, driveraddr;
  uint16_t dosmem_k;
  uint32_t stddosmem;
  uint8_t driveno = 0;
  uint8_t status;
  uint16_t exitcode;
  const struct geometry *geometry;
  int total_size;

  /* Show signs of life */
  puts("Memdisk: Hello, World!\n");

  if ( !shdr->ramdisk_image || !shdr->ramdisk_size ) {
    puts("MEMDISK: No ramdisk image specified!\n");
    die();
  }

  printf("Test of simple printf...\n");
  printf("Ramdisk at 0x%08x, length 0x%08x\n",
	 shdr->ramdisk_image, shdr->ramdisk_size);

  geometry = get_disk_image_geometry(shdr->ramdisk_image, shdr->ramdisk_size);

  printf("Disk is %s, %u K, C/H/S = %u/%u/%u\n",
	 geometry->driveno ? "hard disk" : "floppy",
	 geometry->sectors >> 1,
	 geometry->c, geometry->h, geometry->s);


  puts("e820map_init ");
  e820map_init();		/* Initialize memory data structure */
  puts("get_mem ");
  get_mem();			/* Query BIOS for memory map */
  puts("parse_mem\n");
  parse_mem();			/* Parse memory map */

  printf("dos_mem  = %#10x (%u K)\n"
	 "low_mem  = %#10x (%u K)\n"
	 "high_mem = %#10x (%u K)\n",
	 dos_mem, dos_mem >> 10,
	 low_mem, low_mem >> 10,
	 high_mem, high_mem >> 10);

  /* Reserve the ramdisk memory */
  insertrange(shdr->ramdisk_image, shdr->ramdisk_size, 2);
  parse_mem();			/* Recompute variables */

  /* Figure out where it needs to go */
  hptr = (struct memdisk_header *) &_binary_memdisk_bin_start;
  pptr = (struct patch_area *)(_binary_memdisk_bin_start + hptr->patch_offs);

  dosmem_k = rdz_16(BIOS_BASEMEM);
  pptr->olddosmem = dosmem_k;
  stddosmem = dosmem_k << 10;

  pptr->driveno   = geometry->driveno;
  pptr->drivetype = geometry->type;
  pptr->cylinders = geometry->c;
  pptr->heads     = geometry->h;
  pptr->sectors   = geometry->s;
  pptr->disksize  = geometry->sectors;
  pptr->diskbuf   = shdr->ramdisk_image;

  /* The size is given by hptr->total_size plus the size of the
     E820 map -- 12 bytes per range; we may need as many as
     2 additional ranges plus the terminating range, over what
     nranges currently show. */

  total_size = hptr->total_size + (nranges+3)*12 + STACK_NEEDED;
  printf("Total size needed = %u bytes\n", total_size);

  if ( total_size > dos_mem ) {
    puts("MEMDISK: Insufficient low memory\n");
    die();
  }

  driveraddr  = stddosmem - total_size;
  driveraddr &= ~0x3FF;

  printf("Old dos memory at 0x%05x (map says 0x%05x), loading at 0x%05x\n",
	 stddosmem, dos_mem, driveraddr);

  /* Reserve this range of memory */
  wrz_16(BIOS_BASEMEM, driveraddr >> 10);
  insertrange(driveraddr, dos_mem-driveraddr, 2);
  parse_mem();

  pptr->mem1mb     = low_mem  >> 10;
  pptr->mem16mb    = high_mem >> 16;
  if ( low_mem == (15 << 20) ) {
    /* lowmem maxed out */
    uint32_t int1588mem = (high_mem >> 10)+(low_mem >> 10);
    pptr->memint1588 = (int1588mem > 0xffff) ? 0xffff: int1588mem;
  } else {
    pptr->memint1588 = low_mem >> 10;
  }

  printf("mem1mb  = %5u (0x%04x)\n", pptr->mem1mb, pptr->mem1mb);
  printf("mem16mb = %5u (0x%04x)\n", pptr->mem16mb, pptr->mem16mb);
  printf("mem1588 = %5u (0x%04x)\n", pptr->memint1588, pptr->memint1588);

  driverseg = driveraddr >> 4;
  driverptr = driverseg  << 16;

  /* Anything beyond the end is for the stack */
  pptr->mystack    = (uint16_t)(stddosmem-driveraddr);

  pptr->oldint13 = rdz_32(BIOS_INT13);
  pptr->oldint15 = rdz_32(BIOS_INT15);

  /* Adjust the E820 table: if there are null ranges (type 0)
     at the end, change them to type end of list (-1).
     This is necessary for the driver to be able to report end
     of list correctly. */
  while ( nranges && ranges[nranges-1].type == 0 ) {
    ranges[--nranges].type = -1;
  }

  /* Dump the ranges table for debugging */
  {
    uint32_t *r = (uint32_t *)&ranges;
    while ( 1 ) {
      printf("%08x%08x %d\n", r[1], r[0], r[2]);
      if ( r[2] == (uint32_t)-1 )
	break;
      r += 3;
    }
  }

  /* Copy driver followed by E820 table */
  asm volatile("pushw %%es ; "
	       "movw %0,%%es ; "
	       "cld ; "
	       "rep ; movsl %%ds:(%%si), %%es:(%%di) ; "
	       "movw %1,%%cx ; "
	       "movw %2,%%si ; "
	       "rep ; movsl %%ds:(%%si), %%es:(%%di) ; "
	       "popw %%es"
	       :: "r" (driverseg),
	       "r" ((uint16_t)((nranges+1)*3)), /* 3 dwords/range */
	       "r" ((uint16_t)&ranges),
	       "c" (bin_size >> 2),
	       "S" (&_binary_memdisk_bin_start),
	       "D" (0)
	       : "esi", "edi", "ecx");

  /* Install the interrupt handlers */
  printf("old: int13 = %08x  int15 = %08x\n",
	 rdz_32(BIOS_INT13), rdz_32(BIOS_INT15));

  wrz_32(BIOS_INT13, driverptr+hptr->int13_offs);
  wrz_32(BIOS_INT15, driverptr+hptr->int15_offs);

  printf("new: int13 = %08x  int15 = %08x\n",
	 rdz_32(BIOS_INT13), rdz_32(BIOS_INT15));

  /* Reboot into the new "disk" */
  asm volatile("pushw %%es ; "
	       "xorw %%cx,%%cx ; "
	       "movw %%cx,%%es ; "
	       "incw %%cx ; "
	       "movw $0x0201,%%ax ; "
	       "movw $0x7c00,%%bx ; "
	       "int $0x13 ; "
	       "popw %%es ; "
	       "setc %0 "
	       : "=rm" (status), "=a" (exitcode)
	       : "d" ((uint16_t)driveno)
	       : "ebx", "ecx", "edx", "esi", "edi", "ebp");

  if ( status ) {
    puts("MEMDISK: Failed to load new boot sector\n");
    die();
  }
  
  puts("Booting...\n");

  /* On return the assembly code will jump to the boot vector */
  return driveno;
}
