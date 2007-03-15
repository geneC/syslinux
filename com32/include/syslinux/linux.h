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
 * syslinux/linux.h
 *
 * Definitions used to boot a Linux kernel
 */

#ifndef _SYSLINUX_LINUX_H
#define _SYSLINUX_LINUX_H

#include <stddef.h>
#include <stdint.h>

/* A chunk of an initramfs.  These are kept as a doubly-linked
   circular list with headnode; the headnode is distinguished by
   having len == 0.  The data pointer can be NULL if data_len is zero;
   if data_len < len then the balance of the region is zeroed. */

struct initramfs {
  struct initramfs *prev, *next;
  size_t len;
  void *data;
  size_t data_len;
};

int syslinux_boot_linux(void *kernel_buf, size_t kernel_size,
			struct initramfs *initramfs,
			char *cmdline,
			uint16_t video_mode,
			uint32_t mem_limit);

#endif /* _SYSLINUX_LINUX_H */
