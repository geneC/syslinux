/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
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
 * Please see doc/chain.txt for the detailed documentation.
 */

#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <console.h>
#include <consoles.h>
#include <minmax.h>
#include <stdbool.h>
#include <dprintf.h>
#include <errno.h>
#include <unistd.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/config.h>
#include <syslinux/disk.h>
#include <syslinux/video.h>
#include "common.h"
#include "chain.h"
#include "utility.h"
#include "options.h"
#include "partiter.h"
#include "mangle.h"

struct options opt;

static int fixed_cnt;

static int overlap(const struct data_area *a, const struct data_area *b)
{
    return
	a->base + a->size > b->base &&
	b->base + b->size > a->base;
}

static int is_phys(uint8_t sdifs)
{
    return
	sdifs == SYSLINUX_FS_SYSLINUX ||
	sdifs == SYSLINUX_FS_EXTLINUX ||
	sdifs == SYSLINUX_FS_ISOLINUX;
}

/*
 * Search for a specific drive, based on the MBR signature.
 * Return drive and iterator at 0th position.
 */
static int find_by_sig(uint32_t mbr_sig,
			struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo, 0)))
	    continue;
	/* Check for a MBR disk */
	if (boot_part->type != typedos) {
	    pi_del(&boot_part);
	    continue;
	}
	if (boot_part->sub.dos.disk_sig == mbr_sig) {
	    goto ok;
	}
    }
    drive = -1;
ok:
    *_boot_part = boot_part;
    return drive;
}

/*
 * Search for a specific drive/partition, based on the GPT GUID.
 * Return drive and iterator at proper position.
 */
static int find_by_guid(const struct guid *gpt_guid,
			struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo, 0)))
	    continue;
	/* Check for a GPT disk */
	if (boot_part->type != typegpt) {
	    pi_del(&boot_part);
	    continue;
	}
	/* Check for a matching GPT disk guid */
	if (!memcmp(&boot_part->sub.gpt.disk_guid, gpt_guid, sizeof(*gpt_guid))) {
	    goto ok;
	}
	/* disk guid doesn't match, maybe partition guid will */
	while (pi_next(&boot_part)) {
	    if (!memcmp(&boot_part->sub.gpt.part_guid, gpt_guid, sizeof(*gpt_guid)))
		goto ok;
	}
    }
    drive = -1;
ok:
    *_boot_part = boot_part;
    return drive;
}

/*
 * Search for a specific drive/partition, based on the GPT label.
 * Return drive and iterator at proper position.
 */
static int find_by_label(const char *label, struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo, 0)))
	    continue;
	/* Check for a GPT disk */
	if (!(boot_part->type == typegpt)) {
	    pi_del(&boot_part);
	    continue;
	}
	/* Check for a matching partition */
	while (pi_next(&boot_part)) {
	    if (!strcmp(label, boot_part->sub.gpt.part_label))
		goto ok;
	}
    }
    drive = -1;
ok:
    *_boot_part = boot_part;
    return drive;
}

static void do_boot(struct data_area *data, int ndata)
{
    uint16_t *const bios_fbm = (uint16_t *) 0x413;
    addr_t dosmem = (addr_t)(*bios_fbm << 10);	/* Technically a low bound */
    struct syslinux_memmap *mmap;
    struct syslinux_movelist *mlist = NULL;
    addr_t endimage;
    uint8_t driveno = opt.regs.edx.b[0];
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
	endimage = (endimage + 1023u) & ~1023u;

	/* Create swap stub */
	memcpy(swapstub, swapstub_master, sizeof swapstub_master);
	*(uint16_t *) & swapstub[0x3a] = opt.regs.ds;
	*(uint16_t *) & swapstub[0x3d] = opt.regs.es;
	*(uint32_t *) & swapstub[0x45] = opt.regs.ecx.l;
	*(uint32_t *) & swapstub[0x4b] = opt.regs.esi.l;
	*(uint32_t *) & swapstub[0x51] = opt.regs.edi.l;
	*(uint16_t *) & swapstub[0x56] = opt.regs.ip;
	*(uint16_t *) & swapstub[0x58] = opt.regs.cs;
	p = &swapstub[sizeof swapstub_master];

	/* Mapping table; start out with identity mapping everything */
	for (i = 0; i < 256; i++)
	    p[i] = (uint8_t)i;

	/* And the actual swap */
	p[driveno] = swapdrive;
	p[swapdrive] = driveno;

	/* Adjust registers */
	opt.regs.ds = opt.regs.cs = (uint16_t)(endimage >> 4);
	opt.regs.esi.l = opt.regs.es = 0;
	opt.regs.ecx.l = sizeof swapstub >> 2;
	opt.regs.ip = 0x10;	/* Installer offset */
	opt.regs.ebx.b[0] = opt.regs.edx.b[0] = swapdrive;

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
    syslinux_shuffle_boot_rm(mlist, mmap, opt.keeppxe, &opt.regs);
    error("Chainboot failed!\n");
    return;

too_big:
    error("Loader file too large\n");
    return;

enomem:
    error("Out of memory\n");
    return;
}
#if 0
static void hide_unhide(const struct part_iter *_iter)
{
    int i;
    struct disk_dos_mbr *mbr = NULL;
    struct disk_dos_part_entry *pt;
    const uint16_t mask =
	(1 << 0x01) | (1 << 0x04) | (1 << 0x06) |
	(1 << 0x07) | (1 << 0x0b) | (1 << 0x0c) | (1 << 0x0e);
    uint8_t t;
    bool write_back = false;

    if (_iter->type != typedos) {
	error("Option 'hide' is only meaningful for legacy partition scheme.");
	goto bail;
    }
    if (!(mbr = disk_read_sectors(&_iter->di, 0, 1))) {
	error("WARNING: Couldn't read MBR to hide/unhide partitions.\n");
	goto bail;
    }

    if (_iter->index < 1 || _iter->index > 4)
	error("WARNING: option 'hide' specified with a non-primary partition.\n");

    for (i = 1; i <= 4; i++) {
	pt = mbr->table + i - 1;
	t = pt->ostype;
	if ((t <= 0x1f) && ((mask >> (t & ~0x10u)) & 1)) {
	    /* It's a hideable partition type */
	    if (i == _iter->index)
		t &= (uint8_t)(~0x10u);	/* unhide */
	    else
		t |= 0x10u;	/* hide */
	}
	if (t != pt->ostype) {
	    write_back = true;
	    pt->ostype = t;
	}
    }
    if (write_back && disk_write_verify_sector(&_iter->di, 0, mbr))
	error("WARNING: failed to write MBR for option 'hide'\n");

bail:
    free(mbr);
}
#endif
static int pem_sethide(struct disk_dos_part_entry *dp, int midx, int idx)
{
    static const uint16_t mask =
	(1 << 0x01) | (1 << 0x04) | (1 << 0x06) |
	(1 << 0x07) | (1 << 0x0b) | (1 << 0x0c) | (1 << 0x0e);
    uint8_t t;

    t = dp->ostype;
    if ((t <= 0x1f) && ((mask >> (t & ~0x10u)) & 1)) {
	/* It's a hideable partition type */
	if (midx == idx)
	    t &= (uint8_t)(~0x10u);	/* unhide */
	else
	    t |= 0x10u;	/* hide */
    }
    if (t != dp->ostype) {
	dp->ostype = t;
	return -1;
    }
    return 0;
}

static int pem_setchs(const struct disk_info *di,
		     struct disk_dos_part_entry *dp)
{
    uint32_t ochs1, ochs2;

    ochs1 = *(uint32_t *)dp->start;
    ochs2 = *(uint32_t *)dp->end;

    *(uint32_t *)dp->start =
	lba2chs(di, dp->start_lba) |
	(*(uint32_t *)dp->start & 0xFF000000);

    *(uint32_t *)dp->end =
	lba2chs(di, dp->start_lba + dp->length - 1) |
	(*(uint32_t *)dp->end & 0xFF000000);

    return
	*(uint32_t *)dp->start != ochs1 ||
	*(uint32_t *)dp->end != ochs2;
}

static int pe_mangle(const struct part_iter *_iter)
{
    int wb = 0, werr = 0;
    uint32_t mbr_lba = 0;
    struct part_iter *iter = NULL;
    struct disk_dos_part_entry *dp;
    struct disk_dos_mbr mbr;
    int ridx;

    if (_iter->type != typedos) {
	error("Partition entry mangling ('hide[all]', 'mbrchs')\n"
	      "is meaningful only for legacy partition scheme.");
	goto bail;
    }
    if ((_iter->index < 1 || _iter->index > 4) && opt.hide == 1)
	error("WARNING: option 'hide' specified with a non-primary partition.\n");

    if (!(iter = pi_begin(&_iter->di, 1)))  /* turn on stepall */
	goto bail;

    memcpy(&mbr, iter->data, sizeof(struct disk_dos_mbr));

    while (pi_next(&iter) && !werr) {
	ridx = iter->rawindex;
	if (ridx > 4) {
	    if (opt.hide < 2 && !opt.mbrchs)
		break;
	    if (wb && !werr) {
		werr |= disk_write_verify_sector(&_iter->di, mbr_lba, &mbr);
		wb = false;
	    }
	    memcpy(&mbr, iter->data, sizeof(struct disk_dos_mbr));
	    mbr_lba = iter->sub.dos.mbr_lba;
	    dp = mbr.table;
	} else
	    dp = mbr.table + ridx - 1;
	if (opt.hide == 2 ||
	    (opt.hide == 1 && ridx <= 4)) {
	    wb |= pem_sethide(dp, _iter->index, iter->index);
	    if (_iter->index == iter->index) {
		((struct disk_dos_part_entry *)_iter->record)->ostype =
		    dp->ostype;
	    }
	}
	if (opt.mbrchs) {
	    wb |= pem_setchs(&_iter->di, dp);
	    if (ridx > 4)
		wb |= pem_setchs(&_iter->di, mbr.table + 1);
	}
    }
    /* last write */
    if (wb && !werr)
	werr |= disk_write_verify_sector(&_iter->di, mbr_lba, &mbr);

bail:
    pi_del(&iter);
    if (werr)
	error("WARNING: failed to write E/MBR for partition\n"
	      "mangling options ('hide[all]', 'mbrchs')\n");
    return 0;
}

int find_dp(struct part_iter **_iter)
{
    struct part_iter *iter;
    struct disk_info diskinfo;
    struct guid gpt_guid;
    uint64_t fs_lba;
    int drive, hd, partition;
    const union syslinux_derivative_info *sdi;

    sdi = syslinux_derivative_info();

    if (!strncmp(opt.drivename, "mbr", 3)) {
	if (find_by_sig(strtoul(opt.drivename + 4, NULL, 0), &iter)) {
	    error("Unable to find requested MBR signature.\n");
	    goto bail;
	}
    } else if (!strncmp(opt.drivename, "guid", 4)) {
	if (str_to_guid(opt.drivename + 5, &gpt_guid))
	    goto bail;
	if (find_by_guid(&gpt_guid, &iter)) {
	    error("Unable to find requested GPT disk or partition by guid.\n");
	    goto bail;
	}
    } else if (!strncmp(opt.drivename, "label", 5)) {
	if (!opt.drivename[6]) {
	    error("No label specified.\n");
	    goto bail;
	}
	if (find_by_label(opt.drivename + 6, &iter)) {
	    error("Unable to find requested GPT partition by label.\n");
	    goto bail;
	}
    } else if ((opt.drivename[0] == 'h' || opt.drivename[0] == 'f') &&
	       opt.drivename[1] == 'd') {
	hd = opt.drivename[0] == 'h' ? 0x80 : 0;
	opt.drivename += 2;
	drive = hd | strtol(opt.drivename, NULL, 0);

	if (disk_get_params(drive, &diskinfo))
	    goto bail;
	/* this will start iteration over FDD, possibly raw */
	if (!(iter = pi_begin(&diskinfo, 0)))
	    goto bail;

    } else if (!strcmp(opt.drivename, "boot") || !strcmp(opt.drivename, "fs")) {
	if (!is_phys(sdi->c.filesystem)) {
	    error("When syslinux is not booted from physical disk (or its emulation),\n"
		   "'boot' and 'fs' are meaningless.\n");
	    goto bail;
	}
	/* offsets match, but in case it changes in the future */
	if (sdi->c.filesystem == SYSLINUX_FS_ISOLINUX) {
	    drive = sdi->iso.drive_number;
	    fs_lba = *sdi->iso.partoffset;
	} else {
	    drive = sdi->disk.drive_number;
	    fs_lba = *sdi->disk.partoffset;
	}
	if (disk_get_params(drive, &diskinfo))
	    goto bail;
	/* this will start iteration over disk emulation, possibly raw */
	if (!(iter = pi_begin(&diskinfo, 0)))
	    goto bail;

	/* 'fs' => we should lookup the syslinux partition number and use it */
	if (!strcmp(opt.drivename, "fs")) {
	    while (pi_next(&iter)) {
		if (iter->start_lba == fs_lba)
		    break;
	    }
	    /* broken part structure or other problems */
	    if (!iter) {
		error("Can't find myself on the drive I booted from.\n");
		goto bail;
	    }
	}
    } else {
	error("Unparsable drive specification.\n");
	goto bail;
    }
    /* main options done - only thing left is explicit partition specification,
     * if we're still at the disk stage with the iterator AND user supplied
     * partition number (including disk).
     */
    if (!iter->index && opt.partition) {
	partition = strtol(opt.partition, NULL, 0);
	/* search for matching part#, including disk */
	do {
	    if (iter->index == partition)
		break;
	} while (pi_next(&iter));
	if (!iter) {
	    error("Requested disk / partition combination not found.\n");
	    goto bail;
	}
    }

    if (!(iter->di.disk & 0x80) && iter->index) {
	error("WARNING: Partitions on floppy devices may not work.\n");
    }

    *_iter = iter;

    return 0;

bail:
    return -1;
}

static int setup_handover(const struct part_iter *iter,
		   struct data_area *data)
{
    const struct disk_dos_part_entry *dp;
    const struct disk_gpt_part_entry *gp;
    struct disk_dos_part_entry *ha;
    uint64_t lba_count;
    uint32_t synth_size;
    uint32_t *plen;

    if (iter->type == typegpt) {
	/* GPT handover protocol */
	gp = (const struct disk_gpt_part_entry *)iter->record;
	lba_count = gp->lba_last - gp->lba_first + 1;
	synth_size = sizeof(struct disk_dos_part_entry) +
	    sizeof(uint32_t) + (uint32_t)iter->sub.gpt.pe_size;

	ha = malloc(synth_size);
	if (!ha) {
	    error("Could not build GPT hand-over record!\n");
	    goto bail;
	}
	memset(ha, 0, synth_size);
	*(uint32_t *)ha->start = lba2chs(&iter->di, gp->lba_first);
	*(uint32_t *)ha->end = lba2chs(&iter->di, gp->lba_last);
	ha->active_flag = 0x80;
	ha->ostype = 0xED;
	/* All bits set by default */
	ha->start_lba = ~0u;
	ha->length = ~0u;
	/* If these fit the precision, pass them on */
	if (iter->start_lba < ha->start_lba)
	    ha->start_lba = (uint32_t)iter->start_lba;
	if (lba_count < ha->length)
	    ha->length = (uint32_t)lba_count;
	/* Next comes the GPT partition record length */
	plen = (uint32_t *) (ha + 1);
	plen[0] = (uint32_t)iter->sub.gpt.pe_size;
	/* Next comes the GPT partition record copy */
	memcpy(plen + 1, gp, plen[0]);
#ifdef DEBUG
	dprintf("GPT handover:\n");
	disk_dos_part_dump(ha);
	disk_gpt_part_dump((struct disk_gpt_part_entry *)(plen + 1));
#endif
    } else if (iter->type == typedos) {
	/* MBR handover protocol */
	dp = (const struct disk_dos_part_entry *)iter->record;
	synth_size = sizeof(struct disk_dos_part_entry);
	ha = malloc(synth_size);
	if (!ha) {
	    error("Could not build MBR hand-over record!\n");
	    goto bail;
	}

	*(uint32_t *)ha->start = lba2chs(&iter->di, iter->start_lba);
	*(uint32_t *)ha->end = lba2chs(&iter->di, iter->start_lba + dp->length - 1);
	ha->active_flag = dp->active_flag;
	ha->ostype = dp->ostype;
	ha->start_lba = (uint32_t)iter->start_lba;  /* fine, we iterate over legacy scheme */
	ha->length = dp->length;

#ifdef DEBUG
	dprintf("MBR handover:\n");
	disk_dos_part_dump(ha);
#endif
    } else {
	/* shouldn't ever happen */
	goto bail;
    }

    data->base = 0x7be;
    data->size = synth_size;
    data->data = (void *)ha;

    return 0;
bail:
    return -1;
}

int setdrv_auto(const struct part_iter *iter)
{
    int a, b;
    char *buf;

    if (!(buf = disk_read_sectors(&iter->di, iter->start_lba, 1))) {
	error("Couldn't read a sector to detect 'setdrv' offset.\n");
	return -1;
    }

    a = strncmp(buf + 0x36, "FAT", 3);
    b = strncmp(buf + 0x52, "FAT", 3);

    if ((!a && b && (buf[0x26] & 0xFE) == 0x28) || *((uint8_t*)buf + 0x26) == 0x80) {
	opt.drvoff = 0x24;
    } else if (a && !b && (buf[0x42] & 0xFE) == 0x28) {
	opt.drvoff = 0x40;
    } else {
	error("WARNING: Couldn't autodetect 'setdrv' offset - turning option off.\n");
	opt.setdrv = false;
    }

    free(buf);
    return 0;

}

int main(int argc, char *argv[])
{
    struct part_iter *iter = NULL;

    void *file_area = NULL;
    void *sect_area = NULL;
    struct disk_dos_part_entry *hand_area = NULL;

    struct data_area data[3], bdata[3];
    int ndata = 0, fidx = -1, sidx = -1, hidx = -1;

    console_ansi_raw();
/*    openconsole(&dev_null_r, &dev_stdcon_w);*/

    /* Prepare and set defaults */
    memset(&opt, 0, sizeof(opt));
    opt.sect = true;	/* by def load sector */
    opt.maps = true;	/* by def map sector */
    opt.hand = true;	/* by def prepare handover */
    opt.foff = opt.soff = opt.fip = opt.sip = 0x7C00;
    opt.drivename = "boot";
#ifdef DEBUG
    opt.warn = true;
#endif

    /* Parse arguments */
    if (parse_args(argc, argv))
	goto bail;

    /* Set initial registry values */
    if (opt.file) {
	opt.regs.cs = opt.regs.ds = opt.regs.ss = (uint16_t)opt.fseg;
	opt.regs.ip = (uint16_t)opt.fip;
    } else {
	opt.regs.cs = opt.regs.ds = opt.regs.ss = (uint16_t)opt.sseg;
	opt.regs.ip = (uint16_t)opt.sip;
    }

    if (opt.regs.ip == 0x7C00 && !opt.regs.cs)
	opt.regs.esp.l = 0x7C00;

    /* Get max fixed disk number */
    fixed_cnt = *(uint8_t *)(0x475);

    /* Get disk/part iterator matching user supplied options */
    if (find_dp(&iter))
	goto bail;

    /* Try to autodetect setdrv offest */
    if (opt.setdrv && opt.drvoff == ~0u && setdrv_auto(iter))
	goto bail;

    /* DOS kernels want the drive number in BL instead of DL. Indulge them. */
    opt.regs.ebx.b[0] = opt.regs.edx.b[0] = (uint8_t)iter->di.disk;

    /* Perform initial partition entry mangling */
    if (opt.hide || opt.mbrchs)
	pe_mangle(iter);
/*	hide_unhide(iter);*/

    /* Load the boot file */
    if (opt.file) {
	data[ndata].base = (opt.fseg << 4) + opt.foff;

	if (loadfile(opt.file, &data[ndata].data, &data[ndata].size)) {
	    error("Couldn't read the boot file.\n");
	    goto bail;
	}
	file_area = (void *)data[ndata].data;

	if (data[ndata].base + data[ndata].size - 1 > ADDRMAX) {
	    error("The boot file is too big to load at this address.\n");
	    goto bail;
	}

	fidx = ndata;
	ndata++;
    }

    /* Load the sector */
    if (opt.sect) {
	data[ndata].size = SECTOR;
	data[ndata].base = (opt.sseg << 4) + opt.soff;

	if (opt.file && opt.maps && overlap(data + fidx, data + ndata)) {
	    error("WARNING: The sector won't be loaded, as it would conflict with the boot file.\n");
	} else {
	    if (!(data[ndata].data = disk_read_sectors(&iter->di, iter->start_lba, 1))) {
		error("Couldn't read the sector.\n");
		goto bail;
	    }
	    sect_area = (void *)data[ndata].data;

	    sidx = ndata;
	    ndata++;
	}
    }

    /* Prep the handover */
    if (opt.hand && iter->index) {
	if (setup_handover(iter, data + ndata))
	    goto bail;
	hand_area = (void *)data[ndata].data;

	/* Verify possible conflicts */
	if ( ( fidx >= 0 && overlap(data + fidx, data + ndata)) ||
	     ( sidx >= 0 && opt.maps && overlap(data + sidx, data + ndata)) ) {
	    error("WARNING: Handover area won't be prepared,\n"
		  "as it would conflict with the boot file and/or the sector.\n");
	} else {
	    hidx = ndata;
	    ndata++;
	}
    }

    /*
     *  Adjust registers - ds:si & ds:bp
     *  We do it here, as they might get further
     *  overriden during mangling.
     */

    if (sidx >= 0 && fidx >= 0 && opt.maps && !opt.hptr) {
	opt.regs.esi.l = opt.regs.ebp.l = opt.soff;
	opt.regs.ds = (uint16_t)opt.sseg;
	opt.regs.eax.l = 0;
    } else if (hidx >= 0) {
	opt.regs.esi.l = opt.regs.ebp.l = data[hidx].base;
	opt.regs.ds = 0;
	if (iter->type == typegpt)
	    opt.regs.eax.l = 0x54504721;	/* '!GPT' */
	else
	    opt.regs.eax.l = 0;
    }

    /* Do file related stuff */

    if (fidx >= 0) {
	if (opt.isolinux && manglef_isolinux(data + fidx))
	    goto bail;

	if (opt.grldr && manglef_grldr(iter))
	    goto bail;

	if (opt.grub && manglef_grub(iter, data + fidx))
	    goto bail;

#if 0
	if (opt.drmk && manglef_drmk(data + fidx))
	    goto bail;
#endif

	if (opt.filebpb && manglef_bpb(iter, data + fidx))
	    goto bail;
    }

    /* Do sector related stuff */

    if (sidx >= 0) {
	if (try_mangles_bpb(iter, data + sidx))
	    goto bail;

	if (opt.cmldr && mangles_cmldr(data + sidx))
	    goto bail;
    }

    /* Prepare boot-time mmap data */

    ndata = 0;
    if (sidx >= 0)
	memcpy(bdata + ndata++, data + sidx, sizeof(struct data_area));
    if (fidx >= 0)
	memcpy(bdata + ndata++, data + fidx, sizeof(struct data_area));
    if (hidx >= 0)
	memcpy(bdata + ndata++, data + hidx, sizeof(struct data_area));

#ifdef DEBUG
    printf("iter dsk: %d\n", iter->di.disk);
    printf("iter idx: %d\n", iter->index);
    printf("iter lba: %llu\n", iter->start_lba);
    if (hidx >= 0)
	printf("hand lba: %u\n", hand_area->start_lba);
#endif

    if (opt.warn) {
	puts("Press any key to continue booting...");
	wait_key();
    }

    do_boot(bdata, ndata);
bail:
    pi_del(&iter);
    /* Free allocated areas */
    free(file_area);
    free(sect_area);
    free(hand_area);
    return 255;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
