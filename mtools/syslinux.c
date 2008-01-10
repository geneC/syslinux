/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
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
#include "libfat.h"

char *program;			/* Name of program */
char *device;			/* Device to install to */
pid_t mypid;
off_t filesystem_offset = 0;	/* Offset of filesystem */

void __attribute__((noreturn)) usage(void)
{
  fprintf(stderr, "Usage: %s [-sfr][-d directory][-o offset] device\n", program);
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
ssize_t xpread(int fd, void *buf, size_t count, off_t offset)
{
  char *bufp = (char *)buf;
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pread(fd, bufp, count, offset);
    if ( rv == 0 ) {
      die("short read");
    } else if ( rv == -1 ) {
      if ( errno == EINTR ) {
	continue;
      } else {
	die(strerror(errno));
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

ssize_t xpwrite(int fd, const void *buf, size_t count, off_t offset)
{
  const char *bufp = (const char *)buf;
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pwrite(fd, bufp, count, offset);
    if ( rv == 0 ) {
      die("short write");
    } else if ( rv == -1 ) {
      if ( errno == EINTR ) {
	continue;
      } else {
	die(strerror(errno));
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

/*
 * Version of the read function suitable for libfat
 */
int libfat_xpread(intptr_t pp, void *buf, size_t secsize, libfat_sector_t sector)
{
  off_t offset = (off_t)sector * secsize + filesystem_offset;
  return xpread(pp, buf, secsize, offset);
}


int main(int argc, char *argv[])
{
  static unsigned char sectbuf[512];
  int dev_fd;
  struct stat st;
  int status;
  char **argp, *opt;
  char mtools_conf[] = "/tmp/syslinux-mtools-XXXXXX";
  const char *subdir = NULL;
  int mtc_fd;
  FILE *mtc, *mtp;
  struct libfat_filesystem *fs;
  libfat_sector_t s, *secp, sectors[65]; /* 65 is maximum possible */
  int32_t ldlinux_cluster;
  int nsectors;
  const char *errmsg;

  int force = 0;              /* -f (force) option */
  int stupid = 0;             /* -s (stupid) option */
  int raid_mode = 0;          /* -r (RAID) option */

  (void)argc;			/* Unused */

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
	  stupid = 1;
	} else if ( *opt == 'r' ) {
	  raid_mode = 1;
	} else if ( *opt == 'f' ) {
	  force = 1;		/* Force install */
	} else if ( *opt == 'd' && argp[1] ) {
	  subdir = *++argp;
	} else if ( *opt == 'o' && argp[1] ) {
	  filesystem_offset = (off_t)strtoull(*++argp, NULL, 0); /* Byte offset */
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

  xpread(dev_fd, sectbuf, 512, filesystem_offset);

  /*
   * Check to see that what we got was indeed an MS-DOS boot sector/superblock
   */
  if( (errmsg = syslinux_check_bootsect(sectbuf)) ) {
    die(errmsg);
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
	  /* "MTOOLS_NO_VFAT=1\n" */
	  "MTOOLS_SKIP_CHECK=1\n" /* Needed for some flash memories */
	  "drive s:\n"
	  "  file=\"/proc/%lu/fd/%d\"\n"
	  "  offset=%llu\n",
	  (unsigned long)mypid,
	  dev_fd,
	  (unsigned long long)filesystem_offset);
  fclose(mtc);

  /*
   * Run mtools to create the LDLINUX.SYS file
   */
  if ( setenv("MTOOLSRC", mtools_conf, 1) ) {
    perror(program);
    exit(1);
  }

  /* This command may fail legitimately */
  system("mattrib -h -r -s s:/ldlinux.sys 2>/dev/null");

  mtp = popen("mcopy -D o -D O -o - s:/ldlinux.sys", "w");
  if ( !mtp ||
       (fwrite(syslinux_ldlinux, 1, syslinux_ldlinux_len, mtp)
	!= syslinux_ldlinux_len) ||
       (status = pclose(mtp), !WIFEXITED(status) || WEXITSTATUS(status)) ) {
    die("failed to create ldlinux.sys");
  }

  /*
   * Now, use libfat to create a block map
   */
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

  /* Patch ldlinux.sys and the boot sector */
  syslinux_patch(sectors, nsectors, stupid, raid_mode);

  /* Write the now-patched first sector of ldlinux.sys */
  xpwrite(dev_fd, syslinux_ldlinux, 512,
	  filesystem_offset + ((off_t)sectors[0] << 9));

  /* Move ldlinux.sys to the desired location */
  if (subdir) {
    char target_file[4096], command[5120];
    char *cp = target_file, *ep = target_file+sizeof target_file-16;
    const char *sd;
    int slash = 1;

    cp += sprintf(cp, "'s:/");
    for (sd = subdir; *sd; sd++) {
      if (*sd == '/' || *sd == '\\') {
	if (slash)
	  continue;		/* Remove duplicated slashes */
	slash = 1;
      } else if (*sd == '\'' || *sd == '!') {
	slash = 0;
	if (cp < ep) *cp++ = '\'';
	if (cp < ep) *cp++ = '\\';
	if (cp < ep) *cp++ = *sd;
	if (cp < ep) *cp++ = '\'';
	continue;
      } else {
	slash = 0;
      }

      if (cp < ep)
	*cp++ = *sd;
    }
    if (!slash)
      *cp++ = '/';
    strcpy(cp, "ldlinux.sys'");

    /* This command may fail legitimately */
    sprintf(command, "mattrib -h -r -s %s 2>/dev/null", target_file);
    system(command);

    sprintf(command, "mmove -D o -D O s:/ldlinux.sys %s", target_file);
    status = system(command);

    if ( !WIFEXITED(status) || WEXITSTATUS(status) ) {
      fprintf(stderr,
	      "%s: warning: unable to move ldlinux.sys\n",
	    program);

      status = system("mattrib +r +h +s s:/ldlinux.sys");
    } else {
      sprintf(command, "mattrib +r +h +s %s", target_file);
      status = system(command);
    }
  } else {
    status = system("mattrib +r +h +s s:/ldlinux.sys");
  }

  if ( !WIFEXITED(status) || WEXITSTATUS(status) ) {
    fprintf(stderr,
	    "%s: warning: failed to set system bit on ldlinux.sys\n",
	    program);
  }


  /*
   * Cleanup
   */
  unlink(mtools_conf);

  /*
   * To finish up, write the boot sector
   */

  /* Read the superblock again since it might have changed while mounted */
  xpread(dev_fd, sectbuf, 512, filesystem_offset);

  /* Copy the syslinux code into the boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write new boot sector */
  xpwrite(dev_fd, sectbuf, 512, filesystem_offset);

  close(dev_fd);
  sync();

  /* Done! */

  return 0;
}
