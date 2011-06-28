/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2011 Intel Corporation; author: H. Peter Anvin
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
 * load_linux.c
 *
 * Load a Linux kernel (Image/zImage/bzImage).
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <minmax.h>
#include <suffix_number.h>
#include <syslinux/align.h>
#include <syslinux/linux.h>
#include <syslinux/bootrm.h>
#include <syslinux/movebits.h>
#include <dprintf.h>

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

#define BOOT_MAGIC 0xAA55
#define LINUX_MAGIC ('H' + ('d' << 8) + ('r' << 16) + ('S' << 24))
#define OLD_CMDLINE_MAGIC 0xA33F

/* loadflags */
#define LOAD_HIGH	0x01
#define CAN_USE_HEAP	0x80

/* 
 * Find the last instance of a particular command line argument
 * (which should include the final =; do not use for boolean arguments)
 * Note: the resulting string is typically not null-terminated.
 */
static const char *find_argument(const char *cmdline, const char *argument)
{
    const char *found = NULL;
    const char *p = cmdline;
    bool was_space = true;
    size_t la = strlen(argument);

    while (*p) {
	if (isspace(*p)) {
	    was_space = true;
	} else if (was_space) {
	    if (!memcmp(p, argument, la))
		found = p + la;
	    was_space = false;
	}
	p++;
    }

    return found;
}

/* Truncate to 32 bits, with saturate */
static inline uint32_t saturate32(unsigned long long v)
{
    return (v > 0xffffffff) ? 0xffffffff : (uint32_t) v;
}

/* Get the combined size of the initramfs */
static addr_t initramfs_size(struct initramfs *initramfs)
{
    struct initramfs *ip;
    addr_t size = 0;

    if (!initramfs)
	return 0;

    for (ip = initramfs->next; ip->len; ip = ip->next) {
	size = (size + ip->align - 1) & ~(ip->align - 1);	/* Alignment */
	size += ip->len;
    }

    return size;
}

/* Create the appropriate mappings for the initramfs */
static int map_initramfs(struct syslinux_movelist **fraglist,
			 struct syslinux_memmap **mmap,
			 struct initramfs *initramfs, addr_t addr)
{
    struct initramfs *ip;
    addr_t next_addr, len, pad;

    for (ip = initramfs->next; ip->len; ip = ip->next) {
	len = ip->len;
	next_addr = addr + len;

	/* If this isn't the last entry, extend the zero-pad region
	   to enforce the alignment of the next chunk. */
	if (ip->next->len) {
	    pad = -next_addr & (ip->next->align - 1);
	    len += pad;
	    next_addr += pad;
	}

	if (ip->data_len) {
	    if (syslinux_add_movelist(fraglist, addr, (addr_t) ip->data, len))
		return -1;
	}
	if (len > ip->data_len) {
	    if (syslinux_add_memmap(mmap, addr + ip->data_len,
				    len - ip->data_len, SMT_ZERO))
		return -1;
	}
	addr = next_addr;
    }

    return 0;
}

int syslinux_boot_linux(void *kernel_buf, size_t kernel_size,
			struct initramfs *initramfs, char *cmdline)
{
    struct linux_header hdr, *whdr;
    size_t real_mode_size, prot_mode_size;
    addr_t real_mode_base, prot_mode_base;
    addr_t irf_size;
    size_t cmdline_size, cmdline_offset;
    struct syslinux_rm_regs regs;
    struct syslinux_movelist *fraglist = NULL;
    struct syslinux_memmap *mmap = NULL;
    struct syslinux_memmap *amap = NULL;
    bool ok;
    uint32_t memlimit = 0;
    uint16_t video_mode = 0;
    const char *arg;

    cmdline_size = strlen(cmdline) + 1;

    if (kernel_size < 2 * 512)
	goto bail;

    /* Look for specific command-line arguments we care about */
    if ((arg = find_argument(cmdline, "mem=")))
	memlimit = saturate32(suffix_number(arg));

    if ((arg = find_argument(cmdline, "vga="))) {
	switch (arg[0] | 0x20) {
	case 'a':		/* "ask" */
	    video_mode = 0xfffd;
	    break;
	case 'e':		/* "ext" */
	    video_mode = 0xfffe;
	    break;
	case 'n':		/* "normal" */
	    video_mode = 0xffff;
	    break;
	case 'c':		/* "current" */
	    video_mode = 0x0f04;
	    break;
	default:
	    video_mode = strtoul(arg, NULL, 0);
	    break;
	}
    }

    /* Copy the header into private storage */
    /* Use whdr to modify the actual kernel header */
    memcpy(&hdr, kernel_buf, sizeof hdr);
    whdr = (struct linux_header *)kernel_buf;

    if (hdr.boot_flag != BOOT_MAGIC)
	goto bail;

    if (hdr.header != LINUX_MAGIC) {
	hdr.version = 0x0100;	/* Very old kernel */
	hdr.loadflags = 0;
    }

    whdr->vid_mode = video_mode;

    if (!hdr.setup_sects)
	hdr.setup_sects = 4;

    if (hdr.version < 0x0203)
	hdr.initrd_addr_max = 0x37ffffff;

    if (!memlimit && memlimit - 1 > hdr.initrd_addr_max)
	memlimit = hdr.initrd_addr_max + 1;	/* Zero for no limit */

    if (hdr.version < 0x0205 || !(hdr.loadflags & LOAD_HIGH))
	hdr.relocatable_kernel = 0;

    if (hdr.version < 0x0206)
	hdr.cmdline_max_len = 256;

    if (cmdline_size > hdr.cmdline_max_len) {
	cmdline_size = hdr.cmdline_max_len;
	cmdline[cmdline_size - 1] = '\0';
    }

    if (hdr.version < 0x0202 || !(hdr.loadflags & 0x01))
	cmdline_offset = (0x9ff0 - cmdline_size) & ~15;
    else
	cmdline_offset = 0x10000;

    real_mode_size = (hdr.setup_sects + 1) << 9;
    real_mode_base = (hdr.loadflags & LOAD_HIGH) ? 0x10000 : 0x90000;
    prot_mode_base = (hdr.loadflags & LOAD_HIGH) ? 0x100000 : 0x10000;
    prot_mode_size = kernel_size - real_mode_size;

    if (hdr.version < 0x020a) {
	/*
	 * The 3* here is a total fudge factor... it's supposed to
	 * account for the fact that the kernel needs to be
	 * decompressed, and then followed by the BSS and BRK regions.
	 * This doesn't, however, account for the fact that the kernel
	 * is decompressed into a whole other place, either.
	 */
	hdr.init_size = 3 * prot_mode_size;
    }

    if (!(hdr.loadflags & LOAD_HIGH) && prot_mode_size > 512 * 1024)
	goto bail;		/* Kernel cannot be loaded low */

    if (initramfs && hdr.version < 0x0200)
	goto bail;		/* initrd/initramfs not supported */

    if (hdr.version >= 0x0200) {
	whdr->type_of_loader = 0x30;	/* SYSLINUX unknown module */
	if (hdr.version >= 0x0201) {
	    whdr->heap_end_ptr = cmdline_offset - 0x0200;
	    whdr->loadflags |= CAN_USE_HEAP;
	}
	if (hdr.version >= 0x0202) {
	    whdr->cmd_line_ptr = real_mode_base + cmdline_offset;
	} else {
	    whdr->old_cmd_line_magic = OLD_CMDLINE_MAGIC;
	    whdr->old_cmd_line_offset = cmdline_offset;
	    /* Be paranoid and round up to a multiple of 16 */
	    whdr->setup_move_size = (cmdline_offset + cmdline_size + 15) & ~15;
	}
    }

    /* Get the memory map */
    mmap = syslinux_memory_map();	/* Memory map for shuffle_boot */
    amap = syslinux_dup_memmap(mmap);	/* Keep track of available memory */
    if (!mmap || !amap)
	goto bail;

    dprintf("Initial memory map:\n");
    syslinux_dump_memmap(mmap);

    /* If the user has specified a memory limit, mark that as unavailable.
       Question: should we mark this off-limit in the mmap as well (meaning
       it's unavailable to the boot loader, which probably has already touched
       some of it), or just in the amap? */
    if (memlimit)
	if (syslinux_add_memmap(&amap, memlimit, -memlimit, SMT_RESERVED))
	    goto bail;

    /* Place the kernel in memory */

    /* First, find a suitable place for the protected-mode code */
    if (syslinux_memmap_type(amap, prot_mode_base, prot_mode_size)
	!= SMT_FREE) {
	const struct syslinux_memmap *mp;
	if (!hdr.relocatable_kernel)
	    goto bail;		/* Can't relocate - no hope */

	ok = false;
	for (mp = amap; mp; mp = mp->next) {
	    addr_t start, end;
	    start = mp->start;
	    end = mp->next->start;

	    if (mp->type != SMT_FREE)
		continue;

	    if (end <= prot_mode_base)
		continue;	/* Only relocate upwards */

	    if (start <= prot_mode_base)
		start = prot_mode_base;

	    start = ALIGN_UP(start, hdr.kernel_alignment);
	    if (start >= end)
		continue;

	    if (end - start >= hdr.init_size) {
		whdr->code32_start += start - prot_mode_base;
		prot_mode_base = start;
		ok = true;
		break;
	    }
	}

	if (!ok)
	    goto bail;
    }

    /* Real mode code */
    if (syslinux_memmap_type(amap, real_mode_base,
			     cmdline_offset + cmdline_size) != SMT_FREE) {
	const struct syslinux_memmap *mp;

	ok = false;
	for (mp = amap; mp; mp = mp->next) {
	    addr_t start, end;
	    start = mp->start;
	    end = mp->next->start;

	    if (mp->type != SMT_FREE)
		continue;

	    if (start < real_mode_base)
		start = real_mode_base;	/* Lowest address we'll use */
	    if (end > 640 * 1024)
		end = 640 * 1024;

	    start = ALIGN_UP(start, 16);
	    if (start > 0x90000 || start >= end)
		continue;

	    if (end - start >= cmdline_offset + cmdline_size) {
		real_mode_base = start;
		ok = true;
		break;
	    }
	}
    }

    if (syslinux_add_movelist(&fraglist, real_mode_base, (addr_t) kernel_buf,
			      real_mode_size))
	goto bail;
    if (syslinux_add_memmap
	(&amap, real_mode_base, cmdline_offset + cmdline_size, SMT_ALLOC))
	goto bail;

    /* Zero region between real mode code and cmdline */
    if (syslinux_add_memmap(&mmap, real_mode_base + real_mode_size,
			    cmdline_offset - real_mode_size, SMT_ZERO))
	goto bail;

    /* Command line */
    if (syslinux_add_movelist(&fraglist, real_mode_base + cmdline_offset,
			      (addr_t) cmdline, cmdline_size))
	goto bail;

    /* Protected-mode code */
    if (syslinux_add_movelist(&fraglist, prot_mode_base,
			      (addr_t) kernel_buf + real_mode_size,
			      prot_mode_size))
	goto bail;
    if (syslinux_add_memmap(&amap, prot_mode_base, prot_mode_size, SMT_ALLOC))
	goto bail;

    /* Figure out the size of the initramfs, and where to put it.
       We should put it at the highest possible address which is
       <= hdr.initrd_addr_max, which fits the entire initramfs. */

    irf_size = initramfs_size(initramfs);	/* Handles initramfs == NULL */

    if (irf_size) {
	addr_t best_addr = 0;
	struct syslinux_memmap *ml;
	const addr_t align_mask = INITRAMFS_MAX_ALIGN - 1;

	if (irf_size) {
	    for (ml = amap; ml->type != SMT_END; ml = ml->next) {
		addr_t adj_start = (ml->start + align_mask) & ~align_mask;
		addr_t adj_end = ml->next->start & ~align_mask;
		if (ml->type == SMT_FREE && adj_end - adj_start >= irf_size)
		    best_addr = (adj_end - irf_size) & ~align_mask;
	    }

	    if (!best_addr)
		goto bail;	/* Insufficient memory for initramfs */

	    whdr->ramdisk_image = best_addr;
	    whdr->ramdisk_size = irf_size;

	    if (syslinux_add_memmap(&amap, best_addr, irf_size, SMT_ALLOC))
		goto bail;

	    if (map_initramfs(&fraglist, &mmap, initramfs, best_addr))
		goto bail;
	}
    }

    /* Set up the registers on entry */
    memset(&regs, 0, sizeof regs);
    regs.es = regs.ds = regs.ss = regs.fs = regs.gs = real_mode_base >> 4;
    regs.cs = (real_mode_base >> 4) + 0x20;
    /* regs.ip = 0; */
    /* Linux is OK with sp = 0 = 64K, but perhaps other things aren't... */
    regs.esp.w[0] = min(cmdline_offset, (size_t) 0xfff0);

    dprintf("Final memory map:\n");
    syslinux_dump_memmap(mmap);

    dprintf("Final available map:\n");
    syslinux_dump_memmap(amap);

    dprintf("Initial movelist:\n");
    syslinux_dump_movelist(fraglist);

    syslinux_shuffle_boot_rm(fraglist, mmap, 0, &regs);

bail:
    syslinux_free_movelist(fraglist);
    syslinux_free_memmap(mmap);
    syslinux_free_memmap(amap);
    return -1;
}
