#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2003 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * hd.c
 *
 * Chainload a hard disk (currently rather braindead.)
 *
 * Usage: hd disk# [partition]
 *
 * ... e.g. "hd 0 1" will boot the first partition on the first hard disk.
 *
 * Only partitions 1-4 supported at this time; 0 = boot MBR (default.)
 */

#include <com32.h>
#define NULL ((void *)0)

int printf(const char *, ...);
int puts(const char *);
unsigned int skip_atou(char * const *);

static inline void error(const char *msg)
{
  puts(msg);
}

static inline int isspace(char x)
{
  return (x == ' ') || (x >= '\b' && x <= '\r');
}

/*
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 */
int int13_retry(const com32sys_t *inreg, com32sys_t *outreg)
{
  int retry = 6;		/* Number of retries */
  com32sys_t tmpregs;
  
  if ( !outreg ) outreg = &tmpregs;

  while ( retry ) {
    __com32.cs_intcall(0x13, inreg, outreg);
    if ( (outreg->eflags.l & 1) == 0 )
      return 0;			/* CF=0, OK */
  }

  return -1;			/* Error */
}

/*
 * Query disk parameters and EBIOS availability for a particular disk.
 */
struct diskinfo {
  int disk;
  int head;
  int sect;
  int ebios;
} disk_info;

int get_disk_params(int disk)
{
  com32sys_t getparm, parm, getebios, ebios;

  memset(&getparm, 0, sizeof getparm);
  memset(&getebios, 0, sizeof getebios);
  memset(&disk_info, 0, sizeof disk_info);

  disk_info.disk = disk;

  if ( disk & 0x80 ) {
    /* Get disk parameters -- hard drives only */
    
    getparm.eax.b[1] = 0x08;
    getparm.edx.b[0] = disk;
    if ( int13_retry(&getparm, &parm) )
      return -1;
    
    disk_info.head = parm.edx.b[1]+1;
    disk_info.sect = parm.ecx.b[0] & 0x3f;
    
    /* Get EBIOS support */
    
    getebios.eax.w[0] = 0x4100;
    getebios.edx.b[0] = disk;
    getebios.ebx.w[0] = 0x55aa;
    getebios.eflags.b[0] = 0x3;	/* CF set */
    if ( !int13_retry(&getebios, &ebios) ) {
      if ( ebios.ebx.w[0] == 0xaa55 &&
	   (ebios.ecx.b[0] & 1) )
	disk_info.ebios = 1;
    }
  }

  return 0;
}

/*
 * Get a disk block; buf is REQUIRED TO BE IN LOW MEMORY.
 */
struct ebios_dapa {
  uint16_t len;
  uint16_t count;
  uint16_t off;
  uint16_t seg;
  uint64_t lba;
} *dapa;

int read_sector(void *buf, unsigned int lba)
{
  com32sys_t inreg;

  memset(&inreg, 0, sizeof inreg);

  if ( disk_info.ebios ) {
    dapa->len = sizeof(*dapa);
    dapa->count = 1;		/* 1 sector */
    dapa->off = OFFS(buf);
    dapa->seg = SEG(buf);
    dapa->lba = lba;
    
    inreg.esi.w[0] = OFFS(dapa);
    inreg.ds       = SEG(dapa);
    inreg.edx.b[0] = disk_info.disk;
    inreg.eax.b[1] = 0x42;	/* Extended read */
  } else {
    unsigned int c, h, s, t;

    s = (lba % disk_info.sect) + 1;
    t = lba / disk_info.sect;	/* Track = head*cyl */
    h = t % disk_info.head;
    c = t / disk_info.head;

    if ( s > 63 || h > 256 || c > 1023 )
      return -1;

    inreg.eax.w[0] = 0x0201;	/* Read one sector */
    inreg.ecx.b[1] = c & 0xff;
    inreg.ecx.b[0] = s + (c >> 6);
    inreg.edx.b[1] = h;
    inreg.edx.b[0] = disk_info.disk;
    inreg.ebx.w[0] = OFFS(buf);
    inreg.es       = SEG(buf);
  }
  
  return int13_retry(&inreg, NULL);
}


/* A DOS partition table entry */
struct part_entry {
  uint8_t active_flag;		/* 0x80 if "active" */
  uint8_t start_head;
  uint8_t start_sect;
  uint8_t start_cyl;
  uint8_t ostype;
  uint8_t end_head;
  uint8_t end_sect;
  uint8_t end_cyl;
  uint32_t start_lba;
  uint32_t length;
} __attribute__((packed));

int __start(void)
{
  char *mbr, *boot_sector;
  struct part_entry *partinfo;
  char *cmdline = __com32.cs_cmdline;
  int hd, drive, whichpart;
  static com32sys_t inreg;	/* In bss, so zeroed automatically */
  int retry;

  /* Parse command line */
  while ( isspace(*cmdline) )
    cmdline++;

  hd = 0;
  if ( (cmdline[0] == 'h' || cmdline[0] == 'f') &&
       cmdline[1] == 'd' ) {
    hd = cmdline[0] == 'h';
    cmdline += 2;
  }
  drive = (hd ? 0x80 : 0) | skip_atou(&cmdline);
  whichpart = 0;		/* Default */

  if ( isspace(*cmdline) ) {
    while ( isspace(*cmdline) )
      cmdline++;

    whichpart = skip_atou(&cmdline);
  }

  if ( !(drive & 0x80) && whichpart != 0 ) {
    error("Partitions not supported on floppy drives\n");
    goto bail;
  }

  /* Divvy up the bounce buffer.  To keep things sector-
     aligned, give the EBIOS DAPA the first sector, then
     the MBR next, and the rest is used for the partition-
     chasing stack. */
  dapa = (struct ebios_dapa *)__com32.cs_bounce;
  mbr  = (char *)__com32.cs_bounce + 512;

  /* Get the MBR */
  if ( get_disk_params(drive) ) {
    error("Cannot get disk parameters\n");
    goto bail;
  }

  if ( read_sector(mbr, 0) ) {
    error("Cannot read MBR\n");
    goto bail;
  }

  if ( whichpart == 0 ) {
    /* Boot the MBR */
    partinfo = NULL;
    boot_sector = mbr;
  } else if ( whichpart <= 4 ) {
    /* Boot a primary partition */
    partinfo = &((struct part_entry *)(mbr + 0x1be))[whichpart-1];
    boot_sector = mbr+512;
  } else {
    /* Boot a logical partition */
    error("Logical partitions not implemented yet\n");
    goto bail;
  }

  /* Do the actual chainloading */
  if ( partinfo ) {
    /* Actually read the boot sector */
    if ( read_sector(boot_sector, partinfo->start_lba) ) {
      error("Cannot read boot sector\n");
      goto bail;
    }

    /* 0x7BE is the canonical place for the first partition entry. */
    inreg.esi.w[0] = 0x7be;
    memcpy((char *)0x7be, partinfo, sizeof(*partinfo));
  }
  inreg.eax.w[0] = 0x000d;	/* Clean up and chain boot */
  inreg.edx.w[0] = 0;		/* Should be 3 for "keeppxe" */
  inreg.edi.l    = (uint32_t)boot_sector;
  inreg.ecx.l    = 512;		/* One sector */
  inreg.ebx.b[0] = drive;	/* DL = drive no */

  __com32.cs_intcall(0x22, &inreg, NULL);

  /* If we get here, badness happened */
  error("Chainboot failed!\n");

bail:
  return 255;
}


  
