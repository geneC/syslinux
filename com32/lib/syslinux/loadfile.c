/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2005-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * loadfile.c
 *
 * Read the contents of a data file into a malloc'd buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <syslinux/loadfile.h>

#define INCREMENTAL_CHUNK 1024*1024

int loadfile(const char *filename, void **ptr, size_t *len)
{
  FILE *f;
  int rv;

  f = fopen(filename, "r");
  if ( !f )
    return -1;

  rv = floadfile(f, ptr, len, NULL, 0);
  fclose(f);

  return rv;
}

int floadfile(FILE *f, void **ptr, size_t *len, const void *prefix,
	      size_t prefix_len)
{
  struct stat st;
  void *data, *dp;
  size_t alen, clen, rlen, xlen;

  clen = alen = 0;
  data = NULL;

  if ( fstat(fileno(f), &st) )
    goto err;

  if (!S_ISREG(st.st_mode)) {
    /* Not a regular file, we can't assume we know the file size */
    if (prefix_len) {
      clen = alen = prefix_len;
      data = malloc(prefix_len);
      if (!data)
	goto err;

      memcpy(data, prefix, prefix_len);
    }

    do {
      alen += INCREMENTAL_CHUNK;
      dp = realloc(data, alen);
      if (!dp)
	goto err;
      data = dp;

      rlen = fread((char *)data+clen, 1, alen-clen, f);
      clen += rlen;
    } while (clen == alen);

    *len = clen;
    xlen = (clen + LOADFILE_ZERO_PAD-1) & ~(LOADFILE_ZERO_PAD-1);
    dp = realloc(data, xlen);
    if (dp)
      data = dp;
    *ptr = data;
  } else {
    *len = clen = st.st_size + prefix_len - ftell(f);
    xlen = (clen + LOADFILE_ZERO_PAD-1) & ~(LOADFILE_ZERO_PAD-1);

    *ptr = data = malloc(xlen);
    if ( !data )
      return -1;

    memcpy(data, prefix, prefix_len);

    if ( (off_t)fread((char *)data+prefix_len, 1, clen-prefix_len, f)
	 != clen-prefix_len )
      goto err;
  }

  memset((char *)data + clen, 0, xlen-clen);
  return 0;

 err:
  if (data)
    free(data);
  return -1;
}
