#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998-2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslinux.c - Linux installer program for SYSLINUX
 *
 * Hacked up for DOS.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mystuff.h"

#include "syslinux.h"
#include "libfat.h"

const char *program = "syslinux"; /* Name of program */
uint16_t dos_version;

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

void __attribute__((noreturn)) usage(void)
{
  puts("Usage: syslinux [-sf] drive: [bootsecfile]\n");
  exit(1);
}

void unlock_device(int);

void __attribute__((noreturn)) die(const char *msg)
{
  unlock_device(0);
  puts("syslinux: ");
  puts(msg);
  putchar('\n');
  exit(1);
}

/*
 * read/write wrapper functions
 */
int creat(const char *filename, int mode)
{
  uint16_t rv;
  uint8_t err;

  dprintf("creat(\"%s\", 0x%x)\n", filename, mode);

  rv = 0x3C00;
  asm volatile("int $0x21 ; setc %0"
	       : "=abcdm" (err), "+a" (rv)
	       : "c" (mode), "d" (filename));
  if ( err ) {
    dprintf("rv = %d\n", rv);
    die("cannot open ldlinux.sys");
  }

  return rv;
}

void close(int fd)
{
  uint16_t rv = 0x3E00;

  dprintf("close(%d)\n", fd);

  asm volatile("int $0x21"
	       : "+a" (rv)
	       : "b" (fd));

  /* The only error MS-DOS returns for close is EBADF,
     and we really don't care... */
}

ssize_t write_file(int fd, const void *buf, size_t count)
{
  uint16_t rv;
  ssize_t done = 0;
  uint8_t err;

  dprintf("write_file(%d,%p,%u)\n", fd, buf, count);

  while ( count ) {
    rv = 0x4000;
    asm volatile("int $0x21 ; setc %0"
		 : "=abcdm" (err), "+a" (rv)
		 : "b" (fd), "c" (count), "d" (buf));
    if ( err || rv == 0 )
      die("file write error");

    done += rv;
    count -= rv;
  }

  return done;
}

void write_device(int drive, const void *buf, size_t nsecs, unsigned int sector)
{
  uint8_t err;

  dprintf("write_device(%d,%p,%u,%u)\n", drive, buf, nsecs, sector);

  asm volatile("int $0x26 ; setc %0 ; popfw"
	       : "=abcdm" (err)
	       : "a" (drive-1), "b" (buf), "c" (nsecs), "d" (sector));

  if ( err )
    die("sector write error");
}

void read_device(int drive, const void *buf, size_t nsecs, unsigned int sector)
{
  uint8_t err;

  dprintf("read_device(%d,%p,%u,%u)\n", drive, buf, nsecs, sector);

  asm volatile("int $0x25 ; setc %0 ; popfw"
	       : "=abcdm" (err)
	       : "a" (drive-1), "b" (buf), "c" (nsecs), "d" (sector));

  if ( err )
    die("sector write error");
}  

/* This call can legitimately fail, and we don't care, so ignore error return */
void set_attributes(const char *file, int attributes)
{
  uint16_t rv = 0x4301;

  dprintf("set_attributes(\"%s\", 0x%02x)\n", file, attributes);

  asm volatile("int $0x21"
	       : "+a" (rv)
	       : "c" (attributes), "d" (file));
}

/*
 * Version of the read_device function suitable for libfat
 */
int libfat_xpread(intptr_t pp, void *buf, size_t secsize, libfat_sector_t sector)
{
  read_device(pp, buf, 1, sector);
  return secsize;
}

static inline void get_dos_version(void)
{
  uint16_t ver = 0x3001;
  asm("int $0x21 ; xchgb %%ah,%%al" : "+a" (ver) : : "ebx", "ecx");
  dos_version = ver;
  dprintf("DOS version %d.%d\n", (dos_version >> 8), dos_version & 0xff);
}

/* The locking interface relies on static variables.  A massive hack :( */
static uint16_t lock_level;

static inline void set_lock_device(uint8_t device)
{
  lock_level = device;
}

void lock_device(int level)
{
  uint16_t rv;
  uint8_t err;
  uint16_t lock_call;

  if ( dos_version < 0x0700 )
    return;			/* Win9x/NT only */

#if 0
  /* DOS 7.10 = Win95 OSR2 = first version with FAT32 */
  lock_call = (dos_version >= 0x0710) ? 0x484A : 0x084A;
#else
  lock_call = 0x084A;		/* MSDN says this is OK for all filesystems */
#endif

  while ( (lock_level >> 8) < level ) {
    uint16_t new_level = lock_level + 0x0100;
    dprintf("Trying lock %04x...\n", new_level);
    rv = 0x444d;
    asm volatile("int $0x21 ; setc %0"
		 : "=abcdm" (err), "+a" (rv)
		 : "b" (new_level), "c" (lock_call), "d"(0x0001));
    if ( err ) {
      /* rv == 0x0001 means this call is not supported, if so we
	 assume locking isn't needed (e.g. Win9x in DOS-only mode) */
      if ( rv == 0x0001 )
	return;
      else
	die("could not lock device");
    }

    lock_level = new_level;
  }
  return;
}

void unlock_device(int level)
{
  uint16_t rv;
  uint8_t err;
  uint16_t unlock_call;

  if ( dos_version < 0x0700 )
    return;			/* Win9x/NT only */

#if 0
  /* DOS 7.10 = Win95 OSR2 = first version with FAT32 */
  unlock_call = (dos_version >= 0x0710) ? 0x486A : 0x086A;
#else
  unlock_call = 0x086A;		/* MSDN says this is OK for all filesystems */
#endif

  while ( (lock_level >> 8) > level ) {
    uint16_t new_level = lock_level - 0x0100;
    rv = 0x440d;
    asm volatile("int $0x21 ; setc %0"
		 : "=abcdm" (err), "+a" (rv)
		 : "b" (new_level), "c" (unlock_call));
    lock_level = new_level;
  }
}

int main(int argc, char *argv[])
{
  static unsigned char sectbuf[512];
  int dev_fd, fd;
  static char ldlinux_name[] = "@:\\LDLINUX.SYS";
  char **argp, *opt;
  int force = 0;		/* -f (force) option */
  struct libfat_filesystem *fs;
  libfat_sector_t s, *secp, sectors[65]; /* 65 is maximum possible */
  int32_t ldlinux_cluster;
  int nsectors;
  const char *device = NULL, *bootsecfile = NULL;
  const char *errmsg;
  int i;

  dprintf("argv = %p\n", argv);
  for ( i = 0 ; i <= argc ; i++ )
    dprintf("argv[%d] = %p = \"%s\"\n", i, argv[i], argv[i]);

  (void)argc;			/* Unused */
  
  get_dos_version();

  for ( argp = argv+1 ; *argp ; argp++ ) {
    if ( **argp == '-' ) {
      opt = *argp + 1;
      if ( !*opt )
	usage();

      while ( *opt ) {
	if ( *opt == 's' ) {
	  syslinux_make_stupid();	/* Use "safe, slow and stupid" code */
	} else if ( *opt == 'f' ) {
	  force = 1;		/* Force install */
	} else {
	  usage();
	}
	opt++;
      }
    } else {
      if ( bootsecfile )
	usage();
      else if ( device )
	bootsecfile = *argp;
      else
      device = *argp;
    }
  }

  if ( !device )
    usage();

  /*
   * Figure out which drive we're talking to
   */
  dev_fd = device[0] & 0x1F;
  if ( !(device[0] & 0x40) || device[1] != ':' || device[2] )
    usage();

  set_lock_device(dev_fd);

  lock_device(2);		/* Make sure we can lock the device */
  read_device(dev_fd, sectbuf, 1, 0);
  unlock_device(1);

  /*
   * Check to see that what we got was indeed an MS-DOS boot sector/superblock
   */
  if( (errmsg = syslinux_check_bootsect(sectbuf)) ) {
    unlock_device(0);
    puts(errmsg);
    putchar('\n');
    exit(1);
  }
  
  ldlinux_name[0] = dev_fd | 0x40;

  set_attributes(ldlinux_name, 0);
  fd = creat(ldlinux_name, 0x07); /* SYSTEM HIDDEN READONLY */
  write_file(fd, syslinux_ldlinux, syslinux_ldlinux_len);
  close(fd);

  /*
   * Now, use libfat to create a block map.  This probably
   * should be changed to use ioctl(...,FIBMAP,...) since
   * this is supposed to be a simple, privileged version
   * of the installer.
   */
  lock_device(2);
  fs = libfat_open(libfat_xpread, dev_fd);
  ldlinux_cluster = libfat_searchdir(fs, 0, "LDLINUX SYS", NULL);
  secp = sectors;
  nsectors = 0;
  s = libfat_clustertosector(fs, ldlinux_cluster);
  while ( s && nsectors < 65 ) {
    *secp++ = s;
    nsectors++;
    s = libfat_nextsector(fs, s);
  }
  libfat_close(fs);

  /*
   * Patch ldlinux.sys and the boot sector
   */
  syslinux_patch(sectors, nsectors);

  /*
   * Write the now-patched first sector of ldlinux.sys
   */
  lock_device(3);
  write_device(dev_fd, syslinux_ldlinux, 1, sectors[0]);

  /*
   * To finish up, write the boot sector
   */

  /* Read the superblock again since it might have changed while mounted */
  read_device(dev_fd, sectbuf, 1, 0);

  /* Copy the syslinux code into the boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write new boot sector */
  if ( bootsecfile ) {
    unlock_device(0);
    fd = creat(bootsecfile, 0x20); /* ARCHIVE */
    write_file(fd, sectbuf, 512);
    close(fd);
  } else {
    write_device(dev_fd, sectbuf, 1, 0);
    unlock_device(0);
  }

  /* Done! */

  return 0;
}

