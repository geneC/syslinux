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

#ifndef SYSLINUX_H
#define SYSLINUX_H

#include <inttypes.h>

/* The standard boot sector and ldlinux image */
extern unsigned char syslinux_bootsect[];
extern unsigned int  syslinux_bootsect_len;

extern unsigned char syslinux_ldlinux[];
extern unsigned int  syslinux_ldlinux_len;
extern int           syslinux_ldlinux_mtime;

/* This switches the boot sector to "stupid mode" */
void syslinux_make_stupid(void);

/* This takes a boot sector and merges in the syslinux fields */
void syslinux_make_bootsect(void *);

/* Check to see that what we got was indeed an MS-DOS boot sector/superblock */
int syslinux_check_bootsect(const void *bs, const char *device);

/* This patches the boot sector and ldlinux.sys based on a sector map */
int syslinux_patch(const uint32_t *sectors, int nsectors);

#endif
