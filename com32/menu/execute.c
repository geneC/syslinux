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
#include <com32.h>
#include "menu.h"

void execute(const char *cmdline, enum kernel_type type)
{
    com32sys_t ireg;
    const char *p, *const *pp;
    char *q = __com32.cs_bounce;
    const char *kernel, *args;

    memset(&ireg, 0, sizeof ireg);

    kernel = q;
    p = cmdline;
    while (*p && !my_isspace(*p)) {
	*q++ = *p++;
    }
    *q++ = '\0';

    args = q;
    while (*p && my_isspace(*p))
	p++;

    strcpy(q, p);

    if (kernel[0] == '.' && type == KT_NONE) {
	/* It might be a type specifier */
	enum kernel_type type = KT_NONE;
	for (pp = kernel_types; *pp; pp++, type++) {
	    if (!strcmp(kernel + 1, *pp)) {
		execute(p, type);	/* Strip the type specifier and retry */
	    }
	}
    }

    if (type == KT_LOCALBOOT) {
	ireg.eax.w[0] = 0x0014;	/* Local boot */
	ireg.edx.w[0] = strtoul(kernel, NULL, 0);
    } else {
	if (type < KT_KERNEL)
	    type = KT_KERNEL;

	ireg.eax.w[0] = 0x0016;	/* Run kernel image */
	ireg.esi.w[0] = OFFS(kernel);
	ireg.ds = SEG(kernel);
	ireg.ebx.w[0] = OFFS(args);
	ireg.es = SEG(args);
	ireg.edx.l = type - KT_KERNEL;
	/* ireg.ecx.l    = 0; *//* We do ipappend "manually" */
    }

    __intcall(0x22, &ireg, NULL);

    /* If this returns, something went bad; return to menu */
}
