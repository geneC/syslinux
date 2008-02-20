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
 * refstr.c
 *
 * Simple reference-counted strings
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "refstr.h"

const char *refstrndup(const char *str, size_t len)
{
  char *r;

  if (!str)
    return NULL;

  len = strnlen(str, len);
  r = malloc(sizeof(unsigned int)+len+1);
  *(unsigned int *)r = 1;
  r += sizeof(unsigned int);
  memcpy(r, str, len);
  r[len] = '\0';
  return r;
}

const char *refstrdup(const char *str)
{
  char *r;
  size_t len;

  if (!str)
    return NULL;

  len = strlen(str);
  r = malloc(sizeof(unsigned int)+len+1);
  *(unsigned int *)r = 1;
  r += sizeof(unsigned int);
  memcpy(r, str, len);
  r[len] = '\0';
  return r;
}

int vrsprintf(const char **bufp, const char *fmt, va_list ap)
{
  va_list ap1;
  int bytes;
  char *p;

  va_copy(ap1, ap);
  bytes = vsnprintf(NULL, 0, fmt, ap1)+1;
  va_end(ap1);

  p = malloc(bytes+sizeof(unsigned int));
  if ( !p ) {
    *bufp = NULL;
    return -1;
  }

  *(unsigned int *)p = 1;
  p += sizeof(unsigned int);
  *bufp = p;

  return vsnprintf(p, bytes, fmt, ap);
}

int rsprintf(const char **bufp, const char *fmt, ...)
{
  int rv;
  va_list ap;

  va_start(ap, fmt);
  rv = vrsprintf(bufp, fmt, ap);
  va_end(ap);

  return rv;
}

void refstr_put(const char *r)
{
  unsigned int *ref;

  if (r) {
    ref = (unsigned int *)r - 1;

    if (!--*ref)
      free(ref);
  }
}
