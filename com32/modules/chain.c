/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
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
 * Usage: chain hd<disk#> [<partition>] [options]
 *        chain fd<disk#> [options]
 *	  chain mbr:<id> [<partition>] [options]
 *	  chain boot [<partition>] [options]
 *
 * ... e.g. "chain hd0 1" will boot the first partition on the first hard
 * disk.
 *
 *
 * The mbr: syntax means search all the hard disks until one with a
 * specific MBR serial number (bytes 440-443) is found.
 *
 * Partitions 1-4 are primary, 5+ logical, 0 = boot MBR (default.)
 *
 * Options:
 *
 * file=<loader>:
 *	loads the file <loader> **from the SYSLINUX filesystem**
 *	instead of loading the boot sector.
 *
 * seg=<segment>:
 *	loads at and jumps to <seg>:0000 instead of 0000:7C00.
 *
 * isolinux=<loader>:
 *	chainload another version/build of the ISOLINUX bootloader and patch
 *	the loader with appropriate parameters in memory.
 *	This avoids the need for the -eltorito-alt-boot parameter of mkisofs,
 *	when you want more than one ISOLINUX per CD/DVD.
 *
 * ntldr=<loader>:
 *	equivalent to seg=0x2000 file=<loader> sethidden,
 *      used with WinNT's loaders
 *
 * cmldr=<loader>:
 *      used with Recovery Console of Windows NT/2K/XP.
 *      same as ntldr=<loader> & "cmdcons\0" written to
 *      the system name field in the bootsector
 *
 * freedos=<loader>:
 *	equivalent to seg=0x60 file=<loader> sethidden,
 *      used with FreeDOS kernel.sys.
 *
 * msdos=<loader>
 * pcdos=<loader>
 *	equivalent to seg=0x70 file=<loader> sethidden,
 *      used with DOS' io.sys.
 *
 * swap:
 *	if the disk is not fd0/hd0, install a BIOS stub which swaps
 *	the drive numbers.
 *
 * hide:
 *	change type of primary partitions with IDs 01, 04, 06, 07,
 *	0b, 0c, or 0e to 1x, except for the selected partition, which
 *	is converted the other way.
 *
 * sethidden:
 *      update the "hidden sectors" (partition offset) field in a
 *      FAT/NTFS boot sector.
 */

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <console.h>
#include <minmax.h>
#include <stdbool.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/config.h>
#include <syslinux/video.h>

#define SECTOR 512		/* bytes/sector */

static struct options {
    const char *loadfile;
    uint16_t keeppxe;
    uint16_t seg;
    bool isolinux;
    bool cmldr;
    bool swap;
    bool hide;
    bool sethidden;
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

static void *read_sector(unsigned int lba)
{
    com32sys_t inreg;
    struct ebios_dapa *dapa = __com32.cs_bounce;
    void *buf = (char *)__com32.cs_bounce + SECTOR;
    void *data;

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
	inreg.eax.b[1] = 0x42;	/* Extended read */
    } else {
	unsigned int c, h, s, t;

	if (!disk_info.cbios) {
	    /* We failed to get the geometry */

	    if (lba)
		return NULL;	/* Can only read MBR */

	    s = 1;
	    h = 0;
	    c = 0;
	} else {
	    s = (lba % disk_info.sect) + 1;
	    t = lba / disk_info.sect;	/* Track = head*cyl */
	    h = t % disk_info.head;
	    c = t / disk_info.head;
	}

	if (s > 63 || h > 256 || c > 1023)
	    return NULL;

	inreg.eax.w[0] = 0x0201;	/* Read one sector */
	inreg.ecx.b[1] = c & 0xff;
	inreg.ecx.b[0] = s + (c >> 6);
	inreg.edx.b[1] = h;
	inreg.edx.b[0] = disk_info.disk;
	inreg.ebx.w[0] = OFFS(buf);
	inreg.es = SEG(buf);
    }

    if (int13_retry(&inreg, NULL))
	return NULL;

    data = malloc(SECTOR);
    if (data)
	memcpy(data, buf, SECTOR);
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

	    s = 1;
	    h = 0;
	    c = 0;
	} else {
	    s = (lba % disk_info.sect) + 1;
	    t = lba / disk_info.sect;	/* Track = head*cyl */
	    h = t % disk_info.head;
	    c = t / disk_info.head;
	}

	if (s > 63 || h > 256 || c > 1023)
	    return -1;

	inreg.eax.w[0] = 0x0301;	/* Write one sector */
	inreg.ecx.b[1] = c & 0xff;
	inreg.ecx.b[0] = s + (c >> 6);
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
    rb = read_sector(lba);
    if (!rb)
	return -1;		/* Readback failure */
    rv = memcmp(buf, rb, SECTOR);
    free(rb);
    return rv ? -1 : 0;
}

/* Search for a specific drive, based on the MBR signature; bytes
   440-443. */
static int find_disk(uint32_t mbr_sig)
{
    int drive;
    bool is_me;
    char *buf;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (get_disk_params(drive))
	    continue;		/* Drive doesn't exist */
	if (!(buf = read_sector(0)))
	    continue;		/* Cannot read sector */
	is_me = (*(uint32_t *) ((char *)buf + 440) == mbr_sig);
	free(buf);
	if (is_me)
	    return drive;
    }
    return -1;
}

/* A DOS partition table entry */
struct part_entry {
    uint8_t active_flag;	/* 0x80 if "active" */
    uint8_t start_head;
    uint8_t start_sect;
    uint8_t start_cyl;
    uint8_t ostype;
    uint8_t end_head;
    uint8_t end_sect;
    uint8_t end_cyl;
    uint32_t start_lba;
    uint32_t length;
} __attribute__ ((packed));

/* Search for a logical partition.  Logical partitions are actually implemented
   as recursive partition tables; theoretically they're supposed to form a
   linked list, but other structures have been seen.

   To make things extra confusing: data partition offsets are relative to where
   the data partition record is stored, whereas extended partition offsets
   are relative to the beginning of the extended partition all the way back
   at the MBR... but still not absolute! */

int nextpart;			/* Number of the next logical partition */

static struct part_entry *find_logical_partition(int whichpart, char *table,
						 struct part_entry *self,
						 struct part_entry *root)
{
    static struct part_entry ltab_entry;
    struct part_entry *ptab = (struct part_entry *)(table + 0x1be);
    struct part_entry *found;
    char *sector;

    int i;

    if (*(uint16_t *) (table + 0x1fe) != 0xaa55)
	return NULL;		/* Signature missing */

    /* We are assumed to already having enumerated all the data partitions
       in this table if this is the MBR.  For MBR, self == NULL. */

    if (self) {
	/* Scan the data partitions. */

	for (i = 0; i < 4; i++) {
	    if (ptab[i].ostype == 0x00 || ptab[i].ostype == 0x05 ||
		ptab[i].ostype == 0x0f || ptab[i].ostype == 0x85)
		continue;	/* Skip empty or extended partitions */

	    if (!ptab[i].length)
		continue;

	    /* Adjust the offset to account for the extended partition itself */
	    ptab[i].start_lba += self->start_lba;

	    /*
	     * Sanity check entry: must not extend outside the
	     * extended partition.  This is necessary since some OSes
	     * put crap in some entries.  Note that root is non-NULL here.
	     */
	    if (ptab[i].start_lba + ptab[i].length <= root->start_lba ||
		ptab[i].start_lba >= root->start_lba + root->length)
		continue;

	    /* OK, it's a data partition.  Is it the one we're looking for? */
	    if (nextpart++ == whichpart) {
		memcpy(&ltab_entry, &ptab[i], sizeof ltab_entry);
		return &ltab_entry;
	    }
	}
    }

    /* Scan the extended partitions. */
    for (i = 0; i < 4; i++) {
	if (ptab[i].ostype != 0x05 &&
	    ptab[i].ostype != 0x0f && ptab[i].ostype != 0x85)
	    continue;		/* Skip empty or data partitions */

	if (!ptab[i].length)
	    continue;

	/* Adjust the offset to account for the extended partition itself */
	if (root)
	    ptab[i].start_lba += root->start_lba;

	/* Sanity check entry: must not extend outside the extended partition.
	   This is necessary since some OSes put crap in some entries. */
	if (root)
	    if (ptab[i].start_lba + ptab[i].length <= root->start_lba ||
		ptab[i].start_lba >= root->start_lba + root->length)
		continue;

	/* Process this partition */
	if (!(sector = read_sector(ptab[i].start_lba)))
	    continue;		/* Read error, must be invalid */

	found = find_logical_partition(whichpart, sector, &ptab[i],
				       root ? root : &ptab[i]);
	free(sector);
	if (found)
	    return found;
    }

    /* If we get here, there ain't nothing... */
    return NULL;
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
				  (addr_t)data[i].data, data[i].size))
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

static int hide_unhide(char *mbr, int part)
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
	pt = (struct part_entry *)&mbr[0x1be + 16 * (i - 1)];
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
    error("Usage:   chain.c32 hd<disk#> [<partition>] [options]\n"
	  "         chain.c32 fd<disk#> [options]\n"
	  "         chain.c32 mbr:<id> [<partition>] [options]\n"
	  "         chain.c32 boot [<partition>] [options]\n"
	  "Options: file=<loader>      load file, instead of boot sector\n"
	  "         isolinux=<loader>  load another version of ISOLINUX\n"
	  "         ntldr=<loader>     load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n"
	  "         cmldr=<loader>     load Recovery Console of Windows NT/2K/XP\n"
	  "         freedos=<loader>   load FreeDOS kernel.sys\n"
	  "         msdos=<loader>     load MS-DOS io.sys\n"
	  "         pcdos=<loader>     load PC-DOS ibmbio.com\n"
	  "         seg=<segment>      jump to <seg>:0000 instead of 0000:7C00\n"
	  "         swap               swap drive numbers, if bootdisk is not fd0/hd0\n"
	  "         hide               hide primary partitions, except selected partition\n"
	  "         sethidden          set the FAT/NTFS hidden sectors field\n"
	);
}


int main(int argc, char *argv[])
{
    char *mbr, *p;
    void *boot_sector = NULL;
    struct part_entry *partinfo;
    struct syslinux_rm_regs regs;
    char *drivename, *partition;
    int hd, drive, whichpart;
    int i;
    uint32_t file_lba = 0;
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

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "file=", 5)) {
	    opt.loadfile = argv[i] + 5;
	} else if (!strncmp(argv[i], "seg=", 4)) {
	    uint32_t segval = strtoul(argv[i] + 4, NULL, 0);
	    if (segval < 0x50 || segval > 0x9f000) {
		error("Invalid segment\n");
		goto bail;
	    }
	    opt.seg = segval;
	} else if (!strncmp(argv[i], "isolinux=", 9)) {
	    opt.loadfile = argv[i] + 9;
	    opt.isolinux = true;
	} else if (!strncmp(argv[i], "ntldr=", 6)) {
	    opt.seg = 0x2000;	/* NTLDR wants this address */
	    opt.loadfile = argv[i] + 6;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.seg = 0x2000;    /* CMLDR wants this address */
	    opt.loadfile = argv[i] + 6;
	    opt.cmldr = true;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "freedos=", 8)) {
	    opt.seg = 0x60;	/* FREEDOS wants this address */
	    opt.loadfile = argv[i] + 8;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "msdos=", 6) ||
		   !strncmp(argv[i], "pcdos=", 6)) {
	    opt.seg = 0x70;	/* MS-DOS 2.0+ wants this address */
	    opt.loadfile = argv[i] + 6;
	    opt.sethidden = true;
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
		   || !strcmp(argv[i], "boot")
		   || !strncmp(argv[i], "boot,", 5)) {
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

    if (opt.seg) {
	regs.es = regs.cs = regs.ss = regs.ds = regs.fs = regs.gs = opt.seg;
    } else {
	regs.ip = regs.esp.l = 0x7c00;
    }

    hd = 0;
    if (!strncmp(drivename, "mbr", 3)) {
	drive = find_disk(strtoul(drivename + 4, NULL, 0));
	if (drive == -1) {
	    error("Unable to find requested MBR signature\n");
	    goto bail;
	}
    } else if ((drivename[0] == 'h' || drivename[0] == 'f') &&
	       drivename[1] == 'd') {
	hd = drivename[0] == 'h';
	drivename += 2;
	drive = (hd ? 0x80 : 0) | strtoul(drivename, NULL, 0);
    } else if (!strcmp(drivename, "boot")) {
	const union syslinux_derivative_info *sdi;
	sdi = syslinux_derivative_info();
	if (sdi->c.filesystem == SYSLINUX_FS_PXELINUX)
	    drive = 0x80;	/* Boot drive not available */
	else
	    drive = sdi->disk.drive_number;
    } else {
	error("Unparsable drive specification\n");
	goto bail;
    }

    /* DOS kernels want the drive number in BL instead of DL.  Indulge them. */
    regs.ebx.b[0] = regs.edx.b[0] = drive;

    whichpart = 0;		/* Default */
    if (partition)
	whichpart = strtoul(partition, NULL, 0);

    if (!(drive & 0x80) && whichpart) {
	error("Warning: Partitions of floppy devices may not work\n");
    }

    /* 
     * grldr of Grub4dos wants the partition number in DH:
     * -1:   whole drive (default)
     * 0-3:  primary partitions
     * 4-*:  logical partitions
     */
    regs.edx.b[1] = whichpart-1;

    /* Get the disk geometry and disk access setup */
    if (get_disk_params(drive)) {
	error("Cannot get disk parameters\n");
	goto bail;
    }

    /* Get MBR */
    if (!(mbr = read_sector(0))) {
	error("Cannot read Master Boot Record\n");
	goto bail;
    }

    if (opt.hide) {
	if (whichpart < 1 || whichpart > 4)
	    error("WARNING: hide specified without a non-primary partition\n");
	if (hide_unhide(mbr, whichpart))
	    error("WARNING: failed to write MBR for 'hide'\n");
    }

    if (whichpart == 0) {
	/* Boot the MBR */

	partinfo = NULL;
	boot_sector = mbr;
    } else if (whichpart <= 4) {
	/* Boot a primary partition */

	partinfo = &((struct part_entry *)(mbr + 0x1be))[whichpart - 1];
	if (partinfo->ostype == 0) {
	    error("Invalid primary partition\n");
	    goto bail;
	}
    } else {
	/* Boot a logical partition */

	nextpart = 5;
	partinfo = find_logical_partition(whichpart, mbr, NULL, NULL);

	if (!partinfo || partinfo->ostype == 0) {
	    error("Requested logical partition not found\n");
	    goto bail;
	}
    }

    /* Do the actual chainloading */
    load_base = opt.seg ? (opt.seg << 4) : 0x7c00;

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
		*((uint32_t *) &isolinux_bin[12]) = file_lba;

		/* Set boot file length */
		*((uint32_t *) &isolinux_bin[16]) = data[ndata].size;

		/* Calculate checksum */
		checksum = (uint32_t *) &isolinux_bin[20];
		chkhead = (uint32_t *) &isolinux_bin[64];
		chktail = (uint32_t *) &isolinux_bin[data[ndata].size & ~3];
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

	ndata++;
    }

    if (partinfo && (!opt.loadfile || data[0].base >= 0x7c00 + SECTOR)) {
	/* Actually read the boot sector */
	/* Pick the first buffer that isn't already in use */
	if (!(data[ndata].data = read_sector(partinfo->start_lba))) {
	    error("Cannot read boot sector\n");
	    goto bail;
	}
	data[ndata].size = SECTOR;
	data[ndata].base = load_base;
	
	if (!opt.loadfile &&
	    *(uint16_t *)((char *)data[ndata].data +
			  data[ndata].size - 2) != 0xaa55) {
	    error("Boot sector signature not found (unbootable disk/partition?)\n");
	    goto bail;
	}

	/*
	 * To boot the Recovery Console of Windows NT/2K/XP we need to write
	 * the string "cmdcons\0" to memory location 0000:7C03.
	 * Memory location 0000:7C00 contains the bootsector of the partition.
	 */
	if (opt.cmldr) {
	    memcpy((char *)data[ndata].data+3, cmldr_signature,
		   sizeof cmldr_signature);
	}

	/*
	 * Modify the hidden sectors (partition offset) copy in memory;
	 * this modifies the field used by FAT and NTFS filesystems, and
	 * possibly other boot loaders which use the same format.
	 */
	if (partinfo && opt.sethidden) {
	    *((uint32_t *)(char *)data[ndata].data + 28) =
		partinfo->start_lba;
	}

	ndata++;
    }

    if (partinfo) {
	/* 0x7BE is the canonical place for the first partition entry. */
	data[ndata].data = partinfo;
	data[ndata].base = 0x7be;
	data[ndata].size = sizeof *partinfo;
	ndata++;
	regs.esi.w[0] = 0x7be;
    }

    do_boot(data, ndata, &regs);

  bail:
    return 255;
}
