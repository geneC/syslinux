#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998-2003 H. Peter Anvin - All Rights Reserved
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
 * This program now requires mtools.  It turned out to be a lot
 * easier to deal with than dealing with needing mount privileges.
 * We need device write permission anyway.
 */

#define _XOPEN_SOURCE 500	/* Required on glibc 2.x */
#define _BSD_SOURCE
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mntent.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "syslinux.h"

char *program;			/* Name of program */
char *device;			/* Device to install to */
pid_t mypid;

enum bs_offsets {
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

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static u_int16_t get_16(unsigned char *p)
{
  return (u_int16_t)p[0] + ((u_int16_t)p[1] << 8);
}

static u_int32_t get_32(unsigned char *p)
{
  return (u_int32_t)p[0] + ((u_int32_t)p[1] << 8) +
    ((u_int32_t)p[2] << 16) + ((u_int32_t)p[3] << 24);
}

#if 0				/* Not needed */
static void set_16(unsigned char *p, u_int16_t v)
{
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
}

static void set_32(unsigned char *p, u_int32_t v)
{
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
  p[2] = ((v >> 16) & 0xff);
  p[3] = ((v >> 24) & 0xff);
}
#endif

void usage(void)
{
  fprintf(stderr, "Usage: %s [-sf] [-o offset] device\n", program);
  exit(1);
}

/*
 * read/write wrapper functions
 */
ssize_t xpread(int fd, void *buf, size_t count, off_t offset)
{
  char *bufp = (char *)buf;
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pread(fd, bufp, count, offset);
    if ( rv == 0 ) {
      fprintf(stderr, "%s: short read\n", program);
      exit(1);
    } else if ( rv == -1 ) {
      if ( errno == EINTR ) {
	continue;
      } else {
	perror(program);
	exit(1);
      }
    } else {
      bufp += rv;
      offset += rv;
      done += rv;
      count -= rv;
    }
  }

  return done;
}

ssize_t xpwrite(int fd, void *buf, size_t count, off_t offset)
{
  char *bufp = (char *)buf;
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pwrite(fd, bufp, count, offset);
    if ( rv == 0 ) {
      fprintf(stderr, "%s: short write\n", program);
      exit(1);
    } else if ( rv == -1 ) {
      if ( errno == EINTR ) {
	continue;
      } else {
	perror(program);
	exit(1);
      }
    } else {
      bufp += rv;
      offset += rv;
      done += rv;
      count -= rv;
    }
  }

  return done;
}

int main(int argc, char *argv[])
{
  static unsigned char sectbuf[512];
  int dev_fd;
  struct stat st;
  int veryold;
  unsigned int sectors, clusters;
  int status;
  char **argp, *opt;
  int force = 0;		/* -f (force) option */
  off_t offset = 0;		/* -o (offset) option */
  char mtools_conf[] = "/tmp/syslinux-mtools-XXXXXX";
  int mtc_fd;
  FILE *mtc, *mtp;

  mypid = getpid();
  program = argv[0];
  
  device = NULL;

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
	} else if ( *opt == 'o' && argp[1] ) {
	  offset = strtoul(*++argp, NULL, 0); /* Byte offset */
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
   * First make sure we can open the device at all, and that we have
   * read/write permission.
   */
  dev_fd = open(device, O_RDWR);
  if ( dev_fd < 0 || fstat(dev_fd, &st) < 0 ) {
    perror(device);
    exit(1);
  }

  if ( !force && !S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode) ) {
    fprintf(stderr, "%s: not a block device or regular file (use -f to override)\n", device);
    exit(1);
  }

  if ( !force && offset != 0 && !S_ISREG(st.st_mode) ) {
    fprintf(stderr, "%s: not a regular file and an offset specified (use -f to override)\n", device);
    exit(1);
  }

  xpread(dev_fd, sectbuf, 512, offset);
  
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
      if ( clusters > 4086 ) {
	fprintf(stderr, "%s: ERROR: FAT12 but claims more than 4086 clusters\n",
		device);
	exit(1);
      }
    } else if ( !memcmp(sectbuf+bsFileSysType, "FAT16   ", 8) ) {
      if ( clusters <= 4086 ) {
	fprintf(stderr, "%s: ERROR: FAT16 but claims less than 4086 clusters\n",
		device);
	exit(1);
      }
    } else if ( !memcmp(sectbuf+bsFileSysType, "FAT     ", 8) ) {
      /* OS/2 sets up the filesystem as just `FAT'. */
    } else {
      fprintf(stderr, "%s: filesystem type \"%8.8s\" not supported\n",
	      device, sectbuf+bsFileSysType);
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
  }

  if ( get_16(sectbuf+bsBytesPerSec) != 512 ) {
    fprintf(stderr, "%s: Sector sizes other than 512 not supported\n",
	    device);
    exit(1);
  }
  if ( sectbuf[bsSecPerClust] > 32 ) {
    fprintf(stderr, "%s: Cluster sizes larger than 16K not supported\n",
	    device);
  }

  /*
   * Create an mtools configuration file
   */
  mtc_fd = mkstemp(mtools_conf);
  if ( mtc_fd < 0 || !(mtc = fdopen(mtc_fd, "w")) ) {
    perror(program);
    exit(1);
  }
  fprintf(mtc,
	  "drive s:\n"
	  "  file=\"/proc/%lu/fd/%d\"\n"
	  "  offset=%lld\n",
	  (unsigned long)mypid,
	  dev_fd,
	  (unsigned long long)offset);
  fclose(mtc);
  
  /*
   * Run mtools to create the LDLINUX.SYS file
   */
  if ( setenv("MTOOLSRC", mtools_conf, 1) ) {
    perror(program);
    exit(1);
  }

  /* This command may fail legitimately */
  system("mattrib -h -r -s s:ldlinux.sys 2>/dev/null");

  mtp = popen("mcopy -o - s:ldlinux.sys", "w");
  if ( !mtp ||
       (fwrite(syslinux_ldlinux, 1, syslinux_ldlinux_len, mtp) 
	!= syslinux_ldlinux_len) ||
       (status = pclose(mtp), !WIFEXITED(status) || WEXITSTATUS(status)) ) {
    fprintf(stderr, "%s: failed to create ldlinux.sys\n", program);
    exit(1);
  }

  status = system("mattrib +r s:ldlinux.sys");

  if ( !WIFEXITED(status) || WEXITSTATUS(status) ) {
    fprintf(stderr,
	    "%s: warning: failed to set readonly bit on ldlinux.sys\n",
	    program);
  }

  unlink(mtools_conf);

  /*
   * To finish up, write the boot sector
   */

  /* Read the superblock again since it might have changed while mounted */
  xpread(dev_fd, sectbuf, 512, offset);

  /* Copy the syslinux code into the boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write new boot sector */
  xpwrite(dev_fd, sectbuf, 512, offset);

  close(dev_fd);
  sync();

  /* Done! */

  return 0;
}
