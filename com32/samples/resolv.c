/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * resolv.c
 *
 * Resolve an IP address
 */

#include <string.h>
#include <stdio.h>
#include <console.h>
#include <stdlib.h>
#include <com32.h>

uint32_t resolv(const char *name)
{
    com32sys_t reg;

    strcpy((char *)__com32.cs_bounce, name);

    memset(&reg, 0, sizeof reg);
    reg.eax.w[0] = 0x0010;
    reg.ebx.w[0] = OFFS(__com32.cs_bounce);
    reg.es = SEG(__com32.cs_bounce);

    __intcall(0x22, &reg, &reg);

    if (reg.eflags.l & EFLAGS_CF)
	return 0;
    else
	return reg.eax.l;
}

int main(int argc, char *argv[])
{
    uint32_t ip;

    openconsole(&dev_null_r, &dev_stdcon_w);

    if (argc < 2) {
	fputs("Usage: resolv hostname\n", stderr);
	exit(1);
    }

    ip = resolv(argv[1]);

    if (ip) {
	printf("%s = %u.%u.%u.%u\n", argv[1],
	       (ip & 0xff), (ip >> 8) & 0xff,
	       (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    } else {
	printf("%s not found\n", argv[1]);
    }

    return 0;
}
