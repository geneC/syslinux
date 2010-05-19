/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corp. - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslxopt.c
 *
 * parse cmdline for extlinux and syslinux installer
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <getopt.h>
#include <sysexits.h>
#include "../version.h"
#include "syslxcom.h"
#include "syslxopt.h"

/* These are the options we can set their values */
struct sys_options opt = {
    .sectors = 0,
    .heads = 0,
    .raid_mode = 0,
    .stupid_mode = 0,
    .reset_adv = 0,
    .set_once = NULL,
    .update_only = -1,
    .directory = NULL,
    .device = NULL,
    .offset = 0,
};

const struct option long_options[] = {
    {"install", 0, NULL, 'i'},
    {"directory", 1, NULL, 'd'},
    {"offset", 1, NULL, 'f'},
    {"update", 0, NULL, 'U'},
    {"zipdrive", 0, NULL, 'z'},
    {"sectors", 1, NULL, 'S'},
    {"stupid", 0, NULL, 's'},
    {"heads", 1, NULL, 'H'},
    {"raid-mode", 0, NULL, 'r'},
    {"version", 0, NULL, 'v'},
    {"help", 0, NULL, 'h'},
    {"once", 1, NULL, 'o'},
    {"clear-once", 0, NULL, 'O'},
    {"reset-adv", 0, NULL, OPT_RESET_ADV},
    {0, 0, 0, 0}
};

const char short_options[] = "id:f:UuzS:H:rvho:O";

void __attribute__ ((noreturn)) usage(int rv, int mode)
{
    if (mode) /* for unmounted fs installation */
	fprintf(stderr,
	    "Usage: %s [options] device\n"
	    "  --offset     -f Offset of the file system on the device \n"
	    "  --directory  -d  Directory for installation target\n",
	    program);
    else /* actually extlinux can also use -d to provide directory too */
	fprintf(stderr,
	    "Usage: %s [options] directory\n",
	    program);
    fprintf(stderr,
	    "  --install    -i  Install over the current bootsector\n"
	    "  --update     -U  Update a previous EXTLINUX installation\n"
	    "  --zip        -z  Force zipdrive geometry (-H 64 -S 32)\n"
	    "  --sectors=#  -S  Force the number of sectors per track\n"
	    "  --heads=#    -H  Force number of heads\n"
	    "  --stupid     -s  Slow, safe and stupid mode\n"
	    "  --raid       -r  Fall back to the next device on boot failure\n"
	    "  --once=...   -o  Execute a command once upon boot\n"
	    "  --clear-once -O  Clear the boot-once command\n"
	    "  --reset-adv      Reset auxilliary data\n"
	    "\n"
	    "  Note: geometry is determined at boot time for devices which\n"
	    "  are considered hard disks by the BIOS.  Unfortunately, this is\n"
	    "  not possible for devices which are considered floppy disks,\n"
	    "  which includes zipdisks and LS-120 superfloppies.\n"
	    "\n"
	    "  The -z option is useful for USB devices which are considered\n"
	    "  hard disks by some BIOSes and zipdrives by other BIOSes.\n"
	    );

    exit(rv);
}

void parse_options(int argc, char *argv[], int mode)
{
    int o;

    program = argv[0];
    while ((o = getopt_long(argc, argv, short_options,
			    long_options, NULL)) != EOF) {
	switch (o) {
	case 'z':
	    opt.heads = 64;
	    opt.sectors = 32;
	    break;
	case 'S':
	    opt.sectors = strtoul(optarg, NULL, 0);
	    if (opt.sectors < 1 || opt.sectors > 63) {
		fprintf(stderr,
			"%s: invalid number of sectors: %u (must be 1-63)\n",
			program, opt.sectors);
		exit(EX_USAGE);
	    }
	    break;
	case 'H':
	    opt.heads = strtoul(optarg, NULL, 0);
	    if (opt.heads < 1 || opt.heads > 256) {
		fprintf(stderr,
			"%s: invalid number of heads: %u (must be 1-256)\n",
			program, opt.heads);
		exit(EX_USAGE);
	    }
	    break;
	case 'r':
	    opt.raid_mode = 1;
	    break;
	case 's':
	    opt.stupid_mode = 1;
	    break;
	case 'i':
	    opt.update_only = 0;
	    break;
	case 'u':
	case 'U':
	    opt.update_only = 1;
	    break;
	case 'h':
	    usage(0, mode);
	    break;
	case 'o':
	    opt.set_once = optarg;
	    break;
	case 'f':
	    opt.offset = strtoul(optarg, NULL, 0);
	case 'O':
	    opt.set_once = "";
	    break;
	case 'd':
	    opt.directory = optarg;
	case OPT_RESET_ADV:
	    opt.reset_adv = 1;
	    break;
	case 'v':
	    fputs(program, stderr);
	    fputs(" " VERSION_STR
		  "  Copyright 1994-" YEAR_STR " H. Peter Anvin \n", stderr);
	    exit(0);
	default:
	    usage(EX_USAGE, mode);
	}
    }
    if (mode)
	opt.device = argv[optind];
    else if (!opt.directory)
	opt.directory = argv[optind];
}
