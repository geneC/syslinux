/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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

#ifndef DEBUG
# define DEBUG 0
#endif
#if DEBUG
# include <stdio.h>
# define dprintf printf
#else
# define dprintf(f, ...) ((void)0)
#endif

struct shuffle_descriptor {
  uint32_t dst, src, len;
};

int syslinux_prepare_shuffle(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap)
{
  struct syslinux_movelist *moves = NULL, *mp;
  struct syslinux_memmap *ml;
  struct shuffle_descriptor *dp;
  int np, rv = -1;

  if (syslinux_compute_movelist(&moves, fraglist, memmap))
    goto bail;

#if DEBUG
  dprintf("Final movelist:\n");
  syslinux_dump_movelist(stdout, moves);
#endif

  dp = __com32.cs_bounce;
  np = 0;

  /* Copy the move sequence into the bounce buffer */
  for (mp = moves; mp; mp = mp->next) {
    if (np >= 65536/12)
      goto bail;		/* Way too many descriptors... */

    dp->dst = mp->dst;
    dp->src = mp->src;
    dp->len = mp->len;
    dp++; np++;
  }

  /* Copy any zeroing operations into the bounce buffer */
  for (ml = memmap; ml->type != SMT_END; ml = ml->next) {
    if (ml->type == SMT_ZERO) {
      if (np >= 65536/12)
	goto bail;

      dp->dst = ml->start;
      dp->src = (addr_t)-1;	/* bzero this region */
      dp->len = ml->next->start - ml->start;
      dp++; np++;
    }
  }

  rv = np;

 bail:
  /* This is safe only because free() doesn't use the bounce buffer!!!! */
  if (moves)
    syslinux_free_movelist(moves);

  return rv;
}
