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
 * extlinux.c
 *
 * Install the extlinux boot block on an ext2/3 filesystem
 */

#define  _GNU_SOURCE		/* Enable everything */
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/ext2_fs.h>
#include <linux/fd.h>		/* Floppy geometry */
#include <linux/hdreg.h>	/* Hard disk geometry */
#include <linux/fs.h>		/* FIGETBSZ, FIBMAP */

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
 * pread() and pwrite() augmented with retry on short access or EINTR.
 */
ssize_t xpread(int fd, void *buf, size_t count, off_t offset)
{
  ssize_t rv;
  ssize_t done = 0;

  while ( count ) {
    rv = pread(fd, buf, count, offset);
    if ( rv == 0 ) {
      die("short read");
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
      die("short write");
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
    printf("querying block %u\n", blk);
    if ( ioctl(fd, FIBMAP, &blk) )
      return -1;

    blk *= blksize;
    for ( i = 0 ; i < blksize ; i++ ) {
      if ( !nsectors )
	return 0;

      printf("Sector: %10u\n", blk);
      *sectors++ = blk++;
      nsectors--;
    }
  }

  return 0;
}

/*
 * Query the device geometry and put it into the boot sector.
 * Map the file and put the map in the boot sector and file.
 * Stick the "current directory" inode number into the file.
 */
void
patch_file_and_bootblock(int fd, int dirfd)
{
  struct stat dirst;
  struct hd_geometry   hd_geo;
  struct floppy_struct fd_str;
  uint16_t heads, cylinders;
  uint32_t sectors, offset;
  uint32_t *sectp;
  int nsect;
  unsigned char *p, *patcharea;
  int i, dw;
  uint32_t csum;

  if ( ioctl(fd, HDIO_GETGEO, &hd_geo) == 0 ) {
    heads     = hd_geo.heads;
    sectors   = hd_geo.sectors;
    cylinders = hd_geo.cylinders;
    offset    = hd_geo.start;
  } else if ( ioctl(fd, FDGETPRM, &fd_str) == 0 ) {
    heads     = fd_str.head;
    sectors   = fd_str.sect;
    cylinders = fd_str.track;
    offset    = 0;
  } else {
    fprintf(stderr, "Warning: unable to obtain device geometry (may or may not work)\n");
    heads = sectors = cylinders = offset = 0;
  }

  if ( fstat(dirfd, &dirst) ) {
    perror("fstat dirfd");
    exit(255);			/* This should never happen */
  }

  /* Patch this into a fake FAT superblock.  This isn't because
     FAT is a good format in any way, it's because it lets the
     early bootstrap share code with the FAT version. */
  printf("cyl = %u, heads = %u, sect = %u\n", cylinders, heads, sectors);

  set_16(boot_block+bsBytesPerSec, 512);
  set_16(boot_block+bsSecPerTrack, sectors);
  set_16(boot_block+bsHeads, heads);
  set_32(boot_block+bsHiddenSecs, offset);

  /* Construct the boot file */

  printf("directory inode = %lu\n", (unsigned long) dirst.st_ino);
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
install_bootblock(int fd, char *device)
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
install_file(char *path, struct stat *rst)
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
  patch_file_and_bootblock(fd, dirfd);
  patch_file_and_bootblock(fd, dirfd);

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

int
install_loader(char *path)
{
  struct stat st, dst, fst;
  struct mntent *mnt;
  int devfd, rv;
  FILE *mtab;

  if ( stat(path, &st) || !S_ISDIR(st.st_mode) ) {
    fprintf(stderr, "%s: Not a directory: %s\n", program, path);
    return 1;
  }
  
  if ( !(mtab = setmntent("/etc/mtab", "r")) ) {
    fprintf(stderr, "%s: cannot open /etc/mtab\n", program);
    return 1;
  }

  devfd = -1;
  while ( (mnt = getmntent(mtab)) ) {
    if ( (!strcmp(mnt->mnt_type, "ext2") ||
	  !strcmp(mnt->mnt_type, "ext3")) &&
	 !stat(mnt->mnt_fsname, &dst) &&
	 dst.st_rdev == st.st_dev ) {
      fprintf(stderr, "%s is device %s\n", path, mnt->mnt_fsname);
      if ( (devfd = open(mnt->mnt_fsname, O_RDWR|O_SYNC)) < 0 ) {
	fprintf(stderr, "%s: cannot open device %s\n", program, mnt->mnt_fsname);
	return 1;
      }
      break;
    }
  }

  if ( devfd < 0 ) {
    fprintf(stderr, "%s: cannot find device for path %s", program, path);
    return 1;
  }

  install_file(path, &fst);

  if ( fst.st_dev != st.st_dev ) {
    fprintf(stderr, "%s: file system changed under us - aborting!\n",
	    program);
    return 1;
  }

  sync();
  rv = install_bootblock(devfd, mnt->mnt_fsname);
  close(devfd);
  sync();

  endmntent(mtab);

  if ( rv ) return rv;

  return 0;
}

int
main(int argc, char *argv[])
{
  program = argv[0];

  if ( argc != 2 ) {
    fprintf(stderr, "Usage: %s directory\n", program);
    exit(1);
  }

  return install_loader(argv[1]);
}
