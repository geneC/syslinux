/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
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
 * shuffle.c
 *
 * Common code for "shuffle and boot" operation; generates a shuffle list
 * and puts it in the bounce buffer.  Returns the number of shuffle
 * descriptors.
 */

#include <stdlib.h>
#include <inttypes.h>
#include <com32.h>
#include <syslinux/movebits.h>

struct shuffle_descriptor {
  uint32_t dst, src, len;
};

int syslinux_prepare_shuffle(struct syslinux_movelist *fraglist)
{
  struct syslinux_movelist *moves = NULL, *freelist = NULL, *mp;
  int n;
  struct shuffle_descriptor *dp;
  int np, rv = -1;

  freelist = syslinux_memory_map();
  if (!freelist)
    goto bail;

  if (syslinux_compute_movelist(&moves, fraglist, freelist))
    goto bail;

  dp = __com32.cs_bounce;
  np = 0;

  for (mp = moves; mp; mp = mp->next) {
    if (np >= 65536/12)
      goto bail;		/* Way too many descriptors... */
    
    dp->dst = mp->dst;
    dp->src = mp->src;
    dp->len = mp->len;
    np++;
  }

  rv = np;

 bail:
  if (freelist)
    syslinux_free_movelist(freelist);
  if (moves)
    syslinux_free_movelist(moves);

  return rv;
}

