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

#include <syslinux/disk.h>

/**
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 *
 * @v inreg				CPU register settings upon INT call
 * @v outreg			CPU register settings returned by INT call
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

/*
 * Query disk parameters and EBIOS availability for a particular disk.
 *
 * @v disk			The INT 0x13 disk drive number to process
 * @v diskinfo			The structure to save the queried params to
 */
int disk_get_params(int disk, struct disk_info *diskinfo)
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
