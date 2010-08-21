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
#include "partiter.h"

/* used in checks, whenever addresses supplied by user are sane */

#define ADDRMAX 0x9EFFFu
#define ADDRMIN 0x500u

static const char cmldr_signature[8] = "cmdcons";
static int fixed_cnt;

static struct options {
    unsigned int fseg;
    unsigned int foff;
    unsigned int fip;
    unsigned int sseg;
    unsigned int soff;
    unsigned int sip;
    unsigned int drvoff;
    const char *drivename;
    const char *partition;
    const char *file;
    const char *grubcfg;
    bool isolinux;
    bool cmldr;
    bool drmk;
    bool grub;
    bool grldr;
    bool maps;
    bool hand;
    bool hptr;
    bool swap;
    bool hide;
    bool sethid;
    bool setgeo;
    bool setdrv;
    bool sect;
    bool save;
    bool filebpb;
    bool warn;
    uint16_t keeppxe;
    struct syslinux_rm_regs regs;
} opt;

struct data_area {
    void *data;
    addr_t base;
    addr_t size;
};

static void wait_key(void)
{
    int cnt;
    char junk;

    /* drain */
    do {
	errno = 0;
	cnt = read(0, &junk, 1);
    } while (cnt > 0 || (cnt < 0 && errno == EAGAIN));

    /* wait */
    do {
	errno = 0;
	cnt = read(0, &junk, 1);
    } while (!cnt || (cnt < 0 && errno == EAGAIN));
}

static void error(const char *msg)
{
    fputs(msg, stderr);
}

static int no_ov(const struct data_area *a, const struct data_area *b)
{
    return
	a->base + a->size <= b->base ||
	b->base + b->size <= a->base;
}

static int is_phys(uint8_t sdifs)
{
    return
	sdifs == SYSLINUX_FS_SYSLINUX ||
	sdifs == SYSLINUX_FS_EXTLINUX ||
	sdifs == SYSLINUX_FS_ISOLINUX;
}

/* Search for a specific drive, based on the MBR signature; bytes 440-443 */
static int find_by_sig(uint32_t mbr_sig,
			struct part_iter **_boot_part)
{
    struct part_iter *boot_part = NULL;
    struct disk_info diskinfo;
    int drive;

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
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

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
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

    for (drive = 0x80; drive < 0x80 + fixed_cnt; drive++) {
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

/* Convert string seg:off:ip values into numerical seg:off:ip ones */

static int soi_s2n(char *ptr, unsigned int *seg,
			      unsigned int *off,
			      unsigned int *ip)
{
    unsigned int segval = 0, offval = 0, ipval = 0, val;
    char *p;

    segval = strtoul(ptr, &p, 0);
    if (*p == ':')
	offval = strtoul(p+1, &p, 0);
    if (*p == ':')
	ipval = strtoul(p+1, NULL, 0);

    val = (segval << 4) + offval;

    if (val < ADDRMIN || val > ADDRMAX) {
	error("Invalid seg:off:* address specified..\n");
	goto bail;
    }

    val = (segval << 4) + ipval;

    if (ipval > 0xFFFE || val < ADDRMIN || val > ADDRMAX) {
	error("Invalid seg:*:ip address specified.\n");
	goto bail;
    }

    if (seg)
	*seg = segval;
    if (off)
	*off = offval;
    if (ip)
	*ip  = ipval;

    return 0;

bail:
    return -1;
}

static void usage(void)
{
    static const char *const usage[] = { "\
Usage:\n\
    chain.c32 [options]\n\
    chain.c32 {fd|hd}<disk> [<partition>] [options]\n\
    chain.c32 mbr{:|=}<id> [<partition>] [options]\n\
    chain.c32 guid{:|=}<guid> [<partition>] [options]\n\
    chain.c32 label{:|=}<label> [<partition>] [options]\n\
    chain.c32 boot{,| }[<partition>] [options]\n\
    chain.c32 fs [options]\n\
\nOptions ('no' prefix specify defaulti value):\n\
    file=<loader>        Load and execute file\n\
    seg=<s[:o[:i]]>      Load file at <s:o>, jump to <s:i>\n\
    nofilebpb            Treat file in memory as BPB compatible\n\
    sect[=<s[:o[:i]]>]   Load sector at <s:o>, jump to <s:i>\n\
                         - defaults to 0:0x7C00:0x7C00\n\
    maps                 Map loaded sector into real memory\n\
    nosethid[den]        Set BPB's hidden sectors field\n\
    nosetgeo             Set BPB's sectors per track and heads fields\n\
    nosetdrv[@<off>]     Set BPB's drive unit field at <o>\n\
                         - <off> defaults to autodetection\n\
                         - only 0x24 and 0x40 are accepted\n\
    nosetbpb             Enable set{hid,geo,drv}\n\
    nosave               Write adjusted sector back to disk\n\
    hand                 Prepare handover area\n\
    nohptr               Force ds:si and ds:bp to point to handover area\n\
    noswap               Swap drive numbers, if bootdisk is not fd0/hd0\n\
    nohide               Hide primary partitions, unhide selected partition\n\
    nokeeppxe            Keep the PXE and UNDI stacks in memory (PXELINUX)\n\
    nowarn               Wait for a keypress to continue chainloading\n\
                         - useful to see emited warnings\n\
", "\
\nComposite options:\n\
    isolinux=<loader>    Load another version of ISOLINUX\n\
    ntldr=<loader>       Load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n\
    cmldr=<loader>       Load Recovery Console of Windows NT/2K/XP/2003\n\
    freedos=<loader>     Load FreeDOS KERNEL.SYS\n\
    msdos=<loader>       Load MS-DOS 2.xx - 6.xx IO.SYS\n\
    msdos7=<loader>      Load MS-DOS 7+ IO.SYS\n\
    pcdos=<loader>       Load PC-DOS IBMBIO.COM\n\
    drmk=<loader>        Load DRMK DELLBIO.BIN\n\
    grub=<loader>        Load GRUB Legacy stage2\n\
    grubcfg=<filename>   Set alternative config filename for GRUB Legacy\n\
    grldr=<loader>       Load GRUB4DOS grldr\n\
\nPlease see doc/chain.txt for the detailed documentation.\n"
    };
    error(usage[0]);
    error("Press any key...\n");
    wait_key();
    error(usage[1]);
}

static int parse_args(int argc, char *argv[])
{
    int i;
    unsigned int v;
    char *p;

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "file=", 5)) {
	    opt.file = argv[i] + 5;
	} else if (!strcmp(argv[i], "nofile")) {
	    opt.file = NULL;
	} else if (!strncmp(argv[i], "seg=", 4)) {
	    if (soi_s2n(argv[i] + 4, &opt.fseg, &opt.foff, &opt.fip))
		goto bail;
	} else if (!strncmp(argv[i], "isolinux=", 9)) {
	    opt.file = argv[i] + 9;
	    opt.isolinux = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strncmp(argv[i], "ntldr=", 6)) {
	    opt.fseg = 0x2000;  /* NTLDR wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.file = argv[i] + 6;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = 0x24;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.fseg = 0x2000;  /* CMLDR wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.file = argv[i] + 6;
	    opt.cmldr = true;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = 0x24;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "freedos=", 8)) {
	    opt.fseg = 0x60;    /* FREEDOS wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.sseg = 0x9000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + 8;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if ( (v = 6, !strncmp(argv[i], "msdos=", v) ||
		     !strncmp(argv[i], "pcdos=", v)) ||
		    (v = 7, !strncmp(argv[i], "msdos7=", v)) ) {
	    opt.fseg = 0x70;    /* MS-DOS 2.00 .. 6.xx wants this address */
	    opt.foff = 0;
	    opt.fip = v == 7 ? 0x200 : 0;  /* MS-DOS 7.0+ wants this ip */
	    opt.sseg = 0x9000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + v;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "drmk=", 5)) {
	    opt.fseg = 0x70;    /* DRMK wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.sseg = 0x2000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + 5;
	    /* opt.drmk = true; */
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "grub=", 5)) {
	    opt.fseg = 0x800;	/* stage2 wants this address */
	    opt.foff = 0;
	    opt.fip = 0x200;
	    opt.file = argv[i] + 5;
	    opt.grub = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strncmp(argv[i], "grubcfg=", 8)) {
	    opt.grubcfg = argv[i] + 8;
	} else if (!strncmp(argv[i], "grldr=", 6)) {
	    opt.file = argv[i] + 6;
	    opt.grldr = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strcmp(argv[i], "keeppxe")) {
	    opt.keeppxe = 3;
	} else if (!strcmp(argv[i], "nokeeppxe")) {
	    opt.keeppxe = 0;
	} else if (!strcmp(argv[i], "maps")) {
	    opt.maps = true;
	} else if (!strcmp(argv[i], "nomaps")) {
	    opt.maps = false;
	} else if (!strcmp(argv[i], "hand")) {
	    opt.hand = true;
	} else if (!strcmp(argv[i], "nohand")) {
	    opt.hand = false;
	} else if (!strcmp(argv[i], "hptr")) {
	    opt.hptr = true;
	} else if (!strcmp(argv[i], "nohptr")) {
	    opt.hptr = false;
	} else if (!strcmp(argv[i], "swap")) {
	    opt.swap = true;
	} else if (!strcmp(argv[i], "noswap")) {
	    opt.swap = false;
	} else if (!strcmp(argv[i], "hide")) {
	    opt.hide = true;
	} else if (!strcmp(argv[i], "nohide")) {
	    opt.hide = false;
	} else if (!strcmp(argv[i], "sethid") ||
		   !strcmp(argv[i], "sethidden")) {
	    opt.sethid = true;
	} else if (!strcmp(argv[i], "nosethid") ||
		   !strcmp(argv[i], "nosethidden")) {
	    opt.sethid = false;
	} else if (!strcmp(argv[i], "setgeo")) {
	    opt.setgeo = true;
	} else if (!strcmp(argv[i], "nosetgeo")) {
	    opt.setgeo = false;
	} else if (!strncmp(argv[i], "setdrv",6)) {
	    if (!argv[i][6])
		v = ~0u;    /* autodetect */
	    else if (argv[i][6] == '@' ||
		    argv[i][6] == '=' ||
		    argv[i][6] == ':') {
		v = strtoul(argv[i] + 7, NULL, 0);
		if (!(v == 0x24 || v == 0x40)) {
		    error("Invalid 'setdrv' offset.\n");
		    goto bail;
		}
	    } else {
		    error("Invalid 'setdrv' specification.\n");
		    goto bail;
		}
	    opt.setdrv = true;
	    opt.drvoff = v;
	} else if (!strcmp(argv[i], "nosetdrv")) {
	    opt.setdrv = false;
	} else if (!strcmp(argv[i], "setbpb")) {
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    opt.setgeo = true;
	    opt.sethid = true;
	} else if (!strcmp(argv[i], "nosetbpb")) {
	    opt.setdrv = false;
	    opt.setgeo = false;
	    opt.sethid = false;
	} else if (!strncmp(argv[i], "sect=", 5) ||
		   !strcmp(argv[i], "sect")) {
	    if (argv[i][4]) {
		if (soi_s2n(argv[i] + 5, &opt.sseg, &opt.soff, &opt.sip))
		    goto bail;
		if ((opt.sseg << 4) + opt.soff + SECTOR - 1 > ADDRMAX) {
		    error("Arguments of 'sect=' are invalid - resulting address too big.\n");
		    goto bail;
		}
	    }
	    opt.sect = true;
	} else if (!strcmp(argv[i], "nosect")) {
	    opt.sect = false;
	} else if (!strcmp(argv[i], "save")) {
	    opt.save = true;
	} else if (!strcmp(argv[i], "nosave")) {
	    opt.save = false;
	} else if (!strcmp(argv[i], "filebpb")) {
	    opt.filebpb = true;
	} else if (!strcmp(argv[i], "nofilebpb")) {
	    opt.filebpb = false;
	} else if (!strcmp(argv[i], "warn")) {
	    opt.warn = true;
	} else if (!strcmp(argv[i], "nowarn")) {
	    opt.warn = false;
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

    if ((!opt.maps || !opt.sect) && !opt.file) {
	error("You have to load something.\n");
	goto bail;
    }

    if (opt.filebpb && !opt.file) {
	error("Option 'filebpb' requires file.\n");
	goto bail;
    }

    return 0;
bail:
    return -1;
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
	if (!(iter = pi_begin(&diskinfo)))
	    goto bail;

    } else if (!strcmp(opt.drivename, "boot") || !strcmp(opt.drivename, "fs")) {
	if (!is_phys(sdi->c.filesystem)) {
	    error("When syslinux is not booted from physical disk (or its emulation),\n"
		   "'boot' and 'fs' are meaningless.\n");
	    goto bail;
	}
#if 0
	/* offsets match, but in case it changes in the future */
	if (sdi->c.filesystem == SYSLINUX_FS_ISOLINUX) {
	    drive = sdi->iso.drive_number;
	    fs_lba = *sdi->iso.partoffset;
	} else {
#endif
	    drive = sdi->disk.drive_number;
	    fs_lba = *sdi->disk.partoffset;
#if 0
	}
#endif

	if (disk_get_params(drive, &diskinfo))
	    goto bail;
	/* this will start iteration over disk emulation, possibly raw */
	if (!(iter = pi_begin(&diskinfo)))
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

/* Create boot info table: needed when you want to chainload
 * another version of ISOLINUX (or another bootlaoder that needs
 * the -boot-info-table switch of mkisofs)
 * (will only work when run from ISOLINUX)
 */
static int manglef_isolinux(struct data_area *data)
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

    isolinux_bin = (unsigned char *)data->data;

    /* Get LBA address of bootfile */
    file_lba = get_file_lba(opt.file);

    if (file_lba == 0) {
	error("Failed to find LBA offset of the boot file\n");
	goto bail;
    }
    /* Set it */
    *((uint32_t *) & isolinux_bin[12]) = file_lba;

    /* Set boot file length */
    *((uint32_t *) & isolinux_bin[16]) = data->size;

    /* Calculate checksum */
    checksum = (uint32_t *) & isolinux_bin[20];
    chkhead = (uint32_t *) & isolinux_bin[64];
    chktail = (uint32_t *) & isolinux_bin[data->size & ~3u];
    *checksum = 0;
    while (chkhead < chktail)
	*checksum += *chkhead++;

    /*
     * Deal with possible fractional dword at the end;
     * this *should* never happen...
     */
    if (data->size & 3) {
	uint32_t xword = 0;
	memcpy(&xword, chkhead, data->size & 3);
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
static int manglef_grldr(const struct part_iter *iter)
{
    opt.regs.edx.b[1] = (uint8_t)(iter->index - 1);
    return 0;
}

/*
 * Legacy grub's stage2 chainloading
 */
static int manglef_grub(const struct part_iter *iter, struct data_area *data)
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

    if (data->size < sizeof(struct grub_stage2_patch_area)) {
	error("The file specified by grub=<loader> is too small to be stage2 of GRUB Legacy.\n");
	goto bail;
    }
    stage2 = data->data;

    /*
     * Check the compatibility version number to see if we loaded a real
     * stage2 file or a stage2 file that we support.
     */
    if (stage2->compat_version_major != 3
	    || stage2->compat_version_minor != 2) {
	error("The file specified by grub=<loader> is not a supported stage2 GRUB Legacy binary.\n");
	goto bail;
    }

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
    stage2->install_partition.part1 = (uint8_t)(iter->index - 1);

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
static int manglef_drmk(struct data_area *data)
{
    /*
     * DRMK entry is different than MS-DOS/PC-DOS
     * A new size, aligned to 16 bytes to ease use of ds:[bp+28].
     * We only really need 4 new, usable bytes at the end.
     */

    uint32_t tsize = (data->size + 19) & 0xfffffff0;
    opt.regs.ss = opt.regs.fs = opt.regs.gs = 0;	/* Used before initialized */
    if (!realloc(data->data, tsize)) {
	error("Failed to realloc for DRMK.\n");
	goto bail;
    }
    data->size = tsize;
    /* ds:[bp+28] must be 0x0000003f */
    opt.regs.ds = (uint16_t)((tsize >> 4) + (opt.fseg - 2));
    /* "Patch" into tail of the new space */
    *(uint32_t *)((char*)data->data + tsize - 4) = 0x0000003f;

    return 0;
bail:
    return -1;
}

static uint32_t lba2chs(const struct disk_info *di, uint64_t lba)
{
    uint32_t c, h, s, t;

    if (di->cbios) {
	if (lba >= di->cyl * di->head * di->sect) {
	    s = di->sect;
	    h = di->head - 1;
	    c = di->cyl - 1;
	    goto out;
	}
	s = ((uint32_t)lba % di->sect) + 1;
	t = (uint32_t)lba / di->sect;
	h = t % di->head;
	c = t / di->head;
    } else
	goto fallback;

out:
    return h | (s << 8) | ((c & 0x300) << 6) | ((c & 0xFF) << 16);

fallback:
    if (di->disk & 0x80)
	return 0x00FFFFFE; /* 1023/63/254 */
    else
	/* FIXME ?
	 * this is mostly "useful" with partitioned floppy,
	 * maybe stick to 2.88mb ?
	 */
	return 0x004F1201; /* 79/18/1 */
#if 0
	return 0x004F2401; /* 79/36/1 */
#endif
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

static int manglef_bpb(const struct part_iter *iter, struct data_area *data)
{
    /* BPB: hidden sectors */
    if (opt.sethid) {
	if (iter->start_lba < ~0u)
	    *(uint32_t *) ((char *)data->data + 0x1c) = (uint32_t)iter->start_lba;
	else
	    /* won't really help much, but ... */
	    *(uint32_t *) ((char *)data->data + 0x1c) = ~0u;
    }
    /* BPB: legacy geometry */
    if (opt.setgeo) {
	if (iter->di.cbios)
	    *(uint32_t *)((char *)data->data + 0x18) = (uint32_t)((iter->di.head << 16) | iter->di.sect);
	else {
	    if (iter->di.disk & 0x80)
		*(uint32_t *)((char *)data->data + 0x18) = 0x00FF003F;
	    else
		*(uint32_t *)((char *)data->data + 0x18) = 0x00020012;
	}
    }

    /* BPB: drive */
    if (opt.setdrv)
	*(uint8_t *)((char *)data->data + opt.drvoff) = (uint8_t)
	    (opt.swap ? iter->di.disk & 0x80 : iter->di.disk);

    return 0;
}

static int try_mangles_bpb(const struct part_iter *iter, struct data_area *data)
{
    void *cmp_buf = NULL;

    if (!(opt.setdrv || opt.setgeo || opt.sethid))
	return 0;

#if 0
    /* Turn this off for now. It's hard to find a reason to
     * BPB-mangle sector 0 of whatever there is, but it's
     * "potentially" useful (fixing fdd's sector ?).
     */
    if (!iter->index)
	return 0;
#endif

    if (!(cmp_buf = malloc(data->size))) {
	error("Could not allocate sector-compare buffer.\n");
	goto bail;
    }

    memcpy(cmp_buf, data->data, data->size);

    manglef_bpb(iter, data);

    if (opt.save && memcmp(cmp_buf, data->data, data->size)) {
	if (disk_write_verify_sector(&iter->di, iter->start_lba, data->data)) {
	    error("Cannot write updated boot sector.\n");
	    goto bail;
	}
    }

    free(cmp_buf);
    return 0;

bail:
    return -1;
}

/*
 * To boot the Recovery Console of Windows NT/2K/XP we need to write
 * the string "cmdcons\0" to memory location 0000:7C03.
 * Memory location 0000:7C00 contains the bootsector of the partition.
 */
static int mangles_cmldr(struct data_area *data)
{
    memcpy((char *)data->data + 3, cmldr_signature, sizeof(cmldr_signature));
    return 0;
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

    /* Do hide / unhide if appropriate */
    if (opt.hide)
	hide_unhide(iter);

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

	if (opt.file && opt.maps && !no_ov(data + fidx, data + ndata)) {
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
	if ( ( fidx >= 0 && !no_ov(data + fidx, data + ndata)) ||
	     ( sidx >= 0 && opt.maps && !no_ov(data + sidx, data + ndata)) ) {
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

	if (opt.drmk && manglef_drmk(data + fidx))
	    goto bail;

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
    if (hand_area)
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
