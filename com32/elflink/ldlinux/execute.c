/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dprintf.h>

#include <com32.h>
#include <sys/exec.h>
#include <sys/io.h>
#include <sys/module.h>
#include "core.h"
#include "menu.h"
#include "fs.h"
#include "config.h"
#include "localboot.h"
#include "bios.h"

#include <syslinux/bootrm.h>
#include <syslinux/movebits.h>
#include <syslinux/config.h>
#include <syslinux/boot.h>

const struct image_types image_boot_types[] = {
    { "localboot", IMAGE_TYPE_LOCALBOOT },
    { "kernel", IMAGE_TYPE_KERNEL },
    { "linux", IMAGE_TYPE_LINUX },
    { "boot", IMAGE_TYPE_BOOT },
    { "bss", IMAGE_TYPE_BSS },
    { "pxe", IMAGE_TYPE_PXE },
    { "fdimage", IMAGE_TYPE_FDIMAGE },
    { "comboot", IMAGE_TYPE_COMBOOT },
    { "com32", IMAGE_TYPE_COM32 },
    { "config", IMAGE_TYPE_CONFIG },
    { NULL, 0 },
};

extern int create_args_and_load(char *);

void execute(const char *cmdline, uint32_t type)
{
	const char *kernel, *args;
	const char *p;
	com32sys_t ireg;
	char *q;

	memset(&ireg, 0, sizeof ireg);

	/* for parameter will be passed to __intcall, we need use
	 * lmalloc a block of low memory */
	q = lmalloc(128);
	if (!q) {
		printf("%s(): Fail to lmalloc a buffer to exec %s\n",
			__func__, cmdline);
		return;
	}

	kernel = q;
	p = cmdline;
	while (*p && !my_isspace(*p))
		*q++ = *p++;
	*q++ = '\0';

	args = q;
	while (*p && my_isspace(*p))
		p++;

	strcpy(q, p);

	dprintf("kernel is %s, args = %s  type = %d \n", kernel, args, type);

	if (kernel[0] == '.') {
		/* It might be a type specifier */
		const struct image_types *t;
		for (t = image_boot_types; t->name; t++) {
			if (!strcmp(kernel + 1, t->name)) {
				/* Strip the type specifier and retry */
				execute(p, t->type);
				return;
			}
		}
	}

	if (type == IMAGE_TYPE_COM32) {
		/* new entry for elf format c32 */
		create_args_and_load((char *)cmdline);

		/*
		 * The old COM32 module code would run the module then
		 * drop the user back at the command prompt,
		 * irrespective of how the COM32 module was loaded,
		 * e.g. from vesamenu.c32.
		 */
		unload_modules_since("ldlinux.c32");
		ldlinux_enter_command(!noescape);
	} else if (type == IMAGE_TYPE_CONFIG) {
		char *argv[] = { "ldlinux.c32", NULL };

		/* kernel contains the config file name */
		realpath(ConfigName, kernel, FILENAME_MAX);

		/* If we got anything on the command line, do a chdir */
		if (*args)
			mangle_name(config_cwd, args);

		start_ldlinux(argv);
	} else if (type == IMAGE_TYPE_LOCALBOOT) {
		local_boot(strtoul(kernel, NULL, 0));
	} else if (type == IMAGE_TYPE_PXE || type == IMAGE_TYPE_BSS ||
		   type == IMAGE_TYPE_BOOT) {
		chainboot_file(kernel, type);
	} else {
		/* Need add one item for kernel load, as we don't use
		* the assembly runkernel.inc any more */
		new_linux_kernel((char *)kernel, (char *)cmdline);
	}

	lfree((void *)kernel);

	/* If this returns, something went bad; return to menu */
}
