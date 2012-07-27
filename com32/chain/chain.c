/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *   Copyright 2010 Shao Miller
 *   Copyright 2010 Michal Soltys
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

static int fixed_cnt = 128;   /* see comments in main() */

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
	while (!pi_next(&boot_part)) {
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
	while (!pi_next(&boot_part)) {
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

int find_dp(struct part_iter **_iter)
{
    struct part_iter *iter = NULL;
    struct disk_info diskinfo;
    struct guid gpt_guid;
    uint64_t fs_lba;
    int drive, hd, partition;
    const union syslinux_derivative_info *sdi;

    sdi = syslinux_derivative_info();

    if (!strncmp(opt.drivename, "mbr", 3)) {
	if (find_by_sig(strtoul(opt.drivename + 4, NULL, 0), &iter) < 0) {
	    error("Unable to find requested MBR signature.\n");
	    goto bail;
	}
    } else if (!strncmp(opt.drivename, "guid", 4)) {
	if (str_to_guid(opt.drivename + 5, &gpt_guid))
	    goto bail;
	if (find_by_guid(&gpt_guid, &iter) < 0) {
	    error("Unable to find requested GPT disk or partition by guid.\n");
	    goto bail;
	}
    } else if (!strncmp(opt.drivename, "label", 5)) {
	if (!opt.drivename[6]) {
	    error("No label specified.\n");
	    goto bail;
	}
	if (find_by_label(opt.drivename + 6, &iter) < 0) {
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
	    while (!pi_next(&iter)) {
		if (iter->start_lba == fs_lba)
		    break;
	    }
	    /* broken part structure or other problems */
	    if (iter->status) {
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
     * partition number (including disk pseudo-partition).
     */
    if (!iter->index && opt.partition) {
	partition = strtol(opt.partition, NULL, 0);
	/* search for matching part#, including disk */
	do {
	    if (iter->index == partition)
		break;
	} while (!pi_next(&iter));
	if (iter->status) {
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
    pi_del(&iter);
    return -1;
}

static int setup_handover(const struct part_iter *iter,
		   struct data_area *data)
{
    struct disk_dos_part_entry *ha;
    uint32_t synth_size;
    uint32_t *plen;

    if (!iter->index) { /* implies typeraw or non-iterated */
	uint32_t len;
	/* RAW handover protocol */
	synth_size = sizeof(struct disk_dos_part_entry);
	ha = malloc(synth_size);
	if (!ha) {
	    error("Could not build RAW hand-over record!\n");
	    goto bail;
	}
	len = ~0u;
	if (iter->length < len)
	    len = (uint32_t)iter->length;
	lba2chs(&ha->start, &iter->di, 0, l2c_cadd);
	lba2chs(&ha->end, &iter->di, len - 1, l2c_cadd);
	ha->active_flag = 0x80;
	ha->ostype = 0xDA;	/* "Non-FS Data", anything is good here though ... */
	ha->start_lba = 0;
	ha->length = len;
    } else if (iter->type == typegpt) {
	/* GPT handover protocol */
	synth_size = sizeof(struct disk_dos_part_entry) +
	    sizeof(uint32_t) + (uint32_t)iter->sub.gpt.pe_size;
	ha = malloc(synth_size);
	if (!ha) {
	    error("Could not build GPT hand-over record!\n");
	    goto bail;
	}
	lba2chs(&ha->start, &iter->di, iter->start_lba, l2c_cadd);
	lba2chs(&ha->end, &iter->di, iter->start_lba + iter->length - 1, l2c_cadd);
	ha->active_flag = 0x80;
	ha->ostype = 0xED;
	/* All bits set by default */
	ha->start_lba = ~0u;
	ha->length = ~0u;
	/* If these fit the precision, pass them on */
	if (iter->start_lba < ha->start_lba)
	    ha->start_lba = (uint32_t)iter->start_lba;
	if (iter->length < ha->length)
	    ha->length = (uint32_t)iter->length;
	/* Next comes the GPT partition record length */
	plen = (uint32_t *) (ha + 1);
	plen[0] = (uint32_t)iter->sub.gpt.pe_size;
	/* Next comes the GPT partition record copy */
	memcpy(plen + 1, iter->record, plen[0]);
#ifdef DEBUG
	dprintf("GPT handover:\n");
	disk_dos_part_dump(ha);
	disk_gpt_part_dump((struct disk_gpt_part_entry *)(plen + 1));
#endif
    } else if (iter->type == typedos) {
	/* MBR handover protocol */
	synth_size = sizeof(struct disk_dos_part_entry);
	ha = malloc(synth_size);
	if (!ha) {
	    error("Could not build MBR hand-over record!\n");
	    goto bail;
	}
	memcpy(ha, iter->record, synth_size);
	/* make sure these match bios imaginations and are ebr agnostic */
	lba2chs(&ha->start, &iter->di, iter->start_lba, l2c_cadd);
	lba2chs(&ha->end, &iter->di, iter->start_lba + iter->length - 1, l2c_cadd);
	ha->start_lba = (uint32_t)iter->start_lba;
	ha->length = (uint32_t)iter->length;

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

int main(int argc, char *argv[])
{
    struct part_iter *iter = NULL;
    void *sbck = NULL;
    struct data_area fdat, hdat, sdat, data[3];
    int ndata = 0;

    console_ansi_raw();

    memset(&fdat, 0, sizeof(fdat));
    memset(&hdat, 0, sizeof(hdat));
    memset(&sdat, 0, sizeof(sdat));

    opt_set_defs();
    if (opt_parse_args(argc, argv))
	goto bail;

#if 0
    /* Get max fixed disk number */
    fixed_cnt = *(uint8_t *)(0x475);

    /*
     * hmm, looks like we can't do that -
     * some bioses/vms just set it to 1
     * and go on living happily
     * any better options than hardcoded 0x80 - 0xFF ?
     */
#endif

    /* Get disk/part iterator matching user supplied options */
    if (find_dp(&iter))
	goto bail;

    /* Perform initial partition entry mangling */
    if (manglepe_fixchs(iter))
	goto bail;
    if (manglepe_hide(iter))
	goto bail;

    /* Load the boot file */
    if (opt.file) {
	fdat.base = (opt.fseg << 4) + opt.foff;

	if (loadfile(opt.file, &fdat.data, &fdat.size)) {
	    error("Couldn't read the boot file.\n");
	    goto bail;
	}
	if (fdat.base + fdat.size - 1 > ADDRMAX) {
	    error("The boot file is too big to load at this address.\n");
	    goto bail;
	}
    }

    /* Load the sector */
    if (opt.sect) {
	sdat.base = (opt.sseg << 4) + opt.soff;
	sdat.size = iter->di.bps;

	if (sdat.base + sdat.size - 1 > ADDRMAX) {
	    error("The sector cannot be loaded at such high address.\n");
	    goto bail;
	}
	if (!(sdat.data = disk_read_sectors(&iter->di, iter->start_lba, 1))) {
	    error("Couldn't read the sector.\n");
	    goto bail;
	}
	if (opt.save) {
	    if (!(sbck = malloc(sdat.size))) {
		error("Couldn't allocate cmp-buf for option 'save'.\n");
		goto bail;
	    }
	    memcpy(sbck, sdat.data, sdat.size);
	}
	if (opt.file && opt.maps && overlap(&fdat, &sdat)) {
	    error("WARNING: The sector won't be mmapped, as it would conflict with the boot file.\n");
	    opt.maps = false;
	}
    }

    /* Prep the handover */
    if (opt.hand) {
	if (setup_handover(iter, &hdat))
	    goto bail;
	/* Verify possible conflicts */
	if ( ( opt.file && overlap(&fdat, &hdat)) ||
	     ( opt.maps && overlap(&sdat, &hdat)) ) {
	    error("WARNING: Handover area won't be prepared,\n"
		  "as it would conflict with the boot file and/or the sector.\n");
	    opt.hand = false;
	}
    }

    /* Adjust registers */

    mangler_init(iter);
    mangler_handover(iter, &hdat);
    mangler_grldr(iter);

    /* Patching functions */

    if (manglef_isolinux(&fdat))
	goto bail;

    if (manglef_grub(iter, &fdat))
	goto bail;
#if 0
    if (manglef_drmk(&fdat))
	goto bail;
#endif
    if (manglef_bpb(iter, &fdat))
	goto bail;

    if (mangles_bpb(iter, &sdat))
	goto bail;

    if (mangles_save(iter, &sdat, sbck))
	goto bail;

    if (manglesf_bss(&sdat, &fdat))
	goto bail;

    /* This *must* be after BPB saving or copying */
    if (mangles_cmldr(&sdat))
	goto bail;

    /*
     * Prepare boot-time mmap data. We should to it here, as manglers could
     * potentially alter some of the data.
     */

    if (opt.file)
	memcpy(data + ndata++, &fdat, sizeof(fdat));
    if (opt.maps)
	memcpy(data + ndata++, &sdat, sizeof(sdat));
    if (opt.hand)
	memcpy(data + ndata++, &hdat, sizeof(hdat));

#ifdef DEBUG
    printf("iter->di dsk, bps: %X, %u\niter->di lbacnt, C*H*S: %"PRIu64", %u\n"
	   "iter->di C, H, S: %u, %u, %u\n",
	iter->di.disk, iter->di.bps,
	iter->di.lbacnt, iter->di.cyl * iter->di.head * iter->di.spt,
	iter->di.cyl, iter->di.head, iter->di.spt);
    printf("iter idx: %d\n", iter->index);
    printf("iter lba: %"PRIu64"\n", iter->start_lba);
    if (opt.hand)
	printf("hand lba: %u\n",
		((struct disk_dos_part_entry *)hdat.data)->start_lba);
#endif

    if (opt.warn) {
	puts("Press any key to continue booting...");
	wait_key();
    }

    if (ndata && !opt.brkchain) /* boot only if we actually chainload */
	do_boot(data, ndata);
    else
	error("Service-only run completed, exiting.\n");
bail:
    pi_del(&iter);
    /* Free allocated areas */
    free(fdat.data);
    free(sdat.data);
    free(hdat.data);
    free(sbck);
    return 255;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
