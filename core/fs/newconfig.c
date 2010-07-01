/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * newconfig.c
 *
 * Load a new configuration file
 */

#include "core.h"
#include "fs.h"

void pm_is_config_file(com32sys_t *regs)
{
    char target_cwd[FILENAME_MAX];
    const char *p;

    (void)regs;

    /* Save configuration file as an absolute path for posterity */
    realpath(ConfigName, KernelName, FILENAME_MAX);

    /* If we got anything on the command line, do a chdir */
    p = cmd_line;
    while (*p && !not_whitespace(*p))
	p++;

    if (*p) {
	mangle_name(target_cwd, p);
	chdir(target_cwd);
    }
}
