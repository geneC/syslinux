/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *   Copyright (C) 2010 Shao Miller
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/**
 * @file disk.c
 *
 * Deal with disks and partitions
 */

#include <dprintf.h>
#include <stdlib.h>
#include <string.h>
#include <syslinux/disk.h>

/**
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 *
 * @v inreg			CPU register settings upon INT call
 * @v outreg			CPU register settings returned by INT call
 * @ret (int)			0 upon success, -1 upon failure
 */
int disk_int13_retry(const com32sys_t * inreg, com32sys_t * outreg)
{
    int retry = 6;		/* Number of retries */
    com32sys_t tmpregs;

    if (!outreg)
	outreg = &tmpregs;

    while (retry--) {
	__intcall(0x13, inreg, outreg);
	if (!(outreg->eflags.l & EFLAGS_CF))
	    return 0;		/* CF=0, OK */
    }

    return -1;			/* Error */
}

/**
 * Query disk parameters and EBIOS availability for a particular disk.
 *
 * @v disk			The INT 0x13 disk drive number to process
 * @v diskinfo			The structure to save the queried params to
 * @ret (int)			0 upon success, -1 upon failure
 */
int disk_get_params(int disk, struct disk_info *const diskinfo)
{
    static com32sys_t getparm, parm, getebios, ebios;

    diskinfo->disk = disk;
    diskinfo->ebios = diskinfo->cbios = 0;

    /* Get EBIOS support */
    getebios.eax.w[0] = 0x4100;
    getebios.ebx.w[0] = 0x55aa;
    getebios.edx.b[0] = disk;
    getebios.eflags.b[0] = 0x3;	/* CF set */

    __intcall(0x13, &getebios, &ebios);

    if (!(ebios.eflags.l & EFLAGS_CF) &&
	ebios.ebx.w[0] == 0xaa55 && (ebios.ecx.b[0] & 1)) {
	diskinfo->ebios = 1;
    }

    /* Get disk parameters -- really only useful for
       hard disks, but if we have a partitioned floppy
       it's actually our best chance... */
    getparm.eax.b[1] = 0x08;
    getparm.edx.b[0] = disk;

    __intcall(0x13, &getparm, &parm);

    if (parm.eflags.l & EFLAGS_CF)
	return diskinfo->ebios ? 0 : -1;

    diskinfo->head = parm.edx.b[1] + 1;
    diskinfo->sect = parm.ecx.b[0] & 0x3f;
    if (diskinfo->sect == 0) {
	diskinfo->sect = 1;
    } else {
	diskinfo->cbios = 1;	/* Valid geometry */
    }

    return 0;
}

/**
 * Get disk block(s) and return a malloc'd buffer.
 *
 * @v diskinfo			The disk drive to read from
 * @v lba			The logical block address to begin reading at
 * @v count			The number of sectors to read
 * @ret data			An allocated buffer with the read data
 *
 * Uses the disk number and information from diskinfo.  Read count sectors
 * from drive, starting at lba.  Return a new buffer, or NULL upon failure.
 */
void *disk_read_sectors(const struct disk_info *const diskinfo, uint64_t lba,
			uint8_t count)
{
    com32sys_t inreg;
    struct disk_ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + SECTOR;
    void *data;

    if (!count)
	/* Silly */
	return NULL;

    memset(&inreg, 0, sizeof inreg);

    if (diskinfo->ebios) {
	dapa->len = sizeof(*dapa);
	dapa->count = count;
	dapa->off = OFFS(buf);
	dapa->seg = SEG(buf);
	dapa->lba = lba;

	inreg.esi.w[0] = OFFS(dapa);
	inreg.ds = SEG(dapa);
	inreg.edx.b[0] = diskinfo->disk;
	inreg.eax.b[1] = 0x42;	/* Extended read */
    } else {
	unsigned int c, h, s, t;

	if (!diskinfo->cbios) {
	    /* We failed to get the geometry */

	    if (lba)
		return NULL;	/* Can only read MBR */

	    s = 1;
	    h = 0;
	    c = 0;
	} else {
	    s = (lba % diskinfo->sect) + 1;
	    t = lba / diskinfo->sect;	/* Track = head*cyl */
	    h = t % diskinfo->head;
	    c = t / diskinfo->head;
	}

	if (s > 63 || h > 256 || c > 1023)
	    return NULL;

	inreg.eax.b[0] = count;
	inreg.eax.b[1] = 0x02;	/* Read */
	inreg.ecx.b[1] = c & 0xff;
	inreg.ecx.b[0] = s + (c >> 6);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = diskinfo->disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    if (disk_int13_retry(&inreg, NULL))
	return NULL;

    data = malloc(count * SECTOR);
    if (data)
	memcpy(data, buf, count * SECTOR);
    return data;
}

/**
 * Write a disk block.
 *
 * @v diskinfo			The disk drive to write to
 * @v lba			The logical block address to begin writing at
 * @v data			The data to write
 * @ret (int)			0 upon success, -1 upon failure
 *
 * Uses the disk number and information from diskinfo.
 * Write a sector to a disk drive, starting at lba.
 */
int disk_write_sector(const struct disk_info *const diskinfo, unsigned int lba,
		      const void *data)
{
    com32sys_t inreg;
    struct disk_ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + SECTOR;

    memcpy(buf, data, SECTOR);
    memset(&inreg, 0, sizeof inreg);

    if (diskinfo->ebios) {
	dapa->len = sizeof(*dapa);
	dapa->count = 1;	/* 1 sector */
	dapa->off = OFFS(buf);
	dapa->seg = SEG(buf);
	dapa->lba = lba;

	inreg.esi.w[0] = OFFS(dapa);
	inreg.ds = SEG(dapa);
	inreg.edx.b[0] = diskinfo->disk;
	inreg.eax.w[0] = 0x4300;	/* Extended write */
    } else {
	unsigned int c, h, s, t;

	if (!diskinfo->cbios) {
	    /* We failed to get the geometry */

	    if (lba)
		return -1;	/* Can only write MBR */

	    s = 1;
	    h = 0;
	    c = 0;
	} else {
	    s = (lba % diskinfo->sect) + 1;
	    t = lba / diskinfo->sect;	/* Track = head*cyl */
	    h = t % diskinfo->head;
	    c = t / diskinfo->head;
	}

	if (s > 63 || h > 256 || c > 1023)
	    return -1;

	inreg.eax.w[0] = 0x0301;	/* Write one sector */
	inreg.ecx.b[1] = c & 0xff;
	inreg.ecx.b[0] = s + (c >> 6);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = diskinfo->disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    if (disk_int13_retry(&inreg, NULL))
	return -1;

    return 0;			/* ok */
}

/**
 * Write a disk block and verify it was written.
 *
 * @v diskinfo			The disk drive to write to
 * @v lba			The logical block address to begin writing at
 * @v buf			The data to write
 * @ret rv			0 upon success, -1 upon failure
 *
 * Uses the disk number and information from diskinfo.
 * Writes a sector to a disk drive starting at lba, then reads it back
 * to verify it was written correctly.
 */
int disk_write_verify_sector(const struct disk_info *const diskinfo,
			     unsigned int lba, const void *buf)
{
    char *rb;
    int rv;

    rv = disk_write_sector(diskinfo, lba, buf);
    if (rv)
	return rv;		/* Write failure */
    rb = disk_read_sectors(diskinfo, lba, 1);
    if (!rb)
	return -1;		/* Readback failure */
    rv = memcmp(buf, rb, SECTOR);
    free(rb);
    return rv ? -1 : 0;
}

/**
 * Dump info about a DOS partition entry
 *
 * @v part			The 16-byte partition entry to examine
 */
void disk_dos_part_dump(const struct disk_dos_part_entry *const part)
{
    (void)part;
    dprintf("Partition status _____ : 0x%.2x\n"
	    "Partition CHS start\n"
	    "  Cylinder ___________ : 0x%.4x (%u)\n"
	    "  Head _______________ : 0x%.2x (%u)\n"
	    "  Sector _____________ : 0x%.2x (%u)\n"
	    "Partition type _______ : 0x%.2x\n"
	    "Partition CHS end\n"
	    "  Cylinder ___________ : 0x%.4x (%u)\n"
	    "  Head _______________ : 0x%.2x (%u)\n"
	    "  Sector _____________ : 0x%.2x (%u)\n"
	    "Partition LBA start __ : 0x%.8x (%u)\n"
	    "Partition LBA count __ : 0x%.8x (%u)\n"
	    "-------------------------------\n",
	    part->active_flag,
	    chs_cylinder(part->start),
	    chs_cylinder(part->start),
	    chs_head(part->start),
	    chs_head(part->start),
	    chs_sector(part->start),
	    chs_sector(part->start),
	    part->ostype,
	    chs_cylinder(part->end),
	    chs_cylinder(part->end),
	    chs_head(part->end),
	    chs_head(part->end),
	    chs_sector(part->end),
	    chs_sector(part->end),
	    part->start_lba, part->start_lba, part->length, part->length);
}
