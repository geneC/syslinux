/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *   Copyright (C) 2010 Shao Miller
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

/**
 * @file syslinux/disk.h
 *
 * Deal with disks and partitions
 */

#ifndef _SYSLINUX_DISK_H
#define _SYSLINUX_DISK_H

#include <com32.h>
#include <stdint.h>

#define SECTOR 512		/* bytes/sector */

struct disk_info {
    int disk;
    int ebios;			/* EBIOS supported on this disk */
    int cbios;			/* CHS geometry is valid */
    int head;
    int sect;
};

struct disk_ebios_dapa {
    uint16_t len;
    uint16_t count;
    uint16_t off;
    uint16_t seg;
    uint64_t lba;
};

/**
 * CHS (cylinder, head, sector) value extraction macros.
 * Taken from WinVBlock.  None expand to an lvalue.
*/
#define     chs_head(chs) chs[0]
#define   chs_sector(chs) (chs[1] & 0x3F)
#define chs_cyl_high(chs) (((uint16_t)(chs[1] & 0xC0)) << 2)
#define  chs_cyl_low(chs) ((uint16_t)chs[2])
#define chs_cylinder(chs) (chs_cyl_high(chs) | chs_cyl_low(chs))
typedef uint8_t disk_chs[3];

/* A DOS partition table entry */
struct disk_dos_part_entry {
    uint8_t active_flag;	/* 0x80 if "active" */
    disk_chs start;
    uint8_t ostype;
    disk_chs end;
    uint32_t start_lba;
    uint32_t length;
} __attribute__ ((packed));

/* A DOS MBR */
struct disk_dos_mbr {
    char code[440];
    uint32_t disk_sig;
    char pad[2];
    struct disk_dos_part_entry table[4];
    uint16_t sig;
} __attribute__ ((packed));
#define disk_mbr_sig_magic 0xAA55

extern int disk_int13_retry(const com32sys_t * inreg, com32sys_t * outreg);
extern int disk_get_params(int disk, struct disk_info *const diskinfo);
extern void *disk_read_sectors(const struct disk_info *const diskinfo,
			       uint64_t lba, uint8_t count);
extern int disk_write_sector(const struct disk_info *const diskinfo,
			     unsigned int lba, const void *data);
extern int disk_write_verify_sector(const struct disk_info *const diskinfo,
				    unsigned int lba, const void *buf);
extern void disk_dos_part_dump(const struct disk_dos_part_entry *const part);

#endif /* _SYSLINUX_DISK_H */
