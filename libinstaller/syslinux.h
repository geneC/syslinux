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

#ifndef SYSLINUX_H
#define SYSLINUX_H

#include <inttypes.h>
#include "advconst.h"

/* The standard boot sector and ldlinux image */
extern unsigned char syslinux_bootsect[];
extern unsigned int  syslinux_bootsect_len;
extern int           syslinux_bootsect_mtime;

extern unsigned char syslinux_ldlinux[];
extern unsigned int  syslinux_ldlinux_len;
extern int           syslinux_ldlinux_mtime;

extern unsigned char syslinux_mbr[];
extern unsigned int  syslinux_mbr_len;
extern int           syslinux_mbr_mtime;

/* This takes a boot sector and merges in the syslinux fields */
void syslinux_make_bootsect(void *);

/* Check to see that what we got was indeed an MS-DOS boot sector/superblock */
const char *syslinux_check_bootsect(const void *bs);

/* This patches the boot sector and ldlinux.sys based on a sector map */
int syslinux_patch(const uint32_t *sectors, int nsectors,
		   int stupid, int raid_mode);

/* ADV information */
#define ADV_SIZE	512		/* Total size */
#define ADV_LEN		(ADV_SIZE-3*4)  /* Usable data size */
extern unsigned char syslinux_adv[2*ADV_SIZE];

int syslinux_setadv(int tag, size_t size, const void *data);
void syslinux_reset_adv(unsigned char *advbuf);
int syslinux_validate_adv(unsigned char *advbuf);

#endif
