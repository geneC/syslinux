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

#define COPYYEAR "2001"

extern const char _binary_memdisk_bin_start[], _binary_memdisk_bin_end[];
extern const char _binary_memdisk_bin_size[]; /* Weird, I know */

struct memdisk_header {
  uint16_t int13_offs;
  uint16_t int15_offs;
  uint16_t patch_offs;
  uint16_t total_size;
};

/* The Disk Parameter Table may be required */
typedef union {
  struct hd_dpt {
    uint16_t max_cyl;		/* Max cylinder */
    uint8_t max_head;		/* Max head */
    uint8_t junk1[5];		/* Obsolete junk, leave at zero */
    uint8_t ctrl;		/* Control byte */
    uint8_t junk2[7];		/* More obsolete junk */
  } hd;
  struct fd_dpt {
    uint8_t specify1;		/* "First specify byte" */
    uint8_t specify2;		/* "Second specify byte" */
    uint8_t delay;		/* Delay until motor turn off */
    uint8_t sectors;		/* Sectors/track */

    uint8_t bps;		/* Bytes/sector (02h = 512) */
    uint8_t isgap;		/* Length of intersector gap */
    uint8_t dlen;		/* Data length (0FFh) */
    uint8_t fgap;		/* Formatting gap */

    uint8_t ffill;		/* Format fill byte */
    uint8_t settle;		/* Head settle time (ms) */
    uint8_t mstart;		/* Motor start time */
    uint8_t _pad1;		/* Padding */

    uint32_t old_fd_dpt;	/* Extension: pointer to old INT 1Eh */
  } fd;
} dpt_t;

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
  uint8_t  drivecnt;
  uint8_t  _pad1;

  uint16_t mystack;
  uint16_t statusptr;

  dpt_t dpt;
};

/* This is the header in the boot sector/setup area */
struct setup_header {
  char cmdline[0x1f1];
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

/* Access to high memory */

struct high_mover {
  uint32_t resv1[4];		/* For the BIOS */
  uint16_t src_limit;		/* 0xffff */
  uint16_t src01;		/* Bytes 0-1 of src */
  uint8_t src2;			/* Byte 2 of src */
  uint8_t src_perms;		/* 0x93 */
  uint8_t src_xperms;		/* 0x00 */
  uint8_t src3;			/* Byte 3 of src */
  uint16_t dst_limit;		/* 0xffff */
  uint16_t dst01;		/* Bytes 0-1 of dst */
  uint8_t dst2;			/* Byte 2 of dst */
  uint8_t dst_perms;		/* 0x93 */
  uint8_t dst_xperms;		/* 0x00 */
  uint8_t dst3;			/* Byte 3 of dst */
  uint32_t resv2[4];		/* For the BIOS */
};

/* Note: this version of high_bcopy() is limited to 64K */
static void high_bcopy(uint32_t dst, uint32_t src, uint16_t len)
{
  static struct high_mover high_mover = 
  {
    { 0, 0, 0, 0 },
    0xffff, 0, 0, 0x93, 0x00, 0,
    0xffff, 0, 0, 0x93, 0x00, 0,
    { 0, 0, 0, 0 }
  };
  
  high_mover.src01 = (uint16_t)src;
  high_mover.src2  = src >> 16;
  high_mover.src3  = src >> 24;

  high_mover.dst01 = (uint16_t)dst;
  high_mover.dst2  = dst >> 16;
  high_mover.dst3  = dst >> 24;

  asm volatile("pushfl ; movb $0x87,%%ah ; int $0x15 ; popfl"
	       :: "S" (&high_mover), "c" (len >> 1)
	       : "eax", "ebx", "ecx", "edx",
	       "ebp", "esi", "edi", "memory");
}

#define LOWSEG 0x0800		/* Should match init.S16 */

static inline uint32_t
ptr2linear(void *ptr)
{
  return (LOWSEG << 4) + (uint32_t)ptr;
}

static inline void
copy_to_high(uint32_t dst, void *src, uint16_t len)
{
  high_bcopy(dst, ptr2linear(src), len);
}

static inline void
copy_from_high(void *dst, uint32_t src, uint16_t len)
{
  high_bcopy(ptr2linear(dst), src, len);
}

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
#define BIOS_INT15	(0x15*4) /* INT 15h vector */
#define BIOS_INT1E      (0x1E*4) /* INT 1Eh vector */
#define BIOS_INT40	(0x40*4) /* INT 13h vector */
#define BIOS_INT41      (0x41*4) /* INT 41h vector */
#define BIOS_INT46      (0x46*4) /* INT 46h vector */
#define BIOS_BASEMEM	0x413	 /* Amount of DOS memory */
#define BIOS_EQUIP	0x410	 /* BIOS equipment list */
#define BIOS_HD_COUNT   0x475	 /* Number of hard drives present */

/*
 * Routine to seek for a command-line item and return a pointer
 * to the data portion, if present
 */

/* Magic return values */
#define CMD_NOTFOUND   ((char *)-1) /* Not found */
#define CMD_BOOL       ((char *)-2) /* Found boolean option */
#define CMD_HASDATA(X) ((int)(X) >= 0)

const char *getcmditem(const char *what)
{
  const char *p;
  const char *wp = what;
  int match = 0;

  for ( p = shdr->cmdline ; *p ; p++ ) {
    switch ( match ) {
    case 0:			/* Ground state */
      if ( *p == ' ' )
	break;

      wp = what;
      match = 1;
      /* Fall through */

    case 1:			/* Matching */
      if ( *wp == '\0' ) {
	if ( *p == '=' )
	  return p+1;
	else if ( *p == ' ' )
	  return CMD_BOOL;
	else {
	  match = 2;
	  break;
	}
      }
      if ( *p != *wp++ )
	match = 2;
      break;

    case 2:			/* Mismatch, skip rest of option */
      if ( *p == ' ' )
	match = 0;		/* Next option */
      break;
    }
  }
    
  /* Check for matching string at end of line */
  if ( match == 1 && *wp == '\0' )
    return CMD_BOOL;
  
  return CMD_NOTFOUND;
}

/*
 * Figure out the "geometry" of the disk in question
 */
struct geometry {
  uint32_t sectors;		/* 512-byte sector count */
  uint32_t c, h, s;		/* C/H/S geometry */
  uint8_t type;		        /* Type byte for INT 13h AH=08h */
  uint8_t driveno;		/* Drive no */
};

static const struct geometry geometries[] =
{ 
  {  720, 40,  2,  9, 0x01, 0 }, /*  360 K */
  { 1440, 80,  2,  9, 0x03, 0 }, /*  720 K*/
  { 2400, 80,  2, 15, 0x02, 0 }, /* 1200 K */
  { 2880, 80,  2, 18, 0x04, 0 }, /* 1440 K */
  { 5760, 80,  2, 36, 0x06, 0 }, /* 2880 K */
};
#define known_geometries (sizeof(geometries)/sizeof(struct geometry))

/* Format of a DOS partition table entry */
struct ptab_entry {
  uint8_t active;
  uint8_t start_h, start_s, start_c;
  uint8_t type;
  uint8_t end_h, end_s, end_c;
  uint32_t start;
  uint32_t size;
};

const struct geometry *get_disk_image_geometry(uint32_t where, uint32_t size)
{
  static struct geometry hd_geometry = { 0, 0, 0, 0, 0, 0x80 };
  struct ptab_entry ptab[4];	/* Partition table buffer */
  unsigned int sectors, v;
  unsigned int max_c, max_h, max_s;
  unsigned int c, h, s;
  int i;
  const char *p;

  printf("command line: %s\n", shdr->cmdline);

  if ( size & 0x1ff ) {
    puts("MEMDISK: Image has fractional end sector\n");
    size &= ~0x1ff;
  }

  sectors = size >> 9;
  for ( i = 0 ; i < known_geometries ; i++ ) {
    if ( sectors == geometries[i].sectors ) {
      hd_geometry = geometries[i];
      break;
    }
  }

  hd_geometry.sectors = sectors;

  if ( CMD_HASDATA(p = getcmditem("c")) && (v = atou(p)) )
    hd_geometry.c = v;
  if ( CMD_HASDATA(p = getcmditem("h")) && (v = atou(p)) )
    hd_geometry.h = v;
  if ( CMD_HASDATA(p = getcmditem("s")) && (v = atou(p)) )
    hd_geometry.s = v;
  
  if ( getcmditem("floppy") != CMD_NOTFOUND ) {
    hd_geometry.driveno = 0;
    if ( hd_geometry.type == 0 )
      hd_geometry.type = 0x10;	/* ATAPI floppy, e.g. LS-120 */
  }
  if ( getcmditem("harddisk") != CMD_NOTFOUND ) {
    hd_geometry.driveno = 0x80;
    hd_geometry.type = 0;
  }

  if ( (hd_geometry.c == 0) || (hd_geometry.h == 0) ||
       (hd_geometry.s == 0) ) {
    /* Hard disk image, need to examine the partition table for geometry */
    copy_from_high(&ptab, where+(512-2-4*16), sizeof ptab);
    
    max_c = max_h = 0;  max_s = 1;
    for ( i = 0 ; i < 4 ; i++ ) {
      if ( ptab[i].type ) {
	c = ptab[i].start_c + (ptab[i].start_s >> 6);
	s = (ptab[i].start_s & 0x3f);
	h = ptab[i].start_h;
	
	if ( max_c < c ) max_c = c;
	if ( max_h < h ) max_h = h;
	if ( max_s < s ) max_s = s;
	
	c = ptab[i].end_c + (ptab[i].end_s >> 6);
	s = (ptab[i].end_s & 0x3f);
	h = ptab[i].end_h;
	
	if ( max_c < c ) max_c = c;
	if ( max_h < h ) max_h = h;
	if ( max_s < s ) max_s = s;
      }
    }
    
    max_c++; max_h++;		/* Convert to count (1-based) */
    
    if ( !hd_geometry.h )
      hd_geometry.h = max_h;
    if ( !hd_geometry.s )
      hd_geometry.s = max_s;
    if ( !hd_geometry.c )
      hd_geometry.c = sectors/(hd_geometry.h*hd_geometry.s);
  }

  if ( sectors % (hd_geometry.h*hd_geometry.s) ) {
    puts("MEMDISK: Image seems to have fractional end cylinder\n");
  }
  if ( (hd_geometry.c*hd_geometry.h*hd_geometry.s) > sectors ) {
    puts("MEMDISK: Image appears to be truncated\n");
  }

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
  uint8_t status;
  uint16_t exitcode;
  const struct geometry *geometry;
  int total_size;

  /* Show signs of life */
  puts("MEMDISK " VERSION " " DATE
       "  Copyright " COPYYEAR " H. Peter Anvin\n");

  if ( !shdr->ramdisk_image || !shdr->ramdisk_size ) {
    puts("MEMDISK: No ramdisk image specified!\n");
    die();
  }

  printf("Ramdisk at 0x%08x, length 0x%08x\n",
	 shdr->ramdisk_image, shdr->ramdisk_size);

  geometry = get_disk_image_geometry(shdr->ramdisk_image, shdr->ramdisk_size);

  printf("Disk is %s, %u K, C/H/S = %u/%u/%u\n",
	 geometry->driveno ? "hard disk" : "floppy",
	 geometry->sectors >> 1,
	 geometry->c, geometry->h, geometry->s);

  e820map_init();		/* Initialize memory data structure */
  get_mem();			/* Query BIOS for memory map */
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
  pptr->statusptr = (geometry->driveno & 0x80) ? 0x474 : 0x441;

  /* Set up a drive parameter table */
  if ( geometry->driveno & 0x80 ) {
    /* Hard disk */
    pptr->dpt.hd.max_cyl  = geometry->c-1;
    pptr->dpt.hd.max_head = geometry->h-1;
    pptr->dpt.hd.ctrl     = (geometry->h > 8) ? 0x08: 0;
  } else {
    /* Floppy - most of these fields are bogus and mimic
       a 1.44 MB floppy drive */
    pptr->dpt.fd.specify1 = 0xdf;
    pptr->dpt.fd.specify2 = 0x02;
    pptr->dpt.fd.delay    = 0x25;
    pptr->dpt.fd.sectors  = geometry->s;
    pptr->dpt.fd.bps      = 0x02;
    pptr->dpt.fd.isgap    = 0x12;
    pptr->dpt.fd.dlen     = 0xff;
    pptr->dpt.fd.fgap     = 0x6c;
    pptr->dpt.fd.ffill    = 0xf6;
    pptr->dpt.fd.settle   = 0x0f;
    pptr->dpt.fd.mstart   = 0x05;

    pptr->dpt.fd.old_fd_dpt = rdz_32(BIOS_INT1E);
  }

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
    pptr->memint1588 = (int1588mem > 0xffff) ? 0xffff : int1588mem;
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

  /* Query drive parameters of this type */
  {
    uint16_t bpt_es, bpt_di;
    uint8_t cf, dl;

    asm volatile("pushw %%es ; "
		 "xorw %1,%1 ; "
		 "movw %1,%%es ; "
		 "movb $0x08,%%ah ; "
		 "int $0x13 ; "
		 "setc %2 ; "
		 "movw %%es,%0 ;"
		 "popw %%es"
		 : "=a" (bpt_es), "=D" (bpt_di),
		 "=c" (cf), "=d" (dl)
		 : "d" (geometry->driveno & 0x80)
		 : "esi", "ebx", "ebp");

    if ( cf ) {
      printf("INT 13 08: Failure\n");
      pptr->drivecnt = 1;
    } else {
      printf("INT 13 08: Success, count = %u, BPT = %04x:%04x\n",
	     dl, bpt_es, bpt_di);
      pptr->drivecnt = dl+1;
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
  {
    printf("old: int13 = %08x  int15 = %08x\n",
	   rdz_32(BIOS_INT13), rdz_32(BIOS_INT15));

    wrz_32(BIOS_INT13, driverptr+hptr->int13_offs);
    wrz_32(BIOS_INT15, driverptr+hptr->int15_offs);

    printf("new: int13 = %08x  int15 = %08x\n",
	   rdz_32(BIOS_INT13), rdz_32(BIOS_INT15));
  }

  /* Update various BIOS magic data areas (gotta love this shit) */

  if ( geometry->driveno & 0x80 ) {
    /* Update BIOS hard disk count */
    wrz_8(BIOS_HD_COUNT, rdz_8(BIOS_HD_COUNT)+1);
  } else {
#if 1				/* Apparently this is NOT wanted... */
    /* Update BIOS floppy disk count */
    uint8_t equip = rdz_8(BIOS_EQUIP);
    if ( equip & 1 ) {
      if ( (equip & (3 << 6)) != (3 << 6) ) {
	equip += (1 << 6);
      }
    } else {
      equip |= 1;
      equip &= ~(3 << 6);
    }
    wrz_8(BIOS_EQUIP, equip);
#endif
  }

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
	       : "d" ((uint16_t)geometry->driveno)
	       : "ebx", "ecx", "edx", "esi", "edi", "ebp");

  if ( status ) {
    puts("MEMDISK: Failed to load new boot sector\n");
    die();
  }
  
  puts("Booting...\n");

  /* On return the assembly code will jump to the boot vector */
  return geometry->driveno;
}
