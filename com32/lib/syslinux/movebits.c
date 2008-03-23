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
 * movebits.c
 *
 * Utility function to take a list of memory areas to shuffle and
 * convert it to a set of shuffle operations.
 *
 * Note: a lot of the functions in this file deal with "parent pointers",
 * which are pointers to a pointer to a list, or part of the list.
 * They can be pointers to a variable holding the list root pointer,
 * or pointers to a next field of a previous entry.
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <setjmp.h>

#include <syslinux/movebits.h>

#ifdef TEST
# define DEBUG 1
#else
# define DEBUG 0
#endif

#if DEBUG
# include <stdio.h>
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

static jmp_buf new_movelist_bail;

static struct syslinux_movelist *
new_movelist(addr_t dst, addr_t src, addr_t len)
{
  struct syslinux_movelist *ml = malloc(sizeof(struct syslinux_movelist));

  if ( !ml )
    longjmp(new_movelist_bail, 1);

  ml->dst = dst;
  ml->src = src;
  ml->len = len;
  ml->next = NULL;

  return ml;
}

static struct syslinux_movelist *
dup_movelist(struct syslinux_movelist *src)
{
  struct syslinux_movelist *dst = NULL, **dstp = &dst, *ml;

  while (src) {
    ml = new_movelist(src->dst, src->src, src->len);
    *dstp = ml;
    dstp = &ml->next;
    src = src->next;
  }

  return dst;
}

/*
 * Take a chunk, entirely confined in **parentptr, and split it off so that
 * it has its own structure.
 */
static struct syslinux_movelist **
split_movelist(addr_t start, addr_t len, struct syslinux_movelist **parentptr)
{
  struct syslinux_movelist *m, *ml = *parentptr;

  assert(start >= ml->src);
  assert(start < ml->src+ml->len);

  /* Split off the beginning */
  if ( start > ml->src ) {
    addr_t l = start - ml->src;

    m = new_movelist(ml->dst+l, start, ml->len-l);
    m->next = ml->next;
    ml->len = l;
    ml->next = m;

    parentptr = &ml->next;
    ml = m;			/* Continue processing the new node */
  }

  /* Split off the end */
  if ( ml->len > len ) {
    addr_t l = ml->len - len;

    m = new_movelist(ml->dst+len, ml->src+len, l);
    m->next = ml->next;
    ml->len = len;
    ml->next = m;
  }

  return parentptr;
}

static void
delete_movelist(struct syslinux_movelist **parentptr)
{
  struct syslinux_movelist *o = *parentptr;
  *parentptr = o->next;
  free(o);
}

static void
free_movelist(struct syslinux_movelist **parentptr)
{
  while (*parentptr)
    delete_movelist(parentptr);
}

/*
 * Scan the freelist looking for a particular chunk of memory
 */
static struct syslinux_movelist **
is_free_zone(addr_t start, addr_t len, struct syslinux_movelist **space)
{
  struct syslinux_movelist *s;

  while ( (s = *space) ) {
    if ( s->src <= start && s->src+s->len >= start+len )
      return space;
    space = &s->next;
  }

  return NULL;
}

/*
 * Scan the freelist looking for the smallest chunk of memory which
 * can fit X bytes
 */
static struct syslinux_movelist **
free_area(addr_t len, struct syslinux_movelist **space)
{
  struct syslinux_movelist **best = NULL;
  struct syslinux_movelist *s;

  while ( (s = *space) ) {
    if ( s->len >= len ) {
      if ( !best || (*best)->len > s->len )
	best = space;
    }
    space = &s->next;
  }

  return best;
}


/*
 * Scan the freelist looking for the largest chunk of memory,
 * returns parent ptr
 */
static struct syslinux_movelist **
free_area_max(struct syslinux_movelist **space)
{
  struct syslinux_movelist **best = NULL;
  struct syslinux_movelist *s;

  while ( (s = *space) ) {
    if ( !best || s->len > (*best)->len )
      best = space;
    space = &s->next;
  }

  return best;
}

/*
 * Remove a chunk from the freelist
 */
static void
allocate_from(addr_t start, addr_t len, struct syslinux_movelist **parentptr)
{
  struct syslinux_movelist *c = *parentptr;

  assert(c->src <= start);
  assert(c->src+c->len >= start+len);

  if ( c->src != start || c->len != len ) {
    parentptr = split_movelist(start, len, parentptr);
    c = *parentptr;
  }

  *parentptr = c->next;
  free(c);
}

#if 0
/*
 * Remove any chunk from the freelist which is already
 * claimed by source data.  This somewhat frivolously
 * assumes that the fragments are either completely
 * covered by a free zone or not covered at all.
 */
static void
tidy_freelist(struct syslinux_movelist **frags,
	      struct syslinux_movelist **space)
{
  struct syslinux_movelist **sep;
  struct syslinux_movelist *f;

  while ( (f = *frags) ) {
    if ( (sep = is_free_zone(f->src, f->len, space)) )
      allocate_from(f->src, f->len, sep);
    frags = &f->next;
  }
}
#endif

/*
 * moves is computed from "frags" and "freemem".  "space" lists
 * free memory areas at our disposal, and is (src, cnt) only.
 */

int
syslinux_compute_movelist(struct syslinux_movelist **moves,
			  struct syslinux_movelist *ifrags,
			  struct syslinux_memmap *memmap)
{
  struct syslinux_memmap *mmap = NULL, *mm;
  struct syslinux_movelist *frags = NULL;
  struct syslinux_movelist *mv, *space;
  struct syslinux_movelist *f, **fp, **ep;
  struct syslinux_movelist *o, **op;
  addr_t needbase, needlen, copysrc, copydst, copylen;
  addr_t freebase, freelen;
  addr_t mstart;
  int m_ok;
  int rv = -1;

  dprintf("entering syslinux_compute_movelist()...\n");

  if (setjmp(new_movelist_bail))
    goto bail;

  *moves = NULL;

  /* Mark any memory that's in use by source material busy in the memory map */
  mmap = syslinux_dup_memmap(memmap);
  if (!mmap)
    goto bail;

  frags = dup_movelist(ifrags);

#if DEBUG
  dprintf("Initial memory map:\n");
  syslinux_dump_memmap(stdout, mmap);
#endif

  for (f = frags; f; f = f->next) {
    if (syslinux_add_memmap(&mmap, f->src, f->len, SMT_ALLOC))
      goto bail;
  }

#if DEBUG
  dprintf("Adjusted memory map:\n");
  syslinux_dump_memmap(stdout, mmap);
#endif

  /* Compute the freelist in movelist format. */
  space = NULL;
  ep = &space;
  mstart = -1;
  m_ok = 0;

  /* Yes, we really do want to run through the loop on SMT_END */
  for (mm = mmap; mm; mm = mm->next) {
    /* We can safely use to-be-zeroed memory as a scratch area. */
    if (mmap->type == SMT_FREE || mmap->type == SMT_ZERO) {
      if (!m_ok) {
	m_ok = 1;
	mstart = mmap->start;
      }
    } else {
      if (m_ok) {
	f = new_movelist(0, mstart, mmap->start - mstart);
	*ep = f;
	ep = &f->next;
	m_ok = 0;
      }
    }

    mmap = mmap->next;
  }

#if DEBUG
  dprintf("Computed free list:\n");
  syslinux_dump_movelist(stdout, space);
#endif

  for ( fp = &frags, f = *fp ; f ; fp = &f->next, f = *fp ) {
    dprintf("@: 0x%08x bytes at 0x%08x -> 0x%08x\n",
	    f->len, f->src, f->dst);

    if ( f->src == f->dst ) {
      //delete_movelist(fp);	/* Already in the right place! */
      continue;
    }

    /* See if we can move this chunk into place by claiming
       the destination, or in the case of partial overlap, the
       missing portion. */

    needbase = f->dst;
    needlen  = f->len;

    dprintf("need: base = 0x%08x, len = 0x%08x\n", needbase, needlen);

    if ( f->src < f->dst && (f->dst - f->src) < f->len ) {
      /* "Shift up" type overlap */
      needlen  = f->dst - f->src;
      needbase = f->dst + (f->len - needlen);
    } else if ( f->src > f->dst && (f->src - f->dst) < f->len ) {
      /* "Shift down" type overlap */
      needbase = f->dst;
      needlen  = f->src - f->dst;
    }

    if ( (ep = is_free_zone(needbase, 1, &space)) ) {
      /* We can move at least part of this chunk into place without further ado */
      copylen = min(needlen, (*ep)->len);
      allocate_from(needbase, copylen, ep);
      goto move_chunk;
    }

    /* At this point, we need to evict something out of our space.
       Find the object occupying the first byte of our target space,
       and move it out (the whole object if we can, otherwise a subset.)
       Then move a chunk of ourselves into place. */
    for ( op = &f->next, o = *op ; o ; op = &o->next, o = *op ) {

	dprintf("O: 0x%08x bytes	at 0x%08x -> 0x%08x\n",
		o->len, o->src, o->dst);

      if ( !(o->src <= needbase && o->src+o->len > needbase) )
	continue;		/* Not what we're looking for... */

      /* Find somewhere to put it... */
      if ( (ep = free_area(o->len, &space)) ) {
	/* We got what we wanted... */
	copydst = (*ep)->src;
	copylen = o->len;
      } else {
	ep = free_area_max(&space);
	if ( !ep )
	  goto bail;		/* Stuck! */
	copydst = (*ep)->src;
	copylen = (*ep)->len;
      }
      allocate_from(copydst, copylen, ep);

      if ( copylen >= o->len - (needbase-o->src) ) {
	copysrc = o->src + (o->len - copylen);
      } else {
	copysrc = o->src;
      }

      if ( copylen < o->len ) {
	op = split_movelist(copysrc, copylen, op);
	o = *op;
      }

      mv = new_movelist(copydst, copysrc, copylen);
      dprintf("C: 0x%08x bytes at 0x%08x -> 0x%08x\n",
	      mv->len, mv->src, mv->dst);
      *moves = mv;
      moves = &mv->next;

      o->src = copydst;

      if ( copylen > needlen ) {
	/* We don't need all the memory we freed up.  Mark it free. */
	if ( copysrc < needbase ) {
	  mv = new_movelist(0, copysrc, needbase-copysrc);
	  mv->next = space;
	  space = mv;
	  copylen -= (needbase-copysrc);
	}
	if ( copylen > needlen ) {
	  mv = new_movelist(0, copysrc+needlen, copylen-needlen);
	  mv->next = space;
	  space = mv;
	  copylen = needlen;
	}
      }
      goto move_chunk;
    }
    goto bail;			/* Stuck! */

  move_chunk:
    /* We're allowed to move the chunk into place now. */

    if ( copylen < needlen ) {
      /* Didn't get all we wanted, so we have to split the chunk */
      fp = split_movelist(f->src, copylen+(needbase-f->dst), fp);
      f = *fp;
    }

    mv = new_movelist(f->dst, f->src, f->len);
    dprintf("A: 0x%08x bytes at 0x%08x -> 0x%08x\n",
	    mv->len, mv->src, mv->dst);
    *moves = mv;
    moves = &mv->next;

    /* Figure out what memory we just freed up */
    if ( f->dst > f->src ) {
      freebase = f->src;
      freelen  = min(f->len, f->dst-f->src);
    } else if ( f->src >= f->dst+f->len ) {
      freebase = f->src;
      freelen  = f->len;
    } else {
      freelen  = f->src-f->dst;
      freebase = f->dst+f->len;
    }

    mv = new_movelist(0, freebase, freelen);
    mv->next = space;
    space = mv;
  }

  rv = 0;
 bail:
  if (mmap)
    syslinux_free_memmap(mmap);
  if (frags)
    free_movelist(&frags);
  return rv;
}

#ifdef TEST

#include <stdio.h>

int main(int argc, char *argv[])
{
  FILE *f;
  unsigned long d, s, l;
  struct syslinux_movelist *frags;
  struct syslinux_movelist **fep = &frags;
  struct syslinux_movelist *space;
  struct syslinux_movelist **sep = &space;
  struct syslinux_movelist *mv, *moves;

  f = fopen(argv[1], "r");
  while ( fscanf(f, "%lx %lx %lx", &d, &s, &l) == 3 ) {
    if ( d ) {
      mv = new_movelist(d, s, l);
      *fep = mv;
      fep = &mv->next;
    } else {
      mv = new_movelist(0, s, l);
      *sep = mv;
      sep = &mv->next;
    }
  }
  fclose(f);

  if ( syslinux_compute_movelist(&moves, frags, space) ) {
    printf("Failed to compute a move sequence\n");
    return 1;
  } else {
    for ( mv = moves ; mv ; mv = mv->next ) {
      printf("0x%08x bytes at 0x%08x -> 0x%08x\n",
	     mv->len, mv->src, mv->dst);
    }
    return 0;
  }
 }

#endif /* TEST */
