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

void __attribute__((noreturn)) usage(void)
{
  fprintf(stderr, "Usage: %s [-sf] drive:\n", program);
  exit(1);
}

void __attribute__((noreturn)) die(const char *msg)
{
  fprintf(stderr, "%s: %s\n", program, msg);
  exit(1);
}

/*
 * read/write wrapper functions
 */
int open(const char *filename, int mode)
{
  uint16_t rv;
  uint8_t err;

  rv = 0x3D00 | mode;
  asm volatile("int $0x21 ; setc %0"
	       : "=rm" (err), "+a" (rv)
	       : "d" (filename));
  if ( err )
    die("cannot open ldlinux.sys");

  return rv;
}

void close(int fd)
{
  uint16_t rv = 0x3E00;

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

  while ( count ) {
    rv = 0x4000;
    asm volatile("int $0x21 ; setc %0"
		 : "=rm" (err), "+a" (rv)
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

  asm volatile("int $0x26 ; setc %0 ; popfw"
	       : "=rm" (err)
	       : "a" (drive), "b" (buf), "c" (nsecs), "d" (sector));

  if ( err )
    die("sector write error");
}

void read_device(int drive, const void *buf, size_t nsecs, unsigned int sector)
{
  uint8_t err;

  asm volatile("int $0x25 ; setc %0 ; popfw"
	       : "=rm" (err)
	       : "a" (drive), "b" (buf), "c" (nsecs), "d" (sector));

  if ( err )
    die("sector write error");
}  

void set_attributes(const char *file, int attributes)
{
  uint8_t err;
  uint16_t rv = 0x4301;

  asm volatile("int $0x21 ; setc %0"
	       : "=rm" (err), "+a" (rv)
	       : "c" (attributes), "d" (file));

  if ( err )
    die("set attribute error");
}

/*
 * Version of the read_device function suitable for libfat
 */
int libfat_xpread(void *pp, void *buf, size_t secsize, libfat_sector_t sector)
{
  read_device((int)pp, buf, 1, sector);
  return secsize;
}

int main(int argc, char *argv[])
{
  static unsigned char sectbuf[512];
  int dev_fd, fd;
  static char ldlinux_name[] = "@:LDLINUX.SYS";
  char **argp, *opt;
  int force = 0;		/* -f (force) option */
  struct libfat_filesystem *fs;
  libfat_sector_t s, *secp, sectors[65]; /* 65 is maximum possible */
  int32_t ldlinux_cluster;
  int nsectors;
  char *device = NULL;

  (void)argc;			/* Unused */

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
      if ( device )
	usage();
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

  read_device(dev_fd, sectbuf, 1, 0);

  /*
   * Check to see that what we got was indeed an MS-DOS boot sector/superblock
   */
  if(!syslinux_check_bootsect(sectbuf,device)) {
    exit(1);
  }
  
  ldlinux_name[0] = dev_fd | 0x40;

  set_attributes(ldlinux_name, 0);
  fd = open(ldlinux_name, 2);	/* Open for read/write access */
  write_file(fd, syslinux_ldlinux, syslinux_ldlinux_len);
  close(fd);
  set_attributes(ldlinux_name, 0x27); /* ARCHIVE SYSTEM HIDDEN READONLY */

  /*
   * Now, use libfat to create a block map.  This probably
   * should be changed to use ioctl(...,FIBMAP,...) since
   * this is supposed to be a simple, privileged version
   * of the installer.
   */
  fs = libfat_open(libfat_xpread, (void *)dev_fd);
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
  write_device(dev_fd, syslinux_ldlinux, 1, sectors[0]);

  /*
   * To finish up, write the boot sector
   */

  /* Read the superblock again since it might have changed while mounted */
  read_device(dev_fd, sectbuf, 1, 0);

  /* Copy the syslinux code into the boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write new boot sector */
  write_device(dev_fd, sectbuf, 1, 0);

  /* Done! */

  return 0;
}

