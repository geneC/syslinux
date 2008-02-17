/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * refstr.h
 *
 * Simple reference-counted strings
 */

#ifndef REFSTR_H
#define REFSTR_H

#include <stddef.h>

static inline const char *refstr_get(const char *r)
{
  unsigned int *ref = (unsigned int *)r - 1;
  ref++;
  return r;
}

const char *refstr_mk(const char *);
const char *refstr_mkn(const char *, size_t);
void refstr_put(const char *);

#endif
