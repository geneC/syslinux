/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * read.c
 *
 * Reading from a file descriptor
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <klibc/compiler.h>
#include "file.h"

ssize_t read(int fd, void *buf, size_t count)
{
    char *datap = buf;
    struct file_info *fp = &__file_info[fd];
    size_t n0, n1;

    if (fd >= NFILES || !fp->iop) {
	errno = EBADF;
	return -1;
    }

    if (__unlikely(!count))
	return 0;

    n0 = min(fp->i.unread_bytes, count);
    if (__unlikely(n0)) {
	memcpy(datap, unread_data(fp), n0);
	fp->i.unread_bytes -= n0;
	count -= n0;
	datap += n0;
	if (!count)
	    return n0;
    }

    n1 = fp->iop->read(fp, datap, count);

    if (n1 == -1 && n0 > 0)
	return n0;
    else
	return n0 + n1;
}
