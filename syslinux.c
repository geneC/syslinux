#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998-2001 H. Peter Anvin - All Rights Reserved
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
 * setuid safe.  Or perhaps try to link with mtools code...
 *
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "syslinux.h"

#ifndef _PATH_MOUNT
#define _PATH_MOUNT "/bin/mount"
#endif

#ifndef _PATH_UMOUNT
#define _PATH_UMOUNT "/bin/umount"
#endif

char *program;			/* Name of program */
char *device;			/* Device to install to */
uid_t ruid;			/* Real uid */
uid_t euid;			/* Initial euid */
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
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pread(fd, buf, count, offset);
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
      offset += rv;
      done += rv;
      count -= rv;
    }
  }

  return done;
}

ssize_t xpwrite(int fd, void *buf, size_t count, off_t offset)
{
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pwrite(fd, buf, count, offset);
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
  unsigned char *dp;
  const unsigned char *cdp;
  int dev_fd, fd;
  struct stat st;
  int nb, left, veryold;
  unsigned int sectors, clusters;
  int err = 0;
  pid_t f, w;
  int status;
  char *mntpath = NULL, mntname[64], devfdname[64];
  char *ldlinux_name, **argp, *opt;
  int my_umask;
  int force = 0;		/* -f (force) option */
  off_t offset = 0;		/* -o (offset) option */
  static const char * const clean_environ[] = {
    "PATH=/bin:/usr/bin",
    NULL
  };

  ruid = getuid();
  euid = geteuid();
  mypid = getpid();
  
  if ( !euid )
    setreuid(-1, ruid);		/* Run as regular user until we need it */

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

  if ( lseek(dev_fd, offset, SEEK_SET) != offset ) {
    if ( !(force && errno == EBADF) ) {
      fprintf(stderr, "%s: seek error", device);
      exit(1);
    }
  }

  xpread(dev_fd, sectbuf, 512, offset);
  fsync(dev_fd);
  
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
   * Now mount the device.  If we are non-root we need to find an fstab
   * entry for this device which has the user flag and the appropriate
   * options set.
   */
  if ( euid ) {
    FILE *fstab;
    struct mntent *mnt;

    if ( !(fstab = setmntent(MNTTAB, "r")) ) {
      fprintf(stderr, "%s: cannot open " MNTTAB "\n", program);
    }
    
    while ( (mnt = getmntent(fstab)) ) {
      if ( !strcmp(device, mnt->mnt_fsname) &&
	   ( !strcmp(mnt->mnt_type, "msdos") ||
	     !strcmp(mnt->mnt_type, "umsdos") ||
	     !strcmp(mnt->mnt_type, "vfat") ||
	     !strcmp(mnt->mnt_type, "uvfat") ||
	     !strcmp(mnt->mnt_type, "auto") ) &&
	   ( hasmntopt(mnt, "user") ||
	     (hasmntopt(mnt, "owner") && st.st_uid == euid) ) &&
	   !hasmntopt(mnt, "ro") &&
	   mnt->mnt_dir[0] == '/' &&
	   !!hasmntopt(mnt, "loop") == !!S_ISREG(st.st_mode) &&
	   ( (!hasmntopt(mnt,"offset") && offset == 0) ||
	     (atol(hasmntopt(mnt, "offset")) == offset) ) ) {
	/* Okay, this is an fstab entry we should be able to live with. */
	
	mntpath = mnt->mnt_dir;
	break;
      }
    }
    endmntent(fstab);

    if ( !mntpath ) {
      fprintf(stderr, "%s: not root and no appropriate entry for %s in "
	      MNTTAB "\n", program, device);
      exit(1);
    }
  
    f = fork();
    if ( f < 0 ) {
      perror(program);
      exit(1);
    } else if ( f == 0 ) {
      /* We're not root here, so don't use clean_environ */
      execl(_PATH_MOUNT, _PATH_MOUNT, mntpath, NULL);
      _exit(255);		/* If execl failed, trouble... */
    }
  } else {
    int i = 0;
    struct stat dst;
    int rv;

    /* We're root or at least setuid.
       Make a temp dir and pass all the gunky options to mount. */

    if ( chdir("/tmp") ) {
      perror(program);
      exit(1);
    }

#define TMP_MODE (S_IXUSR|S_IWUSR|S_IXGRP|S_IWGRP|S_IWOTH|S_IXOTH|S_ISVTX)

    if ( stat(".", &dst) || !S_ISDIR(dst.st_mode) ||
	 (dst.st_mode & TMP_MODE) != TMP_MODE ) {
      fprintf(stderr, "%s: possibly unsafe /tmp permissions\n", program);
      exit(1);
    }

    for ( i = 0 ; ; i++ ) {
      snprintf(mntname, sizeof mntname, "syslinux.mnt.%lu.%d",
	       (unsigned long)mypid, i);

      if ( lstat(mntname, &dst) != -1 || errno != ENOENT )
	continue;

      seteuid(0);		/* *** BECOME ROOT *** */
      rv = mkdir(mntname, 0000); /* AS ROOT */
      seteuid(ruid);

      if ( rv == -1 ) {
	if ( errno == EEXIST || errno == EINTR )
	  continue;
	perror(program);
	exit(1);
      }

      if ( lstat(mntname, &dst) || dst.st_mode != (S_IFDIR|0000) ||
	   dst.st_uid != 0 ) {
	fprintf(stderr, "%s: someone is trying to symlink race us!\n", program);
	exit(1);
      }
      break;			/* OK, got something... */
    }

    mntpath = mntname;

    snprintf(devfdname, sizeof devfdname, "/proc/%lu/fd/%d",
	     (unsigned long)mypid, dev_fd);

    f = fork();
    if ( f < 0 ) {
      perror(program);
      rmdir(mntpath);
      exit(1);
    } else if ( f == 0 ) {
      char mnt_opts[128];
      seteuid(0);		/* ***BECOME ROOT*** */
      setuid(0);
      if ( S_ISREG(st.st_mode) ) {
	snprintf(mnt_opts, sizeof mnt_opts,
		 "rw,nodev,noexec,nosuid,loop,offset=%" PRIdMAX ",umask=077,uid=%lu",
		 (uintmax_t)offset, (unsigned long)ruid);
      } else {
	snprintf(mnt_opts, sizeof mnt_opts,
		 "rw,nodev,noexec,nosuid,umask=077,uid=%lu",
		 (unsigned long)ruid);
      }
      /* We're root, use clean_environ */
      execle(_PATH_MOUNT, _PATH_MOUNT, "-t", "msdos", "-o", mnt_opts,\
	     devfdname, mntpath, NULL, clean_environ);
      _exit(255);		/* execl failed */
    }
  }

  w = waitpid(f, &status, 0);
  if ( w != f || status ) {
    if ( !euid )
      rmdir(mntpath);
    exit(1);			/* Mount failed */
  }

  ldlinux_name = alloca(strlen(mntpath)+13);
  if ( !ldlinux_name ) {
    perror(program);
    err = 1;
    goto umount;
  }
  sprintf(ldlinux_name, "%s/ldlinux.sys", mntpath);

  unlink(ldlinux_name);
  fd = open(ldlinux_name, O_WRONLY|O_CREAT|O_TRUNC, 0444);
  if ( fd < 0 ) {
    perror(device);
    err = 1;
    goto umount;
  }

  cdp = syslinux_ldlinux;
  left = syslinux_ldlinux_len;
  while ( left ) {
    nb = write(fd, cdp, left);
    if ( nb == -1 && errno == EINTR )
      continue;
    else if ( nb <= 0 ) {
      perror(device);
      err = 1;
      goto umount;
    }

    dp += nb;
    left -= nb;
  }

  /*
   * I don't understand why I need this.  Does the DOS filesystems
   * not honour the mode passed to open()?
   */
  my_umask = umask(0777);
  umask(my_umask);
  fchmod(fd, 0444 & ~my_umask);

  close(fd);

umount:
  f = fork();
  if ( f < 0 ) {
    perror("fork");
    exit(1);
  } else if ( f == 0 ) {
    seteuid(0);		/* ***BECOME ROOT*** */
    setuid(0);
    execle(_PATH_UMOUNT, _PATH_UMOUNT, mntpath, NULL, clean_environ);
  }

  w = waitpid(f, &status, 0);
  if ( w != f || status ) {
    exit(1);
  }

  sync();

  if ( !euid ) {
    seteuid(0);			/* *** BECOME ROOT *** */
    rmdir(mntpath);		/* AS ROOT */
    seteuid(ruid);
  }

  if ( err )
    exit(err);

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
