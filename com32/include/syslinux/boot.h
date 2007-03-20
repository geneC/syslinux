/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
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
 * syslinux/boot.h
 *
 * SYSLINUX boot API invocation
 */

#ifndef _SYSLINUX_BOOT_H
#define _SYSLINUX_BOOT_H

#include <stdint.h>
#include <klibc/compiler.h>

__noreturn syslinux_run_command(const char *);
__noreturn syslinux_run_default(void);

void syslinux_final_cleanup(uint16_t flags);

void syslinux_chain_bootstrap(uint16_t flags, const void *bootstrap,
			      uint32_t bootstrap_len, uint32_t edx,
			      uint32_t esi, uint16_t ds);

void syslinux_run_kernel_image(const char *filename, const char *cmdline,
			       uint32_t ipappend_flags, uint32_t type);

#endif /* _SYSLINUX_BOOT_H */
