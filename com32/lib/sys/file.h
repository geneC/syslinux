#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2003 H. Peter Anvin - All Rights Reserved
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
 * file.h
 *
 * Internal implementation of file I/O for COM32
 */

#ifndef _COM32_SYS_FILE_H
#define _COM32_SYS_FILE_H

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>

/* Ordinary file */

#define NFILES 32		/* Number of files to support */
#define MAXBLOCK 16384		/* Defined by ABI */

struct file_info {
  int blocklg2;			/* Blocksize log 2 */
  size_t offset;		/* Current file offset */
  size_t length;		/* Total file length */
  uint16_t filedes;		/* File descriptor */
  uint16_t _filler;		/* Unused */
  size_t nbytes;		/* Number of bytes available in buffer */
  char *datap;			/* Current data pointer */
  char buf[MAXBLOCK];
};

extern struct file_info __file_info[NFILES];

/* Special device (tty et al) */

#define __DEV_MAGIC	0x504af4e7
struct dev_info {
  uint32_t dev_magic;		/* Magic number */
  ssize_t (*read)(int, void *, size_t);
  ssize_t (*write)(int, const void *, size_t);
};

#endif /* _COM32_SYS_FILE_H */
