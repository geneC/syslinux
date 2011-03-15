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
#include "core-elf.h"

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

static inline int my_isspace(char c)
{
    return (unsigned char)c <= ' ';
}

void execute(const char *cmdline, enum kernel_type type)
{
	com32sys_t ireg;
	const char *p, *const *pp;
	char *q;
	const char *kernel, *args;

	/* work around for spawn_load parameter */
	char *spawn_load_param[2] = { NULL, NULL};

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
		spawn_load_param[0] = args;
		module_load_dependencies(kernel, "modules.dep");
		spawn_load(kernel, spawn_load_param);
	} else if (type <= KT_KERNEL) {
		/* Need add one item for kernel load, as we don't use
		* the assembly runkernel.inc any more */
		new_linux_kernel(kernel, cmdline);
	} else if (type == KT_CONFIG) {
		/* kernel contains the config file name */
		spawn_load_param[0] = args;
		module_load_dependencies("ui.c32", "modules.dep");
		spawn_load(kernel, spawn_load_param);
	} else {
		/* process the image need int 22 support */
		if (type == KT_LOCALBOOT) {
			ireg.eax.w[0] = 0x0014;	/* Local boot */
			ireg.edx.w[0] = strtoul(kernel, NULL, 0);
		}
		ireg.eax.w[0] = 0x0016;	/* Run kernel image */
		ireg.esi.w[0] = OFFS(kernel);
		ireg.ds = SEG(kernel);
		ireg.ebx.w[0] = OFFS(args);
		ireg.es = SEG(args);
		ireg.edx.l = type - KT_KERNEL;
		/* ireg.ecx.l    = 0; *//* We do ipappend "manually" */

		__intcall(0x22, &ireg, NULL);
	}

	lfree(kernel);

	/* If this returns, something went bad; return to menu */
}
