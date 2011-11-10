/* -----------------------------------------------------------------------
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 */
#include <com32.h>
#include <core.h>
#include <syslinux/memscan.h>
#include <syslinux/firmware.h>

extern void comboot_cleanup_api(void);
extern void bios_timer_cleanup(void);

/*
 * cleanup.c
 *
 * Some final tidying before jumping to a kernel or bootsector
 */

void bios_cleanup_hardware(void)
{
	/*
	 * TODO
	 *
	 * Linux wants the floppy motor shut off before starting the
	 * kernel, at least bootsect.S seems to imply so.  If we don't
	 * load the floppy driver, this is *definitely* so!
	 */
	__intcall(0x13, &zero_regs, NULL);

	call16(comboot_cleanup_api, &zero_regs, NULL);
	call16(bios_timer_cleanup, &zero_regs, NULL);

	/* If we enabled serial port interrupts, clean them up now */
	sirq_cleanup();
}

/*
 * cleanup_hardware:
 *
 *	Shut down anything transient.
 */
void cleanup_hardware(void)
{
	firmware->cleanup();
}
