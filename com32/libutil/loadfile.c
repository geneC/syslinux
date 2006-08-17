/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2005 H. Peter Anvin - All Rights Reserved
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

#include "loadfile.h"

int loadfile(const char *filename, void **ptr, size_t *len)
{
  int fd;
  struct stat st;
  void *data;
  FILE *f;
  size_t xlen;

  fd = open(filename, O_RDONLY);
  if ( fd < 0 )
    return -1;

  f = fdopen(fd, "rb");
  if ( !f ) {
    close(fd);
    return -1;
  }

  if ( fstat(fd, &st) )
    goto err_fclose;

  *len = st.st_size;
  xlen = (st.st_size + LOADFILE_ZERO_PAD-1) & ~(LOADFILE_ZERO_PAD-1);

  *ptr = data = malloc(xlen);
  if ( !data )
    goto err_fclose;

  if ( (off_t)fread(data, 1, st.st_size, f) != st.st_size )
    goto err_free;

  memset((char *)data + st.st_size, 0, xlen-st.st_size);

  fclose(f);
  return 0;

 err_free:
  free(data);
 err_fclose:
  fclose(f);
  return -1;
}
