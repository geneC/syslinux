/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * gpxecmd.c
 *
 * Invoke an arbitrary gPXE command, if available.
 */

#include <alloca.h>
#include <inttypes.h>
#include <stdio.h>
#include <console.h>
#include <com32.h>
#include <string.h>
#include <sys/gpxe.h>

struct segoff16 {
    uint16_t offs, seg;
};

struct s_PXENV_FILE_EXEC {
    uint16_t Status;
    struct segoff16 Command;
};

static void gpxecmd(const char **args)
{
    char *q;
    struct s_PXENV_FILE_EXEC *fx;
    com32sys_t reg;

    memset(&reg, 0, sizeof reg);

    fx = __com32.cs_bounce;
    q = (char *)(fx + 1);

    fx->Status = 1;
    fx->Command.offs = OFFS(q);
    fx->Command.seg = SEG(q);

    while (*args) {
	q = stpcpy(q, *args);
	*q++ = ' ';
	args++;
    }
    *--q = '\0';

    memset(&reg, 0, sizeof reg);
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = 0x00e5;	/* PXENV_FILE_EXEC */
    reg.edi.w[0] = OFFS(fx);
    reg.es = SEG(fx);

    __intcall(0x22, &reg, &reg);

    /* This should not return... */
}

int main(int argc, const char *argv[])
{
    openconsole(&dev_null_r, &dev_stdcon_w);

    if (argc < 2) {
	printf("Usage: gpxecmd command...\n");
	return 1;
    }

    if (!is_gpxe()) {
	printf("gpxecmd: gPXE API not detected\n");
	return 1;
    }

    gpxecmd(argv + 1);

    return 0;
}
