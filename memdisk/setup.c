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

  uint32_t e820table;
  uint32_t mem1mb;
  uint32_t mem16mb;
  uint32_t memint1588;

  uint32_t oldint13;
  uint32_t oldint15;

  uint16_t olddosmem;

  uint8_t  driveno;
  uint8_t  drivetype;
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
 * Actual setup routine
 * Returns the drive number (which is then passed in %dl to the
 * called routine.)
 */
uint32_t setup(void)
{
  unsigned int size = (int) &_binary_memdisk_bin_size;
  struct memdisk_header *hptr;
  struct patch_area *pptr;
  uint16_t driverseg;
  uint32_t driverptr, driveraddr;
  uint8_t driveno = 0;
  uint8_t status;
  uint16_t exitcode;
  const struct geometry *geometry;

  /* Show signs of life */
  puts("Memdisk: hello, world!\n");

  for(;;);

  geometry = get_disk_image_geometry(shdr->ramdisk_image, shdr->ramdisk_size);

  get_mem();
  parse_mem();

  /* Figure out where it needs to go */
  hptr = (struct memdisk_header *) &_binary_memdisk_bin_start;
  pptr = (struct patch_area *)(_binary_memdisk_bin_start + hptr->patch_offs);

  if ( hptr->total_size > dos_mem ) {
    /* Badness... */
  }

  pptr->olddosmem = rdz_16(BIOS_BASEMEM);
  pptr->driveno   = geometry->driveno;
  pptr->drivetype = geometry->type;
  pptr->cylinders = geometry->c;
  pptr->heads     = geometry->h;
  pptr->sectors   = geometry->s;
  pptr->disksize  = geometry->sectors;
  pptr->diskbuf   = shdr->ramdisk_image;

  driveraddr  = dos_mem - hptr->total_size;
  driveraddr &= ~0x3FF;

  /* Reserve this range of memory */
  insertrange(driveraddr, dos_mem-driveraddr, 2);
  parse_mem();

  pptr->mem1mb     = low_mem  >> 10;
  pptr->mem16mb    = high_mem >> 16;
  pptr->memint1588 = (low_mem == 0xf00000)
    ? ((high_mem > 0x30ffc00) ? 0xffff : (high_mem >> 10)+0x3c00)
    : (low_mem >> 10);

  driverseg = driveraddr >> 4;
  driverptr = driverseg  << 16;

  pptr->oldint13 = rdz_32(BIOS_INT13);
  pptr->oldint15 = rdz_32(BIOS_INT15);

  /* Claim the memory and copy the driver into place */
  wrz_16(BIOS_BASEMEM, dos_mem >> 10);

  asm volatile("pushw %%es ; "
	       "movw %0,%%es ; "
	       "rep ; movsl %%ds:(%%si), %%es:(%%di) ; "
	       "popw %%es"
	       :: "r" (driverseg),
	       "c" (size >> 2),
	       "S" (&_binary_memdisk_bin_start),
	       "D" (0));

  /* Install the interrupt handlers */
  wrz_32(BIOS_INT13, driverptr+hptr->int13_offs);
  wrz_32(BIOS_INT15, driverptr+hptr->int15_offs);

  /* Reboot into the new "disk" */
  asm volatile("pushw %%es ; "
	       "xorw %%cx,%%cx ; "
	       "movw %%cx,%%es ; "
	       "incw %%cx ; "
	       "movw $0x0201,%%ax ; "
	       "movw $0x7c00,%%bx ; "
	       "int $0x13 ; "
	       "setc %0 ; "
	       "popw %%es"
	       : "=r" (status), "=a" (exitcode)
	       : "d" ((uint16_t)driveno)
	       : "ebx", "ecx", "edx", "esi", "edi", "ebp");

  if ( status ) {
    /* Badness... */
  }
  
  /* On return the assembly code will jump to the boot vector */
  return driveno;
}
