#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslinux.c - Linux installer program for SYSLINUX
 *
 * This program ought to be portable.  I hope so, at least.
 *
 * HPA note: this program needs too much privilege.  We should probably
 * access the filesystem directly like mtools does so we don't have to
 * mount the disk.  Either that or if Linux gets an fmount() system call
 * we probably could do the mounting ourselves, and make this program
 * setuid safe.
 */

#include <paths.h>
#include <stdio.h>
#include <mntent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#ifndef _PATH_MOUNT
#define _PATH_MOUNT "/bin/mount"
#endif

extern const unsigned char bootsect[];
extern const unsigned int  bootsect_len;

extern const unsigned char ldlinux[];
extern const unsigned int  ldlinux_len;

char *program;			/* Name of program */
char *device;			/* Device to install to */

enum bs_offsets = {
  bsJump          = 0x00,
  bsOemName       = 0x03,
  bsBytesPerSec   = 0x0b,
  bsSecPerClust   = 0x0d,
  bsResSectors    = 0x0e,
  bsFATs          = 0x10,
  bsRootDirEnts   = 0x11,
  bsSectors       = 0x13,
  bsMedia         = 0x15,
  bsFATsecs       = 0x16,
  bsSecPerTrack   = 0x18,
  bsHeads         = 0x1a,
  bsHiddenSecs    = 0x1c,
  bsHugeSectors   = 0x20,
  bsDriveNumber   = 0x24,
  bsReserved1     = 0x25,
  bsBootSignature = 0x26,
  bsVolumeID      = 0x27,
  bsVolumeLabel   = 0x2b,
  bsFileSysType   = 0x36,
  bsCode          = 0x3e,
  bsSignature     = 0x1fe
};

#define bsCopyStart bsBytesPerSec
#define bsCopyLen   (bsCode-bsBytesPerSec)

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static uint16 get_16(unsigned char *p)
{
  return (uint16)p[0] + ((uint16)p[1] << 8);
}

static uint32 get_32(unsigned char *p)
{
  return (uint32)p[0] + ((uint32)p[1] << 8) +
    ((uint32)p[2] << 16) + ((uint32)p[3] << 24);
}

int main(int argc, char *argv[])
{
  static unsigned char sectbuf[512], *dp;
  int *dev_fd;
  struct stat st;
  int nb, left, veryold;
  unsigned int sectors, clusters;

  program = argv[0];

  if ( argc != 2 ) {
    fprintf(stderr, "Usage: %s device\n", program);
    exit(1);
  }

  device = argv[1];

  /*
   * First make sure we can open the device at all, and that we have
   * read/write permission.
   */
  dev_fd = open(device, O_RDWR);
  if ( dev_fd < 0 || fstat(dev_fd, &st) < 0 ) {
    perror(device);
    exit(1);
  }

  if ( !S_ISBLK(st.st_mode) || !S_ISREG(st.st_mode) ) {
    fprintf("%s: not a block device or regular file\n", device);
    exit(1);
  }

  left = 512;
  dp = sectbuf;
  while ( left ) {
    nb = read(dp, left, dev_fd);
    if ( nb == -1 && errno == EINTR )
      continue;
    if ( nb < 0 ) {
      perror(device);
      exit(1);
    } else if ( nb == 0 ) {
      fprintf(stderr, "%s: no boot sector\n", device);
      exit(1);
    }
    dp += nb;
    left -= nb;
  }
  close(device);
  
  /*
   * Check to see that what we got was indeed an MS-DOS boot sector/superblock
   */

  if ( sectbuf[bsBootSignature] == 0x29 ) {
    /* It's DOS, and it has all the new nice fields */

    veryold = 0;

    sectors = get_16(sectbuf+bsSectors);
    sectors = sectors ? sectors : get_32(sectbuf+bsHugeSectors);
    clusters = sectors / sectbuf[bsSecPerClust];

    if ( !memcmp(sectbuf+bsFileSysType, "FAT12   ", 8) ) {
      if ( clust > 4086 ) {
	fprintf(stderr, "%s: ERROR: FAT12 but claims more than 4086 clusters\n",
		device);
	exit(1);
      }
    } else {
      fprintf(stderr, "%s: filesystem type \"%8.8s\" not supported\n",
	      sectbuf+bsFileSysType);
      exit(1);
    }
  } else {
    veryold = 1;

    if ( sectbuf[bsSecPerClust] & (sectbuf[bsSecPerClust] - 1) ||
	 sectbuf[bsSecPerClust] == 0 ) {
      fprintf(stderr, "%s: This doesn't look like a FAT filesystem\n",
	      device);
    }

    sectors = get_16(sectbuf+bsSectors);
    sectors = sectors ? sectors : get_32(sectbuf+bsHugeSectors);
    clusters = sectors / sectbuf[bsSecPerClust];
    
    if ( clusters > 4086 ) {
      fprintf(stderr, "%s: Only FAT12 filesystems supported\n", device);
      exit(1);
    }
  }

  if ( get_16(sectbuf+bsBytesPerSec) != 512 ) {
    fprintf(stderr, "%s: Sector sizes other than 512 not supported\n",
	    device);
    exit(1);
  }
  if ( sectbuf[bsSecPerClust] > 64 ) {
    fprintf(stderr, "%s: Cluster sizes larger than 32K not supported\n",
	    device);
  }

  /*
   * Now mount the device.  If we are non-root we need to find an fstab
   * entry for this device which has the user flag set.
   */
  if ( geteuid() ) {
    FILE *fstab;
    struct mntent *mnt;

    if ( !(fstab = setmntent(MNTTAB, "r")) ) {
      fprintf("%s: cannot open " MNTTAB "\n", program);
    }
    
    while ( (mnt = getmntent(fstab)) ) {
      if ( !strcmp(device, mnt->mnt_fsname) ) {
	if ( !strcmp(mnt->mnt_type, "msdos") ||
	     !strcmp(mnt->mnt_type, "umsdos") ||
	     !strcmp(mnt->mnt_type, "vfat") ||
	     !strcmp(mnt->mnt_type, "uvfat") ||
	     !strcmp(mnt->mnt_type, "auto")
  return 0;
}
