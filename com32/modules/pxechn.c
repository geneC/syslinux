/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010-2012 Gene Cumm - All Rights Reserved
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
#include <unistd.h>
#include <getkey.h>
#include <dhcp.h>


#define PXECHN_DEBUG 1

typedef union {
    uint64_t q;
    uint32_t l[2];
    uint16_t w[4];
    uint8_t b[8];
} reg64_t;

#define dprintf0(f, ...)		((void)0)

#if (PXECHN_DEBUG > 0)
#  define dpressanykey			pressanykey
#  define dprintf			printf
#  define dprint_pxe_bootp_t		print_pxe_bootp_t
#  define dprint_pxe_vendor_blk		print_pxe_vendor_blk
#  define dprint_pxe_vendor_raw		print_pxe_vendor_raw
#else
#  define dpressanykey()		((void)0)
#  define dprintf(f, ...)		((void)0)
#  define dprint_pxe_bootp_t(p, l)	((void)0)
#  define dprint_pxe_vendor_blk(p, l)	((void)0)
#  define dprint_pxe_vendor_raw(p, l)	((void)0)
#endif

#define dprintf_opt_cp		dprintf0
#define dprintf_opt_inj		dprintf0
#define dprintf_pc_pa_hx_pure	dprintf0
#define dprintf_pc_pa_hx_dec	dprintf0
#define dprintf_pc_pa_hx_tl	dprintf0
#define dprintf_pc_pa		dprintf

#define t_PXENV_RESTART_TFTP	t_PXENV_TFTP_READ_FILE

#define STACK_SPLIT	11

/* same as pxelinux.asm REBOOT_TIME */
#define REBOOT_TIME	300

#define NUM_DHCP_OPTS	256
#define DHCP_OPT_LEN_MAX	256
#define PXE_VENDOR_RAW_PRN_MAX	0x7F

const char app_name_str[] = "pxechn.c32";

struct pxelinux_opt {
    char *fn;	/* Filename as passed to us */
    in_addr_t fip;	/* fn's IP component */
    char *fp;	/* fn's path component */
//     char *cfg;
//     char *prefix;
//     uint32_t reboot, rebootn;	/* Host and network order of reboot time out */
//     uint8_t opt52;	/* DHCP Option Overload value */
    uint32_t wait;	/* Additional decision to wait before boot */
    struct dhcp_option pkt0, pkt1;	/* original and modified packets */
    char host[256];	/* 63 bytes per label; 255 max total */
    uint32_t relay;	/* giaddr; Gateway/DHCP relay */
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
    printf("USAGE: %s [-c config] [-p prefix] [-t reboot]"
	" [-X xx...]... [-x opt:xx...]... _new-nbp_\n"
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

int pressanykey(void) {
    int inc;

    printf("Press any key to continue. ");
    inc = KEY_NONE;
    while (inc == KEY_NONE)
	inc = get_key(stdin, 6000);
    puts("");
    return inc;
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
	if (d[i] == ((NUM_DHCP_OPTS) - 1))	/* End of list */
	    break;
	if (d[i]) {		/* Skip padding */
	    oplen = d[++i];
	    i += oplen;
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
    if (vlen > PXE_VENDOR_RAW_PRN_MAX)
	vlen = PXE_VENDOR_RAW_PRN_MAX;
    dprintf("  rawLen = %d", vlen);
    for (i = 0; i < vlen; i++) {
	if ((i & 0xf) == 0)
	    printf("\n  %04X:", i);
	printf(" %02X", p->vendor.d[i]);
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
    magic = ntohl(*((uint32_t *)d));
    printf("    Magic: %08X", ntohl(*((uint32_t *)d)));
    if (magic != VM_RFC1048)	/* Invalid DHCP packet */
	vlen = 0;
    for (i = 4; i < vlen; i++) {
	if (d[i])	/* Skip the padding */
	    printf("\n    @%03X-%3d", i, d[i]);
	if (d[i] == ((NUM_DHCP_OPTS) - 1))	/* End of list */
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
    if (!p || len <= 0) {
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

//FIXME: To a library
/* Parse a filename into an IPv4 address and filename pointer
 *	returns	Based on the interpretation of fn
 *		0 regular file name
 *		1 in format IP::FN
 *		2 TFTP URL
 *		3 HTTP URL
 *		4 FTP URL
 *		3 + 2^30 HTTPS URL
 *		-1 if fn is another URL type
 */
int pxechn_parse_fn(char fn[], in_addr_t *fip, char *host, char *fp[])
{
    in_addr_t tip = 0;
    char *csep, *ssep, *hsep;	/* Colon, Slash separator positions */
    int hlen, plen;	/* Hostname, protocol length */
    int rv = 0;

    csep = strchr(fn, ':');
    if (csep) {
	if (csep[1] == ':') {	/* assume IP::FN */
	    *fp = &csep[2];
	    rv = 1;
	    if (fn[0] != ':') {
		hlen = csep - fn;
		memcpy(host, fn, hlen);
		host[hlen] = 0;
	    }
	} else if ((csep[1] == '/') && (csep[2] == '/')) {
		/* URL: proto://host:port/path/file */
		/* proto://[user[:passwd]@]host[:port]/path/file */
	    ssep = strchr(csep + 3, '/');
	    if (ssep) {
		hlen = ssep - (csep + 3);
		*fp = ssep + 1;
	    } else {
		hlen = strlen(csep + 3);
	    }
	    memcpy(host, (csep + 3), hlen);
	    host[hlen] = 0;
	    plen = csep - fn;
	    if (strncmp(fn, "tftp", plen) == 0)
		rv = 2;
	    else if (strncmp(fn, "http", plen) == 0)
		rv = 3;
	    else if (strncmp(fn, "ftp", plen) == 0)
		rv = 4;
	    else if (strncmp(fn, "https", plen) == 0)
		rv = 3 + ( 1 << 30 );
	    else
		rv = -1;
	} else {
	    csep = NULL;
	}
    }
    if (!csep) {
	*fp = fn;
    }
    if (host[0]) {
	hsep = strchr(host, '@');
	if (!hsep)
	    hsep = host;
	tip = pxe_dns(hsep);
    }
    if (tip != 0)
	*fip = tip;
    dprintf0("  host '%s'\n  fp   '%s'\n  fip  %08x\n", host, *fp, ntohl(*fip));
    return rv;
}

void pxechn_fill_pkt(struct pxelinux_opt *pxe)
{
    int rv = -1;
    if (!pxe_get_cached_info(PXENV_PACKET_TYPE_CACHED_REPLY,
	    (void **)&(pxe->pkt0.data), (size_t *)&(pxe->pkt0.len))) {
	pxe->pkt1.data = malloc(2048);
	if (pxe->pkt1.data) {
	    memcpy(pxe->pkt1.data, pxe->pkt0.data, pxe->pkt0.len);
	    pxe->pkt1.len = pxe->pkt0.len;
	    rv = 0;
	    dprint_pxe_bootp_t((pxe_bootp_t *)(pxe->pkt0.data), pxe->pkt0.len);
	    dpressanykey();
	} else {
	    printf("%s: ERROR: Unable to malloc() for second packet\n", app_name_str);
	    free(pxe->pkt0.data);
	}
    } else {
	printf("%s: ERROR: Unable to retrieve first packet\n", app_name_str);
    }
    if (rv <= -1) {
	pxe->pkt0.data = pxe->pkt1.data = NULL;
	pxe->pkt0.len = pxe->pkt1.len = 0;
    }
}

void pxechn_init(struct pxelinux_opt *pxe)
{
    /* Init for paranoia */
    pxe->fn = pxe->fp = NULL;
//     pxe->cfg = pxe->prefix = NULL;
//     pxe->reboot = REBOOT_TIME;
//     pxe->opt52 = 0;
    pxe->wait = pxe->relay = 0;
    pxe->host[0] = pxe->host[((NUM_DHCP_OPTS) - 1)] = 0;
    pxechn_fill_pkt(pxe);
}

int pxechn_to_hex(char i)
{
    if (i >= '0' && i <= '9')
	return (i - '0');
    if (i >= 'A' && i <= 'F')
	return (i - 'A' + 10);
    if (i >= 'a' && i <= 'f')
	return (i - 'a' + 10);
    if (i == 0)
	return -1;
    return -2;
}

int pxechn_parse_2bhex(char ins[])
{
    int ret = -2;
    int n0 = -3, n1 = -3;
    /* NULL pointer */
    if (!ins) {
	ret = -1;
    /* pxechn_to_hex can handle the NULL character by returning -1 and
       breaking the execution of the statement chain */
    } else if (((n0 = pxechn_to_hex(ins[0])) >= 0) && ((n1 = pxechn_to_hex(ins[1])) >= 0)) {
	ret = (n0 * 16) + n1;
    } else if (n0 == -1) {	/* Leading NULL char */
	ret = -1;
    }
    return ret;
}

int pxechn_optnum_ok(int optnum)
{
    if ((optnum > 0) && (optnum < ((NUM_DHCP_OPTS) - 1)))
	return 1;
    return 0;
}

int pxechn_optnum_ok_notres(int optnum)
{
    if ((optnum <= 0) && (optnum >= ((NUM_DHCP_OPTS) - 1)))
	return 0;
    switch(optnum){
    case 66: case 67:
	return 0;
	break;
    default:	return 1;
    }
}

int pxechn_optlen_ok(int optlen)
{
    if ((optlen >= 0) && (optlen < ((DHCP_OPT_LEN_MAX) - 1)))
	return 1;
    return 0;
}

int pxechn_parse_arg_hex_tail(void **data, char istr[])
{
    int len = -1, p = 0, conv = 0;
    char *ostr;

    if (istr) {
	len = 0;
	ostr = *data;
	while ((len >= 0) && (conv >= 0)) {
	    if (len >= DHCP_OPT_LEN_MAX) {
		dprintf_pc_pa_hx_tl("HEX: break\t");
		break;
	    }
	    /* byte delimiter */
	    if ((conv = pxechn_to_hex(istr[p])) == -2){
		dprintf_pc_pa_hx_tl("HEX:delim\t");
		p++;
	    }
	    conv = pxechn_parse_2bhex(istr + p);
	    dprintf_pc_pa_hx_tl("HEX:%02x@%d\t", conv, p);
	    if (conv >= 0) {
		ostr[len++] = conv;
	    } else if (conv < -1 ) {
		len = -2;	/* Garbage */
	    }
	    p += 2;
	}
	dprintf_pc_pa_hx_tl("\n");
    }
    if (len > 0) {
	dprintf_pc_pa_hx_tl("HEX:l=%d\t", len);
    }
    return len;
}

int pxechn_parse_arg_hex_pure(int *optnum, void **data, char istr[])
{
    int len = -2;

    if (!optnum || !data || !istr)
	return -1;
    if (pxechn_to_hex(istr[0]) >= 0) {
	*optnum = pxechn_parse_2bhex(istr);
	if (*optnum >= 0)
	    len = pxechn_parse_arg_hex_tail(data, istr + 2);
    }
    return len;
}

int pxechn_parse_arg_hex(int *optnum, void **data, char istr[])
{
    int len = -2;
    char *pos = NULL;

    if (!optnum)
	return len;
    *optnum = strtoul(istr, &pos, 0);
    if (pxechn_optnum_ok(*optnum))
	len = pxechn_parse_arg_hex_tail(data, pos);
    return len;
}

void *pxechn_realloc(void *d, int len)
{
    void *p;
    char *c;
    int i;
    if (d) {
	printf("    realloc %d @%08X", len, (uint32_t)d);
	p = realloc(d, len);
	printf(" ->%08X\n", (uint32_t)p);
	return p;
    }
    printf("    malloc %d (%d)", len, errno);
    c = p =  malloc(len);
    for(i=0; i<len; i++) c[i] = (0xDE + i) & 0xFF;
    printf(" ->%08X (%d)\n", (uint32_t)p, errno);
    return p;
}

int pxechn_setopt(struct dhcp_option *opt, void *data, int len)
{
    void *p;
/*int i;
uint8_t *d = data;*/
    if (!opt || !data)
	return -1;
/*printf("setopt %08X = %08X:", (int)(opt->data), (int)d);
for(i=0;i<len;i++)
printf(" %02X", d[i]);
printf("\n");*/
    p = pxechn_realloc(opt->data, len);
// printf("  setopt %08X\n", (int)(p));
    if (!p) {
	return -2;
    }
    opt->data = p;
    memcpy(opt->data, data, len);
    opt->len = len;
    return len;
}

int pxechn_setopt_str(struct dhcp_option *opt, void *data)
{
    return pxechn_setopt(opt, data, strnlen(data, DHCP_OPT_LEN_MAX));
}

/*
 * returns: -1 if an arg is null; -2 if tlen invalid; -3 on strtoul error;
	-4 on value overflow; -5 bad option number
 */
int pxechn_parseuint_setopt(struct dhcp_option opts[], char istr[], int tlen)
{
    int optnum, rv;
    char *pos = NULL;

    if ((!opts) || (!istr))
	return -1;
printf("pxechn_parseuint_setopt '%s'\n", istr);
    optnum = strtoul(istr, &pos, 0);
    if (!pxechn_optnum_ok(optnum))
	return -5;
    int terr = errno;
    pos++;
    if ((tlen == 1) || (tlen == 2) || (tlen == 4)) {
	errno = 0;
	uint32_t optval = strtoul(pos, NULL, 0);
	if (errno)
	    return -3;
	errno = terr;
// printf("  opt %d val %x len %d '%s'\n", optnum, optval, tlen, pos);
	switch(tlen){
	case  1:
	    if (optval & 0xFFFFFF00)
		return -4;
	    break;
	case  2:
	    if (optval & 0xFFFF0000)
		return -4;
	    optval = htons(optval);
	    break;
	case  4:
	    optval = htonl(optval);
	    break;
	}
printf("  opt %d val %x len %d '%s'\n", optnum, optval, tlen, pos);
	pxechn_setopt(&(opts[optnum]), (void *)(&(optval)), tlen);
    } else if (tlen == 8) {
	errno = 0;
	uint64_t optval = strtoull(pos, NULL, 0);
	if (errno)
	    return -3;
	errno = terr;
/*printf("  opt %d val %llx len %d '%s'\n", optnum, optval, tlen, pos);*/
	optval = htonq(optval);
printf("  opt %d val %llx len %d '%s'\n", optnum, optval, tlen, pos);
	pxechn_setopt(&(opts[optnum]), (void *)(&optval), tlen);
    } else {
	return -2;
    }
    rv = tlen;
    return rv;
}

int pxechn_parse_int(char *data, char istr[], int tlen)
{
    int terr = errno;

    if ((tlen == 1) || (tlen == 2) || (tlen == 4)) {
	errno = 0;
	uint32_t optval = strtoul(istr, NULL, 0);
	if (errno)
	    return -3;
	errno = terr;
	switch(tlen){
	case  1:
	    if (optval & 0xFFFFFF00)
		return -4;
	    break;
	case  2:
	    if (optval & 0xFFFF0000)
		return -4;
	    optval = htons(optval);
	    break;
	case  4:
	    optval = htonl(optval);
	    break;
	}
	memcpy(data, &optval, tlen);
    } else if (tlen == 8) {
	errno = 0;
	uint64_t optval = strtoull(istr, NULL, 0);
	if (errno)
	    return -3;
	errno = terr;
	optval = htonq(optval);
	memcpy(data, &optval, tlen);
    } else {
	return -2;
    }
    return tlen;
}

int pxechn_parse_hex_sep(char *data, char istr[], char sep)
{
    int len = 0;
    int ipos = 0, ichar;
    
    if (!data || !istr)
	return -1;
    while ((istr[ipos]) && (len < DHCP_OPT_LEN_MAX)) {
	printf(" %02X%02X", *((int *)(istr + ipos)) & 0xFF, *((int *)(istr + ipos +1)) & 0xFF);
	ichar = pxechn_parse_2bhex(istr + ipos);
	if (ichar >=0) {
	    data[len++] = ichar;
	} else {
	    return -EINVAL;
	}
	if (!istr[ipos+2]){
	    ipos += 2;
	} else if (istr[ipos+2] != sep) {
	    return -(EINVAL + 1);
	} else {
	    ipos += 3;
	}
    }
    return len;
}

int pxechn_parse_opttype(char istr[], int optnum)
{
    char *pos;
    int tlen, type, tmask;

    if (!istr)
	return -1;
    pos = strchr(istr, '=');
    if (!pos)
	return -2;
    if (istr[0] != '.') {
	if (!pxechn_optnum_ok(optnum))
	    return -3;
	return -3;	/* do lookup here */
    } else {
	tlen = pos - istr - 1;
	if ((tlen < 1) || (tlen > 4))
	    return -4;
	tmask = 0xFFFFFFFF >> (8 * (4 - tlen));
	type = (*(int*)(istr + 1)) & tmask;
    }
    return type;
}

int pxechn_parse_setopt(struct dhcp_option opts[], struct dhcp_option *iopt,
			char istr[])
{
    int rv = 0, optnum, opttype;
    char *cpos = NULL, *pos;

    if (!opts || !iopt || !(iopt->data))
	return -1;
    if (!istr || !istr[0])
	return -2;
    // -EINVAL;
    optnum = strtoul(istr, &cpos, 0);
    if (!pxechn_optnum_ok(optnum))
	return -3;
    pos = strchr(cpos, '=');
    if (!pos)
	return -4;
    opttype = pxechn_parse_opttype(cpos, optnum);
    pos++;
    switch(opttype) {
    case 'b':
	iopt->len = pxechn_parse_int(iopt->data, pos, 1);
	break;
    case 'l':
	iopt->len = pxechn_parse_int(iopt->data, pos, 4);
	break;
    case 'q':
	iopt->len = pxechn_parse_int(iopt->data, pos, 8);
	break;
    case 's':
    case ('s' + 256 * ('t' + 256 * 'r')):
	iopt->len = strlen(pos);
	if (iopt->len > DHCP_OPT_LEN_MAX)
	    iopt->len = DHCP_OPT_LEN_MAX;
	memcpy(iopt->data, pos, iopt->len);
	break;
    case 'w':
	iopt->len = pxechn_parse_int(iopt->data, pos, 2);
	break;
    case 'x':
	iopt->len = pxechn_parse_hex_sep(iopt->data, pos, ':');
	break;
    default:
	return -6;
	break;
    }
    if (iopt->len >= 0) {
	rv = pxechn_setopt(&(opts[optnum]), (void *)(iopt->data), iopt->len);
    }
    return rv;
}

int pxechn_parse_args(int argc, char *argv[], struct pxelinux_opt *pxe,
			 struct dhcp_option opts[])
{
    int arg, optnum, rv = 0;
    const char optstr[] = "c:p:t:wo:f:x:X:b:s:i:l:";
    struct dhcp_option iopt;

    if (pxe->pkt1.data)
	pxe->fip = ( (pxe_bootp_t *)(pxe->pkt1.data) )->sip;
    else
	pxe->fip = 0;
    /* Fill */
    pxe->fn = argv[0];
    iopt.data = malloc(DHCP_OPT_LEN_MAX);
    iopt.len = 0;
    while ((rv == 0) && (arg = getopt(argc, argv, optstr)) >= 0) {
	dprintf_pc_pa("  Got arg '%c' val %s\n", arg, optarg ? optarg : "");
	switch(arg) {
	case 'b':	/* byte */
	    rv = pxechn_parseuint_setopt(opts, optarg, 1);
	    dprintf_pc_pa("brv=%d", rv);
	    rv = 0;
	    break;
	case 'c':	/* config */
// 	    pxe->cfg = optarg;
	    pxechn_setopt_str(&(opts[209]), optarg);
	    break;
	case 'f':	/* force */
	    puts("Unimplemented option utilized");
	    break;
	case 'g':	/* gateway/DHCP relay */
	    optnum = pxe_dns(optarg);
	    if (optnum)
		pxe->relay = optnum;
	    break;
	case 'i':	/* int */
	    pxechn_parseuint_setopt(opts, optarg, 4);
	    break;
	case 'l':	/* long long */
	    pxechn_parseuint_setopt(opts, optarg, 8);
	    break;
	case 'n':	/* native */
	    break;
	case 'o':	/* option */
	    rv = pxechn_parse_setopt(opts, &iopt, optarg);
	    printf("pc_pso() returned %d\n", rv);
rv = 0;
	    break;
	case 'p':	/* prefix */
//	    pxe->prefix = optarg;
	    pxechn_setopt_str(&(opts[210]), optarg);
	    break;
	case 's':	/* short uint16 */
	    pxechn_parseuint_setopt(opts, optarg, 2);
	    break;
	case 't':	/* timeout */
/*	    pxe->reboot = strtoul(optarg, NULL, 0);
	    pxe->rebootn = htonl(pxe->reboot);
	    pxechn_setopt(&(opts[211]), (void *)(&(pxe->rebootn)), 4);*/
	    optnum = htonl(strtoul(optarg, NULL, 0));
	    pxechn_setopt(&(opts[211]), (void *)(&optnum), 4);
	    break;
	case 'w':	/* wait */
	    pxe->wait = 1;
	    if (optarg)
		pxe->wait = (uint32_t)atoi(optarg);
	    break;
	case 'x':	/* Friendly hex string */
	    iopt.len = pxechn_parse_arg_hex(&optnum, &(iopt.data), optarg);
	    if (pxechn_optlen_ok(iopt.len) && pxechn_optnum_ok(optnum)) {
		pxechn_setopt(&(opts[optnum]), iopt.data, iopt.len);
	    }
	    break;
	case 'X':	/* Full heX string */
	    iopt.len = pxechn_parse_arg_hex_pure(&optnum, &(iopt.data), optarg);
	    if (pxechn_optlen_ok(iopt.len) && pxechn_optnum_ok(optnum)) {
		pxechn_setopt(&(opts[optnum]), iopt.data, iopt.len);
	    }
	    break;
	default:
	    break;
	}
    }
    if (iopt.data)
	free(iopt.data);
    pxechn_parse_fn(pxe->fn, &(pxe->fip), pxe->host, &(pxe->fp));
    return 0;
}

int pxechn_args(int argc, char *argv[], struct pxelinux_opt *pxe)
{
    pxe_bootp_t *bootp0, *bootp1;
    int i;
    int ret = 0;
    struct dhcp_option *opts;

    opts = calloc(NUM_DHCP_OPTS, sizeof(struct dhcp_option));
    if (!opts) {
	error("Could not allocate for options\n");
	return -2;
    }
    for (i = 0; i < NUM_DHCP_OPTS; i++) {
	opts[i].len = -1;
    }
    /* Start filling packet #1 */
    bootp0 = (pxe_bootp_t *)(pxe->pkt0.data);
    bootp1 = (pxe_bootp_t *)(pxe->pkt1.data);

    ret = dhcp_unpack_packet(bootp0, pxe->pkt0.len, opts);
    if (ret) {
	error("Could not unpack packet\n");
	return -ret;	/* dhcp_unpack_packet always returns positive errors */
    }

    ret = pxechn_parse_args(argc, argv, pxe, opts);
    bootp1->sip = pxe->fip;
    opts[67].len = strlen(pxe->fp);
    opts[67].data = pxe->fp;
    opts[66].len = strlen(pxe->host);
    opts[66].data = pxe->host;

    ret = dhcp_pack_packet(bootp1, (size_t *)&(pxe->pkt1.len), opts);

    return ret;
}

/* dhcp_copy_pkt_to_pxe: Copy packet to PXE's BC data for a ptype packet
 *	Input:
 *	p	Packet data to copy
 *	len	length of data to copy
 *	ptype	Packet type to overwrite
 */
int dhcp_copy_pkt_to_pxe(pxe_bootp_t *p, size_t len, int ptype)
{
//FIXME::HERE
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

/* pxechn: Chainload to new PXE file ourselves
 *	Input:
 *	argc	Count of arguments passed
 *	argv	Values of arguments passed
 *	Returns	0 on success (which should never happen)
 *		1 on loadfile() error
 *		2 if DHCP Option 52 (Option Overload) used file field
 *		-1 on usage error
 */
int pxechn(int argc, char *argv[])
{
    struct pxelinux_opt pxe;
    pxe_bootp_t *bootp0, *bootp1;
    int rv = 0;
    struct data_area file;
    struct syslinux_rm_regs regs;

    pxechn_init(&pxe);
    bootp0 = (pxe_bootp_t *)(pxe.pkt0.data);
    bootp1 = (pxe_bootp_t *)(pxe.pkt1.data);

    /* Parse arguments and patch packet 1 */
    pxechn_args(argc, argv, &pxe);
    dpressanykey();
    pxe_set_regs(&regs);
    /* Load the file late; it's the most time-expensive operation */
    printf("%s: Attempting to load '%s': ", app_name_str, pxe.fn);
    if (loadfile(pxe.fn, &file.data, &file.size)) {
	pxe_error(errno, NULL, NULL);
	rv = -2;
	goto ret;
    }
    puts("loaded.");
    /* we'll be shuffling to the standard location of 7C00h */
    file.base = 0x7C00;
//FIXME::HERE
    rv = dhcp_copy_pkt_to_pxe(bootp1, pxe.pkt1.len,
			      PXENV_PACKET_TYPE_CACHED_REPLY);
    dprint_pxe_bootp_t((pxe_bootp_t *)(pxe.pkt1.data), pxe.pkt1.len);
    if (pxe.wait) {
	pressanykey();
    } else {
	dpressanykey();
    }
    if (true) {
	puts("  Attempting to boot...");
	do_boot(&file, 1, &regs);
    }
    /* If failed, copy backup back in and abort */
    dhcp_copy_pkt_to_pxe(bootp0, pxe.pkt0.len, PXENV_PACKET_TYPE_CACHED_REPLY);
ret:
    return rv;
}

/* pxe_restart: Restart the PXE environment with a new PXE file
 *	Input:
 *	ifn	Name of file to chainload to in a format PXELINUX understands
 *		This must strictly be TFTP or relative file
 */
int pxe_restart(char *ifn)
{
    int rv = 0;
    struct pxelinux_opt pxe;
    com32sys_t reg;
    t_PXENV_RESTART_TFTP *pxep;	/* PXENV callback Parameter */

    pxe.fn = ifn;
    pxechn_fill_pkt(&pxe);
    if (pxe.pkt1.data)
	pxe.fip = ( (pxe_bootp_t *)(pxe.pkt1.data) )->sip;
    else
	pxe.fip = 0;
    rv = pxechn_parse_fn(pxe.fn, &(pxe.fip), pxe.host, &(pxe.fp));
    if ((rv > 2) || (rv < 0)) {
	printf("%s: ERROR: Unparsable filename argument: '%s'\n\n", app_name_str, pxe.fn);
	goto ret;
    }
    printf("  Attempting to boot '%s'...\n\n", pxe.fn);
    memset(&reg, 0, sizeof reg);
    if (sizeof(t_PXENV_TFTP_READ_FILE) <= __com32.cs_bounce_size) {
	pxep = __com32.cs_bounce;
	memset(pxep, 0, sizeof(t_PXENV_RESTART_TFTP));
    } else if (!(pxep = lzalloc(sizeof(t_PXENV_RESTART_TFTP)))){
	dprintf("Unable to lzalloc() for PXE call structure\n");
	goto ret;
    }
    pxep->Status = PXENV_STATUS_SUCCESS;	/* PXENV_STATUS_FAILURE */
    strcpy((char *)pxep->FileName, ifn);
    pxep->BufferSize = 0x8000;
    pxep->Buffer = (void *)0x7c00;
    pxep->ServerIPAddress = pxe.fip;
    dprintf("FN='%s'  %08X %08X %08X %08X\n\n", (char *)pxep->FileName,
	pxep->ServerIPAddress, (unsigned int)pxep,
	pxep->BufferSize, (unsigned int)pxep->Buffer);
    dprintf("PXENV_RESTART_TFTP status %d\n", pxep->Status);
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = PXENV_RESTART_TFTP;
    reg.edi.w[0] = OFFS(pxep);
    reg.es = SEG(pxep);

    __intcall(0x22, &reg, &reg);

    printf("PXENV_RESTART_TFTP returned %d\n", pxep->Status);
    if (pxep != __com32.cs_bounce)
	lfree(pxep);

ret:
    return rv;
}

/* pxechn_gpxe: Use gPXE to chainload a new NBP
 * If it's around, don't bother with the heavy lifting ourselves
 *	Input:
 *	argc	Count of arguments passed
 *	argv	Values of arguments passed
 *	Returns	0 on success (which should never happen)
 *		1 on loadfile() error
 *		-1 on usage error
 */
//FIXME:Implement
int pxechn_gpxe(int argc, char *argv[])
{
    int rv = 0;
    struct pxelinux_opt pxe;

    if (argc) {
	printf("%s\n", argv[0]);
	pxechn_args(argc, argv, &pxe);
    }
    return rv;
}

int main(int argc, char *argv[])
{
    int rv= -1;
int err;
    const struct syslinux_version *sv;

    /* Initialization */
err = errno;
    console_ansi_raw();
printf("%d %d\n", err, errno);
    errno = err;
    sv = syslinux_version();
    if (sv->filesystem != SYSLINUX_FS_PXELINUX) {
	printf("%s: May only run in PXELINUX\n", app_name_str);
	argc = 1;	/* prevents further processing to boot */
/*    } else if (is_gpxe()) {
	rv = pxechn_gpxe(argc - 1, &argv[1]);
	if (rv >= 0)
	    argc = 1;*/
    }
    if (argc == 2) {
	if ((strcasecmp(argv[1], "-h") == 0) || ((strcmp(argv[1], "-?") == 0))
		|| (strcasecmp(argv[1], "--help") == 0)) {
	    argc = 1;
	} else {
	    rv = pxechn(argc - 1, &argv[1]);
	}
    } else if (argc >= 3) {
	if ((strcmp(argv[1], "-r") == 0)) {
	    if (argc == 3)
		rv = pxe_restart(argv[2]);
	} else {
	    rv = pxechn(argc - 1, &argv[1]);
	}
    }
    if (rv <= -1 ) {
	usage();
	rv = 1;
    }
    return rv;
}
