/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
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
#include "syslxint.h"

#define sbs ((struct boot_sector *)syslinux_bootsect)

void syslinux_make_bootsect(void *bs)
{
  struct boot_sector *bootsect = bs;

  memcpy(&bootsect->bsHead, &sbs->bsHead, bsHeadLen);
  memcpy(&bootsect->bsCode, &sbs->bsCode, bsCodeLen);
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
  const struct boot_sector *sectbuf = bs;

  veryold = 0;

  /* Must be 0xF0 or 0xF8..0xFF */
  if ( get_8(&sectbuf->bsMedia) != 0xF0 &&
       get_8(&sectbuf->bsMedia) < 0xF8 )
    goto invalid;

  sectorsize = get_16(&sectbuf->bsBytesPerSec);
  if ( sectorsize == 512 )
    ; /* ok */
  else if ( sectorsize == 1024 || sectorsize == 2048 || sectorsize == 4096 )
    return "only 512-byte sectors are supported";
  else
    goto invalid;

  clustersize = get_8(&sectbuf->bsSecPerClust);
  if ( clustersize == 0 || (clustersize & (clustersize-1)) )
    goto invalid;		/* Must be nonzero and a power of 2 */

  sectors = get_16(&sectbuf->bsSectors);
  sectors = sectors ? sectors : get_32(&sectbuf->bsHugeSectors);

  dsectors = sectors - get_16(&sectbuf->bsResSectors);

  fatsectors = get_16(&sectbuf->bsFATsecs);
  fatsectors = fatsectors ? fatsectors : get_32(&sectbuf->bs32.FATSz32);
  fatsectors *= get_8(&sectbuf->bsFATs);
  dsectors -= fatsectors;

  rootdirents = get_16(&sectbuf->bsRootDirEnts);
  dsectors -= (rootdirents+sectorsize/32-1)/sectorsize;

  if ( dsectors < 0 || fatsectors == 0 )
    goto invalid;

  clusters = dsectors/clustersize;

  if ( clusters < 0xFFF5 ) {
    /* FAT12 or FAT16 */

    if ( !get_16(&sectbuf->bsFATsecs) )
      goto invalid;

    if ( get_8(&sectbuf->bs16.BootSignature) == 0x29 ) {
      if ( !memcmp(&sectbuf->bs16.FileSysType, "FAT12   ", 8) ) {
	if ( clusters >= 0xFF5 )
	  return "more than 4084 clusters but claims FAT12";
      } else if ( !memcmp(&sectbuf->bs16.FileSysType, "FAT16   ", 8) ) {
	if ( clusters < 0xFF5 )
	  return "less than 4084 clusters but claims FAT16";
      } else if ( memcmp(&sectbuf->bs16.FileSysType, "FAT     ", 8) ) {
	static char fserr[] = "filesystem type \"????????\" not supported";
	memcpy(fserr+17, &sectbuf->bs16.FileSysType, 8);
	return fserr;
      }
    }
  } else if ( clusters < 0x0FFFFFF5 ) {
    /* FAT32 */
    /* Moving the FileSysType and BootSignature was a lovely stroke of M$ idiocy */
    if ( get_8(&sectbuf->bs32.BootSignature) != 0x29 ||
	 memcmp(&sectbuf->bs32.FileSysType, "FAT32   ", 8) )
      goto invalid;
  } else {
    goto invalid;
  }

  return NULL;

 invalid:
  return "this doesn't look like a valid FAT filesystem";
}

/*
 * This patches the boot sector and the beginning of ldlinux.sys
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
  struct patch_area *patcharea;
  uint32_t *wp;
  int nsect = (syslinux_ldlinux_len+511) >> 9;
  uint32_t csum;
  int i, dw, nptrs;

  if ( nsectors < nsect )
    return -1;

  /* Patch in options, as appropriate */
  if (stupid) {
    /* Access only one sector at a time */
    set_16(&sbs->MaxTransfer, 1);
  }

  i = get_16(&sbs->bsSignature);
  if (raid_mode)
    set_16((uint16_t *)((char *)sbs+i), 0x18CD); /* INT 18h */
  set_16(&sbs->bsSignature, 0xAA55);

  /* First sector need pointer in boot sector */
  set_32(&sbs->NextSector, *sectors++);

  /* Search for LDLINUX_MAGIC to find the patch area */
  for (wp = (uint32_t *)syslinux_ldlinux; get_32(wp) != LDLINUX_MAGIC; wp++)
    ;
  patcharea = (struct patch_area *)wp;

  /* Set up the totals */
  dw = syslinux_ldlinux_len >> 2;	/* COMPLETE dwords, excluding ADV */
  set_16(&patcharea->data_sectors, nsect); /* Not including ADVs */
  set_16(&patcharea->adv_sectors, 0);	   /* ADVs not supported yet */
  set_32(&patcharea->dwords, dw);
  set_32(&patcharea->currentdir, 0);

  /* Set the sector pointers */
  wp = (uint32_t *)((char *)syslinux_ldlinux+get_16(&patcharea->secptroffset));
  nptrs = get_16(&patcharea->secptrcnt);

  memset(wp, 0, nptrs*4);
  while ( nsect-- )
    set_32(wp++, *sectors++);

  /* Now produce a checksum */
  set_32(&patcharea->checksum, 0);

  csum = LDLINUX_MAGIC;
  for (i = 0, wp = (uint32_t *)syslinux_ldlinux; i < dw; i++, wp++)
    csum -= get_32(wp);		/* Negative checksum */

  set_32(&patcharea->checksum, csum);

  return 0;
}
