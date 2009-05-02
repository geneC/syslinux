/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * mboot.h
 *
 * Module to load a multiboot kernel
 */

#ifndef MBOOT_H

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <minmax.h>
#include <sys/stat.h>
#include <elf.h>
#include <console.h>

#include <syslinux/loadfile.h>
#include <syslinux/movebits.h>
#include <syslinux/bootpm.h>

#include "mb_header.h"
#include "mb_info.h"

#define DEBUG 0
#if DEBUG
# define dprintf printf
#else
# define dprintf(f, ...) ((void)0)
#endif

static inline void error(const char *msg)
{
  fputs(msg, stderr);
}

/* mboot.c */
extern struct multiboot_info mbinfo;
extern struct syslinux_pm_regs regs;
extern struct my_options {
  bool solaris;
  bool aout;
} opt;

/* map.c */
#define MAP_HIGH	1
#define MAP_NOPAD	2
addr_t map_data(const void *data, size_t len, size_t align, int flags);
addr_t map_string(const char *string);
int map_image(void *ptr, size_t len);
void mboot_run(int bootflags);
int init_map(void);

/* mem.c */
void mboot_make_memmap(void);

/* apm.c */
void mboot_apm(void);

/* solaris.c */
void mboot_solaris_dhcp_hack(void);

#endif /* MBOOT_H */
