#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2001-2003 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * memdisk.h
 *
 * Miscellaneous header definitions
 */

#ifndef MEMDISK_H
#define MEMDISK_H

/* We use the com32 interface for calling 16-bit code */
#include <com32.h>

/* The real-mode segment */
#define LOW_SEG 0x0800

typedef void (*syscall_t)(uint8_t, com32sys_t *, com32sys_t *);
extern syscall_t syscall;
extern void *sys_bounce;

/* What to call when we're dead */
extern void __attribute__((noreturn)) die(void);

/* Standard routines */
#define memcpy(a,b,c) __builtin_memcpy(a,b,c)
#define memset(a,b,c) __builtin_memset(a,b,c)

/* Decompression */
void *unzip(void *indata, unsigned long zbytes, void *target);

#endif
