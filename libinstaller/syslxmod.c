/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author H. Peter Anvin
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
#include <stdlib.h>

#include "syslinux.h"
#include "syslxint.h"

void syslinux_make_bootsect(void *bs)
{
    struct boot_sector *bootsect = bs;
    const struct boot_sector *sbs =
	(const struct boot_sector *)boot_sector;

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
    if (get_8(&sectbuf->bsMedia) != 0xF0 && get_8(&sectbuf->bsMedia) < 0xF8)
	return "invalid media signature (not a FAT filesystem?)";

    sectorsize = get_16(&sectbuf->bsBytesPerSec);
    if (sectorsize == SECTOR_SIZE)
	;			/* ok */
    else if (sectorsize >= 512 && sectorsize <= 4096 &&
	     (sectorsize & (sectorsize - 1)) == 0)
	return "unsupported sectors size";
    else
	return "impossible sector size";

    clustersize = get_8(&sectbuf->bsSecPerClust);
    if (clustersize == 0 || (clustersize & (clustersize - 1)))
	return "impossible cluster size";

    sectors = get_16(&sectbuf->bsSectors);
    sectors = sectors ? sectors : get_32(&sectbuf->bsHugeSectors);

    dsectors = sectors - get_16(&sectbuf->bsResSectors);

    fatsectors = get_16(&sectbuf->bsFATsecs);
    fatsectors = fatsectors ? fatsectors : get_32(&sectbuf->bs32.FATSz32);
    fatsectors *= get_8(&sectbuf->bsFATs);
    dsectors -= fatsectors;

    rootdirents = get_16(&sectbuf->bsRootDirEnts);
    dsectors -= (rootdirents + sectorsize / 32 - 1) / sectorsize;

    if (dsectors < 0)
	return "negative number of data sectors";

    if (fatsectors == 0)
	return "zero FAT sectors";

    clusters = dsectors / clustersize;

    if (clusters < 0xFFF5) {
	/* FAT12 or FAT16 */

	if (!get_16(&sectbuf->bsFATsecs))
	    return "zero FAT sectors (FAT12/16)";

	if (get_8(&sectbuf->bs16.BootSignature) == 0x29) {
	    if (!memcmp(&sectbuf->bs16.FileSysType, "FAT12   ", 8)) {
		if (clusters >= 0xFF5)
		    return "more than 4084 clusters but claims FAT12";
	    } else if (!memcmp(&sectbuf->bs16.FileSysType, "FAT16   ", 8)) {
		if (clusters < 0xFF5)
		    return "less than 4084 clusters but claims FAT16";
	    } else if (!memcmp(&sectbuf->bs16.FileSysType, "FAT32   ", 8)) {
		    return "less than 65525 clusters but claims FAT32";
	    } else if (memcmp(&sectbuf->bs16.FileSysType, "FAT     ", 8)) {
		static char fserr[] =
		    "filesystem type \"????????\" not supported";
		memcpy(fserr + 17, &sectbuf->bs16.FileSysType, 8);
		return fserr;
	    }
	}
    } else if (clusters < 0x0FFFFFF5) {
	/*
	 * FAT32...
	 *
	 * Moving the FileSysType and BootSignature was a lovely stroke
	 * of M$ idiocy...
	 */
	if (get_8(&sectbuf->bs32.BootSignature) != 0x29 ||
	    memcmp(&sectbuf->bs32.FileSysType, "FAT32   ", 8))
	    return "missing FAT32 signature";
    } else {
	return "impossibly large number of clusters";
    }

    return NULL;
}

/*
 * Special handling for the MS-DOS derivative: syslinux_ldlinux
 * is a "far" object...
 */
#ifdef __MSDOS__

#define __noinline __attribute__((noinline))

extern uint16_t ldlinux_seg;	/* Defined in dos/syslinux.c */

static inline __attribute__ ((const))
uint16_t ds(void)
{
    uint16_t v;
asm("movw %%ds,%0":"=rm"(v));
    return v;
}

static inline void *set_fs(const void *p)
{
    uint16_t seg;

    seg = ldlinux_seg + ((size_t) p >> 4);
    asm volatile ("movw %0,%%fs"::"rm" (seg));
    return (void *)((size_t) p & 0xf);
}

#if 0				/* unused */
static __noinline uint8_t get_8_sl(const uint8_t * p)
{
    uint8_t v;

    p = set_fs(p);
    asm volatile ("movb %%fs:%1,%0":"=q" (v):"m"(*p));
    return v;
}
#endif

static __noinline uint16_t get_16_sl(const uint16_t * p)
{
    uint16_t v;

    p = set_fs(p);
    asm volatile ("movw %%fs:%1,%0":"=r" (v):"m"(*p));
    return v;
}

static __noinline uint32_t get_32_sl(const uint32_t * p)
{
    uint32_t v;

    p = set_fs(p);
    asm volatile ("movl %%fs:%1,%0":"=r" (v):"m"(*p));
    return v;
}

#if 0				/* unused */
static __noinline void set_8_sl(uint8_t * p, uint8_t v)
{
    p = set_fs(p);
    asm volatile ("movb %1,%%fs:%0":"=m" (*p):"qi"(v));
}
#endif

static __noinline void set_16_sl(uint16_t * p, uint16_t v)
{
    p = set_fs(p);
    asm volatile ("movw %1,%%fs:%0":"=m" (*p):"ri"(v));
}

static __noinline void set_32_sl(uint32_t * p, uint32_t v)
{
    p = set_fs(p);
    asm volatile ("movl %1,%%fs:%0":"=m" (*p):"ri"(v));
}

#else

/* Sane system ... */
#define get_8_sl(x)    get_8(x)
#define get_16_sl(x)   get_16(x)
#define get_32_sl(x)   get_32(x)
#define set_8_sl(x,y)  set_8(x,y)
#define set_16_sl(x,y) set_16(x,y)
#define set_32_sl(x,y) set_32(x,y)

#endif

/*
 * This patches the boot sector and the beginning of ldlinux.sys
 * based on an ldlinux.sys sector map passed in.  Typically this is
 * handled by writing ldlinux.sys, mapping it, and then overwrite it
 * with the patched version.  If this isn't safe to do because of
 * an OS which does block reallocation, then overwrite it with
 * direct access since the location is known.
 *
 * Returns the number of modified bytes in ldlinux.sys if successful,
 * otherwise -1.
 */
int syslinux_patch(const uint32_t * sectors, int nsectors,
		   int stupid, int raid_mode, const char *subdir)
{
    struct patch_area *patcharea;
    uint32_t *wp;
    int nsect = (boot_image_len + 511) >> 9;
    uint32_t csum;
    int i, dw, nptrs;
    struct boot_sector *sbs = (struct boot_sector *)boot_sector;
    size_t diroffset, dirlen;

    if (nsectors < nsect)
	return -1;

    /* Handle RAID mode, write proper bsSignature */
    i = get_16(&sbs->bsSignature);
    if (raid_mode)
	set_16((uint16_t *) ((char *)sbs + i), 0x18CD);	/* INT 18h */
    set_16(&sbs->bsSignature, 0xAA55);

    /* First sector need pointer in boot sector */
    set_32(&sbs->NextSector, *sectors++);

    /* Search for LDLINUX_MAGIC to find the patch area */
    for (wp = (uint32_t *)boot_image; get_32_sl(wp) != LDLINUX_MAGIC;
	 wp++) ;
    patcharea = (struct patch_area *)wp;

    /* Set up the totals */
    dw = boot_image_len >> 2;	/* COMPLETE dwords, excluding ADV */
    set_16_sl(&patcharea->data_sectors, nsect);	/* Not including ADVs */
    set_16_sl(&patcharea->adv_sectors, 2);	/* ADVs need 2 sectors */
    set_32_sl(&patcharea->dwords, dw);

    /* Poke in the base directory path */
    if (subdir) {
	diroffset = get_16(&patcharea->diroffset);
	dirlen = get_16(&patcharea->dirlen);
	if (dirlen <= strlen(subdir)) {
	    fprintf(stderr, "Subdirectory path too long... aborting install!\n");
	    exit(1);
	}
	memcpy((char *)boot_image + diroffset, subdir, strlen(subdir) + 1);
    }

    /* Handle Stupid mode */
    if (stupid) {
	/* Access only one sector at a time */
	set_16(&patcharea->maxtransfer, 1);
    }

    /* Set the sector pointers */
    wp = (uint32_t *) ((char *)boot_image +
		       get_16_sl(&patcharea->secptroffset));
    nptrs = get_16_sl(&patcharea->secptrcnt);

    nsect += 2;
    while (--nsect) { /* the first sector is in bs->NextSector */
	set_32_sl(wp++, *sectors++);
	nptrs--;
    }
    while (nptrs--)
	set_32_sl(wp++, 0);

    /* Now produce a checksum */
    set_32_sl(&patcharea->checksum, 0);

    csum = LDLINUX_MAGIC;
    for (i = 0, wp = (uint32_t *)boot_image; i < dw; i++, wp++)
	csum -= get_32_sl(wp);	/* Negative checksum */

    set_32_sl(&patcharea->checksum, csum);

    return dw << 2;
}
