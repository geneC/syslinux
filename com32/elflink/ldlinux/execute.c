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
#include "core.h"
#include "menu.h"

/* Must match enum kernel_type */
const char *const kernel_types[] = {
    "none",
    "localboot",
    "kernel",
    "linux",
    "boot",
    "bss",
    "pxe",
    "fdimage",
    "comboot",
    "com32",
    "config",
    NULL
};

extern int create_args_and_load(char *);

void execute(const char *cmdline, enum kernel_type type)
{
	const char *p, *const *pp;
	const char *kernel, *args;
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

	if (kernel[0] == '.' && type == KT_NONE) {
		/* It might be a type specifier */
		enum kernel_type type = KT_NONE;
		for (pp = kernel_types; *pp; pp++, type++) {
			if (!strcmp(kernel + 1, *pp))
				execute(p, type);	/* Strip the type specifier and retry */
		}
	}

	if (type == KT_COM32) {
		/* new entry for elf format c32 */
		lfree(kernel);
		create_args_and_load(cmdline);
	} else if (type == KT_KERNEL) {
		/* Need add one item for kernel load, as we don't use
		* the assembly runkernel.inc any more */
		new_linux_kernel(kernel, cmdline);
	} else if (type == KT_CONFIG) {
		/* kernel contains the config file name */
		char *spawn_load_param[2] = { args, NULL };
		spawn_load(kernel, 1, spawn_load_param);
	} else {
		/* process the image need int 22 support */
		if (type == KT_LOCALBOOT) {
			ireg.eax.w[0] = 0x0014;	/* Local boot */
			ireg.edx.w[0] = strtoul(kernel, NULL, 0);
		} else {
			ireg.eax.w[0] = 0x0016;	/* Run kernel image */
			ireg.esi.w[0] = OFFS(kernel);
			ireg.ds = SEG(kernel);
			ireg.ebx.w[0] = OFFS(args);
			ireg.es = SEG(args);
			ireg.edx.l = type - KT_KERNEL;
			/* ireg.ecx.l    = 0; *//* We do ipappend "manually" */
		}

		__intcall(0x22, &ireg, NULL);
	}

	lfree(kernel);

	/* If this returns, something went bad; return to menu */
}
