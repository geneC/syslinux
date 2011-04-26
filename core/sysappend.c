/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <string.h>
#include <stdio.h>
#include "core.h"

/*
 * sysappend.c
 *
 */

extern uint32_t SysAppends;	/* Configuration variable */
const char *sysappend_strings[SYSAPPEND_MAX];

/*
 * Handle sysappend strings for the old real-mode command line generator...
 * this code should be replaced when all that code is coverted to C.
 *
 * Writes the output to ES:DI with a space after each option,
 * and updates DI to point to the final null.
 */
void do_sysappend(com32sys_t *regs)
{
    char *q = MK_PTR(regs->es, regs->ebx.w[0]);
    int i;
    uint32_t mask = SysAppends;

    for (i = 0; i < SYSAPPEND_MAX; i++) {
	if (mask & 1) {
	    q = stpcpy(q, sysappend_strings[i]);
	    *q++ = ' ';
	}
	mask >>= 1;
    }
    *q = '\0';

    regs->ebx.w[0] = OFFS_WRT(q, regs->es);
}

/*
 * Print the sysappend strings, in order
 */
void print_sysappend(void)
{
    int i;

    for (i = 0; i < SYSAPPEND_MAX; i++) {
	if (sysappend_strings[i])
	    printf("%s\n", sysappend_strings[i]);
    }
}
