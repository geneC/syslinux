/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef SYSLXINT_H
#define SYSLXINT_H

#include "syslinux.h"

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static inline uint8_t get_8(const uint8_t * p)
{
    return *p;
}

static inline uint16_t get_16(const uint16_t * p)
{
#if defined(__i386__) || defined(__x86_64__)
    /* Littleendian and unaligned-capable */
    return *p;
#else
    const uint8_t *pp = (const uint8_t *)p;
    return (uint16_t) pp[0] + ((uint16_t) pp[1] << 8);
#endif
}

static inline uint32_t get_32(const uint32_t * p)
{
#if defined(__i386__) || defined(__x86_64__)
    /* Littleendian and unaligned-capable */
    return *p;
#else
    const uint8_t *pp = (const uint8_t *)p;
    return (uint32_t) pp[0] + ((uint32_t) pp[1] << 8) +
	((uint32_t) pp[2] << 16) + ((uint32_t) pp[3] << 24);
#endif
}

static inline void set_8(uint8_t * p, uint8_t v)
{
    *p = v;
}

static inline void set_16(uint16_t * p, uint16_t v)
{
#if defined(__i386__) || defined(__x86_64__)
    /* Littleendian and unaligned-capable */
    *(uint16_t *) p = v;
#else
    uint8_t *pp = (uint8_t *) p;
    pp[0] = (v & 0xff);
    pp[1] = ((v >> 8) & 0xff);
#endif
}

static inline void set_32(uint32_t * p, uint32_t v)
{
#if defined(__i386__) || defined(__x86_64__)
    /* Littleendian and unaligned-capable */
    *(uint32_t *) p = v;
#else
    uint8_t *pp = (uint8_t *) p;
    pp[0] = (v & 0xff);
    pp[1] = ((v >> 8) & 0xff);
    pp[2] = ((v >> 16) & 0xff);
    pp[3] = ((v >> 24) & 0xff);
#endif
}

#define LDLINUX_MAGIC	0x3eb202fe

/* Patch area for disk-based installers */
struct patch_area {
    uint32_t magic;		/* LDLINUX_MAGIC */
    uint32_t instance;		/* Per-version value */
    uint16_t data_sectors;
    uint16_t adv_sectors;
    uint32_t dwords;
    uint32_t checksum;
    uint16_t diroffset;
    uint16_t dirlen;
    uint16_t subvoloffset;
    uint16_t subvollen;
    uint16_t secptroffset;
    uint16_t secptrcnt;
};

  /* FAT bootsector format, also used by other disk-based derivatives */
struct boot_sector {
    uint8_t bsJump[3];
    char bsOemName[8];
    uint16_t bsBytesPerSec;
    uint8_t bsSecPerClust;
    uint16_t bsResSectors;
    uint8_t bsFATs;
    uint16_t bsRootDirEnts;
    uint16_t bsSectors;
    uint8_t bsMedia;
    uint16_t bsFATsecs;
    uint16_t bsSecPerTrack;
    uint16_t bsHeads;
    uint32_t bsHiddenSecs;
    uint32_t bsHugeSectors;

    union {
	struct {
	    uint8_t DriveNumber;
	    uint8_t Reserved1;
	    uint8_t BootSignature;
	    uint32_t VolumeID;
	    char VolumeLabel[11];
	    char FileSysType[8];
	    uint8_t Code[442];
	} __attribute__ ((packed)) bs16;
	struct {
	    uint32_t FATSz32;
	    uint16_t ExtFlags;
	    uint16_t FSVer;
	    uint32_t RootClus;
	    uint16_t FSInfo;
	    uint16_t BkBootSec;
	    uint8_t Reserved0[12];
	    uint8_t DriveNumber;
	    uint8_t Reserved1;
	    uint8_t BootSignature;
	    uint32_t VolumeID;
	    char VolumeLabel[11];
	    char FileSysType[8];
	    uint8_t Code[414];
	} __attribute__ ((packed)) bs32;
    } __attribute__ ((packed));

    uint32_t NextSector;	/* Pointer to the first unused sector */
    uint16_t MaxTransfer;	/* Max sectors per transfer */
    uint16_t bsSignature;
} __attribute__ ((packed));

#define bsHead      bsJump
#define bsHeadLen   offsetof(struct boot_sector, bsOemName)
#define bsCode	    bs32.Code	/* The common safe choice */
#define bsCodeLen   (offsetof(struct boot_sector, bsSignature) - \
		     offsetof(struct boot_sector, bsCode))

#endif /* SYSLXINT_H */
