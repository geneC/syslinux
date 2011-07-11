/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2011 Intel Corporation; author: H. Peter Anvin
 *   Significant portions copyright (C) 2010 Shao Miller
 *					[partition iteration, GPT, "fs"]
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * chain.c
 *
 * Chainload a hard disk (currently rather braindead.)
 *
 * Usage: chain [options]
 *	  chain hd<disk#> [<partition>] [options]
 *	  chain fd<disk#> [options]
 *	  chain mbr:<id> [<partition>] [options]
 *	  chain guid:<guid> [<partition>] [options]
 *	  chain label:<label> [<partition>] [options]
 *	  chain boot [<partition>] [options]
 *
 * For example, "chain msdos=io.sys" will load DOS from the current Syslinux
 * filesystem.  "chain hd0 1" will boot the first partition on the first hard
 * disk.
 *
 * When none of the "hdX", "fdX", "mbr:", "guid:", "label:", "boot" or "fs"
 * options are specified, the default behaviour is equivalent to "boot".
 * "boot" means to use the current Syslinux drive, and you can also specify
 * a partition.
 *
 * The mbr: syntax means search all the hard disks until one with a
 * specific MBR serial number (bytes 440-443) is found.
 *
 * Partitions 1-4 are primary, 5+ logical, 0 = boot MBR (default.)
 *
 * "fs" will use the current Syslinux filesystem as the boot drive/partition.
 * When booting from PXELINUX, you will most likely wish to specify a disk.
 *
 * Options:
 *
 * file=<loader>
 *	loads the file <loader> **from the Syslinux filesystem**
 *	instead of loading the boot sector.
 *
 * seg=<segment>[:<offset>][{+@}entry]
 *	loads at <segment>:<offset> and jumps to <seg>:<entry> instead
 *	of the default 0000:7C00.  <offset> and <entry> default to 0 and +0
 *	repectively.  If <entry> start with + (rather than @) then the
 *	entry point address is added to the offset.
 *
 * isolinux=<loader>
 *	chainload another version/build of the ISOLINUX bootloader and patch
 *	the loader with appropriate parameters in memory.
 *	This avoids the need for the -eltorito-alt-boot parameter of mkisofs,
 *	when you want more than one ISOLINUX per CD/DVD.
 *
 * ntldr=<loader>
 *	equivalent to seg=0x2000 file=<loader> sethidden,
 *	used with WinNT's loaders
 *
 * cmldr=<loader>
 *	used with Recovery Console of Windows NT/2K/XP.
 *	same as ntldr=<loader> & "cmdcons\0" written to
 *	the system name field in the bootsector
 *
 * freedos=<loader>
 *	equivalent to seg=0x60 file=<loader> sethidden,
 *	used with FreeDOS' kernel.sys.
 *
 * msdos=<loader>
 * pcdos=<loader>
 *	equivalent to seg=0x70 file=<loader> sethidden,
 *	used with DOS' io.sys.
 *
 * drmk=<loader>
 *	Similar to msdos=<loader> but prepares the special options
 *	for the Dell Real Mode Kernel.
 *
 * grub=<loader>
 *	same as seg=0x800 file=<loader> & jumping to seg 0x820,
 *	used with GRUB Legacy stage2 files.
 *
 * grubcfg=<filename>
 *	set an alternative config filename in stage2 of Grub Legacy,
 *	only applicable in combination with "grub=<loader>".
 *
 * grldr=<loader>
 *	pass the partition number to GRUB4DOS,
 *	used with GRUB4DOS' grldr.
 *
 * swap
 *	if the disk is not fd0/hd0, install a BIOS stub which swaps
 *	the drive numbers.
 *
 * hide
 *	change type of primary partitions with IDs 01, 04, 06, 07,
 *	0b, 0c, or 0e to 1x, except for the selected partition, which
 *	is converted the other way.
 *
 * sethidden
 *	update the "hidden sectors" (partition offset) field in a
 *	FAT/NTFS boot sector.
 *
 * keeppxe
 *	keep the PXE and UNDI stacks in memory (PXELINUX only).
 *
 * freeldr=<loader>
 *  loads ReactOS' FreeLdr.sys to 0:8000 and jumps to the PE entry-point
 */

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <console.h>
#include <minmax.h>
#include <stdbool.h>
#include <dprintf.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/config.h>
#include <syslinux/video.h>

#define SECTOR 512		/* bytes/sector */

static struct options {
    const char *loadfile;
    uint16_t keeppxe;
    uint16_t seg;
    uint16_t offs;
    uint16_t entry;
    bool isolinux;
    bool cmldr;
    bool grub;
    bool grldr;
    const char *grubcfg;
    bool swap;
    bool hide;
    bool sethidden;
    bool drmk;
} opt;

struct data_area {
    void *data;
    addr_t base;
    addr_t size;
};

static inline void error(const char *msg)
{
    fputs(msg, stderr);
}

/*
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 */
static int int13_retry(const com32sys_t * inreg, com32sys_t * outreg)
{
    int retry = 6;		/* Number of retries */
    com32sys_t tmpregs;

    if (!outreg)
	outreg = &tmpregs;

    while (retry--) {
	__intcall(0x13, inreg, outreg);
	if (!(outreg->eflags.l & EFLAGS_CF))
	    return 0;		/* CF=0, OK */
    }

    return -1;			/* Error */
}

/*
 * Query disk parameters and EBIOS availability for a particular disk.
 */
struct diskinfo {
    int disk;
    int ebios;			/* EBIOS supported on this disk */
    int cbios;			/* CHS geometry is valid */
    int head;
    int sect;
} disk_info;

static int get_disk_params(int disk)
{
    static com32sys_t getparm, parm, getebios, ebios;

    disk_info.disk = disk;
    disk_info.ebios = disk_info.cbios = 0;

    /* Get EBIOS support */
    getebios.eax.w[0] = 0x4100;
    getebios.ebx.w[0] = 0x55aa;
    getebios.edx.b[0] = disk;
    getebios.eflags.b[0] = 0x3;	/* CF set */

    __intcall(0x13, &getebios, &ebios);

    if (!(ebios.eflags.l & EFLAGS_CF) &&
	ebios.ebx.w[0] == 0xaa55 && (ebios.ecx.b[0] & 1)) {
	disk_info.ebios = 1;
    }

    /* Get disk parameters -- really only useful for
       hard disks, but if we have a partitioned floppy
       it's actually our best chance... */
    getparm.eax.b[1] = 0x08;
    getparm.edx.b[0] = disk;

    __intcall(0x13, &getparm, &parm);

    if (parm.eflags.l & EFLAGS_CF)
	return disk_info.ebios ? 0 : -1;

    disk_info.head = parm.edx.b[1] + 1;
    disk_info.sect = parm.ecx.b[0] & 0x3f;
    if (disk_info.sect == 0) {
	disk_info.sect = 1;
    } else {
	disk_info.cbios = 1;	/* Valid geometry */
    }

    return 0;
}

/*
 * Get a disk block and return a malloc'd buffer.
 * Uses the disk number and information from disk_info.
 */
struct ebios_dapa {
    uint16_t len;
    uint16_t count;
    uint16_t off;
    uint16_t seg;
    uint64_t lba;
};

/* Read count sectors from drive, starting at lba.  Return a new buffer */
static void *read_sectors(uint64_t lba, uint8_t count)
{
    com32sys_t inreg;
    struct ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + SECTOR;
    void *data;

    if (!count)
	/* Silly */
	return NULL;

    memset(&inreg, 0, sizeof inreg);

    if (disk_info.ebios) {
	dapa->len = sizeof(*dapa);
	dapa->count = count;
	dapa->off = OFFS(buf);
	dapa->seg = SEG(buf);
	dapa->lba = lba;

	inreg.esi.w[0] = OFFS(dapa);
	inreg.ds = SEG(dapa);
	inreg.edx.b[0] = disk_info.disk;
	inreg.eax.b[1] = 0x42;	/* Extended read */
    } else {
	unsigned int c, h, s, t;

	if (!disk_info.cbios) {
	    /* We failed to get the geometry */

	    if (lba)
		return NULL;	/* Can only read MBR */

	    s = h = c = 0;
	} else {
	    s = lba % disk_info.sect;
	    t = lba / disk_info.sect;	/* Track = head*cyl */
	    h = t % disk_info.head;
	    c = t / disk_info.head;
	}

	if (s >= 63 || h >= 256 || c >= 1024)
	    return NULL;

	inreg.eax.b[0] = count;
	inreg.eax.b[1] = 0x02;	/* Read */
	inreg.ecx.b[1] = c;
	inreg.ecx.b[0] = ((c & 0x300) >> 2) | (s + 1);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = disk_info.disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    if (int13_retry(&inreg, NULL))
	return NULL;

    data = malloc(count * SECTOR);
    if (data)
	memcpy(data, buf, count * SECTOR);
    return data;
}

static int write_sector(unsigned int lba, const void *data)
{
    com32sys_t inreg;
    struct ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + SECTOR;

    memcpy(buf, data, SECTOR);
    memset(&inreg, 0, sizeof inreg);

    if (disk_info.ebios) {
	dapa->len = sizeof(*dapa);
	dapa->count = 1;	/* 1 sector */
	dapa->off = OFFS(buf);
	dapa->seg = SEG(buf);
	dapa->lba = lba;

	inreg.esi.w[0] = OFFS(dapa);
	inreg.ds = SEG(dapa);
	inreg.edx.b[0] = disk_info.disk;
	inreg.eax.w[0] = 0x4300;	/* Extended write */
    } else {
	unsigned int c, h, s, t;

	if (!disk_info.cbios) {
	    /* We failed to get the geometry */

	    if (lba)
		return -1;	/* Can only write MBR */

	    s = h = c = 0;
	} else {
	    s = lba % disk_info.sect;
	    t = lba / disk_info.sect;	/* Track = head*cyl */
	    h = t % disk_info.head;
	    c = t / disk_info.head;
	}

	if (s >= 63 || h >= 256 || c >= 1024)
	    return -1;

	inreg.eax.w[0] = 0x0301;	/* Write one sector */
	inreg.ecx.b[1] = c;
	inreg.ecx.b[0] = ((c & 0x300) >> 2) | (s + 1);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = disk_info.disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    if (int13_retry(&inreg, NULL))
	return -1;

    return 0;			/* ok */
}

static int write_verify_sector(unsigned int lba, const void *buf)
{
    char *rb;
    int rv;

    rv = write_sector(lba, buf);
    if (rv)
	return rv;		/* Write failure */
    rb = read_sectors(lba, 1);
    if (!rb)
	return -1;		/* Readback failure */
    rv = memcmp(buf, rb, SECTOR);
    free(rb);
    return rv ? -1 : 0;
}

/*
 * CHS (cylinder, head, sector) value extraction macros.
 * Taken from WinVBlock.  Does not expand to an lvalue
*/
#define     chs_head(chs) chs[0]
#define   chs_sector(chs) (chs[1] & 0x3F)
#define chs_cyl_high(chs) (((uint16_t)(chs[1] & 0xC0)) << 2)
#define  chs_cyl_low(chs) ((uint16_t)chs[2])
#define chs_cylinder(chs) (chs_cyl_high(chs) | chs_cyl_low(chs))
typedef uint8_t chs[3];

/* A DOS partition table entry */
struct part_entry {
    uint8_t active_flag;	/* 0x80 if "active" */
    chs start;
    uint8_t ostype;
    chs end;
    uint32_t start_lba;
    uint32_t length;
} __attribute__ ((packed));

static void mbr_part_dump(const struct part_entry *part)
{
    (void)part;
    dprintf("Partition status _____ : 0x%.2x\n"
	    "Partition CHS start\n"
	    "  Cylinder ___________ : 0x%.4x (%u)\n"
	    "  Head _______________ : 0x%.2x (%u)\n"
	    "  Sector _____________ : 0x%.2x (%u)\n"
	    "Partition type _______ : 0x%.2x\n"
	    "Partition CHS end\n"
	    "  Cylinder ___________ : 0x%.4x (%u)\n"
	    "  Head _______________ : 0x%.2x (%u)\n"
	    "  Sector _____________ : 0x%.2x (%u)\n"
	    "Partition LBA start __ : 0x%.8x (%u)\n"
	    "Partition LBA count __ : 0x%.8x (%u)\n"
	    "-------------------------------\n",
	    part->active_flag,
	    chs_cylinder(part->start),
	    chs_cylinder(part->start),
	    chs_head(part->start),
	    chs_head(part->start),
	    chs_sector(part->start),
	    chs_sector(part->start),
	    part->ostype,
	    chs_cylinder(part->end),
	    chs_cylinder(part->end),
	    chs_head(part->end),
	    chs_head(part->end),
	    chs_sector(part->end),
	    chs_sector(part->end),
	    part->start_lba, part->start_lba, part->length, part->length);
}

/* A DOS MBR */
struct mbr {
    char code[440];
    uint32_t disk_sig;
    char pad[2];
    struct part_entry table[4];
    uint16_t sig;
} __attribute__ ((packed));
static const uint16_t mbr_sig_magic = 0xAA55;

/* Search for a specific drive, based on the MBR signature; bytes 440-443 */
static int find_disk(uint32_t mbr_sig)
{
    int drive;
    bool is_me;
    struct mbr *mbr;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (get_disk_params(drive))
	    continue;		/* Drive doesn't exist */
	if (!(mbr = read_sectors(0, 1)))
	    continue;		/* Cannot read sector */
	is_me = (mbr->disk_sig == mbr_sig);
	free(mbr);
	if (is_me)
	    return drive;
    }
    return -1;
}

/* Forward declaration */
struct disk_part_iter;

/* Partition-/scheme-specific routine returning the next partition */
typedef struct disk_part_iter *(*disk_part_iter_func) (struct disk_part_iter *
						       part);

/* Contains details for a partition under examination */
struct disk_part_iter {
    /* The block holding the table we are part of */
    char *block;
    /* The LBA for the beginning of data */
    uint64_t lba_data;
    /* The partition number, as determined by our heuristic */
    int index;
    /* The DOS partition record to pass, if applicable */
    const struct part_entry *record;
    /* Function returning the next available partition */
    disk_part_iter_func next;
    /* Partition-/scheme-specific details */
    union {
	/* MBR specifics */
	int mbr_index;
	/* EBR specifics */
	struct {
	    /* The first extended partition's start LBA */
	    uint64_t lba_extended;
	    /* Any applicable parent, or NULL */
	    struct disk_part_iter *parent;
	    /* The parent extended partition index */
	    int parent_index;
	} ebr;
	/* GPT specifics */
	struct {
	    /* Real (not effective) index in the partition table */
	    int index;
	    /* Current partition GUID */
	    const struct guid *part_guid;
	    /* Current partition label */
	    const char *part_label;
	    /* Count of entries in GPT */
	    int parts;
	    /* Partition record size */
	    uint32_t size;
	} gpt;
    } private;
};

static struct disk_part_iter *next_ebr_part(struct disk_part_iter *part)
{
    const struct part_entry *ebr_table;
    const struct part_entry *parent_table =
	((const struct mbr *)part->private.ebr.parent->block)->table;
    static const struct part_entry phony = {.start_lba = 0 };
    uint64_t ebr_lba;

    /* Don't look for a "next EBR" the first time around */
    if (part->private.ebr.parent_index >= 0)
	/* Look at the linked list */
	ebr_table = ((const struct mbr *)part->block)->table + 1;
    /* Do we need to look for an extended partition? */
    if (part->private.ebr.parent_index < 0 || !ebr_table->start_lba) {
	/* Start looking for an extended partition in the MBR */
	while (++part->private.ebr.parent_index < 4) {
	    uint8_t type = parent_table[part->private.ebr.parent_index].ostype;

	    if ((type == 0x05) || (type == 0x0F) || (type == 0x85))
		break;
	}
	if (part->private.ebr.parent_index == 4)
	    /* No extended partitions found */
	    goto out_finished;
	part->private.ebr.lba_extended =
	    parent_table[part->private.ebr.parent_index].start_lba;
	ebr_table = &phony;
    }
    /* Load next EBR */
    ebr_lba = ebr_table->start_lba + part->private.ebr.lba_extended;
    free(part->block);
    part->block = read_sectors(ebr_lba, 1);
    if (!part->block) {
	error("Could not load EBR!\n");
	goto err_ebr;
    }
    ebr_table = ((const struct mbr *)part->block)->table;
    dprintf("next_ebr_part:\n");
    mbr_part_dump(ebr_table);

    /*
     * Sanity check entry: must not extend outside the
     * extended partition.  This is necessary since some OSes
     * put crap in some entries.
     */
    {
	const struct mbr *mbr =
	    (const struct mbr *)part->private.ebr.parent->block;
	const struct part_entry *extended =
	    mbr->table + part->private.ebr.parent_index;

	if (ebr_table[0].start_lba >= extended->start_lba + extended->length) {
	    dprintf("Insane logical partition!\n");
	    goto err_insane;
	}
    }
    /* Success */
    part->lba_data = ebr_table[0].start_lba + ebr_lba;
    dprintf("Partition %d logical lba %"PRIu64"\n",
	    part->index, part->lba_data);
    part->index++;
    part->record = ebr_table;
    return part;

err_insane:

    free(part->block);
    part->block = NULL;
err_ebr:

out_finished:
    free(part->private.ebr.parent->block);
    free(part->private.ebr.parent);
    free(part->block);
    free(part);
    return NULL;
}

static struct disk_part_iter *next_mbr_part(struct disk_part_iter *part)
{
    struct disk_part_iter *ebr_part;
    /* Look at the partition table */
    struct part_entry *table = ((struct mbr *)part->block)->table;

    /* Look for data partitions */
    while (++part->private.mbr_index < 4) {
	uint8_t type = table[part->private.mbr_index].ostype;

	if (type == 0x00 || type == 0x05 || type == 0x0F || type == 0x85)
	    /* Skip empty or extended partitions */
	    continue;
	if (!table[part->private.mbr_index].length)
	    /* Empty */
	    continue;
	break;
    }
    /* If we're currently the last partition, it's time for EBR processing */
    if (part->private.mbr_index == 4) {
	/* Allocate another iterator for extended partitions */
	ebr_part = malloc(sizeof(*ebr_part));
	if (!ebr_part) {
	    error("Could not allocate extended partition iterator!\n");
	    goto err_alloc;
	}
	/* Setup EBR iterator parameters */
	ebr_part->block = NULL;
	ebr_part->index = 4;
	ebr_part->record = NULL;
	ebr_part->next = next_ebr_part;
	ebr_part->private.ebr.parent = part;
	/* Trigger an initial EBR load */
	ebr_part->private.ebr.parent_index = -1;
	/* The EBR iterator is responsible for freeing us */
	return next_ebr_part(ebr_part);
    }
    dprintf("next_mbr_part:\n");
    mbr_part_dump(table + part->private.mbr_index);

    /* Update parameters to reflect this new partition.  Re-use iterator */
    part->lba_data = table[part->private.mbr_index].start_lba;
    dprintf("Partition %d primary lba %"PRIu64"\n",
	    part->private.mbr_index, part->lba_data);
    part->index = part->private.mbr_index + 1;
    part->record = table + part->private.mbr_index;
    return part;

    free(ebr_part);
err_alloc:

    free(part->block);
    free(part);
    return NULL;
}

/*
 * GUID
 * Be careful with endianness, you must adjust it yourself
 * iff you are directly using the fourth data chunk
 */
struct guid {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint64_t data4;
} __attribute__ ((packed));

    /*
     * This walk-map effectively reverses the little-endian
     * portions of the GUID in the output text
     */
static const char guid_le_walk_map[] = {
    3, -1, -1, -1, 0,
    5, -1, 0,
    3, -1, 0,
    2, 1, 0,
    1, 1, 1, 1, 1, 1
};

#if DEBUG
/*
 * Fill a buffer with a textual GUID representation.
 * The buffer must be >= char[37] and will be populated
 * with an ASCII NUL C string terminator.
 * Example: 11111111-2222-3333-4444-444444444444
 * Endian:  LLLLLLLL-LLLL-LLLL-BBBB-BBBBBBBBBBBB
 */
static void guid_to_str(char *buf, const struct guid *id)
{
    unsigned int i = 0;
    const char *walker = (const char *)id;

    while (i < sizeof(guid_le_walk_map)) {
	walker += guid_le_walk_map[i];
	if (!guid_le_walk_map[i])
	    *buf = '-';
	else {
	    *buf = ((*walker & 0xF0) >> 4) + '0';
	    if (*buf > '9')
		*buf += 'A' - '9' - 1;
	    buf++;
	    *buf = (*walker & 0x0F) + '0';
	    if (*buf > '9')
		*buf += 'A' - '9' - 1;
	}
	buf++;
	i++;
    }
    *buf = 0;
}
#endif

/*
 * Create a GUID structure from a textual GUID representation.
 * The input buffer must be >= 32 hexadecimal chars and be
 * terminated with an ASCII NUL.  Returns non-zero on failure.
 * Example: 11111111-2222-3333-4444-444444444444
 * Endian:  LLLLLLLL-LLLL-LLLL-BBBB-BBBBBBBBBBBB
 */
static int str_to_guid(const char *buf, struct guid *id)
{
    char guid_seq[sizeof(struct guid) * 2];
    unsigned int i = 0;
    char *walker = (char *)id;

    while (*buf && i < sizeof(guid_seq)) {
	switch (*buf) {
	    /* Skip these three characters */
	case '{':
	case '}':
	case '-':
	    break;
	default:
	    /* Copy something useful to the temp. sequence */
	    if ((*buf >= '0') && (*buf <= '9'))
		guid_seq[i] = *buf - '0';
	    else if ((*buf >= 'A') && (*buf <= 'F'))
		guid_seq[i] = *buf - 'A' + 10;
	    else if ((*buf >= 'a') && (*buf <= 'f'))
		guid_seq[i] = *buf - 'a' + 10;
	    else {
		/* Or not */
		error("Illegal character in GUID!\n");
		return -1;
	    }
	    i++;
	}
	buf++;
    }
    /* Check for insufficient valid characters */
    if (i < sizeof(guid_seq)) {
	error("Too few GUID characters!\n");
	return -1;
    }
    buf = guid_seq;
    i = 0;
    while (i < sizeof(guid_le_walk_map)) {
	if (!guid_le_walk_map[i])
	    i++;
	walker += guid_le_walk_map[i];
	*walker = *buf << 4;
	buf++;
	*walker |= *buf;
	buf++;
	i++;
    }
    return 0;
}

/* A GPT partition */
struct gpt_part {
    struct guid type;
    struct guid uid;
    uint64_t lba_first;
    uint64_t lba_last;
    uint64_t attribs;
    char name[72];
} __attribute__ ((packed));

static void gpt_part_dump(const struct gpt_part *gpt_part)
{
#ifdef DEBUG
    unsigned int i;
    char guid_text[37];

    dprintf("----------------------------------\n"
	    "GPT part. LBA first __ : 0x%.16llx\n"
	    "GPT part. LBA last ___ : 0x%.16llx\n"
	    "GPT part. attribs ____ : 0x%.16llx\n"
	    "GPT part. name _______ : '",
	    gpt_part->lba_first, gpt_part->lba_last, gpt_part->attribs);
    for (i = 0; i < sizeof(gpt_part->name); i++) {
	if (gpt_part->name[i])
	    dprintf("%c", gpt_part->name[i]);
    }
    dprintf("'");
    guid_to_str(guid_text, &gpt_part->type);
    dprintf("GPT part. type GUID __ : {%s}\n", guid_text);
    guid_to_str(guid_text, &gpt_part->uid);
    dprintf("GPT part. unique ID __ : {%s}\n", guid_text);
#endif
    (void)gpt_part;
}

/* A GPT header */
struct gpt {
    char sig[8];
    union {
	struct {
	    uint16_t minor;
	    uint16_t major;
	} fields __attribute__ ((packed));
	uint32_t uint32;
	char raw[4];
    } rev __attribute__ ((packed));
    uint32_t hdr_size;
    uint32_t chksum;
    char reserved1[4];
    uint64_t lba_cur;
    uint64_t lba_alt;
    uint64_t lba_first_usable;
    uint64_t lba_last_usable;
    struct guid disk_guid;
    uint64_t lba_table;
    uint32_t part_count;
    uint32_t part_size;
    uint32_t table_chksum;
    char reserved2[1];
} __attribute__ ((packed));
static const char gpt_sig_magic[] = "EFI PART";

#if DEBUG
static void gpt_dump(const struct gpt *gpt)
{
    char guid_text[37];

    printf("GPT sig ______________ : '%8.8s'\n"
	   "GPT major revision ___ : 0x%.4x\n"
	   "GPT minor revision ___ : 0x%.4x\n"
	   "GPT header size ______ : 0x%.8x\n"
	   "GPT header checksum __ : 0x%.8x\n"
	   "GPT reserved _________ : '%4.4s'\n"
	   "GPT LBA current ______ : 0x%.16llx\n"
	   "GPT LBA alternative __ : 0x%.16llx\n"
	   "GPT LBA first usable _ : 0x%.16llx\n"
	   "GPT LBA last usable __ : 0x%.16llx\n"
	   "GPT LBA part. table __ : 0x%.16llx\n"
	   "GPT partition count __ : 0x%.8x\n"
	   "GPT partition size ___ : 0x%.8x\n"
	   "GPT part. table chksum : 0x%.8x\n",
	   gpt->sig,
	   gpt->rev.fields.major,
	   gpt->rev.fields.minor,
	   gpt->hdr_size,
	   gpt->chksum,
	   gpt->reserved1,
	   gpt->lba_cur,
	   gpt->lba_alt,
	   gpt->lba_first_usable,
	   gpt->lba_last_usable,
	   gpt->lba_table, gpt->part_count, gpt->part_size, gpt->table_chksum);
    guid_to_str(guid_text, &gpt->disk_guid);
    printf("GPT disk GUID ________ : {%s}\n", guid_text);
}
#endif

static struct disk_part_iter *next_gpt_part(struct disk_part_iter *part)
{
    const struct gpt_part *gpt_part = NULL;

    while (++part->private.gpt.index < part->private.gpt.parts) {
	gpt_part =
	    (const struct gpt_part *)(part->block +
				      (part->private.gpt.index *
				       part->private.gpt.size));
	if (!gpt_part->lba_first)
	    continue;
	break;
    }
    /* Were we the last partition? */
    if (part->private.gpt.index == part->private.gpt.parts) {
	goto err_last;
    }
    part->lba_data = gpt_part->lba_first;
    part->private.gpt.part_guid = &gpt_part->uid;
    part->private.gpt.part_label = gpt_part->name;
    /* Update our index */
    part->index = part->private.gpt.index + 1;
    gpt_part_dump(gpt_part);

    /* In a GPT scheme, we re-use the iterator */
    return part;

err_last:
    free(part->block);
    free(part);

    return NULL;
}

static struct disk_part_iter *get_first_partition(struct disk_part_iter *part)
{
    const struct gpt *gpt_candidate;

    /*
     * Ignore any passed partition iterator.  The caller should
     * have passed NULL.  Allocate a new partition iterator
     */
    part = malloc(sizeof(*part));
    if (!part) {
	error("Count not allocate partition iterator!\n");
	goto err_alloc_iter;
    }
    /* Read MBR */
    part->block = read_sectors(0, 2);
    if (!part->block) {
	error("Could not read two sectors!\n");
	goto err_read_mbr;
    }
    /* Check for an MBR */
    if (((struct mbr *)part->block)->sig != mbr_sig_magic) {
	error("No MBR magic!\n");
	goto err_mbr;
    }
    /* Establish a pseudo-partition for the MBR (index 0) */
    part->index = 0;
    part->record = NULL;
    part->private.mbr_index = -1;
    part->next = next_mbr_part;
    /* Check for a GPT disk */
    gpt_candidate = (const struct gpt *)(part->block + SECTOR);
    if (!memcmp(gpt_candidate->sig, gpt_sig_magic, sizeof(gpt_sig_magic))) {
	/* LBA for partition table */
	uint64_t lba_table;

	/* It looks like one */
	/* TODO: Check checksum.  Possibly try alternative GPT */
#if DEBUG
	puts("Looks like a GPT disk.");
	gpt_dump(gpt_candidate);
#endif
	/* TODO: Check table checksum (maybe) */
	/* Note relevant GPT details */
	part->next = next_gpt_part;
	part->private.gpt.index = -1;
	part->private.gpt.parts = gpt_candidate->part_count;
	part->private.gpt.size = gpt_candidate->part_size;
	lba_table = gpt_candidate->lba_table;
	gpt_candidate = NULL;
	/* Load the partition table */
	free(part->block);
	part->block =
	    read_sectors(lba_table,
			 ((part->private.gpt.size * part->private.gpt.parts) +
			  SECTOR - 1) / SECTOR);
	if (!part->block) {
	    error("Could not read GPT partition list!\n");
	    goto err_gpt_table;
	}
    }
    /* Return the pseudo-partition's next partition, which is real */
    return part->next(part);

err_gpt_table:

err_mbr:

    free(part->block);
    part->block = NULL;
err_read_mbr:

    free(part);
err_alloc_iter:

    return NULL;
}

/*
 * Search for a specific drive/partition, based on the GPT GUID.
 * We return the disk drive number if found, as well as populating the
 * boot_part pointer with the matching partition, if applicable.
 * If no matching partition is found or the GUID is a disk GUID,
 * boot_part will be populated with NULL.  If not matching disk is
 * found, we return -1.
 */
static int find_by_guid(const struct guid *gpt_guid,
			struct disk_part_iter **boot_part)
{
    int drive;
    bool is_me;
    struct gpt *header;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (get_disk_params(drive))
	    continue;		/* Drive doesn't exist */
	if (!(header = read_sectors(1, 1)))
	    continue;		/* Cannot read sector */
	if (memcmp(&header->sig, gpt_sig_magic, sizeof(gpt_sig_magic))) {
	    /* Not a GPT disk */
	    free(header);
	    continue;
	}
#if DEBUG
	gpt_dump(header);
#endif
	is_me = !memcmp(&header->disk_guid, gpt_guid, sizeof(*gpt_guid));
	free(header);
	if (!is_me) {
	    /* Check for a matching partition */
	    boot_part[0] = get_first_partition(NULL);
	    while (boot_part[0]) {
		is_me =
		    !memcmp(boot_part[0]->private.gpt.part_guid, gpt_guid,
			    sizeof(*gpt_guid));
		if (is_me)
		    break;
		boot_part[0] = boot_part[0]->next(boot_part[0]);
	    }
	} else
	    boot_part[0] = NULL;
	if (is_me)
	    return drive;
    }
    return -1;
}

/*
 * Search for a specific partition, based on the GPT label.
 * We return the disk drive number if found, as well as populating the
 * boot_part pointer with the matching partition, if applicable.
 * If no matching partition is found, boot_part will be populated with
 * NULL and we return -1.
 */
static int find_by_label(const char *label, struct disk_part_iter **boot_part)
{
    int drive;
    bool is_me;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (get_disk_params(drive))
	    continue;		/* Drive doesn't exist */
	/* Check for a GPT disk */
	boot_part[0] = get_first_partition(NULL);
	if (!(boot_part[0]->next == next_gpt_part)) {
	    /* Not a GPT disk */
	    while (boot_part[0]) {
		/* Run through until the end */
		boot_part[0] = boot_part[0]->next(boot_part[0]);
	    }
	    continue;
	}
	/* Check for a matching partition */
	while (boot_part[0]) {
	    char gpt_label[sizeof(((struct gpt_part *) NULL)->name)];
	    const char *gpt_label_scanner =
		boot_part[0]->private.gpt.part_label;
	    int j = 0;

	    /* Re-write the GPT partition label as ASCII */
	    while (gpt_label_scanner <
		   boot_part[0]->private.gpt.part_label + sizeof(gpt_label)) {
		if ((gpt_label[j] = *gpt_label_scanner))
		    j++;
		gpt_label_scanner++;
	    }
	    if ((is_me = !strcmp(label, gpt_label)))
		break;
	    boot_part[0] = boot_part[0]->next(boot_part[0]);
	}
	if (is_me)
	    return drive;
    }

    return -1;
}

static void do_boot(struct data_area *data, int ndata,
		    struct syslinux_rm_regs *regs)
{
    uint16_t *const bios_fbm = (uint16_t *) 0x413;
    addr_t dosmem = *bios_fbm << 10;	/* Technically a low bound */
    struct syslinux_memmap *mmap;
    struct syslinux_movelist *mlist = NULL;
    addr_t endimage;
    uint8_t driveno = regs->edx.b[0];
    uint8_t swapdrive = driveno & 0x80;
    int i;

    mmap = syslinux_memory_map();

    if (!mmap) {
	error("Cannot read system memory map\n");
	return;
    }

    endimage = 0;
    for (i = 0; i < ndata; i++) {
	if (data[i].base + data[i].size > endimage)
	    endimage = data[i].base + data[i].size;
    }
    if (endimage > dosmem)
	goto too_big;

    for (i = 0; i < ndata; i++) {
	if (syslinux_add_movelist(&mlist, data[i].base,
				  (addr_t) data[i].data, data[i].size))
	    goto enomem;
    }

    if (opt.swap && driveno != swapdrive) {
	static const uint8_t swapstub_master[] = {
	    /* The actual swap code */
	    0x53,		/* 00: push bx */
	    0x0f, 0xb6, 0xda,	/* 01: movzx bx,dl */
	    0x2e, 0x8a, 0x57, 0x60,	/* 04: mov dl,[cs:bx+0x60] */
	    0x5b,		/* 08: pop bx */
	    0xea, 0, 0, 0, 0,	/* 09: jmp far 0:0 */
	    0x90, 0x90,		/* 0E: nop; nop */
	    /* Code to install this in the right location */
	    /* Entry with DS = CS; ES = SI = 0; CX = 256 */
	    0x26, 0x66, 0x8b, 0x7c, 0x4c,	/* 10: mov edi,[es:si+4*0x13] */
	    0x66, 0x89, 0x3e, 0x0a, 0x00,	/* 15: mov [0x0A],edi */
	    0x26, 0x8b, 0x3e, 0x13, 0x04,	/* 1A: mov di,[es:0x413] */
	    0x4f,		/* 1F: dec di */
	    0x26, 0x89, 0x3e, 0x13, 0x04,	/* 20: mov [es:0x413],di */
	    0x66, 0xc1, 0xe7, 0x16,	/* 25: shl edi,16+6 */
	    0x26, 0x66, 0x89, 0x7c, 0x4c,	/* 29: mov [es:si+4*0x13],edi */
	    0x66, 0xc1, 0xef, 0x10,	/* 2E: shr edi,16 */
	    0x8e, 0xc7,		/* 32: mov es,di */
	    0x31, 0xff,		/* 34: xor di,di */
	    0xf3, 0x66, 0xa5,	/* 36: rep movsd */
	    0xbe, 0, 0,		/* 39: mov si,0 */
	    0xbf, 0, 0,		/* 3C: mov di,0 */
	    0x8e, 0xde,		/* 3F: mov ds,si */
	    0x8e, 0xc7,		/* 41: mov es,di */
	    0x66, 0xb9, 0, 0, 0, 0,	/* 43: mov ecx,0 */
	    0x66, 0xbe, 0, 0, 0, 0,	/* 49: mov esi,0 */
	    0x66, 0xbf, 0, 0, 0, 0,	/* 4F: mov edi,0 */
	    0xea, 0, 0, 0, 0,	/* 55: jmp 0:0 */
	    /* pad out to segment boundary */
	    0x90, 0x90,		/* 5A: ... */
	    0x90, 0x90, 0x90, 0x90,	/* 5C: ... */
	};
	static uint8_t swapstub[1024];
	uint8_t *p;

	/* Note: we can't rely on either INT 13h nor the dosmem
	   vector to be correct at this stage, so we have to use an
	   installer stub to put things in the right place.
	   Round the installer location to a 1K boundary so the only
	   possible overlap is the identity mapping. */
	endimage = (endimage + 1023) & ~1023;

	/* Create swap stub */
	memcpy(swapstub, swapstub_master, sizeof swapstub_master);
	*(uint16_t *) & swapstub[0x3a] = regs->ds;
	*(uint16_t *) & swapstub[0x3d] = regs->es;
	*(uint32_t *) & swapstub[0x45] = regs->ecx.l;
	*(uint32_t *) & swapstub[0x4b] = regs->esi.l;
	*(uint32_t *) & swapstub[0x51] = regs->edi.l;
	*(uint16_t *) & swapstub[0x56] = regs->ip;
	*(uint16_t *) & swapstub[0x58] = regs->cs;
	p = &swapstub[sizeof swapstub_master];

	/* Mapping table; start out with identity mapping everything */
	for (i = 0; i < 256; i++)
	    p[i] = i;

	/* And the actual swap */
	p[driveno] = swapdrive;
	p[swapdrive] = driveno;

	/* Adjust registers */
	regs->ds = regs->cs = endimage >> 4;
	regs->es = regs->esi.l = 0;
	regs->ecx.l = sizeof swapstub >> 2;
	regs->ip = 0x10;	/* Installer offset */
	regs->ebx.b[0] = regs->edx.b[0] = swapdrive;

	if (syslinux_add_movelist(&mlist, endimage, (addr_t) swapstub,
				  sizeof swapstub))
	    goto enomem;

	endimage += sizeof swapstub;
    }

    /* Tell the shuffler not to muck with this area... */
    syslinux_add_memmap(&mmap, endimage, 0xa0000 - endimage, SMT_RESERVED);

    /* Force text mode */
    syslinux_force_text_mode();

    fputs("Booting...\n", stdout);
    syslinux_shuffle_boot_rm(mlist, mmap, opt.keeppxe, regs);
    error("Chainboot failed!\n");
    return;

too_big:
    error("Loader file too large\n");
    return;

enomem:
    error("Out of memory\n");
    return;
}

static int hide_unhide(struct mbr *mbr, int part)
{
    int i;
    struct part_entry *pt;
    const uint16_t mask =
	(1 << 0x01) | (1 << 0x04) | (1 << 0x06) | (1 << 0x07) | (1 << 0x0b) | (1
									       <<
									       0x0c)
	| (1 << 0x0e);
    uint8_t t;
    bool write_back = false;

    for (i = 1; i <= 4; i++) {
	pt = mbr->table + i - 1;
	t = pt->ostype;
	if ((t <= 0x1f) && ((mask >> (t & ~0x10)) & 1)) {
	    /* It's a hideable partition type */
	    if (i == part)
		t &= ~0x10;	/* unhide */
	    else
		t |= 0x10;	/* hide */
	}
	if (t != pt->ostype) {
	    write_back = true;
	    pt->ostype = t;
	}
    }

    if (write_back)
	return write_verify_sector(0, mbr);

    return 0;			/* ok */
}

static uint32_t get_file_lba(const char *filename)
{
    com32sys_t inregs;
    uint32_t lba;

    /* Start with clean registers */
    memset(&inregs, 0, sizeof(com32sys_t));

    /* Put the filename in the bounce buffer */
    strlcpy(__com32.cs_bounce, filename, __com32.cs_bounce_size);

    /* Call comapi_open() which returns a structure pointer in SI
     * to a structure whose first member happens to be the LBA.
     */
    inregs.eax.w[0] = 0x0006;
    inregs.esi.w[0] = OFFS(__com32.cs_bounce);
    inregs.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inregs, &inregs);

    if ((inregs.eflags.l & EFLAGS_CF) || inregs.esi.w[0] == 0) {
	return 0;		/* Filename not found */
    }

    /* Since the first member is the LBA, we simply cast */
    lba = *((uint32_t *) MK_PTR(inregs.ds, inregs.esi.w[0]));

    /* Clean the registers for the next call */
    memset(&inregs, 0, sizeof(com32sys_t));

    /* Put the filename in the bounce buffer */
    strlcpy(__com32.cs_bounce, filename, __com32.cs_bounce_size);

    /* Call comapi_close() to free the structure */
    inregs.eax.w[0] = 0x0008;
    inregs.esi.w[0] = OFFS(__com32.cs_bounce);
    inregs.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inregs, &inregs);

    return lba;
}

static void usage(void)
{
    static const char usage[] = "\
Usage:   chain.c32 [options]\n\
         chain.c32 hd<disk#> [<partition>] [options]\n\
         chain.c32 fd<disk#> [options]\n\
         chain.c32 mbr:<id> [<partition>] [options]\n\
         chain.c32 guid:<guid> [<partition>] [options]\n\
         chain.c32 label:<label> [<partition>] [options]\n\
         chain.c32 boot [<partition>] [options]\n\
         chain.c32 fs [options]\n\
Options: file=<loader>      Load and execute file, instead of boot sector\n\
         isolinux=<loader>  Load another version of ISOLINUX\n\
         ntldr=<loader>     Load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n\
         cmldr=<loader>     Load Recovery Console of Windows NT/2K/XP/2003\n\
         freedos=<loader>   Load FreeDOS KERNEL.SYS\n\
         freeldr=<loader>   Load ReactOS' FREELDR.SYS\n\
         msdos=<loader>     Load MS-DOS IO.SYS\n\
         pcdos=<loader>     Load PC-DOS IBMBIO.COM\n\
         drmk=<loader>      Load DRMK DELLBIO.BIN\n\
         grub=<loader>      Load GRUB Legacy stage2\n\
         grubcfg=<filename> Set alternative config filename for GRUB Legacy\n\
         grldr=<loader>     Load GRUB4DOS grldr\n\
         seg=<seg>          Jump to <seg>:0000, instead of 0000:7C00\n\
         seg=<seg>[:<offs>][{+@}<entry>] also specified offset and entrypoint\n\
         swap               Swap drive numbers, if bootdisk is not fd0/hd0\n\
         hide               Hide primary partitions, except selected partition\n\
         sethidden          Set the FAT/NTFS hidden sectors field\n\
         keeppxe            Keep the PXE and UNDI stacks in memory (PXELINUX)\n\
See syslinux/com32/modules/chain.c for more information\n";
    error(usage);
}

int main(int argc, char *argv[])
{
    struct mbr *mbr = NULL;
    char *p;
    struct disk_part_iter *cur_part = NULL;
    struct syslinux_rm_regs regs;
    char *drivename, *partition;
    int hd, drive, whichpart = 0;	/* MBR by default */
    int i;
    uint64_t fs_lba = 0;	/* Syslinux partition */
    uint32_t file_lba = 0;
    struct guid gpt_guid;
    unsigned char *isolinux_bin;
    uint32_t *checksum, *chkhead, *chktail;
    struct data_area data[3];
    int ndata = 0;
    addr_t load_base;
    static const char cmldr_signature[8] = "cmdcons";

    openconsole(&dev_null_r, &dev_stdcon_w);

    drivename = "boot";
    partition = NULL;

    /* Prepare the register set */
    memset(&regs, 0, sizeof regs);

    opt.seg   = 0;
    opt.offs  = 0x7c00;
    opt.entry = 0x7c00;

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "file=", 5)) {
	    opt.loadfile = argv[i] + 5;
	} else if (!strncmp(argv[i], "seg=", 4)) {
	    uint32_t v;
	    bool add_entry = true;
	    char *ep, *p = argv[i] + 4;
	    
	    v = strtoul(p, &ep, 0);
	    if (ep == p || v < 0x50 || v > 0x9f000) {
		error("seg: Invalid segment\n");
		goto bail;
	    }
	    opt.seg = v;
	    opt.offs = opt.entry = 0;
	    if (*ep == ':') {
		p = ep+1;
		v = strtoul(p, &ep, 0);
		if (ep == p) {
		    error("seg: Invalid offset\n");
		    goto bail;
		}
		opt.offs = v;
	    }
	    if (*ep == '@' || *ep == '+') {
		add_entry = (*ep == '+');
		p = ep+1;
		v = strtoul(p, &ep, 0);
		if (ep == p) {
		    error("seg: Invalid entry point\n");
		    goto bail;
		}
		opt.entry = v;
	    }
	    if (add_entry)
		opt.entry += opt.offs;
	} else if (!strncmp(argv[i], "isolinux=", 9)) {
	    opt.loadfile = argv[i] + 9;
	    opt.isolinux = true;
	} else if (!strncmp(argv[i], "ntldr=", 6)) {
	    opt.seg = 0x2000;	/* NTLDR wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 6;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.seg = 0x2000;	/* CMLDR wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 6;
	    opt.cmldr = true;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "freedos=", 8)) {
	    opt.seg = 0x60;	/* FREEDOS wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 8;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "freeldr=", 8)) {
	    opt.loadfile = argv[i] + 8;
	    opt.sethidden = true;
	    /* The FreeLdr PE wants to be at 0:8000 */
	    opt.seg = 0;
	    opt.offs = 0x8000;
	    /* TODO: Properly parse the PE.  Right now, this is hard-coded */
	    opt.entry = 0x8100;
	} else if (!strncmp(argv[i], "msdos=", 6) ||
		   !strncmp(argv[i], "pcdos=", 6)) {
	    opt.seg = 0x70;	/* MS-DOS 2.0+ wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 6;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "drmk=", 5)) {
	    opt.seg = 0x70;	/* DRMK wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 5;
	    opt.sethidden = true;
	    opt.drmk = true;
	} else if (!strncmp(argv[i], "grub=", 5)) {
	    opt.seg = 0x800;	/* stage2 wants this address */
	    opt.offs = opt.entry = 0;
	    opt.loadfile = argv[i] + 5;
	    opt.grub = true;
	} else if (!strncmp(argv[i], "grubcfg=", 8)) {
	    opt.grubcfg = argv[i] + 8;
	} else if (!strncmp(argv[i], "grldr=", 6)) {
	    opt.loadfile = argv[i] + 6;
	    opt.grldr = true;
	} else if (!strcmp(argv[i], "swap")) {
	    opt.swap = true;
	} else if (!strcmp(argv[i], "noswap")) {
	    opt.swap = false;
	} else if (!strcmp(argv[i], "hide")) {
	    opt.hide = true;
	} else if (!strcmp(argv[i], "nohide")) {
	    opt.hide = false;
	} else if (!strcmp(argv[i], "keeppxe")) {
	    opt.keeppxe = 3;
	} else if (!strcmp(argv[i], "sethidden")) {
	    opt.sethidden = true;
	} else if (!strcmp(argv[i], "nosethidden")) {
	    opt.sethidden = false;
	} else if (((argv[i][0] == 'h' || argv[i][0] == 'f')
		    && argv[i][1] == 'd')
		   || !strncmp(argv[i], "mbr:", 4)
		   || !strncmp(argv[i], "mbr=", 4)
		   || !strncmp(argv[i], "guid:", 5)
		   || !strncmp(argv[i], "guid=", 5)
		   || !strncmp(argv[i], "uuid:", 5)
		   || !strncmp(argv[i], "uuid=", 5)
		   || !strncmp(argv[i], "label:", 6)
		   || !strncmp(argv[i], "label=", 6)
		   || !strcmp(argv[i], "boot")
		   || !strncmp(argv[i], "boot,", 5)
		   || !strcmp(argv[i], "fs")) {
	    drivename = argv[i];
	    p = strchr(drivename, ',');
	    if (p) {
		*p = '\0';
		partition = p + 1;
	    } else if (argv[i + 1] && argv[i + 1][0] >= '0'
		       && argv[i + 1][0] <= '9') {
		partition = argv[++i];
	    }
	} else {
	    usage();
	    goto bail;
	}
    }

    if (opt.grubcfg && !opt.grub) {
	error("grubcfg=<filename> must be used together with grub=<loader>.\n");
	goto bail;
    }

    /*
     * Set up initial register values
     */
    regs.es = regs.cs = regs.ss = regs.ds = regs.fs = regs.gs = opt.seg;
    regs.ip = opt.entry;

    /* 
     * For the special case of the standard 0:7C00 entry point, put
     * the stack below; otherwise leave the stack pointer at the end
     * of the segment (sp = 0).
     */
    if (opt.seg == 0 && opt.offs == 0x7c00)
	regs.esp.l = 0x7c00;


    hd = 0;
    if (!strncmp(drivename, "mbr", 3)) {
	drive = find_disk(strtoul(drivename + 4, NULL, 0));
	if (drive == -1) {
	    error("Unable to find requested MBR signature\n");
	    goto bail;
	}
    } else if (!strncmp(drivename, "guid", 4) || !strncmp(drivename, "uuid", 4)) {
	if (str_to_guid(drivename + 5, &gpt_guid))
	    goto bail;
	drive = find_by_guid(&gpt_guid, &cur_part);
	if (drive == -1) {
	    error("Unable to find requested GPT disk/partition\n");
	    goto bail;
	}
    } else if (!strncmp(drivename, "label", 5)) {
	if (!drivename[6]) {
	    error("No label specified.\n");
	    goto bail;
	}
	drive = find_by_label(drivename + 6, &cur_part);
	if (drive == -1) {
	    error("Unable to find requested partition by label\n");
	    goto bail;
	}
    } else if ((drivename[0] == 'h' || drivename[0] == 'f') &&
	       drivename[1] == 'd') {
	hd = drivename[0] == 'h';
	drivename += 2;
	drive = (hd ? 0x80 : 0) | strtoul(drivename, NULL, 0);
    } else if (!strcmp(drivename, "boot") || !strcmp(drivename, "fs")) {
	const union syslinux_derivative_info *sdi;

	sdi = syslinux_derivative_info();
	if (sdi->c.filesystem == SYSLINUX_FS_PXELINUX)
	    drive = 0x80;	/* Boot drive not available */
	else
	    drive = sdi->disk.drive_number;
	if (!strcmp(drivename, "fs")
	    && (sdi->c.filesystem == SYSLINUX_FS_SYSLINUX
		|| sdi->c.filesystem == SYSLINUX_FS_EXTLINUX
		|| sdi->c.filesystem == SYSLINUX_FS_ISOLINUX))
	    /* We should lookup the Syslinux partition number and use it */
	    fs_lba = *sdi->disk.partoffset;
    } else {
	error("Unparsable drive specification\n");
	goto bail;
    }

    /* DOS kernels want the drive number in BL instead of DL.  Indulge them. */
    regs.ebx.b[0] = regs.edx.b[0] = drive;

    /* Get the disk geometry and disk access setup */
    if (get_disk_params(drive)) {
	error("Cannot get disk parameters\n");
	goto bail;
    }

    /* Get MBR */
    if (!(mbr = read_sectors(0, 1))) {
	error("Cannot read Master Boot Record or sector 0\n");
	goto bail;
    }

    if (partition)
	whichpart = strtoul(partition, NULL, 0);
    /* "guid:" or "label:" might have specified a partition */
    if (cur_part)
	whichpart = cur_part->index;

    /* Boot the MBR by default */
    if (!cur_part && (whichpart || fs_lba)) {
	/* Boot a partition, possibly the Syslinux partition itself */
	cur_part = get_first_partition(NULL);
	while (cur_part) {
	    if ((cur_part->index == whichpart)
		|| (cur_part->lba_data == fs_lba))
		/* Found the partition to boot */
		break;
	    cur_part = cur_part->next(cur_part);
	}
	if (!cur_part) {
	    error("Requested partition not found!\n");
	    goto bail;
	}
	whichpart = cur_part->index;
    }

    if (!(drive & 0x80) && whichpart) {
	error("Warning: Partitions of floppy devices may not work\n");
    }

    /* 
     * GRLDR of GRUB4DOS wants the partition number in DH:
     * -1:   whole drive (default)
     * 0-3:  primary partitions
     * 4-*:  logical partitions
     */
    if (opt.grldr)
	regs.edx.b[1] = whichpart - 1;

    if (opt.hide) {
	if (whichpart < 1 || whichpart > 4)
	    error("WARNING: hide specified without a non-primary partition\n");
	if (hide_unhide(mbr, whichpart))
	    error("WARNING: failed to write MBR for 'hide'\n");
    }

    /* Do the actual chainloading */
    load_base = (opt.seg << 4) + opt.offs;

    if (opt.loadfile) {
	fputs("Loading the boot file...\n", stdout);
	if (loadfile(opt.loadfile, &data[ndata].data, &data[ndata].size)) {
	    error("Failed to load the boot file\n");
	    goto bail;
	}
	data[ndata].base = load_base;
	load_base = 0x7c00;	/* If we also load a boot sector */

	/* Create boot info table: needed when you want to chainload
	   another version of ISOLINUX (or another bootlaoder that needs
	   the -boot-info-table switch of mkisofs)
	   (will only work when run from ISOLINUX) */
	if (opt.isolinux) {
	    const union syslinux_derivative_info *sdi;
	    sdi = syslinux_derivative_info();

	    if (sdi->c.filesystem == SYSLINUX_FS_ISOLINUX) {
		/* Boot info table info (integers in little endian format)

		   Offset Name         Size      Meaning
		   8     bi_pvd       4 bytes   LBA of primary volume descriptor
		   12     bi_file      4 bytes   LBA of boot file
		   16     bi_length    4 bytes   Boot file length in bytes
		   20     bi_csum      4 bytes   32-bit checksum
		   24     bi_reserved  40 bytes  Reserved

		   The 32-bit checksum is the sum of all the 32-bit words in the
		   boot file starting at byte offset 64. All linear block
		   addresses (LBAs) are given in CD sectors (normally 2048 bytes).

		   LBA of primary volume descriptor should already be set to 16. 
		 */

		isolinux_bin = (unsigned char *)data[ndata].data;

		/* Get LBA address of bootfile */
		file_lba = get_file_lba(opt.loadfile);

		if (file_lba == 0) {
		    error("Failed to find LBA offset of the boot file\n");
		    goto bail;
		}
		/* Set it */
		*((uint32_t *) & isolinux_bin[12]) = file_lba;

		/* Set boot file length */
		*((uint32_t *) & isolinux_bin[16]) = data[ndata].size;

		/* Calculate checksum */
		checksum = (uint32_t *) & isolinux_bin[20];
		chkhead = (uint32_t *) & isolinux_bin[64];
		chktail = (uint32_t *) & isolinux_bin[data[ndata].size & ~3];
		*checksum = 0;
		while (chkhead < chktail)
		    *checksum += *chkhead++;

		/*
		 * Deal with possible fractional dword at the end;
		 * this *should* never happen...
		 */
		if (data[ndata].size & 3) {
		    uint32_t xword = 0;
		    memcpy(&xword, chkhead, data[ndata].size & 3);
		    *checksum += xword;
		}
	    } else {
		error
		    ("The isolinux= option is only valid when run from ISOLINUX\n");
		goto bail;
	    }
	}

	if (opt.grub) {
	    /* Layout of stage2 file (from byte 0x0 to 0x270) */
	    struct grub_stage2_patch_area {
		/* 0x0 to 0x205 */
		char unknown[0x206];
		/* 0x206: compatibility version number major */
		uint8_t compat_version_major;
		/* 0x207: compatibility version number minor */
		uint8_t compat_version_minor;

		/* 0x208: install_partition variable */
		struct {
		    /* 0x208: sub-partition in sub-partition part2 */
		    uint8_t part3;
		    /* 0x209: sub-partition in top-level partition */
		    uint8_t part2;
		    /* 0x20a: top-level partiton number */
		    uint8_t part1;
		    /* 0x20b: BIOS drive number (must be 0) */
		    uint8_t drive;
		} __attribute__ ((packed)) install_partition;

		/* 0x20c: deprecated (historical reason only) */
		uint32_t saved_entryno;
		/* 0x210: stage2_ID: will always be STAGE2_ID_STAGE2 = 0 in stage2 */
		uint8_t stage2_id;
		/* 0x211: force LBA */
		uint8_t force_lba;
		/* 0x212: version string (will probably be 0.97) */
		char version_string[5];
		/* 0x217: config filename */
		char config_file[89];
		/* 0x270: start of code (after jump from 0x200) */
		char codestart[1];
	    } __attribute__ ((packed)) * stage2;

	    if (data[ndata].size < sizeof(struct grub_stage2_patch_area)) {
		error
		    ("The file specified by grub=<loader> is to small to be stage2 of GRUB Legacy.\n");
		goto bail;
	    }

	    stage2 = data[ndata].data;

	    /*
	     * Check the compatibility version number to see if we loaded a real
	     * stage2 file or a stage2 file that we support.
	     */
	    if (stage2->compat_version_major != 3
		|| stage2->compat_version_minor != 2) {
		error
		    ("The file specified by grub=<loader> is not a supported stage2 GRUB Legacy binary\n");
		goto bail;
	    }

	    /* jump 0x200 bytes into the loadfile */
	    regs.ip = 0x200;

	    /*
	     * GRUB Legacy wants the partition number in the install_partition
	     * variable, located at offset 0x208 of stage2.
	     * When GRUB Legacy is loaded, it is located at memory address 0x8208.
	     *
	     * It looks very similar to the "boot information format" of the
	     * Multiboot specification:
	     *   http://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Boot-information-format
	     *
	     *   0x208 = part3: sub-partition in sub-partition part2
	     *   0x209 = part2: sub-partition in top-level partition
	     *   0x20a = part1: top-level partition number
	     *   0x20b = drive: BIOS drive number (must be 0)
	     *
	     * GRUB Legacy doesn't store the BIOS drive number at 0x20b, but at
	     * another location.
	     *
	     * Partition numbers always start from zero.
	     * Unused partition bytes must be set to 0xFF. 
	     *
	     * We only care about top-level partition, so we only need to change
	     * "part1" to the appropriate value:
	     *   -1:   whole drive (default) (-1 = 0xFF)
	     *   0-3:  primary partitions
	     *   4-*:  logical partitions
	     */
	    stage2->install_partition.part1 = whichpart - 1;

	    /*
	     * Grub Legacy reserves 89 bytes (from 0x8217 to 0x826f) for the
	     * config filename. The filename passed via grubcfg= will overwrite
	     * the default config filename "/boot/grub/menu.lst".
	     */
	    if (opt.grubcfg) {
		if (strlen(opt.grubcfg) > sizeof(stage2->config_file) - 1) {
		    error
			("The config filename length can't exceed 88 characters.\n");
		    goto bail;
		}

		strcpy((char *)stage2->config_file, opt.grubcfg);
	    }
	}

	if (opt.drmk) {
	    /* DRMK entry is different than MS-DOS/PC-DOS */
	    /*
	     * A new size, aligned to 16 bytes to ease use of ds:[bp+28].
	     * We only really need 4 new, usable bytes at the end.
	     */
	    int tsize = (data[ndata].size + 19) & 0xfffffff0;
	    const union syslinux_derivative_info *sdi;

	    sdi = syslinux_derivative_info();
	    /* We should lookup the Syslinux partition offset and use it */
	    fs_lba = *sdi->disk.partoffset;
	    /*
	     * fs_lba should be verified against the disk as some DRMK
	     * variants will check and fail if it does not match
	     */
	    dprintf("  fs_lba offset is %"PRIu64"\n", fs_lba);
	    /* DRMK only uses a DWORD */
	    if (fs_lba > 0xffffffff) {
		error
		    ("LBA very large; Only using lower 32 bits; DRMK will probably fail\n");
	    }
	    regs.ss = regs.fs = regs.gs = 0;	/* Used before initialized */
	    if (!realloc(data[ndata].data, tsize)) {
		error("Failed to realloc for DRMK\n");
		goto bail;	/* We'll never make it */
	    }
	    data[ndata].size = tsize;
	    /* ds:bp is assumed by DRMK to be the boot sector */
	    /* offset 28 is the FAT HiddenSectors value */
	    regs.ds = (tsize >> 4) + (opt.seg - 2);
	    /* "Patch" into tail of the new space */
	    *(int *)(data[ndata].data + tsize - 4) = (int)(fs_lba & 0xffffffff);
	}

	ndata++;
    }

    if (!opt.loadfile || data[0].base >= 0x7c00 + SECTOR) {
	/* Actually read the boot sector */
	if (!cur_part) {
	    data[ndata].data = mbr;
	} else if (!(data[ndata].data = read_sectors(cur_part->lba_data, 1))) {
	    error("Cannot read boot sector\n");
	    goto bail;
	}
	data[ndata].size = SECTOR;
	data[ndata].base = load_base;

	if (!opt.loadfile) {
	    const struct mbr *br =
		(const struct mbr *)((char *)data[ndata].data +
				     data[ndata].size - sizeof(struct mbr));
	    if (br->sig != mbr_sig_magic) {
		error
		    ("Boot sector signature not found (unbootable disk/partition?)\n");
		goto bail;
	    }
	}
	/*
	 * To boot the Recovery Console of Windows NT/2K/XP we need to write
	 * the string "cmdcons\0" to memory location 0000:7C03.
	 * Memory location 0000:7C00 contains the bootsector of the partition.
	 */
	if (cur_part && opt.cmldr) {
	    memcpy((char *)data[ndata].data + 3, cmldr_signature,
		   sizeof cmldr_signature);
	}

	/*
	 * Modify the hidden sectors (partition offset) copy in memory;
	 * this modifies the field used by FAT and NTFS filesystems, and
	 * possibly other boot loaders which use the same format.
	 */
	if (cur_part && opt.sethidden) {
	    *(uint32_t *) ((char *)data[ndata].data + 28) = cur_part->lba_data;
	}

	ndata++;
    }

    if (cur_part) {
	if (cur_part->next == next_gpt_part) {
	    /* Do GPT hand-over, if applicable (as per syslinux/doc/gpt.txt) */
	    struct part_entry *record;
	    /* Look at the GPT partition */
	    const struct gpt_part *gp = (const struct gpt_part *)
		(cur_part->block +
		 (cur_part->private.gpt.size * cur_part->private.gpt.index));
	    /* Note the partition length */
	    uint64_t lba_count = gp->lba_last - gp->lba_first + 1;
	    /* The length of the hand-over */
	    int synth_size =
		sizeof(struct part_entry) + sizeof(uint32_t) +
		cur_part->private.gpt.size;
	    /* Will point to the partition record length in the hand-over */
	    uint32_t *plen;

	    /* Allocate the hand-over record */
	    record = malloc(synth_size);
	    if (!record) {
		error("Could not build GPT hand-over record!\n");
		goto bail;
	    }
	    /* Synthesize the record */
	    memset(record, 0, synth_size);
	    record->active_flag = 0x80;
	    record->ostype = 0xED;
	    /* All bits set by default */
	    record->start_lba = ~(uint32_t) 0;
	    record->length = ~(uint32_t) 0;
	    /* If these fit the precision, pass them on */
	    if (cur_part->lba_data < record->start_lba)
		record->start_lba = cur_part->lba_data;
	    if (lba_count < record->length)
		record->length = lba_count;
	    /* Next comes the GPT partition record length */
	    plen = (uint32_t *) (record + 1);
	    plen[0] = cur_part->private.gpt.size;
	    /* Next comes the GPT partition record copy */
	    memcpy(plen + 1, gp, plen[0]);
	    cur_part->record = record;

	    regs.eax.l = 0x54504721;	/* '!GPT' */
	    data[ndata].base = 0x7be;
	    data[ndata].size = synth_size;
	    data[ndata].data = (void *)record;
	    ndata++;
	    regs.esi.w[0] = 0x7be;

	    dprintf("GPT handover:\n");
	    mbr_part_dump(record);
	    gpt_part_dump((struct gpt_part *)(plen + 1));
	} else if (cur_part->record) {
	    /* MBR handover protocol */
	    static struct part_entry handover_record;

	    handover_record = *cur_part->record;
	    handover_record.start_lba = cur_part->lba_data;

	    data[ndata].base = 0x7be;
	    data[ndata].size = sizeof handover_record;
	    data[ndata].data = &handover_record;
	    ndata++;
	    regs.esi.w[0] = 0x7be;

	    dprintf("MBR handover:\n");
	    mbr_part_dump(&handover_record);
	}
    }

    do_boot(data, ndata, &regs);

bail:
    if (cur_part) {
	free(cur_part->block);
	free((void *)cur_part->record);
    }
    free(cur_part);
    free(mbr);
    return 255;
}
