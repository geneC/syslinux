/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   Some parts borrowed from chain.c32:
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <com32.h>
#include <stdlib.h>
#include <string.h>

#include <disk/geom.h>
#include <disk/read.h>
#include <disk/util.h>
#include <disk/common.h>

/**
 * read_mbr - return a pointer to a malloced buffer containing the mbr
 * @drive:	Drive number
 * @error:	Return the error code on failure
 **/
void *read_mbr(int drive, int *error)
{
	struct driveinfo drive_info;
	drive_info.disk = drive;

	/* MBR: lba = 0, 1 sector */
	return read_sectors(&drive_info, 0, 1, error);
}

/**
 * dev_read - read from a drive
 * @drive:	Drive number
 * @lba:	Position to start reading from
 * @sectors:	Number of sectors to read
 * @error:	Return the error code on failure
 *
 * High-level routine to read from a hard drive.
 **/
void *dev_read(int drive, unsigned int lba, int sectors, int *error)
{
	struct driveinfo drive_info;
	drive_info.disk = drive;

	return read_sectors(&drive_info, lba, sectors, error);
}

/**
 * read_sectors - read several sectors from disk
 * @drive_info:		driveinfo struct describing the disk
 * @lba:		Position to read
 * @sectors:		Number of sectors to read
 * @error:		Return the error code on failure
 *
 * Return a pointer to a malloc'ed buffer containing the data.
 **/
void *read_sectors(struct driveinfo* drive_info, const unsigned int lba,
		   const int sectors, int *error)
{
	com32sys_t inreg, outreg;
	struct ebios_dapa *dapa = __com32.cs_bounce;
	void *buf = (char *)__com32.cs_bounce + sectors * SECTOR;
	void *data;

	if (get_drive_parameters(drive_info) == -1)
		return NULL;

	memset(&inreg, 0, sizeof inreg);

	if (drive_info->ebios) {
		dapa->len      = sizeof(*dapa);
		dapa->count    = sectors;
		dapa->off      = OFFS(buf);
		dapa->seg      = SEG(buf);
		dapa->lba      = lba;

		inreg.esi.w[0] = OFFS(dapa);
		inreg.ds       = SEG(dapa);
		inreg.edx.b[0] = drive_info->disk;
		inreg.eax.b[1] = 0x42;	/* Extended read */
	} else {
		unsigned int c, h, s;

		if (!drive_info->cbios) {
			/* We failed to get the geometry */
			if (lba)
				return NULL;	/* Can only read MBR */

			s = 1;  h = 0;  c = 0;
		} else
			lba_to_chs(drive_info, lba, &s, &h, &c);

		if ( s > 63 || h > 256 || c > 1023 )
			return NULL;

		inreg.eax.w[0] = 0x0201;	/* Read one sector */
		inreg.ecx.b[1] = c & 0xff;
		inreg.ecx.b[0] = s + (c >> 6);
		inreg.edx.b[1] = h;
		inreg.edx.b[0] = drive_info->disk;
		inreg.ebx.w[0] = OFFS(buf);
		inreg.es       = SEG(buf);
	}

	/* Perform the read */
	if (int13_retry(&inreg, &outreg)) {
		if (error)
			*error = outreg.eax.b[1];
		return NULL;	/* Give up */
	} else {
		if (error)
			*error = 0;
	}

	data = malloc(sectors * SECTOR);
	if (data)
		memcpy(data, buf, sectors * SECTOR);

	return data;
}
