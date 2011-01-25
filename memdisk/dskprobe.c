/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Shao Miller - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * dskprobe.c
 *
 * Routines for probing BIOS disk drives
 */

/* Change to 1 for debugging */
#define DBG_DSKPROBE 0

#include <stdint.h>
#include "memdisk.h"
#include "bda.h"
#include "conio.h"

/* Function type for printf() */
typedef int (f_printf) (const char *, ...);

/* Dummy printf() that does nothing */
static f_printf no_printf;
static f_printf *dskprobe_printfs[] = { no_printf, printf };

#define dskprobe_printf (dskprobe_printfs[DBG_DSKPROBE])

/*
 * We will probe a BIOS drive numer using INT 13h, AH=probe
 * and will pass along that call's success or failure
 */
int probe_int13_ah(uint8_t drive, uint8_t probe)
{
    int err;
    com32sys_t regs;

    memset(&regs, 0, sizeof regs);

    regs.eax.b[1] = probe;	/* AH = probe                 */
    regs.edx.b[0] = drive;	/* DL = drive number to probe */
    intcall(0x13, &regs, &regs);

    err = !(regs.eflags.l & 1);
    dskprobe_printf("probe_int13_ah(0x%02x, 0x%02x) == %d\n", drive, probe,
		    err);
    return err;
}

/*
 * We will probe the BIOS Data Area and count the drives found there.
 * This heuristic then assumes that all drives of 'drive's type are
 * found in a contiguous range, and returns 1 if the probed drive
 * is less than or equal to the BDA count.
 * This particular function's code is derived from code in setup.c by
 * H. Peter Anvin.  Please respect that file's copyright for this function
 */
int probe_bda_drive(uint8_t drive)
{
    int bios_drives;
    int err;

    if (drive & 0x80) {
	bios_drives = rdz_8(BIOS_HD_COUNT);	/* HDD count */
    } else {
	uint8_t equip = rdz_8(BIOS_EQUIP);
	if (equip & 1)
	    bios_drives = (equip >> 6) + 1;	/* Floppy count */
	else
	    bios_drives = 0;
    }
    err = (drive - (drive & 0x80)) >= bios_drives ? 0 : 1;
    dskprobe_printf("probe_bda_drive(0x%02x) == %d, count: %d\n", drive, err,
		    bios_drives);
    return err;
}

/*
 * We will probe a drive with a few different methods, returning
 * the count of succesful probes
 */
int probe_drive(uint8_t drive)
{
    int c = 0;
    /* Only probe the BDA for floppies */
    if (drive & 0x80) {
	c += probe_int13_ah(drive, 0x08);
	c += probe_int13_ah(drive, 0x15);
	c += probe_int13_ah(drive, 0x41);
    }
    c += probe_bda_drive(drive);
    return c;
}

/*
 * We will probe a contiguous range of BIOS drive, starting with drive
 * number 'start'.  We probe with a few different methods, and return
 * the first drive which doesn't respond to any of the probes.
 */
uint8_t probe_drive_range(uint8_t start)
{
    uint8_t drive = start;
    while (probe_drive(drive)) {
	drive++;
	/* Check for passing the floppy/HDD boundary */
	if ((drive & 0x7F) == 0)
	    break;
    }
    return drive;
}

/* Dummy printf() that does nothing */
static int no_printf(const char *ignored, ...)
{
    (void)ignored;
    return 0;
}
