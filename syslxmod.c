#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 1998-2003 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslxmod.c - Code to provide a SYSLINUX code set to an installer.
 */

#define _XOPEN_SOURCE 500	/* Required on glibc 2.x */
#define _BSD_SOURCE
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "syslinux.h"

enum bs_offsets {
  bsJump          = 0x00,
  bsOemName       = 0x03,
  bsBytesPerSec   = 0x0b,
  bsSecPerClust   = 0x0d,
  bsResSectors    = 0x0e,
  bsFATs          = 0x10,
  bsRootDirEnts   = 0x11,
  bsSectors       = 0x13,
  bsMedia         = 0x15,
  bsFATsecs       = 0x16,
  bsSecPerTrack   = 0x18,
  bsHeads         = 0x1a,
  bsHiddenSecs    = 0x1c,
  bsHugeSectors   = 0x20,
  bsDriveNumber   = 0x24,
  bsReserved1     = 0x25,
  bsBootSignature = 0x26,
  bsVolumeID      = 0x27,
  bsVolumeLabel   = 0x2b,
  bsFileSysType   = 0x36,
  bsCode          = 0x3e,
  bsSignature     = 0x1fe
};

#define bsHead      bsJump
#define bsHeadLen   (bsBytesPerSec-bsHead)
#define bsCodeLen   (bsSignature-bsCode)

/*
 * Access functions for littleendian numbers, possibly misaligned.
 */
static inline u_int16_t get_16(unsigned char *p)
{
  return (u_int16_t)p[0] + ((u_int16_t)p[1] << 8);
}

static inline u_int32_t get_32(unsigned char *p)
{
  return (u_int32_t)p[0] + ((u_int32_t)p[1] << 8) +
    ((u_int32_t)p[2] << 16) + ((u_int32_t)p[3] << 24);
}

static inline void set_16(unsigned char *p, u_int16_t v)
{
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
}

#if 0				/* Not needed */
static inline void set_32(unsigned char *p, u_int32_t v)
{
  p[0] = (v & 0xff);
  p[1] = ((v >> 8) & 0xff);
  p[2] = ((v >> 16) & 0xff);
  p[3] = ((v >> 24) & 0xff);
}
#endif

/* Patch the code so that we're running in stupid mode */
void syslinux_make_stupid(void)
{
  /* Access only one sector at a time */
  set_16(syslinux_ldlinux+PATCH_OFFSET, 1);
}
  
void syslinux_make_bootsect(void *bs)
{
  unsigned char *bootsect = bs;

  memcpy(bootsect+bsHead, syslinux_bootsect+bsHead, bsHeadLen);
  memcpy(bootsect+bsCode, syslinux_bootsect+bsCode, bsCodeLen);
}
