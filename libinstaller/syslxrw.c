/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corp. - All Rights Reserved
 *   Copyright 2015 Paulo Alcantara <pcacjr@zytor.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "syslxrw.h"

static void die(const char *msg)
{
    fputs(msg, stderr);
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
