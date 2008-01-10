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
 * syslxmod.c - Code to provide a SYSLINUX code set to an installer.
 */

#define _XOPEN_SOURCE 500	/* Required on glibc 2.x */
#define _BSD_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>

#include "syslinux.h"

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

void syslinux_make_bootsect(void *bs)
{
  unsigned char *bootsect = bs;

  memcpy(bootsect+bsHead, syslinux_bootsect+bsHead, bsHeadLen);
  memcpy(bootsect+bsCode, syslinux_bootsect+bsCode, bsCodeLen);
}

/*
 * Check to see that what we got was indeed an MS-DOS boot sector/superblock;
 * Return NULL if OK and otherwise an error message;
 */
const char *syslinux_check_bootsect(const void *bs)
{
  int veryold;
  int sectorsize;
  long long sectors, fatsectors, dsectors;
  long long clusters;
  int rootdirents, clustersize;
  const unsigned char *sectbuf = bs;

  veryold = 0;

  /* Must be 0xF0 or 0xF8..0xFF */
  if ( get_8(sectbuf+bsMedia) != 0xF0 &&
       get_8(sectbuf+bsMedia) < 0xF8 )
    goto invalid;

  sectorsize = get_16(sectbuf+bsBytesPerSec);
  if ( sectorsize == 512 )
    ; /* ok */
  else if ( sectorsize == 1024 || sectorsize == 2048 || sectorsize == 4096 )
    return "only 512-byte sectors are supported";
  else
    goto invalid;

  clustersize = get_8(sectbuf+bsSecPerClust);
  if ( clustersize == 0 || (clustersize & (clustersize-1)) )
    goto invalid;		/* Must be nonzero and a power of 2 */

  sectors = get_16(sectbuf+bsSectors);
  sectors = sectors ? sectors : get_32(sectbuf+bsHugeSectors);

  dsectors = sectors - get_16(sectbuf+bsResSectors);

  fatsectors = get_16(sectbuf+bsFATsecs);
  fatsectors = fatsectors ? fatsectors : get_32(sectbuf+bs32FATSz32);
  fatsectors *= get_8(sectbuf+bsFATs);
  dsectors -= fatsectors;

  rootdirents = get_16(sectbuf+bsRootDirEnts);
  dsectors -= (rootdirents+sectorsize/32-1)/sectorsize;

  if ( dsectors < 0 || fatsectors == 0 )
    goto invalid;

  clusters = dsectors/clustersize;

  if ( clusters < 0xFFF5 ) {
    /* FAT12 or FAT16 */

    if ( !get_16(sectbuf+bsFATsecs) )
      goto invalid;

    if ( get_8(sectbuf+bs16BootSignature) == 0x29 ) {
      if ( !memcmp(sectbuf+bs16FileSysType, "FAT12   ", 8) ) {
	if ( clusters >= 0xFF5 )
	  return "more than 4084 clusters but claims FAT12";
      } else if ( !memcmp(sectbuf+bs16FileSysType, "FAT16   ", 8) ) {
	if ( clusters < 0xFF5 )
	  return "less than 4084 clusters but claims FAT16";
      } else if ( memcmp(sectbuf+bs16FileSysType, "FAT     ", 8) ) {
	static char fserr[] = "filesystem type \"????????\" not supported";
	memcpy(fserr+17, sectbuf+bs16FileSysType, 8);
	return fserr;
      }
    }
  } else if ( clusters < 0x0FFFFFF5 ) {
    /* FAT32 */
    /* Moving the FileSysType and BootSignature was a lovely stroke of M$ idiocy */
    if ( get_8(sectbuf+bs32BootSignature) != 0x29 ||
	 memcmp(sectbuf+bs32FileSysType, "FAT32   ", 8) )
      goto invalid;
  } else {
    goto invalid;
  }

  return NULL;

 invalid:
  return "this doesn't look like a valid FAT filesystem";
}

/*
 * This patches the boot sector and the first sector of ldlinux.sys
 * based on an ldlinux.sys sector map passed in.  Typically this is
 * handled by writing ldlinux.sys, mapping it, and then overwrite it
 * with the patched version.  If this isn't safe to do because of
 * an OS which does block reallocation, then overwrite it with
 * direct access since the location is known.
 *
 * Return 0 if successful, otherwise -1.
 */
int syslinux_patch(const uint32_t *sectors, int nsectors,
		   int stupid, int raid_mode)
{
  unsigned char *patcharea, *p;
  int nsect = (syslinux_ldlinux_len+511) >> 9;
  uint32_t csum;
  int i, dw;

  if ( nsectors < nsect )
    return -1;

  /* Patch in options, as appropriate */
  if (stupid) {
    /* Access only one sector at a time */
    set_16(syslinux_bootsect+0x1FC, 1);
  }

  i = get_16(syslinux_bootsect+0x1FE);
  if (raid_mode)
    set_16(syslinux_bootsect+i, 0x18CD); /* INT 18h */
  set_16(syslinux_bootsect+0x1FE, 0xAA55);

  /* First sector need pointer in boot sector */
  set_32(syslinux_bootsect+0x1F8, *sectors++);
  nsect--;

  /* Search for LDLINUX_MAGIC to find the patch area */
  for ( p = syslinux_ldlinux ; get_32(p) != LDLINUX_MAGIC ; p += 4 );
  patcharea = p+8;

  /* Set up the totals */
  dw = syslinux_ldlinux_len >> 2; /* COMPLETE dwords! */
  set_16(patcharea, dw);
  set_16(patcharea+2, nsect);	/* Does not include the first sector! */

  /* Set the sector pointers */
  p = patcharea+8;

  memset(p, 0, 64*4);
  while ( nsect-- ) {
    set_32(p, *sectors++);
    p += 4;
  }

  /* Now produce a checksum */
  set_32(patcharea+4, 0);

  csum = LDLINUX_MAGIC;
  for ( i = 0, p = syslinux_ldlinux ; i < dw ; i++, p += 4 )
    csum -= get_32(p);		/* Negative checksum */

  set_32(patcharea+4, csum);

    return 0;
}
