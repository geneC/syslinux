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
 * syslxmod.c - Code to provide a SYSLINUX code set to an installer.
 */

#define _XOPEN_SOURCE 500	/* Required on glibc 2.x */
#define _BSD_SOURCE
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "syslinux.h"

#define LDLINUX_MAGIC	0x3eb202fe

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

#define bsHead      bsJump
#define bsHeadLen   (bsOemName-bsHead)
#define bsCodeLen   (bsSignature-bsCode)

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static inline uint16_t get_16(const unsigned char *p)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  return *(uint16_t *)p;
#else
  return (uint16_t)p[0] + ((uint16_t)p[1] << 8);
#endif
}

static inline uint32_t get_32(const unsigned char *p)
{
#if defined(__i386__) || defined(__x86_64__)
  /* Littleendian and unaligned-capable */
  return *(uint32_t *)p;
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

/* Patch the code so that we're running in stupid mode */
void syslinux_make_stupid(void)
{
  /* Access only one sector at a time */
  set_16(syslinux_bootsect+0x1FC, 1);
}
  
void syslinux_make_bootsect(void *bs)
{
  unsigned char *bootsect = bs;

  memcpy(bootsect+bsHead, syslinux_bootsect+bsHead, bsHeadLen);
  memcpy(bootsect+bsCode, syslinux_bootsect+bsCode, bsCodeLen);
}

/*
 * Check to see that what we got was indeed an MS-DOS boot sector/superblock;
 * Return 0 if bad and 1 if OK.
 */
int syslinux_check_bootsect(const void *bs, const char *device)
{
  int veryold;
  unsigned int sectors, clusters;
  const unsigned char *sectbuf = bs;

  /*** FIX: Handle FAT32 ***/

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
	return 0;
      }
    } else if ( !memcmp(sectbuf+bsFileSysType, "FAT16   ", 8) ) {
      if ( clusters <= 4086 ) {
	fprintf(stderr, "%s: ERROR: FAT16 but claims less than 4086 clusters\n",
		device);
	return 0;
      }
    } else if ( !memcmp(sectbuf+bsFileSysType, "FAT     ", 8) ) {
      /* OS/2 sets up the filesystem as just `FAT'. */
    } else {
      fprintf(stderr, "%s: filesystem type \"%8.8s\" not supported\n",
	      device, sectbuf+bsFileSysType);
      return 0;
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
    return 0;
  }

  return 1;
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
int syslinux_patch(const uint32_t *sectors, int nsectors)
{
  unsigned char *patcharea, *p;
  int nsect = (syslinux_ldlinux_len+511) >> 9;
  uint32_t csum;
  int i, dw;

  if ( nsectors < nsect )
    return -1;

  /* First sector need pointer in boot sector */
  set_32(syslinux_bootsect+0x1F8, *sectors++);
  nsect--;
  
  /* Search for LDLINUX_MAGIC to find the patch area */
  for ( p = syslinux_ldlinux ; get_32(p) != LDLINUX_MAGIC ; p += 4 );
  patcharea = p+4;

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

