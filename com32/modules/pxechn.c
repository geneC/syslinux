/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Gene Cumm - All Rights Reserved
 *
 *   Portions from chain.c:
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
 * pxechn.c
 *
 * PXE Chain Loader; Chain load to another PXE network boot program
 * that may be on another host.
 */

#include <stdio.h>
#include <stdlib.h>
#include <consoles.h>
#include <console.h>
#include <errno.h>
#include <string.h>
#include <syslinux/config.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/video.h>
#include <com32.h>
#include <stdint.h>
#include <syslinux/pxe.h>
#include <sys/gpxe.h>

/*
#include <ctype.h>
#include <string.h>
#include <minmax.h>
#include <stdbool.h>
#include <dprintf.h>
#include <syslinux/loadfile.h>
#include <syslinux/bootrm.h>
#include <syslinux/config.h>
*/

#define DEBUG

#ifdef DEBUG
#  define dprintf			printf
#  define dprint_pxe_bootp_t		print_pxe_bootp_t
#  define dprint_pxe_vendor_blk		print_pxe_vendor_blk
#  define dprint_pxe_vendor_raw		print_pxe_vendor_raw
#else
#  define dprintf(f, ...)		((void)0)
#  define dprint_pxe_bootp_t(p, l)	((void)0)
#  define dprint_pxe_vendor_blk(p, l)	((void)0)
#  define dprint_pxe_vendor_raw(p, l)	((void)0)
#endif

#define t_PXENV_RESTART_TFTP	t_PXENV_TFTP_READ_FILE

#define STACK_SPLIT	11

/* same as pxelinux.asm REBOOT_TIME */
#define REBOOT_TIME	300

const char app_name_str[] = "pxechn.c32";

/* A generic payload struct */
struct payload {
    char *d;	/* pointer to Data */
    addr_t s;	/* size in bytes */
};

struct pxelinux_opt {
    const char *fn;	/* Filename as passed to us */
    in_addr_t fip;	/* fn's IP component */
    const char *fp;	/* fn's path component */
    char *cfg;
    char *prefix;
    uint32_t reboot;
    struct payload pkt0, pkt1;	/* original and modified packets */
};


/* from chain.c */
struct data_area {
    void *data;
    addr_t base;
    addr_t size;
};

/* From chain.c */
static inline void error(const char *msg)
{
    fputs(msg, stderr);
}

/* From chain.c */
static void do_boot(struct data_area *data, int ndata,
		    struct syslinux_rm_regs *regs)
{
    uint16_t *const bios_fbm = (uint16_t *) 0x413;
    addr_t dosmem = *bios_fbm << 10;	/* Technically a low bound */
    struct syslinux_memmap *mmap;
    struct syslinux_movelist *mlist = NULL;
    addr_t endimage;
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


    /* Tell the shuffler not to muck with this area... */
    syslinux_add_memmap(&mmap, endimage, 0xa0000 - endimage, SMT_RESERVED);

    /* Force text mode */
    syslinux_force_text_mode();

    fputs("Booting...\n", stdout);
    syslinux_shuffle_boot_rm(mlist, mmap, 3, regs);
    error("Chainboot failed!\n");
    return;

too_big:
    error("Loader file too large\n");
    return;

enomem:
    error("Out of memory\n");
    return;
}

void usage(void)
{
    printf("USAGE: %s _new-nbp_ [-c config] [-p prefix] [-t reboot]\n"
	"    %s -r _new-nbp_    (calls PXE stack PXENV_RESTART_TFTP)\n",
	app_name_str, app_name_str);
}

void pxe_error(int ierr, const char *evt, const char *msg)
{
    if (msg)
	printf("%s", msg);
    else if (evt)
	printf("Error while %s: ", evt);
    printf("%d:%s\n", ierr, strerror(ierr));
}

int dhcp_find_opt(pxe_bootp_t *p, size_t len, uint8_t opt)
{
    int rv = -1;
    int i, vlen, oplen;
    uint8_t *d;
    uint32_t magic;

    if (!p) {
	dprintf("  packet pointer is null\n");
	return rv;
    }
    vlen = len - ((void *)&(p->vendor) - (void *)p);
    d = p->vendor.d;
    magic = ntohl(*((uint32_t *)d));
    if (magic != VM_RFC1048)	/* Invalid DHCP packet */
	vlen = 0;
    for (i = 4; i < vlen; i++) {
	if (d[i] == opt) {
	    dprintf("\n    @%03X-%2d\n", i, d[i]);
	    rv = i;
	    break;
	}
	if (d[i] == 255)	/* End of list */
	    break;
	if (d[i]) {		/* Skip padding */
	    oplen = d[++i];
	    i = i + oplen;
	}
    }
    return rv;
}

void print_pxe_vendor_raw(pxe_bootp_t *p, size_t len)
{
    int i, vlen;
    if (!p) {
	printf("  packet pointer is null\n");
	return;
    }
    vlen = len - ((void *)&(p->vendor) - (void *)p);
    dprintf("  rawLen = %d", vlen);
    for (i = 0; i < vlen; i++) {
	if ((i & 0xf) == 0)
	    printf("\n  %04X:", i);
	printf(" %02X", p->vendor.d[i]);
	if (i >= 0x7F)
	    break;
    }
    printf("\n");
}

void print_pxe_vendor_blk(pxe_bootp_t *p, size_t len)
{
    int i, vlen, oplen, j;
    uint8_t *d;
    uint32_t magic;
    if (!p) {
	printf("  packet pointer is null\n");
	return;
    }
    vlen = len - ((void *)&(p->vendor) - (void *)p);
    printf("  Vendor Data:    Len=%d", vlen);
    d = p->vendor.d;
    /* Print only 256 characters of the vendor/option data */
    /*
    print_pxe_vendor_raw(p, (len - vlen) + 256);
    vlen = 0;
    */
    magic = ntohl(*((uint32_t *)d));
    printf("    Magic: %08X", ntohl(*((uint32_t *)d)));
    if (magic != VM_RFC1048)	/* Invalid DHCP packet */
	vlen = 0;
    for (i = 4; i < vlen; i++) {
	if (d[i])	/* Skip the padding */
	    printf("\n    @%03X-%2d", i, d[i]);
	if (d[i] == 255)	/* End of list */
	    break;
	if (d[i]) {
	    oplen = d[++i];
	    printf(" l=%3d:", oplen);
	    for (j = (++i + oplen); i < vlen && i < j; i++) {
		printf(" %02X", d[i]);
	    }
	    i--;
	}
    }
    printf("\n");
}

void print_pxe_bootp_t(pxe_bootp_t *p, size_t len)
{
    if (!p) {
	printf("  packet pointer is null\n");
	return;
    }
    printf("  op:%02X  hw:%02X  hl:%02X  gh:%02X  id:%08X se:%04X f:%04X"
	"  cip:%08X\n", p->opcode, p->Hardware, p->Hardlen, p->Gatehops,
	ntohl(p->ident), ntohs(p->seconds), ntohs(p->Flags), ntohl(p->cip));
    printf("  yip:%08X  sip:%08X  gip:%08X",
	ntohl(p->yip), ntohl(p->sip), ntohl(p->gip));
    printf("  caddr-%02X:%02X:%02X:%02X:%02X:%02X\n", p->CAddr[0],
	p->CAddr[1], p->CAddr[2], p->CAddr[3], p->CAddr[4], p->CAddr[5]);
    printf("  sName: '%s'\n", p->Sname);
    printf("  bootfile: '%s'\n", p->bootfile);
    dprint_pxe_vendor_blk(p, len);
}

void pxe_set_regs(struct syslinux_rm_regs *regs)
{
    com32sys_t tregs;

    regs->ip = 0x7C00;
    /* Plan A uses SS:[SP + 4] */
    /* sdi->pxe.stack is a usable pointer, not something that can be nicely
       and reliably split to SS:SP without causing issues */
    tregs.eax.l = 0x000A;
    __intcall(0x22, &tregs, &tregs);
    regs->ss = tregs.fs;
    regs->esp.l = tregs.esi.w[0] + sizeof(tregs);
    /* Plan B uses [ES:BX] */
    regs->es = tregs.es;
    regs->ebx = tregs.ebx;
    dprintf("\nsp:%04x    ss:%04x    es:%04x    bx:%04x\n", regs->esp.w[0],
	regs->ss, regs->es, regs->ebx.w[0]);
    /* Zero out everything else just to be sure */
    regs->cs = regs->ds = regs->fs = regs->gs = 0;
    regs->eax.l = regs->ecx.l = regs->edx.l = 0;
}

/* Parse a filename into an IPv4 address and filename pointer
 *	returns	Based on the interpretation of fn
 *		0 regular file name
 *		1 in format IP::FN
 *		2 TFTP URL
 *		3 HTTP URL
 *		4 HTTPS URL
 *		-1 if fn is another URL type
 */
int pxechain_parse_fn(const char fn[], in_addr_t *fip, const char *fp[])
{
    char host[256];	/* 63 bytes per label; 255 max total */
    in_addr_t tip = 0;
    char *csep, *ssep;	/* Colon, Slash separator positions */
    int hlen;
    int rv = 0;

    host[0] = 0;
    csep = strchr(fn, ':');
    if (csep) {
	if (csep[1] == ':') {	/* IP::FN */
	    *fp = &csep[2];
	    if (fn[0] != ':') {
		hlen = csep - fn;
		memcpy(host, fn, hlen);
		host[hlen] = 0;
		rv = 1;
	    } else {	/* assume plain filename */
		csep = NULL;
	    }
	} else if ((csep[1] == '/') && (csep[2] == '/')) {
		/* URL: proto://host:port/path/file */
	    ssep = strchr(csep + 3, '/');
	    if (ssep) {
		hlen = ssep - (csep + 3);
		*fp = ssep + 1;
	    } else {
		hlen = strlen(csep + 3);
	    }
	    memcpy(host, (csep + 3), hlen);
	    host[hlen] = 0;
	    if (strncmp(fn, "tftp", 4) == 0)
		rv = 2;
	    else if (strncmp(fn, "http", 4) == 0)
		rv = 3;
	    else if (strncmp(fn, "https", 5) == 0)
		rv = 4;
	    else
		rv = -1;
	} else {
	    csep = NULL;
	}
    }
    if (!csep) {
	*fp = fn;
    }
    if (host[0])
	tip = pxe_dns(host);
    if (tip != 0)
	*fip = tip;
    dprintf("  host '%s'\n  fp   '%s'\n  fip  %08x\n", host, *fp, ntohl(*fip));
    return rv;
}

void pxechain_fill_pkt(struct pxelinux_opt *pxe)
{
    int rv = -1;
    /* PXENV_PACKET_TYPE_DHCP_DISCOVER PXENV_PACKET_TYPE_DHCP_ACK PXENV_PACKET_TYPE_CACHED_REPLY */
/*    if (!pxe_get_cached_info(PXENV_PACKET_TYPE_DHCP_ACK,
	    (void **)&(pxe->pkt0.d), &(pxe->pkt0.s)))
	dprint_pxe_bootp_t((pxe_bootp_t *)(pxe->pkt0.d));*/
    if (!pxe_get_cached_info(PXENV_PACKET_TYPE_CACHED_REPLY,
	    (void **)&(pxe->pkt0.d), &(pxe->pkt0.s))) {
	pxe->pkt1.d = malloc(2048);
	if (pxe->pkt1.d) {
	    memcpy(pxe->pkt1.d, pxe->pkt0.d, pxe->pkt0.s);
	    pxe->pkt1.s = pxe->pkt0.s;
	    rv = 0;
	    dprint_pxe_bootp_t((pxe_bootp_t *)(pxe->pkt0.d), pxe->pkt0.s);
	} else {
	    printf("%s: ERROR: Unable to malloc() for second packet\n", app_name_str);
	    free(pxe->pkt0.d);
	}
    } else {
	printf("%s: ERROR: Unable to retrieve first packet\n", app_name_str);
    }
    if (rv <= -1) {
	pxe->pkt0.d = pxe->pkt1.d = NULL;
	pxe->pkt0.s = pxe->pkt1.s = 0;
    }
}

void pxechain_init(struct pxelinux_opt *pxe)
{
    pxe->fn = pxe->fp = pxe->cfg = pxe->prefix = NULL;
    pxe->reboot = REBOOT_TIME;
    pxechain_fill_pkt(pxe);
}

void pxechain_args(int argc, char *argv[], struct pxelinux_opt *pxe)
{
    /* Init for paranoia */
    if (pxe->pkt1.d)
	pxe->fip = ( (pxe_bootp_t *)(pxe->pkt1.d) )->sip;
    else
	pxe->fip = 0;
    /* Fill */
    pxe->fn = argv[0];
    if (argc > 1) {
	if (strcmp(argv[1], "--") != 0)
	    pxe->cfg = argv[1];
	if (argc > 2) {
	    if (strcmp(argv[2], "--") != 0)
		pxe->prefix = argv[2];
	    if ((argc = 4) && (strcmp(argv[3], "--") != 0))
		pxe->reboot = strtoul(argv[3], NULL, 0);
	}
    }
    pxechain_parse_fn(pxe->fn, &(pxe->fip), &(pxe->fp));
}

/* dhcp_copy_pkt_to_pxe: Copy packet to PXE's BC data for a ptype packet
 *	Input:
 *	p	Packet data to copy
 *	len	length of data to copy
 *	ptype	Packet type to overwrite
 */
int dhcp_copy_pkt_to_pxe(pxe_bootp_t *p, size_t len, int ptype)
{
    com32sys_t reg;
    t_PXENV_GET_CACHED_INFO *ci;
    void *cp;
    int rv = -1;

    if (!(ci = lzalloc(sizeof(t_PXENV_GET_CACHED_INFO)))){
	dprintf("Unable to lzalloc() for PXE call structure\n");
	rv = 1;
	goto ret;
    }
    ci->Status = PXENV_STATUS_FAILURE;
    ci->PacketType = ptype;
    memset(&reg, 0, sizeof(reg));
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = PXENV_GET_CACHED_INFO;
    reg.edi.w[0] = OFFS(ci);
    reg.es = SEG(ci);
    __intcall(0x22, &reg, &reg);

    if (ci->Status != PXENV_STATUS_SUCCESS) {
	dprintf("PXE Get Cached Info failed: %d\n", ci->Status);
	rv = 2;
	goto ret;
    }

    cp = MK_PTR(ci->Buffer.seg, ci->Buffer.offs);
    if (!(memcpy(cp, p, len))) {
	dprintf("Failed to copy packet\n");
	rv = 3;
	goto ret;
    }
ret:
   return rv;
}

void pxechain_patch_pkt1(int argc, char *argv[], struct pxelinux_opt *pxe)
{
    pxe_bootp_t *bootp1;
    char *t;

    bootp1 = (pxe_bootp_t *)(pxe->pkt1.d);
    bootp1->sip = pxe->fip;
    strncpy((char *)(bootp1->bootfile), pxe->fp, 127);
    bootp1->bootfile[127] = 0;
    if (argc >= 1)
	t = argv[0];
}

/* pxechain: Chainload to new PXE file ourselves
 *	Input:
 *	argc	Count of arguments passed
 *	argv	Values of arguments passed
 *	Returns	0 on success (which should never happen)
 *		1 on loadfile() error
 *		2 if DHCP Option 52 (Option Overload) found
 *		-1 on usage error
 */
int pxechain(int argc, char *argv[])
{
    struct pxelinux_opt pxe;
    pxe_bootp_t *bootp0, *bootp1;
    int rv;
    struct data_area file;
    struct syslinux_rm_regs regs;

    pxechain_init(&pxe);
    bootp0 = (pxe_bootp_t *)(pxe.pkt0.d);
    bootp1 = (pxe_bootp_t *)(pxe.pkt1.d);
    if ((rv = dhcp_find_opt(bootp0, pxe.pkt0.s, 52)) >= 0) {
	puts(" Found UNSUPPORTED option (52) overload in DHCP packet; aborting");
	rv = 2;
	goto ret;
    } else {
	rv = 0;
    }
//     --parse-options
    pxechain_args(argc, argv, &pxe);
//     goto tabort;
//     --rebuild copy #1 applying new options in order ensuring an option is only specified once in patched packet
    pxechain_patch_pkt1(argc, argv, &pxe);
    /* Parse the filename to understand if a PXE parameter update is needed. */
    /* How does BKO do this for HTTP? option 209/210 */
//     --set_registers
    pxe_set_regs(&regs);
    /* Load the file last; it's the most time-expensive operation */
    printf("%s: Attempting to load '%s': ", app_name_str, pxe.fn);
    if (loadfile(pxe.fn, &file.data, &file.size)) {
	pxe_error(errno, NULL, NULL);
	rv = 1;
	goto ret;
    }
    puts("loaded.");
    /* we'll be shuffling to the standard location of 7C00h */
    file.base = 0x7C00;
//     --copy patched copy into cache
    dhcp_copy_pkt_to_pxe(bootp1, pxe.pkt1.s, PXENV_PACKET_TYPE_CACHED_REPLY);
//     --try boot
    if (true) {
	puts("  Attempting to boot...");
	do_boot(&file, 1, &regs);
    }
//     --if failed, copy backup back in and abort
    dhcp_copy_pkt_to_pxe(bootp0, pxe.pkt0.s, PXENV_PACKET_TYPE_CACHED_REPLY);
ret:
    return rv;
}

/* pxe_restart: Restart the PXE environment with a new PXE file
 *	Input:
 *	ifn	Name of file to chainload to in a format PXELINUX understands
 *		This must strictly be TFTP or relative file
 */
int pxe_restart(const char *ifn)
{
    int rv = 0;
    struct pxelinux_opt pxe;
    com32sys_t reg;
    t_PXENV_RESTART_TFTP *pxep;	/* PXENV callback Parameter */

    pxe.fn = ifn;
    pxechain_fill_pkt(&pxe);
    if (pxe.pkt1.d)
	pxe.fip = ( (pxe_bootp_t *)(pxe.pkt1.d) )->sip;
    else
	pxe.fip = 0;
    rv = pxechain_parse_fn(pxe.fn, &(pxe.fip), &(pxe.fp));
    if ((rv > 2) || (rv < 0)) {
	printf("%s: ERROR: Unparsable filename argument: '%s'\n\n", app_name_str, pxe.fn);
	goto ret;
    }
    printf("  Attempting to boot '%s'...\n\n", pxe.fn);
//     goto ret;
    memset(&reg, 0, sizeof reg);
    if (sizeof(t_PXENV_TFTP_READ_FILE)<= __com32.cs_bounce_size) {
	pxep = __com32.cs_bounce;
	memset(pxep, 0, sizeof(t_PXENV_RESTART_TFTP));
    } else if (!(pxep = lzalloc(sizeof(t_PXENV_RESTART_TFTP)))){
	dprintf("Unable to lzalloc() for PXE call structure\n");
	goto ret;
    }
    pxep->Status = PXENV_STATUS_SUCCESS;	/* PXENV_STATUS_FAILURE */
    strcpy((char *)pxep->FileName, ifn);
    pxep->BufferSize = 0x8000;	/* 0x90000; */
//     if (!(pxep->Buffer = lmalloc(pxep->BufferSize))) {
// 	dprintf("Unable to lalloc() for buffer; using default\n");
	pxep->Buffer = (void *)0x7c00;
//     }
    pxep->ServerIPAddress = pxe.fip;
    dprintf("FN='%s'  %08X %08X %08X %08X\n\n", (char *)pxep->FileName,
	pxep->ServerIPAddress, (unsigned int)pxep,
	pxep->BufferSize, (unsigned int)pxep->Buffer);
    dprintf("PXENV_RESTART_TFTP status %d\n", pxep->Status);
// --here
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = PXENV_RESTART_TFTP;
    reg.edi.w[0] = OFFS(pxep);
    reg.es = SEG(pxep);

    __intcall(0x22, &reg, &reg);

    printf("PXENV_RESTART_TFTP returned %d\n", pxep->Status);
    lfree(pxep);

ret:
    return rv;
}

/* pxechain_gpxe: Use gPXE to chainload a new NBP
 * If it's around, don't bother with the heavy lifting ourselves
 *	Input:
 *	argc	Count of arguments passed
 *	argv	Values of arguments passed
 *	Returns	0 on success (which should never happen)
 *		1 on loadfile() error
 *		-1 on usage error
 */
int pxechain_gpxe(int argc, char *argv[])
{
    int rv = 0;
    struct pxelinux_opt pxe;

    if (argc) {
	printf("%s\n", argv[0]);
	pxechain_args(argc, argv, &pxe);
    }
    return rv;
}

int main(int argc, char *argv[])
{
    int rv= -1;
    const struct syslinux_version *sv;

    /* Initialization */
    openconsole(&dev_null_r, &dev_stdcon_w);
//     console_ansi_raw();
    sv = syslinux_version();
    if (sv->filesystem != SYSLINUX_FS_PXELINUX) {
	printf("%s: May only run in PXELINUX\n", app_name_str);
	argc = 1;	/* prevents further processing to boot */
/*    } else if (is_gpxe()) {
	rv = pxechain_gpxe(argc - 1, &argv[1]);
	if (rv >= 0)
	    argc = 1;*/
    }
    if (argc == 2) {
	if ((strcmp(argv[1], "-h") == 0) || ((strcmp(argv[1], "-?") == 0))
		|| (strcmp(argv[1], "--help") == 0)) {
	    argc = 1;
	} else {
	    rv = pxechain(argc - 1, &argv[1]);
	}
    } else if (argc >= 3) {	/* change to 3 for processing -q */
	if ((strcmp(argv[1], "-r") == 0)) {
	    if (argc == 3)
		rv = pxe_restart(argv[2]);
	} else {
	    rv = pxechain(argc - 1, &argv[1]);
	}
    }
    if (rv <= -1 ) {
	usage();
	rv = 1;
    }
puts("tmp2");
    return rv;
}
