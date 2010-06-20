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
 * Generate sector extents
 */
static void generate_extents(struct syslinux_extent *ex, int nptrs,
			     const sector_t *sectp, int nsect)
{
    uint32_t addr = 0x7c00 + 2*SECTOR_SIZE;
    uint32_t base;
    sector_t sect, lba;
    unsigned int len;

    len = lba = base = 0;

    memset(ex, 0, nptrs * sizeof *ex);

    while (nsect) {
	sect = *sectp++;

	if (len && sect == lba + len &&
	    ((addr ^ (base + len * SECTOR_SIZE)) & 0xffff0000) == 0) {
	    /* We can add to the current extent */
	    len++;
	    goto next;
	}

	if (len) {
	    set_64_sl(&ex->lba, lba);
	    set_16_sl(&ex->len, len);
	    ex++;
	}

	base = addr;
	lba  = sect;
	len  = 1;

    next:
	addr += SECTOR_SIZE;
	nsect--;
    }

    if (len) {
	set_64_sl(&ex->lba, lba);
	set_16_sl(&ex->len, len);
	ex++;
    }
}

/*
 * Form a pointer based on a 16-bit patcharea/epa field
 */
static inline void *ptr(void *img, uint16_t *offset_p)
{
    return (char *)img + get_16_sl(offset_p);
}

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
#define NADV 2

int syslinux_patch(const sector_t *sectp, int nsectors,
		   int stupid, int raid_mode, const char *subdir)
{
    struct patch_area *patcharea;
    struct ext_patch_area *epa;
    struct syslinux_extent *ex;
    uint32_t *wp;
    int nsect = ((boot_image_len + SECTOR_SIZE - 1) >> SECTOR_SHIFT) + 2;
    uint32_t csum;
    int i, dw, nptrs;
    struct boot_sector *sbs = (struct boot_sector *)boot_sector;
    uint64_t *advptrs;

    if (nsectors < nsect)
	return -1;		/* The actual file is too small for content */

    /* Search for LDLINUX_MAGIC to find the patch area */
    for (wp = (uint32_t *)boot_image; get_32_sl(wp) != LDLINUX_MAGIC;
	 wp++)
	;
    patcharea = (struct patch_area *)wp;
    epa = ptr(boot_image, &patcharea->epaoffset);

    /* First sector need pointer in boot sector */
    set_32(ptr(sbs, &epa->sect1ptr0), sectp[0]);
    set_32(ptr(sbs, &epa->sect1ptr1), sectp[0] >> 32);
    sectp++;

    /* Handle RAID mode */
    if (raid_mode) {
	/* Patch in INT 18h = CD 18 */
	set_16(ptr(sbs, &epa->raidpatch), 0x18CD);
    }

    /* Set up the totals */
    dw = boot_image_len >> 2;	/* COMPLETE dwords, excluding ADV */
    set_16_sl(&patcharea->data_sectors, nsect - 2); /* Not including ADVs */
    set_16_sl(&patcharea->adv_sectors, 2);	/* ADVs need 2 sectors */
    set_32_sl(&patcharea->dwords, dw);

    /* Handle Stupid mode */
    if (stupid) {
	/* Access only one sector at a time */
	set_16_sl(&patcharea->maxtransfer, 1);
    }

    /* Set the sector extents */
    ex = ptr(boot_image, &epa->secptroffset);
    nptrs = get_16_sl(&epa->secptrcnt);

    if (nsect > nptrs) {
	/* Not necessarily an error in this case, but a general problem */
	fprintf(stderr, "Insufficient extent space, build error!\n");
	exit(1);
    }

    /* -1 for the pointer in the boot sector, -2 for the two ADVs */
    generate_extents(ex, nptrs, sectp, nsect-1-2);

    /* ADV pointers */
    advptrs = ptr(boot_image, &epa->advptroffset);
    set_64_sl(&advptrs[0], sectp[nsect-1-2]);
    set_64_sl(&advptrs[1], sectp[nsect-1-1]);

    /* Poke in the base directory path */
    if (subdir) {
	int sublen = strlen(subdir) + 1;
	if (get_16_sl(&epa->dirlen) < sublen) {
	    fprintf(stderr, "Subdirectory path too long... aborting install!\n");
	    exit(1);
	}
	memcpy_to_sl(ptr(boot_image, &epa->diroffset), subdir, sublen);
    }

    /* Now produce a checksum */
    set_32_sl(&patcharea->checksum, 0);

    csum = LDLINUX_MAGIC;
    for (i = 0, wp = (uint32_t *)boot_image; i < dw; i++, wp++)
	csum -= get_32_sl(wp);	/* Negative checksum */

    set_32_sl(&patcharea->checksum, csum);

    /*
     * Assume all bytes modified.  This can be optimized at the expense
     * of keeping track of what the highest modified address ever was.
     */
    return dw << 2;
}
