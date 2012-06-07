/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2012 Intel Corporation, author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * chainbooting - replace the current bootloader completely.  This
 * is BIOS-specific.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dprintf.h>

#include <com32.h>
#include <sys/exec.h>
#include <sys/io.h>
#include "core.h"
#include "menu.h"
#include "fs.h"
#include "config.h"
#include "localboot.h"
#include "bios.h"

#include <syslinux/bootrm.h>
#include <syslinux/movebits.h>
#include <syslinux/config.h>

void chainboot_file(const char *file, enum kernel_type type)
{
    uint8_t keeppxe = 0;
    const union syslinux_derivative_info *sdi;
    struct syslinux_rm_regs regs;
    struct syslinux_movelist *fraglist = NULL;
    struct syslinux_memmap *mmap = NULL;
    struct com32_filedata fd;
    unsigned int free_mem, new_free_mem;
    unsigned int edx, esi, bx;
    com32sys_t reg;
    char *stack;
    void *buf;
    int rv, max, size;
    
    max = 0xA0000;		/* Maximum load */
    buf = malloc(max);
    if (!buf)
	goto bail;
    
    rv = open_file(file, &fd);
    if (rv == -1) {
	free(buf);
	goto bail;
    }
    
    reg.eax.l = max;
    reg.ebx.l = 0;
    reg.edx.w[0] = 0;
    reg.edi.l = (uint32_t)buf;
    reg.ebp.l = -1;	/* XXX: limit? */
    reg.esi.w[0] = rv;

    pm_load_high(&reg);

    size = reg.edi.l - (unsigned long)buf;
    if (size > 0xA0000 - 0x7C00) {
	printf("Too large for a boostrap (need LINUX instead of KERNEL?)\n");
	goto boot_bail;
    }

    esi = 0;
    bx = 0;

    sdi = syslinux_derivative_info();
    edx = sdi->rr.r.edx.b[0];

    memset(&regs, 0, sizeof(regs));

    if (sdi->c.filesystem == SYSLINUX_FS_SYSLINUX ||
	sdi->c.filesystem == SYSLINUX_FS_EXTLINUX) {
	memcpy((void *)0x800 - 18, sdi->r.esbx, 16);

	/* DS:SI points to partition info */
	esi = 0x800 - 18;
    }

    /*
     * For a BSS boot sector we have to transfer the
     * superblock.
     */
    if (sdi->c.filesystem == SYSLINUX_FS_SYSLINUX &&
	type == KT_BSS && vfat_copy_superblock(buf))
	goto boot_bail;

    /*
     * Set up initial stack frame (not used by PXE if
     * keeppxe is set - we use the PXE stack then.)
     */
    if (sdi->c.filesystem == SYSLINUX_FS_PXELINUX) {
	keeppxe = 0x03;		/* Chainloading + keep PXE */
	stack = (char *)sdi->r.fssi;

	/*
	 * Restore DS, EDX and ESI to the true initial
	 * values.
	 */
	bx = *(uint16_t *)&stack[6];
	edx = *(uint32_t *)&stack[28];
	esi = *(uint32_t *)&stack[12];

	/* Reset stack to PXE original */
	regs.es = regs.ss = sdi->rr.r.fs;
	regs.esp.w[0] = sdi->rr.r.esi.w[0] + 44;
    } else {
	char *esdi = (char *)sdi->disk.esdi_ptr;

	/*
	 * StackBuf is guaranteed to have 44 bytes
	 * free immediately above it, and will not
	 * interfere with our existing stack.
	 */
	stack = StackBuf;
	memset(stack, 0, 44);

	regs.esp.w[0] = (uint16_t)(unsigned long)stack + 44;

	/*
	 * DON'T DO THIS FOR PXELINUX...
	 * For PXE, ES:BX -> PXENV+, and this would
	 * corrupt that use.
	 *
	 * Restore ES:DI -> $PnP (if we were ourselves
	 * called that way...)
	 */

	/* New DI */
	*(uint16_t *)&stack[8] = *(uint16_t *)&esdi[0];

	/* New ES */
	*(uint16_t *)&stack[4] = *(uint16_t *)&esdi[2];

    }

    *(uint32_t *)&stack[28] = edx; /* New EDX */
    *(uint32_t *)&stack[12] = esi; /* New ESI */
    *(uint16_t *)&stack[6] = bx; /* New DS */

    regs.ip = 0x7c00;
    regs.esi.l = esi;
    regs.edx.l = edx;

    free_mem = *(volatile unsigned int *)BIOS_fbm;
    free_mem <<= 10;
    new_free_mem = free_mem - (0x7c00 + size);

    mmap = syslinux_memory_map();
    if (!mmap)
	goto boot_bail;

    if (!syslinux_add_movelist(&fraglist, 0x7c00, (addr_t)buf, size))
	syslinux_shuffle_boot_rm(fraglist, mmap, keeppxe, &regs);

    free(mmap);
boot_bail:
    free(buf);
bail:
    return;
}
