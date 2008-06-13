/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * chain.c
 *
 * Chainload a hard disk (currently rather braindead.)
 *
 * Usage: chain hd<disk#> [<partition>] [-ntldr] [-file <loader>]
 *        chain fd<disk#> [-ntldr] [-file <loader>]
 *	  chain mbr:<id> [<partition>] [-ntldr] [-file <loader>]
 *
 * ... e.g. "chain hd0 1" will boot the first partition on the first hard
 * disk.
 *
 * The mbr: syntax means search all the hard disks until one with a
 * specific MBR serial number (bytes 440-443) is found.
 *
 * Partitions 1-4 are primary, 5+ logical, 0 = boot MBR (default.)
 *
 * -file <loader> loads the file <loader> **from the SYSLINUX filesystem**
 * instead of loading the boot sector.
 *
 * -ntldr jumps to 07C0:0000 instead of 0000:7C00, not sure if this is
 * really necessary.
 *
 */

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <console.h>
#include <stdbool.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>

#define SECTOR 512		/* bytes/sector */

static inline void error(const char *msg)
{
  fputs(msg, stderr);
}

/*
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 */
static int int13_retry(const com32sys_t *inreg, com32sys_t *outreg)
{
  int retry = 6;		/* Number of retries */
  com32sys_t tmpregs;

  if ( !outreg ) outreg = &tmpregs;

  while ( retry-- ) {
    __intcall(0x13, inreg, outreg);
    if ( !(outreg->eflags.l & EFLAGS_CF) )
      return 0;			/* CF=0, OK */
  }

  return -1;			/* Error */
}

/*
 * Query disk parameters and EBIOS availability for a particular disk.
 */
struct diskinfo {
  int disk;
  int ebios;			/* EBIOS supported on this disk */
  int cbios;			/* CHS geometry is valid */
  int head;
  int sect;
} disk_info;

static int get_disk_params(int disk)
{
  static com32sys_t getparm, parm, getebios, ebios;

  disk_info.disk = disk;
  disk_info.ebios = disk_info.cbios = 0;

  /* Get EBIOS support */
  getebios.eax.w[0] = 0x4100;
  getebios.ebx.w[0] = 0x55aa;
  getebios.edx.b[0] = disk;
  getebios.eflags.b[0] = 0x3;	/* CF set */

  __intcall(0x13, &getebios, &ebios);

  if ( !(ebios.eflags.l & EFLAGS_CF) &&
       ebios.ebx.w[0] == 0xaa55 &&
       (ebios.ecx.b[0] & 1) ) {
    disk_info.ebios = 1;
  }

  /* Get disk parameters -- really only useful for
     hard disks, but if we have a partitioned floppy
     it's actually our best chance... */
  getparm.eax.b[1] = 0x08;
  getparm.edx.b[0] = disk;

  __intcall(0x13, &getparm, &parm);

  if ( parm.eflags.l & EFLAGS_CF )
    return disk_info.ebios ? 0 : -1;

  disk_info.head = parm.edx.b[1]+1;
  disk_info.sect = parm.ecx.b[0] & 0x3f;
  if ( disk_info.sect == 0 ) {
    disk_info.sect = 1;
  } else {
    disk_info.cbios = 1;	/* Valid geometry */
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

static int read_sector(void *buf, unsigned int lba)
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

    if ( !disk_info.cbios ) {
      /* We failed to get the geometry */

      if ( lba )
	return -1;		/* Can only read MBR */

      s = 1;  h = 0;  c = 0;
    } else {
      s = (lba % disk_info.sect) + 1;
      t = lba / disk_info.sect;	/* Track = head*cyl */
      h = t % disk_info.head;
      c = t / disk_info.head;
    }

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

/* Search for a specific drive, based on the MBR signature; bytes
   440-443. */
static int find_disk(uint32_t mbr_sig, void *buf)
{
  int drive;

  for (drive = 0x80; drive <= 0xff; drive++) {
    if (get_disk_params(drive))
      continue;			/* Drive doesn't exist */
    if (read_sector(buf, 0))
      continue;			/* Cannot read sector */

    if (*(uint32_t *)((char *)buf + 440) == mbr_sig)
      return drive;
  }

  return -1;
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


/* Search for a logical partition.  Logical partitions are actually implemented
   as recursive partition tables; theoretically they're supposed to form a
   linked list, but other structures have been seen.

   To make things extra confusing: data partition offsets are relative to where
   the data partition record is stored, whereas extended partition offsets
   are relative to the beginning of the extended partition all the way back
   at the MBR... but still not absolute! */

int nextpart;			/* Number of the next logical partition */

static struct part_entry *
find_logical_partition(int whichpart, char *table, struct part_entry *self,
		       struct part_entry *root)
{
  struct part_entry *ptab = (struct part_entry *)(table + 0x1be);
  struct part_entry *found;
  int i;

  if ( *(uint16_t *)(table + 0x1fe) != 0xaa55 )
    return NULL;		/* Signature missing */

  /* We are assumed to already having enumerated all the data partitions
     in this table if this is the MBR.  For MBR, self == NULL. */

  if ( self ) {
    /* Scan the data partitions. */

    for ( i = 0 ; i < 4 ; i++ ) {
      if ( ptab[i].ostype == 0x00 || ptab[i].ostype == 0x05 ||
	   ptab[i].ostype == 0x0f || ptab[i].ostype == 0x85 )
	continue;		/* Skip empty or extended partitions */

      if ( !ptab[i].length )
	continue;

      /* Adjust the offset to account for the extended partition itself */
      ptab[i].start_lba += self->start_lba;

      /* Sanity check entry: must not extend outside the extended partition.
	 This is necessary since some OSes put crap in some entries. */
      if ( ptab[i].start_lba + ptab[i].length <= self->start_lba ||
	   ptab[i].start_lba >= self->start_lba + self->length )
	continue;

      /* OK, it's a data partition.  Is it the one we're looking for? */
      if ( nextpart++ == whichpart )
	return &ptab[i];
    }
  }

  /* Scan the extended partitions. */
  for ( i = 0 ; i < 4 ; i++ ) {
    if ( ptab[i].ostype != 0x05 &&
	 ptab[i].ostype != 0x0f && ptab[i].ostype != 0x85 )
      continue;		/* Skip empty or data partitions */

    if ( !ptab[i].length )
      continue;

    /* Adjust the offset to account for the extended partition itself */
    if ( root )
      ptab[i].start_lba += root->start_lba;

    /* Sanity check entry: must not extend outside the extended partition.
       This is necessary since some OSes put crap in some entries. */
    if ( root )
      if ( ptab[i].start_lba + ptab[i].length <= root->start_lba ||
	   ptab[i].start_lba >= root->start_lba + root->length )
	continue;

    /* Process this partition */
    if ( read_sector(table+SECTOR, ptab[i].start_lba) )
      continue;			/* Read error, must be invalid */

    if ( (found = find_logical_partition(whichpart, table+SECTOR, &ptab[i],
					 root ? root : &ptab[i])) )
      return found;
  }

  /* If we get here, there ain't nothing... */
  return NULL;
}

static void do_boot(uint16_t keeppxe, void *boot_sector,
		    size_t boot_size, struct syslinux_rm_regs *regs)
{
  struct syslinux_memmap *mmap;
  struct syslinux_movelist *mlist = NULL;

  mmap = syslinux_memory_map();

  if (!mmap) {
    error("Cannot read system memory map");
    return;
  }

  if (syslinux_memmap_type(mmap, 0x7c00, boot_size) != SMT_FREE) {
    error("Loader file too large");
    return;
  }

  if (syslinux_add_movelist(&mlist, 0x7c00, (addr_t)boot_sector, boot_size)) {
    error("Out of memory");
    return;
  }

  fputs("Booting...\n", stdout);

  syslinux_shuffle_boot_rm(mlist, mmap, keeppxe, regs);

  /* If we get here, badness happened */
  error("Chainboot failed!\n");
}

int main(int argc, char *argv[])
{
  char *mbr;
  void *boot_sector = NULL;
  struct part_entry *partinfo;
  struct syslinux_rm_regs regs;
  char *drivename, *partition;
  int hd, drive, whichpart;
  int i;
  uint16_t keeppxe = 0;
  bool ntldr = false;
  const char *load_file;
  size_t boot_size = SECTOR;

  openconsole(&dev_null_r, &dev_stdcon_w);

  drivename = NULL;
  partition = NULL;
  load_file = NULL;

  /* Prepare the register set */
  memset(&regs, 0, sizeof regs);

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-file") && argv[i+1]) {
      load_file = argv[++i];
    } else if (!strcmp(argv[i], "-ntldr")) {
      ntldr = true;
    } else if (!strcmp(argv[i], "keeppxe")) {
      keeppxe = 3;
    } else {
      if (!drivename)
	drivename = argv[i];
      else if (!partition)
	partition = argv[i];
    }
  }

  if ( !drivename ) {
    error("Usage: chain.c32 (hd#|fd#|mbr:#) [partition] [-ntldr] "
	  "[-file loader]\n");
    goto bail;
  }

  if (ntldr) {
    regs.es = regs.cs = regs.ss = regs.ds = regs.fs = regs.gs = 0x07c0;
  } else {
    regs.ip = regs.esp.l = 0x7c00;
  }

  /* Divvy up the bounce buffer.  To keep things sector-
     aligned, give the EBIOS DAPA the first sector, then
     the MBR next, and the rest is used for the partition-
     chasing stack. */
  dapa = (struct ebios_dapa *)__com32.cs_bounce;
  mbr  = (char *)__com32.cs_bounce + SECTOR;

  drivename = argv[1];
  partition = argv[2];		/* Possibly null */

  hd = 0;
  if ( !memcmp(drivename, "mbr:", 4) ) {
    drive = find_disk(strtoul(drivename+4, NULL, 0), mbr);
    if (drive == -1) {
      error("Unable to find requested MBR signature\n");
      goto bail;
    }
  } else {
    if ( (drivename[0] == 'h' || drivename[0] == 'f') &&
	 drivename[1] == 'd' ) {
      hd = drivename[0] == 'h';
      drivename += 2;
    }
    drive = (hd ? 0x80 : 0) | strtoul(drivename, NULL, 0);
  }

  regs.edx.b[0] = drive;

  whichpart = 0;		/* Default */

  if ( partition )
    whichpart = strtoul(partition, NULL, 0);

  if ( !(drive & 0x80) && whichpart ) {
    error("Warning: Partitions of floppy devices may not work\n");
  }

  /* Get the disk geometry and disk access setup */
  if ( get_disk_params(drive) ) {
    error("Cannot get disk parameters\n");
    goto bail;
  }

  /* Get MBR */
  if ( read_sector(mbr, 0) ) {
    error("Cannot read Master Boot Record\n");
    goto bail;
  }

  if ( whichpart == 0 ) {
    /* Boot the MBR */
    partinfo = NULL;
    boot_sector = mbr;
  } else if ( whichpart <= 4 ) {
    /* Boot a primary partition */
    partinfo = &((struct part_entry *)(mbr + 0x1be))[whichpart-1];
    if ( partinfo->ostype == 0 ) {
      error("Invalid primary partition\n");
      goto bail;
    }
  } else {
    /* Boot a logical partition */

    nextpart = 5;
    partinfo = find_logical_partition(whichpart, mbr, NULL, NULL);

    if ( !partinfo || partinfo->ostype == 0 ) {
      error("Requested logical partition not found\n");
      goto bail;
    }
  }

  /* Do the actual chainloading */
  if (load_file) {
    if ( loadfile(load_file, &boot_sector, &boot_size) ) {
      error("Failed to load the boot file\n");
      goto bail;
    }
  } else if (partinfo) {
    /* Actually read the boot sector */
    /* Pick the first buffer that isn't already in use */
    boot_sector = (void *)(((uintptr_t)partinfo + 511) & ~511);
    if ( read_sector(boot_sector, partinfo->start_lba) ) {
      error("Cannot read boot sector\n");
      goto bail;
    }
  }

  if (partinfo) {
    /* 0x7BE is the canonical place for the first partition entry. */
    regs.esi.w[0] = 0x7be;
    memcpy((char *)0x7be, partinfo, sizeof(*partinfo));
  }

  do_boot(keeppxe, boot_sector, boot_size, &regs);

bail:
  return 255;
}
