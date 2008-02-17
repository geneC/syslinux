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
#include "refstr.h"

const char *refstr_mkn(const char *str, size_t len)
{
  char *r;

  len = strnlen(str, len);
  r = malloc(sizeof(unsigned int)+len+1);
  *(unsigned int *)r = 1;
  r += sizeof(unsigned int);
  memcpy(r, str, len);
  r[len] = '\0';
  return r;
}

const char *refstr_mk(const char *str)
{
  refstr *r;
  size_t len;

  len = strlen(str);
  r = malloc(sizeof(unsigned int)+len+1);
  *(unsigned int *)r = 1;
  r += sizeof(unsigned int);
  memcpy(r, str, len);
  r[len] = '\0';
  return r;
}

void refstr_put(const char *r)
{
  unsigned int *ref = (unsigned int *)r - 1;

  if (!--*ref)
    free(ref);
}
