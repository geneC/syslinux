/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <dprintf.h>
#include <console.h>
#include <sys/cpu.h>
#include "backend.h"
#include "sysdump.h"

const char *program = "sysdump";

__noreturn die(const char *msg)
{
    printf("%s: %s\n", program, msg);
    exit(1);
}

static void dump_all(struct backend *be, const char *argv[], size_t len)
{

    cpio_init(be, argv, len);

    dump_memory(be);
    dump_dmi(be);
    dump_vesa_tables(be);

    cpio_close(be);
}

static struct backend *backends[] =
{
    &be_tftp,
    &be_ymodem,
    &be_null,
    NULL
};

__noreturn usage(void)
{
    struct backend **bep, *be;

    printf("Usage:\n");
    for (bep = backends ; (be = *bep) ; bep++)
	printf("    %s %s %s\n", program, be->name, be->helpmsg);

    exit(1);
}

int main(int argc, char *argv[])
{
    struct backend **bep, *be;
    size_t len = 0;

    openconsole(&dev_null_r, &dev_stdcon_w);

    if (argc < 2)
	usage();

    for (bep = backends ; (be = *bep) ; bep++) {
	if (!strcmp(be->name, argv[1]))
	    break;
    }

    if (!be || argc < be->minargs + 2)
	usage();

    /* Do this as early as possible */
    snapshot_lowmem();

    if (be->flags & BE_NEEDLEN) {
	dump_all(&be_null, NULL, 0);
	dump_all(be, (const char **)argv + 2, be_null.zbytes);
    } else {
	dump_all(be, (const char **)argv + 2, 0);
    }
    return 0;
}
