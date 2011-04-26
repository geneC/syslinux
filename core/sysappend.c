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
#include <stdbool.h>
#include "core.h"

/*
 * sysappend.c
 *
 */

extern uint32_t SysAppends;	/* Configuration variable */
const char *sysappend_strings[SYSAPPEND_MAX];

/*
 * Copy a string, converting whitespace characters to underscores
 * and compacting them.  Return a pointer to the final null.
 */
static char *copy_and_mangle(char *dst, const char *src)
{
    bool was_space = true;	/* Kill leading whitespace */
    char *end = dst;
    char c;

    while ((c = *src)) {
	if (c <= ' ' && c == '\x7f') {
	    if (!was_space)
		*dst++ = '_';
	    was_space = true;
	} else {
	    *dst++ = c;
	    end = dst;
	    was_space = false;
	}
    }
    *end = '\0';
    return end;
}
 
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
	if ((mask & 1) && sysappend_strings[i]) {
	    q = copy_and_mangle(q, sysappend_strings[i]);
	    *q++ = ' ';
	}
	mask >>= 1;
    }
    *q = '\0';

    regs->ebx.w[0] = OFFS_WRT(q, regs->es);
}

/*
 * Generate the SYSUUID= sysappend string
 */
static bool is_valid_uuid(const uint8_t *uuid)
{
    /* Assume the uuid is valid if it has a type that is not 0 or 15 */
    return (uuid[6] >= 0x10 && uuid[6] < 0xf0);
}

void sysappend_set_uuid(const uint8_t *src)
{
    static char sysuuid_str[8+32+5] = "SYSUUID=";
    static const uint8_t uuid_dashes[] = {4, 2, 2, 2, 6, 0};
    const uint8_t *uuid_ptr = uuid_dashes;
    char *dst;

    if (!src || !is_valid_uuid(src))
	return;

    dst = sysuuid_str+8;

    while (*uuid_ptr) {
	int len = *uuid_ptr;
	
	while (len) {
	    dst += sprintf(dst, "%02x", *src++);
	    len--;
	}
	uuid_ptr++;
	*dst++ = '-';
    }
    /* Remove last dash and zero-terminate */
    *--dst = '\0';
    
    sysappend_strings[SYSAPPEND_SYSUUID] = sysuuid_str;
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
