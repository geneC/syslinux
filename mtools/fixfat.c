/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Gene Cumm - All Rights Reserved
 *   Portions copied/derived from syslinux:libfat/open.c
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *   Portions copied/derived from syslinux:mtools/syslinux.c
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * fixfat.c - Linux program to forcibly fix the count of sectors
 *
 * returns 0 on success
 *	1 on command line issue
 *	2 on error checking FAT boot sector
 *	3 when the FAT needs to be fixed and can be fixed by this tool
 */

#define _GNU_SOURCE
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <mntent.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "syslinux.h"
#include "libfat.h"
#include "setadv.h"
#include "syslxopt.h"

#include "libfatint.h"
#include "ulint.h"

/* FIXME: Double documenting returns.  Recommendations? */
/* Success */
#define FIXFAT_RET_OK	0
/* Bad command line option(s) */
#define FIXFAT_RET_CMD	1
/* Error in FAT/file beyond scope of this tool; includes permissions or bad file name */
#define FIXFAT_RET_FAT	2
/* FAT needs to be fixed and can be fixed */
#define FIXFAT_RET_FIX	3

int goffset = 0;

/* BEGIN FROM mtools/syslinux.c */

char *program;			/* Name of program */

void __attribute__ ((noreturn)) die(const char *msg)
{
    fprintf(stderr, "%s: %s\n", program, msg);
    exit(1);
}

void __attribute__ ((noreturn)) die_err(const char *msg)
{
    fprintf(stderr, "%s: %s: %s\n", program, msg, strerror(errno));
    exit(1);
}

/*
 * read/write wrapper functions
 */
ssize_t xpread(int fd, void *buf, size_t count, off_t offset)
{
    char *bufp = (char *)buf;
    ssize_t rv;
    ssize_t done = 0;

    while (count) {
	rv = pread(fd, bufp, count, offset);
	if (rv == 0) {
	    die("short read");
	} else if (rv == -1) {
	    if (errno == EINTR) {
		continue;
	    } else {
		die(strerror(errno));
	    }
	} else {
	    bufp += rv;
	    offset += rv;
	    done += rv;
	    count -= rv;
	}
    }

    return done;
}

ssize_t xpwrite(int fd, const void *buf, size_t count, off_t offset)
{
    const char *bufp = (const char *)buf;
    ssize_t rv;
    ssize_t done = 0;

    while (count) {
	rv = pwrite(fd, bufp, count, offset);
	if (rv == 0) {
	    die("short write");
	} else if (rv == -1) {
	    if (errno == EINTR) {
		continue;
	    } else {
		die(strerror(errno));
	    }
	} else {
	    bufp += rv;
	    offset += rv;
	    done += rv;
	    count -= rv;
	}
    }

    return done;
}

/*
 * Version of the read function suitable for libfat
 */
int libfat_xpread(intptr_t pp, void *buf, size_t secsize,
		  libfat_sector_t sector)
{
    off_t offset = (off_t) sector * secsize + goffset;
    return xpread(pp, buf, secsize, offset);
}

/* END FROM mtools/syslinux.c */

void fixfat_usage(int usetype, FILE *outdev, const char *argv0)
{
    switch(usetype){
    case 0:
	fprintf(outdev, "%s: Test/Fix sector count in a FAT filesystem\n"
	    "Usage: %s [options] <FILE>\n"
	    "  -h    Help\n"
	    "  -f    Fix a FAT\n"
	    "  -o #  Use offset into file/device\n",
	    argv0, argv0);
    }
}

/* Much is copied from libfat */

int check_fix_fat_sector(int force, const char *fn)
{
    struct libfat_filesystem *fs = NULL;
    struct fat_bootsect *bs;
    int i, fd = 0, rv = 0;
    uint32_t sectors, fatsize, minfatsize, rootdirsize;
    uint32_t nclusters;

    fs = malloc(sizeof(struct libfat_filesystem));
    if (!fs) {
	fprintf(stderr, "Error allocating memory: %d-%s\n", errno, strerror(errno));
	goto abort;
    }
    fd = open(fn, O_RDWR);
    if (fd <= 0) {
	fprintf(stderr, "Error opening %s: %d-%s\n", fn, errno, strerror(errno));
	goto abort;
    }

    fs->sectors = NULL;
    fs->read = libfat_xpread;
    fs->readptr = fd;

    bs = libfat_get_sector(fs, 0);
    if (!bs) {
	fprintf(stderr, "Error fetching sector 0 at offset %d: %d-%s\n",
	    goffset, errno, strerror(errno));
	goto abort;
    }

    if (read16(&bs->bsBytesPerSec) != LIBFAT_SECTOR_SIZE) {
	fprintf(stderr, "Sector Size is non-standard\n");
	goto abort;
    }
    for (i = 0; i <= 8; i++) {
	if ((uint8_t) (1 << i) == read8(&bs->bsSecPerClust))
	    break;
    }
    if (i > 8) {
	fprintf(stderr, "Too many sectors per cluster\n");
	goto abort;
    }

    fs->clustsize = 1 << i;	/* Treat 0 as 2^8 = 64K */
    fs->clustshift = i;

    sectors = read16(&bs->bsSectors);
    if (!sectors)
	sectors = read32(&bs->bsHugeSectors);

    fs->end = sectors;

    fs->fat = read16(&bs->bsResSectors);
    fatsize = read16(&bs->bsFATsecs);
    if (!fatsize)
	fatsize = read32(&bs->u.fat32.bpb_fatsz32);

    fs->rootdir = fs->fat + fatsize * read8(&bs->bsFATs);

    rootdirsize = ((read16(&bs->bsRootDirEnts) << 5) + LIBFAT_SECTOR_MASK)
	>> LIBFAT_SECTOR_SHIFT;
    fs->data = fs->rootdir + rootdirsize;

    /* Sanity checking */
    if (fs->data >= fs->end) {
	fprintf(stderr, "Data extends beyond end\n");
	goto abort;
    }

    /* Figure out how many clusters */
    nclusters = (fs->end - fs->data) >> fs->clustshift;
    fs->endcluster = nclusters + 2;

    if (nclusters <= 0xff4) {
	fs->fat_type = FAT12;
	minfatsize = fs->endcluster + (fs->endcluster >> 1);
    } else if (nclusters <= 0xfff4) {
	fs->fat_type = FAT16;
	minfatsize = fs->endcluster << 1;
    } else if (nclusters <= 0xffffff4) {
	fs->fat_type = FAT28;
	minfatsize = fs->endcluster << 2;
    } else {
	fprintf(stderr, "Too many clusters\n");
	goto abort;
    }

    minfatsize = (minfatsize + LIBFAT_SECTOR_SIZE - 1) >> LIBFAT_SECTOR_SHIFT;

    if (minfatsize > fatsize) {
	printf("FAT reports %d but needs %d\n", fatsize, minfatsize);
	if (force) {
	    uint32_t onclusters = nclusters;
	    off_t bsoff;
	    minfatsize = fatsize << LIBFAT_SECTOR_SHIFT;
	    switch(fs->fat_type){
	    case FAT12:
		/* FIXME: Is there a better way to do this without division? */
		fs->endcluster = (minfatsize << 1) / 3;
		break;
	    case FAT16:
		fs->endcluster = minfatsize >> 1;
		break;
	    case FAT28:
		fs->endcluster = minfatsize >> 2;
		break;
	    }
	    fs->end = ((fs->endcluster - 2) << fs->clustshift) + fs->data;
	    /* This concludes the attempt to adjust values */

	    /* Sanity check on the AMS */
	    if (fs->data >= fs->end)
		goto abort;

	    /* Adjust based on adjusted value */
	    nclusters = (fs->end - fs->data) >> fs->clustshift;
	    fs->endcluster = nclusters + 2;

	    /* Check */
	    switch(fs->fat_type){
	    case FAT12:
		minfatsize = fs->endcluster + (fs->endcluster >> 1);
		break;
	    case FAT16:
		minfatsize = fs->endcluster << 1;
		break;
	    case FAT28:
		minfatsize = fs->endcluster << 2;
		break;
	    }

	    minfatsize = (minfatsize + LIBFAT_SECTOR_SIZE - 1) >> LIBFAT_SECTOR_SHIFT;
	    if (minfatsize > fatsize)
		goto abort;
	    printf("FS End was %8d; try %8lu; was %8u clusters; to %8u\n",
		sectors, (uint64_t)fs->end, onclusters, nclusters);
	    /*
	     * (uint64_t)((sectors - fs->data) >> fs->clustshift)
	     * (uint64_t)((fs->end - fs->data) >> fs->clustshift)
	     */
	    /* write it out */
	    sectors = read16(&bs->bsSectors);
	    if (!sectors) {
		sectors = fs->end & 0xffffffff;
		bsoff = goffset + ((void *)&bs->bsHugeSectors - (void *)bs);
		if (xpwrite(fd, &sectors, 4, bsoff) != 4) {
		    fprintf(stderr, "Failed to write bsHugeSectors\n");
		    goto abort;
		}
	    } else {
		sectors = fs->end & 0xffff;
		bsoff = goffset + ((void *)&bs->bsSectors - (void *)bs);
		if (xpwrite(fd, &sectors, 2, bsoff) != 2) {
		    fprintf(stderr, "Failed to write bsSectors\n");
		    goto abort;
		}
	    }
	    printf("Fixed\n");
	} else {
	    rv = FIXFAT_RET_FIX;
	    goto abort;
	}
    } else {
	if (force) {
	    fprintf(stderr, "FAT seems ok with %d; Force without need\n", fatsize);
	    rv = FIXFAT_RET_CMD;
	    goto abort;
	}
	printf("FAT seems ok with %d\n", fatsize);
    }

    close(fd);
    free(fs);
    return 0;


abort:
    if (fs)
	free(fs);
    if (fd)
	close(fd);
    if (!rv)
	rv = FIXFAT_RET_FAT;
    return rv;
}

int main(int argc, char *argv[])
{
    int opt, force = 0;
    char *optstring = "hfo:";
    while ((opt = getopt(argc, argv, optstring)) != EOF) {
	switch (opt) {
	case 'h':
	    fixfat_usage(0, stdout, argv[0]);
	    return 0;
	    break;
	case 'f':
	    force = 1;
	    break;
	case 'o':
	    goffset = atoi(optarg);
	    break;
	default:
	    fixfat_usage(0, stderr, argv[0]);
	    return 1;
	    break;
	}
    }
    if (optind >= argc) {
	fprintf(stderr, "ERROR: Please specify a file\n");
	fixfat_usage(0, stderr, argv[0]);
	return 1;
    }
    program = argv[0];
    return check_fix_fat_sector(force, argv[optind]);
}
