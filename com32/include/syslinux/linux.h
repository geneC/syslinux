/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
#include <klibc/compiler.h>

/* A chunk of an initramfs.  These are kept as a doubly-linked
   circular list with headnode; the headnode is distinguished by
   having len == 0.  The data pointer can be NULL if data_len is zero;
   if data_len < len then the balance of the region is zeroed. */

struct initramfs {
    struct initramfs *prev, *next;
    size_t len;
    size_t align;
    const void *data;
    size_t data_len;
};
#define INITRAMFS_MAX_ALIGN	4096

struct linux_header {
    uint8_t boot_sector_1[0x0020];
    uint16_t old_cmd_line_magic;
    uint16_t old_cmd_line_offset;
    uint8_t boot_sector_2[0x01f1 - 0x0024];
    uint8_t setup_sects;
    uint16_t root_flags;
    uint32_t syssize;
    uint16_t ram_size;
    uint16_t vid_mode;
    uint16_t root_dev;
    uint16_t boot_flag;
    uint16_t jump;
    uint32_t header;
    uint16_t version;
    uint32_t realmode_swtch;
    uint16_t start_sys;
    uint16_t kernel_version;
    uint8_t type_of_loader;
    uint8_t loadflags;
    uint16_t setup_move_size;
    uint32_t code32_start;
    uint32_t ramdisk_image;
    uint32_t ramdisk_size;
    uint32_t bootsect_kludge;
    uint16_t heap_end_ptr;
    uint16_t pad1;
    uint32_t cmd_line_ptr;
    uint32_t initrd_addr_max;
    uint32_t kernel_alignment;
    uint8_t relocatable_kernel;
    uint8_t pad2[3];
    uint32_t cmdline_max_len;
    uint32_t hardware_subarch;
    uint64_t hardware_subarch_data;
    uint32_t payload_offset;
    uint32_t payload_length;
    uint64_t setup_data;
    uint64_t pref_address;
    uint32_t init_size;
} __packed;

int syslinux_boot_linux(void *kernel_buf, size_t kernel_size,
			struct initramfs *initramfs, char *cmdline);

/* Initramfs manipulation functions */

struct initramfs *initramfs_init(void);
int initramfs_add_data(struct initramfs *ihead, const void *data,
		       size_t data_len, size_t len, size_t align);
int initramfs_mknod(struct initramfs *ihead, const char *filename,
		    int do_mkdir,
		    uint16_t mode, size_t len, uint32_t major, uint32_t minor);
int initramfs_add_file(struct initramfs *ihead, const void *data,
		       size_t data_len, size_t len,
		       const char *filename, int do_mkdir, uint32_t mode);
int initramfs_load_file(struct initramfs *ihead, const char *src_filename,
			const char *dst_filename, int do_mkdir, uint32_t mode);
int initramfs_add_trailer(struct initramfs *ihead);
int initramfs_load_archive(struct initramfs *ihead, const char *filename);

/* Get the combined size of the initramfs */
static inline uint32_t initramfs_size(struct initramfs *initramfs)
{
    struct initramfs *ip;
    uint32_t size = 0;

    if (!initramfs)
	return 0;

    for (ip = initramfs->next; ip->len; ip = ip->next) {
	size = (size + ip->align - 1) & ~(ip->align - 1);	/* Alignment */
	size += ip->len;
    }

    return size;
}

#endif /* _SYSLINUX_LINUX_H */
