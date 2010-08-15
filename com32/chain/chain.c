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
#include <minmax.h>
#include <stdbool.h>
#include <dprintf.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/config.h>
#include <syslinux/disk.h>
#include <syslinux/video.h>
#include "partiter.h"

/* used in checks, whenever addresses supplied by user are sane */

#define ADDRMAX 0x9EFFF
#define ADDRMIN 0x500

static const char cmldr_signature[8] = "cmdcons";

static struct options {
    const char *drivename;
    const char *partition;
    const char *loadfile;
    const char *grubcfg;
    uint16_t keeppxe;
    uint16_t seg;
    bool isolinux;
    bool cmldr;
    bool grub;
    bool grldr;
    bool swap;
    bool hide;
    bool sethidden;
    bool drmk;
    struct syslinux_rm_regs regs;
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

/* Search for a specific drive, based on the MBR signature; bytes 440-443 */
static int find_by_sig(uint32_t mbr_sig,
			struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo)))
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
 * We return the disk drive number if found, as well as populating the
 * boot_part pointer with the matching partition, if applicable.
 * If no matching partition is found or the GUID is a disk GUID,
 * boot_part will be populated with NULL.  If not matching disk is
 * found, we return -1.
 */
static int find_by_guid(const struct guid *gpt_guid,
			struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo)))
	    continue;
	/* Check for a GPT disk */
	if (boot_part->type != typegpt) {
	    pi_del(&boot_part);
	    continue;
	}
	/* Check for a matching GPT disk guid */
	if(!memcmp(&boot_part->sub.gpt.disk_guid, gpt_guid, sizeof(*gpt_guid))) {
	    goto ok;
	}
	/* disk guid doesn't match, maybe partition guid will */
	while (pi_next(&boot_part)) {
	    if(!memcmp(&boot_part->sub.gpt.part_guid, gpt_guid, sizeof(*gpt_guid)))
		goto ok;
	}
    }
    drive = -1;
ok:
    *_boot_part = boot_part;
    return drive;
}

/*
 * Search for a specific partition, based on the GPT label.
 * We return the disk drive number if found, as well as populating the
 * boot_part pointer with the matching partition, if applicable.
 * If no matching partition is found, boot_part will be populated with
 * NULL and we return -1.
 */
static int find_by_label(const char *label, struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive <= 0xff; drive++) {
	if (disk_get_params(drive, &diskinfo))
	    continue;		/* Drive doesn't exist */
	if (!(boot_part = pi_begin(&diskinfo)))
	    continue;
	/* Check for a GPT disk */
	boot_part = pi_begin(&diskinfo);
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
	opt.regs.ebx.b[0] = opt.regs.edx.b[0] = swapdrive;	//FIXME this silently assumes DOS expectations

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
	goto out;
    }
    if (!(mbr = disk_read_sectors(&_iter->di, 0, 1))) {
	error("WARNING: Couldn't read MBR to hide/unhide partitions.\n");
	goto out;
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

out:
    free(mbr);
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

/* Convert seg:off:ip values into numerical seg:linear_address:ip */

static int soi2sli(char *ptr, uint16_t *seg, uint32_t *lin, uint16_t *ip)
{
    uint32_t segval = 0, offval = 0, ipval = 0, val;
    char *p;
    
    segval = strtoul(ptr, &p, 0);
    if(*p == ':')
	offval = strtoul(p+1, &p, 0);
    if(*p == ':')
	ipval = strtoul(p+1, NULL, 0);

    offval = (segval << 4) + offval;

    if (offval < ADDRMIN || offval > ADDRMAX) {
	error("Invalid seg:off:* address specified..\n");
	goto bail;
    }

    val = (segval << 4) + ipval;

    if (ipval > 0xFFFE || val < ADDRMIN || val > ADDRMAX) {
	error("Invalid *:*:ip address specified.\n");
	goto bail;
    }

    if(seg)
	*seg = (uint16_t)segval;
    if(lin)
	*lin = offval;
    if(ip)
	*ip  = (uint16_t)ipval;

    return 0;

bail:
    return -1;
}

static void usage(void)
{
    static const char usage[] = "\
Usage:\n\
    chain.c32 [options]\n\
    chain.c32 {fd|hd}<disk> [<partition>] [options]\n\
    chain.c32 mbr{:|=}<id> [<partition>] [options]\n\
    chain.c32 guid{:|=}<guid> [<partition>] [options]\n\
    chain.c32 label{:|=}<label> [<partition>] [options]\n\
    chain.c32 boot{,| }[<partition>] [options]\n\
    chain.c32 fs [options]\n\
\nOptions:\n\
    file=<loader>      Load and execute file, instead of boot sector\n\
    isolinux=<loader>  Load another version of ISOLINUX\n\
    ntldr=<loader>     Load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n\
    cmldr=<loader>     Load Recovery Console of Windows NT/2K/XP/2003\n\
    freedos=<loader>   Load FreeDOS KERNEL.SYS\n\
    msdos=<loader>     Load MS-DOS IO.SYS\n\
    pcdos=<loader>     Load PC-DOS IBMBIO.COM\n\
    drmk=<loader>      Load DRMK DELLBIO.BIN\n\
    grub=<loader>      Load GRUB Legacy stage2\n\
    grubcfg=<filename> Set alternative config filename for GRUB Legacy\n\
    grldr=<loader>     Load GRUB4DOS grldr\n\
    seg=<segment>      Jump to <seg>:0000, instead of 0000:7C00\n\
    swap               Swap drive numbers, if bootdisk is not fd0/hd0\n\
    hide               Hide primary partitions, except selected partition\n\
    sethidden          Set the FAT/NTFS hidden sectors field\n\
    keeppxe            Keep the PXE and UNDI stacks in memory (PXELINUX)\n\
\nPlease see doc/chain.txt for the detailed documentation.\n";
    error(usage);
}

static int parse_args(int argc, char *argv[])
{
    int i;
    char *p;

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "file=", 5)) {
	    opt.loadfile = argv[i] + 5;
	} else if (!strncmp(argv[i], "seg=", 4)) {
	    if(soi2sli(argv[i] + 4, &opt.seg, NULL, NULL))
		goto bail;
	} else if (!strncmp(argv[i], "isolinux=", 9)) {
	    opt.loadfile = argv[i] + 9;
	    opt.isolinux = true;
	} else if (!strncmp(argv[i], "ntldr=", 6)) {
	    opt.seg = 0x2000;	/* NTLDR wants this address */
	    opt.loadfile = argv[i] + 6;
	    opt.sethidden = true;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.seg = 0x2000;	/* CMLDR wants this address */
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
	} else if (!strncmp(argv[i], "drmk=", 5)) {
	    opt.seg = 0x70;	/* DRMK wants this address */
	    opt.loadfile = argv[i] + 5;
	    opt.sethidden = true;
	    opt.drmk = true;
	} else if (!strncmp(argv[i], "grub=", 5)) {
	    opt.seg = 0x800;	/* stage2 wants this address */
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
		   || !strncmp(argv[i], "label:", 6)
		   || !strncmp(argv[i], "label=", 6)
		   || !strcmp(argv[i], "boot")
		   || !strncmp(argv[i], "boot,", 5)
		   || !strcmp(argv[i], "fs")) {
	    opt.drivename = argv[i];
	    p = strchr(opt.drivename, ',');
	    if (p) {
		*p = '\0';
		opt.partition = p + 1;
	    } else if (argv[i + 1] && argv[i + 1][0] >= '0'
		       && argv[i + 1][0] <= '9') {
		opt.partition = argv[++i];
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

    return 0;
bail:
    return -1;
}

inline static int is_phys(uint8_t sdifs)
{
    return
	sdifs == SYSLINUX_FS_SYSLINUX ||
	sdifs == SYSLINUX_FS_EXTLINUX ||
	sdifs == SYSLINUX_FS_ISOLINUX;
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
	drive = hd | (int)strtoul(opt.drivename, NULL, 0);

	if (disk_get_params(drive, &diskinfo))
	    goto bail;
	if (!(iter = pi_begin(&diskinfo)))
	    goto bail;

    } else if (!strcmp(opt.drivename, "boot") || !strcmp(opt.drivename, "fs")) {
	if (!is_phys(sdi->c.filesystem)) {
	    error("When syslinux is not booted from physical disk (or its emulation),\n"
		   "'boot' and 'fs' are meaningless.\n");
	    goto bail;
	}
	/* offsets match, but in case it changes in the future */
	if(sdi->c.filesystem == SYSLINUX_FS_ISOLINUX) {
	    drive = sdi->iso.drive_number;
	    fs_lba = *sdi->iso.partoffset;
	} else {
	    drive = sdi->disk.drive_number;
	    fs_lba = *sdi->disk.partoffset;
	}

	if (disk_get_params(drive, &diskinfo))
	    goto bail;
	if (!(iter = pi_begin(&diskinfo)))
	    goto bail;

	/* 'fs' => we should lookup the Syslinux partition number and use it */
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
    /* main options done, only thing left is explicit parition specification
     * if we're still at the disk stage with the iterator AND user supplied
     * partition number (including disk).
     */
    if (!iter->index && opt.partition) {
	partition = (int)strtoul(opt.partition, NULL, 0);
	/* search for matching part#, including disk */
	do {
	    if (iter->index == partition)
		break;
	} while (pi_next(&iter));
    }

    if (!(iter->di.disk & 0x80) && iter->index) {
	error("WARNING: Partitions on floppy devices may not work.\n");
    }

    *_iter = iter;
    return 0;

bail:
    return -1;
}

/* Create boot info table: needed when you want to chainload
 * another version of ISOLINUX (or another bootlaoder that needs
 * the -boot-info-table switch of mkisofs)
 * (will only work when run from ISOLINUX)
 */
static int manglef_isolinux(struct data_area *_data)
{
    const union syslinux_derivative_info *sdi;
    unsigned char *isolinux_bin;
    uint32_t *checksum, *chkhead, *chktail;
    uint32_t file_lba = 0;

    sdi = syslinux_derivative_info();

    if (sdi->c.filesystem != SYSLINUX_FS_ISOLINUX) {
	error ("The isolinux= option is only valid when run from ISOLINUX.\n");
	goto bail;
    }

    /* Boot info table info (integers in little endian format)

       Offset Name         Size      Meaning
       8      bi_pvd       4 bytes   LBA of primary volume descriptor
       12     bi_file      4 bytes   LBA of boot file
       16     bi_length    4 bytes   Boot file length in bytes
       20     bi_csum      4 bytes   32-bit checksum
       24     bi_reserved  40 bytes  Reserved

       The 32-bit checksum is the sum of all the 32-bit words in the
       boot file starting at byte offset 64. All linear block
       addresses (LBAs) are given in CD sectors (normally 2048 bytes).

       LBA of primary volume descriptor should already be set to 16.
       */

    isolinux_bin = (unsigned char *)_data->data;

    /* Get LBA address of bootfile */
    file_lba = get_file_lba(opt.loadfile);

    if (file_lba == 0) {
	error("Failed to find LBA offset of the boot file\n");
	goto bail;
    }
    /* Set it */
    *((uint32_t *) & isolinux_bin[12]) = file_lba;

    /* Set boot file length */
    *((uint32_t *) & isolinux_bin[16]) = _data->size;

    /* Calculate checksum */
    checksum = (uint32_t *) & isolinux_bin[20];
    chkhead = (uint32_t *) & isolinux_bin[64];
    chktail = (uint32_t *) & isolinux_bin[_data->size & ~3u];
    *checksum = 0;
    while (chkhead < chktail)
	*checksum += *chkhead++;

    /*
     * Deal with possible fractional dword at the end;
     * this *should* never happen...
     */
    if (_data->size & 3) {
	uint32_t xword = 0;
	memcpy(&xword, chkhead, _data->size & 3);
	*checksum += xword;
    }
    return 0;
bail:
    return -1;
}

/*
 * GRLDR of GRUB4DOS wants the partition number in DH:
 * -1:   whole drive (default)
 * 0-3:  primary partitions
 * 4-*:  logical partitions
 */
static int manglef_grldr(const struct part_iter *_iter)
{
    opt.regs.edx.b[1] = (uint8_t)(_iter->index - 1);
    return 0;
}

/*
 * Legacy grub's stage2 chainloading
 */
static int manglef_grublegacy(const struct part_iter *_iter, struct data_area *_data)
{
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
    } __attribute__ ((packed)) *stage2;

    if (_data->size < sizeof(struct grub_stage2_patch_area)) {
	error("The file specified by grub=<loader> is too small to be stage2 of GRUB Legacy.\n");
	goto bail;
    }
    stage2 = _data->data;

    /*
     * Check the compatibility version number to see if we loaded a real
     * stage2 file or a stage2 file that we support.
     */
    if (stage2->compat_version_major != 3
	    || stage2->compat_version_minor != 2) {
	error("The file specified by grub=<loader> is not a supported stage2 GRUB Legacy binary.\n");
	goto bail;
    }

    /* jump 0x200 bytes into the loadfile */
    opt.regs.ip = 0x200;

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
    stage2->install_partition.part1 = (uint8_t)(_iter->index - 1);

    /*
     * Grub Legacy reserves 89 bytes (from 0x8217 to 0x826f) for the
     * config filename. The filename passed via grubcfg= will overwrite
     * the default config filename "/boot/grub/menu.lst".
     */
    if (opt.grubcfg) {
	if (strlen(opt.grubcfg) > sizeof(stage2->config_file) - 1) {
	    error ("The config filename length can't exceed 88 characters.\n");
	    goto bail;
	}

	strcpy((char *)stage2->config_file, opt.grubcfg);
    }

    return 0;
bail:
    return -1;
}

/*
 * Dell's DRMK chainloading.
 */
static int manglef_drmk(struct data_area *_data)
{
    /*
     * DRMK entry is different than MS-DOS/PC-DOS
     * A new size, aligned to 16 bytes to ease use of ds:[bp+28].
     * We only really need 4 new, usable bytes at the end.
     */

    uint32_t tsize = (_data->size + 19) & 0xfffffff0;
    opt.regs.ss = opt.regs.fs = opt.regs.gs = 0;	/* Used before initialized */
    if (!realloc(_data->data, tsize)) {
	error("Failed to realloc for DRMK.\n");
	goto bail;
    }
    _data->size = tsize;
    /* ds:[bp+28] must be 0x0000003f */
    opt.regs.ds = (uint16_t)((tsize >> 4) + (opt.seg - 2u));
    /* "Patch" into tail of the new space */
    *(uint32_t *)((char*)_data->data + tsize - 4) = 0x0000003f;

    return 0;
bail:
    return -1;
}

int main(int argc, char *argv[])
{
    struct part_iter *cur_part = NULL;

    void *sect_area = NULL;
    void *file_area = NULL;
    struct disk_dos_part_entry *hand_area = NULL;

    struct data_area data[3];
    int ndata = 0, fidx = -1, sidx = -1;
    addr_t load_base;

    openconsole(&dev_null_r, &dev_stdcon_w);

    /* Prepare and set default values */
    memset(&opt, 0, sizeof(opt));
    opt.drivename = "boot";	/* potential FIXME: maybe we shouldn't assume boot by default */

    /* Parse arguments */
    if(parse_args(argc, argv))
	goto bail;

    if (opt.seg) {
	opt.regs.es = opt.regs.cs = opt.regs.ss =
	    opt.regs.ds = opt.regs.fs = opt.regs.gs = opt.seg;
    } else {
	opt.regs.esp.l = opt.regs.ip = 0x7c00;
    }

    /* Get disk/part iterator matching user supplied options */
    if(find_dp(&cur_part))
	goto bail;

    /* DOS kernels want the drive number in BL instead of DL. Indulge them. */
    opt.regs.ebx.b[0] = opt.regs.edx.b[0] = (uint8_t)cur_part->di.disk;

    /* Do hide / unhide if appropriate */
    if (opt.hide)
	hide_unhide(cur_part); 
   
    /* Load file and bs/mbr */

    load_base = opt.seg ? (uint32_t)(opt.seg << 4) : 0x7c00;

    if (opt.loadfile) {
	fputs("Loading the boot file...\n", stdout);
	if (loadfile(opt.loadfile, &data[ndata].data, &data[ndata].size)) {
	    error("Couldn't read the boot file.\n");
	    goto bail;
	}
	file_area = (void *)data[ndata].data;
	data[ndata].base = load_base;
	load_base = 0x7c00;	/* If we also load a boot sector */
	fidx = ndata;
	ndata++;
    }

    if (!opt.loadfile || data[0].base >= 0x7c00 + SECTOR) {
	if (!(data[ndata].data = disk_read_sectors(&cur_part->di, cur_part->start_lba, 1))) {
	    error("Couldn't read the boot sector or mbr.\n");
	    goto bail;
	}

	sect_area = (void *)data[ndata].data;
	data[ndata].base = load_base;
	data[ndata].size = SECTOR;
	sidx = ndata;
	ndata++;
    }

    /* Mangle file area */

    if (opt.isolinux && manglef_isolinux(data + fidx))
	goto bail;

    if (opt.grldr && manglef_grldr(cur_part))
	goto bail;

    if (opt.grub && manglef_grublegacy(cur_part, data + fidx))
	goto bail;

    if (opt.drmk && manglef_drmk(data + fidx))
	goto bail;


    /* Mangle bs/mbr area */

    /*
     * To boot the Recovery Console of Windows NT/2K/XP we need to write
     * the string "cmdcons\0" to memory location 0000:7C03.
     * Memory location 0000:7C00 contains the bootsector of the partition.
     */
    if (cur_part->index && opt.cmldr) {
	memcpy((char *)data[sidx].data + 3, cmldr_signature,
		sizeof cmldr_signature);
    }

    /*
     * Modify the hidden sectors (partition offset) copy in memory;
     * this modifies the field used by FAT and NTFS filesystems, and
     * possibly other boot loaders which use the same format.
     */
    if (cur_part->index && opt.sethidden) {
	if(cur_part->start_lba < 0x100000000)
	    *(uint32_t *) ((char *)data[sidx].data + 0x1c) = (uint32_t)cur_part->start_lba;
	else
	    *(uint32_t *) ((char *)data[sidx].data + 0x1c) = ~0u;
    }


    if (cur_part->index) {
	if (cur_part->type == typegpt) {
	    /* Do GPT hand-over, if applicable (as per syslinux/doc/gpt.txt) */
	    /* Look at the GPT partition */
	    const struct disk_gpt_part_entry *gp =
		(const struct disk_gpt_part_entry *)cur_part->record;
	    /* Note the partition length */
	    uint64_t lba_count = gp->lba_last - gp->lba_first + 1;
	    /* The length of the hand-over */
	    uint32_t synth_size =
		sizeof(struct disk_dos_part_entry) + sizeof(uint32_t) +
		(uint32_t)cur_part->sub.gpt.pe_size;
	    /* Will point to the partition record length in the hand-over */
	    uint32_t *plen;

	    /* Allocate the hand-over record */
	    hand_area = malloc(synth_size);
	    if (!hand_area) {
		error("Could not build GPT hand-over record!\n");
		goto bail;
	    }
	    /* Synthesize the record */
	    memset(hand_area, 0, synth_size);
	    hand_area->active_flag = 0x80;
	    hand_area->ostype = 0xED;
	    /* All bits set by default */
	    hand_area->start_lba = ~0u;
	    hand_area->length = ~0u;
	    /* If these fit the precision, pass them on */
	    if (cur_part->start_lba < hand_area->start_lba)
		hand_area->start_lba = (uint32_t)cur_part->start_lba;
	    if (lba_count < hand_area->length)
		hand_area->length = (uint32_t)lba_count;
	    /* Next comes the GPT partition record length */
	    plen = (uint32_t *) (hand_area + 1);
	    plen[0] = (uint32_t)cur_part->sub.gpt.pe_size;
	    /* Next comes the GPT partition record copy */
	    memcpy(plen + 1, gp, plen[0]);

	    opt.regs.eax.l = 0x54504721;	/* '!GPT' */
	    data[ndata].base = 0x7be;
	    data[ndata].size = synth_size;
	    data[ndata].data = (void *)hand_area;
	    ndata++;
	    opt.regs.esi.w[0] = 0x7be;
#ifdef DEBUG
	    dprintf("GPT handover:\n");
	    disk_dos_part_dump(hand_area);
	    disk_gpt_part_dump((struct disk_gpt_part_entry *)(plen + 1));
#endif
	} else if (cur_part->type == typedos) {
	    /* MBR handover protocol */
	    /* Allocate the hand-over record */
	    hand_area = malloc(sizeof(struct disk_dos_part_entry));
	    if (!hand_area) {
		error("Could not build MBR hand-over record!\n");
		goto bail;
	    }

	    memcpy(hand_area, cur_part->record, sizeof(struct disk_dos_part_entry));
	    hand_area->start_lba = (uint32_t)cur_part->start_lba;

	    data[ndata].base = 0x7be;
	    data[ndata].size = sizeof(struct disk_dos_part_entry);
	    data[ndata].data = (void *)hand_area;
	    ndata++;
	    opt.regs.esi.w[0] = 0x7be;
#ifdef DEBUG
	    dprintf("MBR handover:\n");
	    disk_dos_part_dump(hand_area);
#endif
	}
    }

    do_boot(data, ndata);

bail:
    pi_del(&cur_part);
    /* Free allocated areas */
    free(file_area);
    free(sect_area);
    free(hand_area);
    return 255;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
