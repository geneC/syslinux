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

/* Device structure; contains the relevant operations */

struct file_info;

#define __DEV_MAGIC	0xf4e7
#define __DEV_TTY	0x0001	/* TTY - must be bit 0 */
#define __DEV_FILE	0x0002	/* Ordinary file */
struct dev_info {
  uint16_t dev_magic;		/* Magic number */
  uint16_t flags;		/* Flags */
  int fileflags;		/* Permitted file flags */
  ssize_t (*read)(struct file_info *, void *, size_t);
  ssize_t (*write)(struct file_info *, const void *, size_t);
  int (*close)(struct file_info *);
};

/* File structure */

#define NFILES 32		/* Number of files to support */
#define MAXBLOCK 16384		/* Defined by ABI */

struct file_info {
  const struct dev_info *ops;	/* Operations structure */

  union {
    /* Structure used for ordinary files */
    struct {
      int blocklg2;		/* Blocksize log 2 */
      size_t offset;		/* Current file offset */
      size_t length;		/* Total file length */
      uint16_t filedes;		/* File descriptor */
      uint16_t _filler;		/* Unused */
      size_t nbytes;		/* Number of bytes available in buffer */
      char *datap;		/* Current data pointer */
      char buf[MAXBLOCK];
    } f;
  } p;
};

extern struct file_info __file_info[NFILES];

#endif /* _COM32_SYS_FILE_H */
