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
#  define dprintf	printf
#else
#  define dprintf(f, ...)	((void)0)
#endif

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
    char *fn;	/* Filename as passed to us */
    uint32_t fip;	/* fn's IP component */
    char *fp;	/* fn's path component */
    char *cfg;
    char *prefix;
    uint32_t reboot;
    struct payload pkt0, pkt1;
};

/* BEGIN per pxespec.pdf */
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned long UINT32;
typedef signed char INT8;
typedef signed short INT16;
typedef signed long INT32;
typedef UINT16 PXENV_STATUS;
typedef UINT16 UDP_PORT;
typedef UINT32 ADDR32;
typedef UINT16 OFF16;
typedef UINT16 SEGSEL;

#define IP_ADDR_LEN 4
typedef union u_IP4 {
    UINT32 num;
    UINT8 array[IP_ADDR_LEN];
} IP4;

typedef struct s_PXENV_TFTP_READ_FILE {
    PXENV_STATUS Status;
    UINT8 FileName[128];
    UINT32 BufferSize;
    ADDR32 Buffer;
    IP4 ServerIPAddress;
    IP4 GatewayIPAddress;
    IP4 McastIPAddress;
    UDP_PORT TFTPClntPort;
    UDP_PORT TFTPSrvPort;
    UINT16 TFTPOpenTimeOut;
    UINT16 TFTPReopenDelay;
} t_PXENV_TFTP_READ_FILE;

typedef struct s_SEGOFF16 {
    OFF16 offset;
    SEGSEL segment;
} SEGOFF16;

#define PXENV_PACKET_TYPE_CACHED_REPLY 3
typedef struct s_PXENV_GET_CACHED_INFO {
    PXENV_STATUS Status;
    UINT16 PacketType;
    UINT16 BufferSize;
    SEGOFF16 Buffer;
    UINT16 BufferLimit;
} t_PXENV_GET_CACHED_INFO;
/* END per pxespec.pdf */

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
goto return1;
    syslinux_shuffle_boot_rm(mlist, mmap, 3, regs);
    error("Chainboot failed!\n");
    goto return1;
return1:
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

void pxechain_parse_fn(char fn[], uint32_t *fip, char *fp[])
{
}

void pxechain_args(int argc, char *argv[], struct pxelinux_opt *pxe)
{
    /* Init for paranoia */
    pxe->fn = pxe->fp = pxe->cfg = pxe->prefix = NULL;
    pxe->reboot = REBOOT_TIME;
    pxe->pkt0.d = pxe->pkt1.d = NULL;
    pxe->pkt0.s = pxe->pkt1.s = 0;
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
}

/* pxechain: Chainload to new PXE file ourselves
 *	Input:
 *	argc	Count of arguments passed
 *	argv	Values of arguments passed
 *	Returns	0 on success (which should never happen)
 *		1 on loadfile() error
 */
int pxechain(int argc, char *argv[])
{
    struct pxelinux_opt pxe;
    int rv = 0;
    struct data_area file;
    struct syslinux_rm_regs regs;

//     --parse-options
    pxechain_args(argc, argv, &pxe);
//     --make 2 copies of cache packet #3
//     --rebuild copy #1 applying new options in order ensuring an option is only specified once in patched packet
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
//     --try boot
    if (true) {
	puts("  Attempting to boot...");
	do_boot(&file, 1, &regs);
    }
//     --if failed, copy backup back in and abort
    puts("temp abort");
ret:
    return rv;
}

/* pxe_restart: Restart the PXE environment with a new PXE file
 *	Input:
 *	filename	Name of file to chainload to in a format PXELINUX understands
 *		This must strictly be TFTP
 */
int pxe_restart(const char *filename)
{
    int rv = 0;
    puts(filename);
    goto ret;
ret:
    return rv;
}

int main(int argc, char *argv[])
{
    int rv;
    const struct syslinux_version *sv;

    /* Initialization */
    rv = -1;
    openconsole(&dev_null_r, &dev_stdcon_w);
//     console_ansi_raw();
    sv = syslinux_version();
    if (sv->filesystem != SYSLINUX_FS_PXELINUX) {
	printf("%s: May only run in PXELINUX\n", app_name_str);
	argc = 1;	/* prevents further processing to boot */
    } else if (isgpxe()) {
	rv = pxechain_gpxe(argc - 1, &argv[1]);
	argc = 1;
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
    return rv;
}
