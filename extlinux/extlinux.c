/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1998-2005 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * extlinux.c
 *
 * Install the extlinux boot block on an ext2/3 filesystem
 */

#define  _GNU_SOURCE		/* Enable everything */
#include <inttypes.h>
/* This is needed to deal with the kernel headers imported into glibc 3.3.3. */
typedef uint64_t u64;
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#ifndef __KLIBC__
#include <mntent.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>

#include <linux/fd.h>		/* Floppy geometry */
#include <linux/hdreg.h>	/* Hard disk geometry */
#include <linux/fs.h>		/* FIGETBSZ, FIBMAP */

#include "ext2_fs.h"
#include "../version.h"

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

/* Global option handling */

const char *program;

/* These are the options we can set and their values */
struct my_options {
  unsigned int sectors;
  unsigned int heads;
} opt = {
  .sectors = 0,
  .heads = 0,
};

static void __attribute__((noreturn)) usage(int rv)
{
  fprintf(stderr,
	  "Usage: %s [options] directory\n"
	  "  --install    -i  Install over the current bootsector\n"
	  "  --update     -U  Update a previous EXTLINUX installation\n"
	  "  --zip        -z  Force zipdrive geometry (-H 64 -S 32)\n"
	  "  --sectors=#  -S  Force the number of sectors per track\n"
	  "  --heads=#    -H  Force number of heads\n"
	  "\n"
	  "  Note: geometry is determined at boot time for devices which\n"
	  "  are considered hard disks by the BIOS.  Unfortunately, this is\n"
	  "  not possible for devices which are considered floppy disks,\n"
	  "  which includes zipdisks and LS-120 superfloppies.\n"
	  "\n"
	  "  The -z option is useful for USB devices which are considered\n"
	  "  hard disks by some BIOSes and zipdrives by other BIOSes.\n",
	  program);

  exit(rv);
}

static const struct option long_options[] = {
  { "install",  0, NULL, 'i' },
  { "update",   0, NULL, 'U' },
  { "zipdrive", 0, NULL, 'z' },
  { "sectors",  1, NULL, 'S' },
  { "heads",    1, NULL, 'H' },
  { "version",  0, NULL, 'v' },
  { "help",     0, NULL, 'h' },
  { 0, 0, 0, 0 }
};

static const char short_options[] = "iUuzS:H:vh";



#if defined(__linux__) && !defined(BLKGETSIZE64)
/* This takes a u64, but the size field says size_t.  Someone screwed big. */
# define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif

#define LDLINUX_MAGIC	0x3eb202fe

enum bs_offsets {
  bsJump            = 0x00,
  bsOemName         = 0x03,
  bsBytesPerSec     = 0x0b,
  bsSecPerClust     = 0x0d,
  bsResSectors      = 0x0e,
  bsFATs            = 0x10,
  bsRootDirEnts     = 0x11,
  bsSectors         = 0x13,
  bsMedia           = 0x15,
  bsFATsecs         = 0x16,
  bsSecPerTrack     = 0x18,
  bsHeads           = 0x1a,
  bsHiddenSecs      = 0x1c,
  bsHugeSectors     = 0x20,

  /* FAT12/16 only */
  bs16DriveNumber   = 0x24,
  bs16Reserved1     = 0x25,
  bs16BootSignature = 0x26,
  bs16VolumeID      = 0x27,
  bs16VolumeLabel   = 0x2b,
  bs16FileSysType   = 0x36,
  bs16Code          = 0x3e,

  /* FAT32 only */
  bs32FATSz32       = 36,
  bs32ExtFlags      = 40,
  bs32FSVer         = 42,
  bs32RootClus      = 44,
  bs32FSInfo        = 48,
  bs32BkBootSec     = 50,
  bs32Reserved      = 52,
  bs32DriveNumber   = 64,
  bs32Reserved1     = 65,
  bs32BootSignature = 66,
  bs32VolumeID      = 67,
  bs32VolumeLabel   = 71,
  bs32FileSysType   = 82,
  bs32Code          = 90,

  bsSignature     = 0x1fe
};

#define bsHead      bsJump
#define bsHeadLen   (bsOemName-bsHead)
#define bsCode	    bs32Code	/* The common safe choice */
#define bsCodeLen   (bsSignature-bs32Code)

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static inline uint8_t get_8(const unsigned char *p)
{
  return *(const uint8_t *)p;
}

static inline uint16_t get_16(const unsigned char *p)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  return *(const uint16_t *)p;
#else
  return (uint16_t)p[0] + ((uint16_t)p[1] << 8);
#endif
}

static inline uint32_t get_32(const unsigned char *p)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  return *(const uint32_t *)p;
#else
  return (uint32_t)p[0] + ((uint32_t)p[1] << 8) +
    ((uint32_t)p[2] << 16) + ((uint32_t)p[3] << 24);
#endif
}

static inline void set_16(unsigned char *p, uint16_t v)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  *(uint16_t *)p = v;
#else
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
#endif
}

static inline void set_32(unsigned char *p, uint32_t v)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  *(uint32_t *)p = v;
#else
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
  p[2] = ((v >> 16) & 0xff);
  p[3] = ((v >> 24) & 0xff);
#endif
}

#ifndef EXT2_SUPER_OFFSET
#define EXT2_SUPER_OFFSET 1024
#endif

#define SECTOR_SHIFT	9	/* 512-byte sectors */
#define SECTOR_SIZE	(1 << SECTOR_SHIFT)

const char *program;

/*
 * Boot block
 */
extern unsigned char extlinux_bootsect[];
extern unsigned int  extlinux_bootsect_len;
#define boot_block	extlinux_bootsect
#define boot_block_len  extlinux_bootsect_len

/*
 * Image file
 */
extern unsigned char extlinux_image[];
extern unsigned int  extlinux_image_len;
#define boot_image	extlinux_image
#define boot_image_len  extlinux_image_len

/*
 * Common abort function
 */
void __attribute__((noreturn)) die(const char *msg)
{
  fputs(msg, stderr);
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
 * Produce file map
 */
int
sectmap(int fd, uint32_t *sectors, int nsectors)
{
  unsigned int blksize, blk, nblk;
  unsigned int i;

  /* Get block size */
  if ( ioctl(fd, FIGETBSZ, &blksize) )
    return -1;

  /* Number of sectors per block */
  blksize >>= SECTOR_SHIFT;

  nblk = 0;
  while ( nsectors ) {

    blk = nblk++;
    dprintf("querying block %u\n", blk);
    if ( ioctl(fd, FIBMAP, &blk) )
      return -1;

    blk *= blksize;
    for ( i = 0 ; i < blksize ; i++ ) {
      if ( !nsectors )
	return 0;

      dprintf("Sector: %10u\n", blk);
      *sectors++ = blk++;
      nsectors--;
    }
  }

  return 0;
}

/*
 * Get the size of a block device
 */
uint64_t get_size(int devfd)
{
  uint64_t bytes;
  uint32_t sects;
  struct stat st;

#ifdef BLKGETSIZE64
  if ( !ioctl(devfd, BLKGETSIZE64, &bytes) )
    return bytes;
#endif
  if ( !ioctl(devfd, BLKGETSIZE, &sects) )
    return (uint64_t)sects << 9;
  else if ( !fstat(devfd, &st) && st.st_size )
    return st.st_size;
  else
    return 0;
}


/*
 * Get device geometry and partition offset
 */
struct geometry_table {
  uint64_t bytes;
  struct hd_geometry g;
};

/* Standard floppy disk geometries, plus LS-120.  Zipdisk geometry
   (x/64/32) is the final fallback.  I don't know what LS-240 has
   as its geometry, since I don't have one and don't know anyone that does,
   and Google wasn't helpful... */
static const struct geometry_table standard_geometries[] = {
  {    360*1024, {  2,  9,  40, 0 } },
  {    720*1024, {  2,  9,  80, 0 } },
  {   1200*1024, {  2, 15,  80, 0 } },
  {   1440*1024, {  2, 18,  80, 0 } },
  {   1680*1024, {  2, 21,  80, 0 } },
  {   1722*1024, {  2, 21,  80, 0 } },
  {   2880*1024, {  2, 36,  80, 0 } },
  {   3840*1024, {  2, 48,  80, 0 } },
  { 123264*1024, {  8, 32, 963, 0 } }, /* LS120 */
  { 0, {0,0,0,0} }
};

int
get_geometry(int devfd, uint64_t totalbytes, struct hd_geometry *geo)
{
  struct floppy_struct fd_str;
  const struct geometry_table *gp;

  memset(geo, 0, sizeof *geo);

  if ( !ioctl(devfd, HDIO_GETGEO, &geo) ) {
    return 0;
  } else if ( !ioctl(devfd, FDGETPRM, &fd_str) ) {
    geo->heads     = fd_str.head;
    geo->sectors   = fd_str.sect;
    geo->cylinders = fd_str.track;
    geo->start     = 0;
    return 0;
  }

  /* Didn't work.  Let's see if this is one of the standard geometries */
  for ( gp = standard_geometries ; gp->bytes ; gp++ ) {
    if ( gp->bytes == totalbytes ) {
      memcpy(geo, &gp->g, sizeof *geo);
      return 0;
    }
  }

  /* Didn't work either... assign a geometry of 64 heads, 32 sectors; this is
     what zipdisks use, so this would help if someone has a USB key that
     they're booting in USB-ZIP mode. */

  geo->heads     = opt.heads ?: 64;
  geo->sectors   = opt.sectors ?: 32;
  geo->cylinders = totalbytes/(geo->heads*geo->sectors << SECTOR_SHIFT);
  geo->start     = 0;

  if ( !opt.sectors && !opt.heads )
    fprintf(stderr, "Warning: unable to obtain device geometry (defaulting to %d heads, %d sectors)\n"
	    "         (on hard disks, this is usually harmless.)\n",
	    geo->heads, geo->sectors);

  return 1;
}

/*
 * Query the device geometry and put it into the boot sector.
 * Map the file and put the map in the boot sector and file.
 * Stick the "current directory" inode number into the file.
 */
void
patch_file_and_bootblock(int fd, int dirfd, int devfd)
{
  struct stat dirst;
  struct hd_geometry geo;
  uint32_t *sectp;
  uint64_t totalbytes, totalsectors;
  int nsect;
  unsigned char *p, *patcharea;
  int i, dw;
  uint32_t csum;

  if ( fstat(dirfd, &dirst) ) {
    perror("fstat dirfd");
    exit(255);			/* This should never happen */
  }

  totalbytes = get_size(devfd);
  get_geometry(devfd, totalbytes, &geo);

  if ( opt.heads )
    geo.heads = opt.heads;
  if ( opt.sectors )
    geo.sectors = opt.sectors;

  /* Patch this into a fake FAT superblock.  This isn't because
     FAT is a good format in any way, it's because it lets the
     early bootstrap share code with the FAT version. */
  dprintf("heads = %u, sect = %u\n", geo.heads, geo.sectors);

  totalsectors = totalbytes >> SECTOR_SHIFT;
  if ( totalsectors >= 65536 ) {
    set_16(boot_block+bsSectors, 0);
  } else {
    set_16(boot_block+bsSectors, totalsectors);
  }
  set_32(boot_block+bsHugeSectors, totalsectors);

  set_16(boot_block+bsBytesPerSec, SECTOR_SIZE);
  set_16(boot_block+bsSecPerTrack, geo.sectors);
  set_16(boot_block+bsHeads, geo.heads);
  set_32(boot_block+bsHiddenSecs, geo.start);

  /* Construct the boot file */

  dprintf("directory inode = %lu\n", (unsigned long) dirst.st_ino);
  nsect = (boot_image_len+SECTOR_SIZE-1) >> SECTOR_SHIFT;
  sectp = alloca(sizeof(uint32_t)*nsect);
  if ( sectmap(fd, sectp, nsect) ) {
    perror("bmap");
    exit(1);
  }

  /* First sector need pointer in boot sector */
  set_32(boot_block+0x1F8, *sectp++);
  nsect--;

  /* Search for LDLINUX_MAGIC to find the patch area */
  for ( p = boot_image ; get_32(p) != LDLINUX_MAGIC ; p += 4 );
  patcharea = p+8;

  /* Set up the totals */
  dw = boot_image_len >> 2; /* COMPLETE dwords! */
  set_16(patcharea, dw);
  set_16(patcharea+2, nsect);	/* Does not include the first sector! */
  set_32(patcharea+8, dirst.st_ino); /* "Current" directory */

  /* Set the sector pointers */
  p = patcharea+12;

  memset(p, 0, 64*4);
  while ( nsect-- ) {
    set_32(p, *sectp++);
    p += 4;
  }

  /* Now produce a checksum */
  set_32(patcharea+4, 0);

  csum = LDLINUX_MAGIC;
  for ( i = 0, p = boot_image ; i < dw ; i++, p += 4 )
    csum -= get_32(p);		/* Negative checksum */

  set_32(patcharea+4, csum);
}

/*
 * Install the boot block on the specified device.
 * Must be run AFTER install_file()!
 */
int
install_bootblock(int fd, const char *device)
{
  struct ext2_super_block sb;

  if ( xpread(fd, &sb, sizeof sb, EXT2_SUPER_OFFSET) != sizeof sb ) {
    perror("reading superblock");
    return 1;
  }

  if ( sb.s_magic != EXT2_SUPER_MAGIC ) {
    fprintf(stderr, "no ext2/ext3 superblock found on %s\n", device);
    return 1;
  }

  if ( xpwrite(fd, boot_block, boot_block_len, 0) != boot_block_len ) {
    perror("writing bootblock");
    return 1;
  }

  return 0;
}

int
install_file(const char *path, int devfd, struct stat *rst)
{
  char *file;
  int fd = -1, dirfd = -1, flags;
  struct stat st;

  asprintf(&file, "%s%sextlinux.sys",
	   path,
	   path[0] && path[strlen(path)-1] == '/' ? "" : "/");
  if ( !file ) {
    perror(program);
    return 1;
  }

  dirfd = open(path, O_RDONLY|O_DIRECTORY);
  if ( dirfd < 0 ) {
    perror(path);
    goto bail;
  }

  fd = open(file, O_RDONLY);
  if ( fd < 0 ) {
    if ( errno != ENOENT ) {
      perror(file);
      goto bail;
    }
  } else {
    /* If file exist, remove the immutable flag and set u+w mode */
    if ( !ioctl(fd, EXT2_IOC_GETFLAGS, &flags) ) {
      flags &= ~EXT2_IMMUTABLE_FL;
      ioctl(fd, EXT2_IOC_SETFLAGS, &flags);
    }
    if ( !fstat(fd, &st) ) {
      fchmod(fd, st.st_mode | S_IWUSR);
    }
  }
  close(fd);

  fd = open(file, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IRGRP|S_IROTH);
  if ( fd < 0 ) {
    perror(file);
    goto bail;
  }

  /* Write it the first time */
  if ( xpwrite(fd, boot_image, boot_image_len, 0) != boot_image_len ) {
    fprintf(stderr, "%s: write failure on %s\n", program, file);
    goto bail;
  }

  /* Map the file, and patch the initial sector accordingly */
  patch_file_and_bootblock(fd, dirfd, devfd);

  /* Write it again - this relies on the file being overwritten in place! */
  if ( xpwrite(fd, boot_image, boot_image_len, 0) != boot_image_len ) {
    fprintf(stderr, "%s: write failure on %s\n", program, file);
    goto bail;
  }

  /* Attempt to set immutable flag and remove all write access */
  /* Only set immutable flag if file is owned by root */
  if ( !fstat(fd, &st) ) {
    fchmod(fd, st.st_mode & (S_IRUSR|S_IRGRP|S_IROTH));
    if ( st.st_uid == 0 && !ioctl(fd, EXT2_IOC_GETFLAGS, &flags) ) {
      flags |= EXT2_IMMUTABLE_FL;
      ioctl(fd, EXT2_IOC_SETFLAGS, &flags);
    }
  }

  if ( fstat(fd, rst) ) {
    perror(file);
    goto bail;
  }

  close(dirfd);
  close(fd);
  return 0;

 bail:
  if ( dirfd >= 0 )
    close(dirfd);
  if ( fd >= 0 )
    close(fd);

  return 1;
}

/* EXTLINUX installs the string 'EXTLINUX' at offset 3 in the boot
   sector; this is consistent with FAT filesystems. */
int
already_installed(int devfd)
{
  char buffer[8];

  xpread(devfd, buffer, 8, 3);
  return !memcmp(buffer, "EXTLINUX", 8);
}

int
install_loader(const char *path, int update_only)
{
  struct stat st, fst;
  int devfd, rv;
  const char *devname = NULL;
#ifndef __KLIBC__
  struct mntent *mnt = NULL;
  struct stat dst;
  FILE *mtab;
#endif

  if ( stat(path, &st) || !S_ISDIR(st.st_mode) ) {
    fprintf(stderr, "%s: Not a directory: %s\n", program, path);
    return 1;
  }

  devfd = -1;

#ifdef __KLIBC__

  /* klibc doesn't have getmntent and friends; instead, just create
     a new device with the appropriate device type */
  
  {
    static char devname_buf[64];

    snprintf(devname_buf, sizeof devname_buf, "/tmp/dev-%u:%u",
	     major(st.st_dev), minor(st.st_dev));
    
    if (mknod(devname_buf, 0600, st.st_dev)) {
      fprintf(stderr, "%s: cannot create device %s\n", program, devname);
      return 1;
    }

    devname = devname_buf;
  }

#else

  if ( (mtab = setmntent("/proc/mounts", "r")) ) {
    while ( (mnt = getmntent(mtab)) ) {
      if ( (!strcmp(mnt->mnt_type, "ext2") ||
	    !strcmp(mnt->mnt_type, "ext3")) &&
	   !stat(mnt->mnt_fsname, &dst) &&
	   dst.st_rdev == st.st_dev ) {
	devname = mnt->mnt_fsname;
	break;
      }
    }
  }

  if ( !devname ) {
    /* Didn't find it in /proc/mounts, try /etc/mtab */
    if ( (mtab = setmntent("/etc/mtab", "r")) ) {
      while ( (mnt = getmntent(mtab)) ) {
	devname = mnt->mnt_fsname;
	break;
      }
    }
  }

  if ( !devname ) {
    fprintf(stderr, "%s: cannot find device for path %s\n", program, path);
    return 1;
  }

  fprintf(stderr, "%s is device %s\n", path, devname);

#endif

  if ( (devfd = open(devname, O_RDWR|O_SYNC)) < 0 ) {
    fprintf(stderr, "%s: cannot open device %s\n", program, devname);
    return 1;
  }

  if ( update_only && !already_installed(devfd) ) {
    fprintf(stderr, "%s: no previous extlinux boot sector found\n", program);
    return 1;
  }

  install_file(path, devfd, &fst);

  if ( fst.st_dev != st.st_dev ) {
    fprintf(stderr, "%s: file system changed under us - aborting!\n",
	    program);
    return 1;
  }

  sync();
  rv = install_bootblock(devfd, devname);
  close(devfd);
  sync();

#ifdef __KLIBC__
  unlink(devname);
#else
  endmntent(mtab);
#endif

  if ( rv ) return rv;

  return 0;
}

int
main(int argc, char *argv[])
{
  int o;
  const char *directory;
  int update_only = -1;

  program = argv[0];

  while ( (o = getopt_long(argc, argv, short_options,
			     long_options, NULL)) != EOF ) {
    switch ( o ) {
    case 'z':
      opt.heads = 64;
      opt.sectors = 32;
      break;
    case 'S':
      opt.sectors = strtoul(optarg, NULL, 0);
      if ( opt.sectors < 1 || opt.sectors > 63 ) {
	fprintf(stderr, "%s: invalid number of sectors: %u (must be 1-63)\n",
		program, opt.sectors);
	exit(EX_USAGE);
      }
      break;
    case 'H':
      opt.heads = strtoul(optarg, NULL, 0);
      if ( opt.heads < 1 || opt.heads > 256 ) {
	fprintf(stderr, "%s: invalid number of heads: %u (must be 1-256)\n",
		program, opt.heads);
	exit(EX_USAGE);
      }
      break;
    case 'i':
      update_only = 0;
      break;
    case 'u':
    case 'U':
      update_only = 1;
      break;
    case 'h':
      usage(0);
      break;
    case 'v':
      fputs("extlinux " VERSION "\n", stderr);
      exit(0);
    default:
      fprintf(stderr, "%s: Unknown option: %c\n", program, optopt);
      exit(EX_USAGE);
    }
  }

  directory = argv[optind];

  if ( !directory )
    usage(EX_USAGE);

  if ( update_only == -1 ) {
    fprintf(stderr, "%s: warning: a future version will require --install or --update\n",
	    program);
    update_only = 0;
  }

  return install_loader(directory, update_only);
}
