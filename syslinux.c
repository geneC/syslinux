#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998-1999 H. Peter Anvin - All Rights Reserved
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
 *
 */

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef _PATH_MOUNT
#define _PATH_MOUNT "/bin/mount"
#endif

#ifndef _PATH_UMOUNT
#define _PATH_UMOUNT "/bin/umount"
#endif

extern unsigned char bootsect[];
extern unsigned int  bootsect_len;

extern unsigned char ldlinux[];
extern unsigned int  ldlinux_len;

extern void make_stupid(void);	/* Patching routine for stupid mode */

char *program;			/* Name of program */
char *device;			/* Device to install to */
uid_t euid;			/* Our current euid */

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

#define bsCopyStart bsBytesPerSec
#define bsCopyLen   (bsCode-bsBytesPerSec)

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

void usage(void)
{
  fprintf(stderr, "Usage: %s [-sf] device\n", program);
  exit(1);
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
  char *mntpath = NULL, mntname[64];
  char *ldlinux_name, **argp, *opt;
  int my_umask;
  int force = 0;		/* -f (force) option */

  program = argv[0];
  
  device = NULL;

  for ( argp = argv+1 ; *argp ; argp++ ) {
    if ( **argp == '-' ) {
      opt = *argp + 1;
      if ( !*opt )
	usage();

      while ( *opt ) {
	if ( *opt == 's' ) {
	  make_stupid();	/* Use "safe, slow and stupid" code */
	} else if ( *opt == 'f' ) {
	  force = 1;
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

  left = 512;
  dp = sectbuf;
  while ( left ) {
    nb = read(dev_fd, dp, left);
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
  close(dev_fd);

  sync();
  
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
  if ( (euid = geteuid()) ) {
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
	   hasmntopt(mnt, "user") &&
	   !hasmntopt(mnt, "ro") &&
	   mnt->mnt_dir[0] == '/' &&
	   !!hasmntopt(mnt, "loop") == !!S_ISREG(st.st_mode)) {
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
      execl(_PATH_MOUNT, _PATH_MOUNT, mntpath, NULL);
      _exit(255);		/* If execl failed, trouble... */
    }
  } else {
    int i = 0;

    /* We're root.  Make a temp dir and pass all the gunky options to mount. */

    do {
      sprintf(mntname, "/tmp/syslinux.mnt.%lu.%d", (unsigned long)getpid(), i++);
    } while ( (errno = 0, mkdir(mntname, 0700) == -1) &&
	      (errno == EEXIST || errno == EINTR) );
    if ( errno ) {
      perror(program);
      exit(1);
    }
    
    mntpath = mntname;

    f = fork();
    if ( f < 0 ) {
      perror(program);
      rmdir(mntpath);
      exit(1);
    } else if ( f == 0 ) {
      if ( S_ISREG(st.st_mode) )
	execl(_PATH_MOUNT, _PATH_MOUNT, "-t", "msdos", "-o", "loop", "-w",
	      device, mntpath, NULL);
      else
	execl(_PATH_MOUNT, _PATH_MOUNT, "-t", "msdos", "-w", device, mntpath,
	      NULL);
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
    perror("malloc");
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

  cdp = ldlinux;
  left = ldlinux_len;
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
    execl(_PATH_UMOUNT, _PATH_UMOUNT, mntpath, NULL);
  }

  w = waitpid(f, &status, 0);
  if ( w != f || status ) {
    exit(1);
  }

  sync();

  if ( !euid )
    rmdir(mntpath);

  if ( err )
    exit(err);

  /*
   * To finish up, write the boot sector
   */
  dev_fd = open(device, O_RDWR);
  if ( dev_fd < 0 ) {
    perror(device);
    exit(1);
  }

  /* Copy the old superblock into the new boot sector */
  memcpy(bootsect+bsCopyStart, sectbuf+bsCopyStart, bsCopyLen);
  
  dp = bootsect;
  left = bootsect_len;
  while ( left ) {
    nb = write(dev_fd, dp, left);
    if ( nb == -1 && errno == EINTR )
      continue;
    else if ( nb <= 0 ) {
      perror(device);
      exit(1);
    }
    
    dp += nb;
    left -= nb;
  }
  close(dev_fd);

  sync();

  /* Done! */

  return 0;
}
