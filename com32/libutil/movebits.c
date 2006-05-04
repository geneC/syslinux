#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <setjmp.h>

#include "movebits.h"

#ifdef TEST
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

/*
 * Public utility function
 *
 * Creates a movelist consisting of only one element, and
 * if parent == NULL insert into the movelist chain immediately after
 * the parent element.
 */
struct movelist *
make_movelist(struct movelist *pptr, uintptr_t dst, uintptr_t src, uintptr_t len)
{
  struct movelist *ml = malloc(sizeof(struct movelist));

  if ( !ml )
    return NULL;

  ml->dst = dst;
  ml->src = src;
  ml->len = len;
  ml->next = pptr ? pptr->next : NULL;

  if ( pptr )
    pptr->next = ml;

  return ml;
}

/*
 * Public utility function
 *
 * Convert a movelist into a linear array of struct move_descriptors
 */
size_t
linearize_movelist(struct move_descriptor **d, struct movelist *m)
{
  size_t n;
  struct movelist *mm;
  struct move_descriptor *l;

  /* Count the length of the list */
  n = 0;
  for ( mm = m ; mm ; mm = mm->next )
    n++;

  *d = l = malloc(n * sizeof(struct move_descriptor));
  if ( !l )
    return (size_t)-1;

  while ( m ) {
    l->dst = m->dst;
    l->src = m->src;
    l->len = m->len;
    l++;
    mm = m;
    m = m->next;
    free(mm);
  }

  return n;
}


static jmp_buf new_movelist_bail;

static struct movelist *
new_movelist(uintptr_t dst, uintptr_t src, uintptr_t len)
{
  struct movelist *ml = malloc(sizeof(struct movelist));

  if ( !ml )
    longjmp(new_movelist_bail, 1);

  ml->dst = dst;
  ml->src = src;
  ml->len = len;
  ml->next = NULL;

  return ml;
}

static struct movelist **
split_movelist(uintptr_t start, uintptr_t len, struct movelist **parentptr)
{
  struct movelist *m, *ml = *parentptr;

  assert(start >= ml->src);
  assert(start < ml->src+ml->len);

  /* Split off the beginning */
  if ( start > ml->src ) {
    uintptr_t l = start - ml->src;

    m = new_movelist(ml->dst+l, start, ml->len-l);
    m->next = ml->next;
    ml->len = l;
    ml->next = m;

    parentptr = &ml->next;
    ml = m;			/* Continue processing the new node */
  }

  if ( ml->len > len ) {
    uintptr_t l = ml->len - len;

    m = new_movelist(ml->dst+len, ml->src+len, l);
    m->next = ml->next;
    ml->len = len;
    ml->next = m;
  }

  return parentptr;
}

#if 0
static void
delete_movelist(struct movelist **parentptr)
{
  struct movelist *o = *parentptr;
  *parentptr = o->next;
  free(o);
}
#endif

/*
 * Scan the freelist looking for a particular chunk of memory
 */
static struct movelist **
is_free_zone(uintptr_t start, uintptr_t len, struct movelist **space)
{
  struct movelist *s;

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
static struct movelist **
free_area(uintptr_t len, struct movelist **space)
{
  struct movelist **best = NULL;
  struct movelist *s;

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
 * Scan the freelist looking for the largest chunk of memory, returns parent ptr
 */
static struct movelist **
free_area_max(struct movelist **space)
{
  struct movelist **best = NULL;
  struct movelist *s;

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
allocate_from(uintptr_t start, uintptr_t len, struct movelist **parentptr)
{
  struct movelist *c = *parentptr;

  assert(c->src <= start);
  assert(c->src+c->len >= start+len);

  if ( c->src != start || c->len != len ) {
    parentptr = split_movelist(start, len, parentptr);
    c = *parentptr;
  }

  *parentptr = c->next;
  free(c);
}

/*
 * Remove any chunk from the freelist which is already
 * claimed by source data.  This somewhat frivolously
 * assumes that the fragments are either completely
 * covered by a free zone or not covered at all.
 */
static void
tidy_freelist(struct movelist **frags, struct movelist **space)
{
  struct movelist **sep;
  struct movelist *f;

  while ( (f = *frags) ) {
    if ( (sep = is_free_zone(f->src, f->len, space)) )
      allocate_from(f->src, f->len, sep);
    frags = &f->next;
  }
}

/*
 * moves is computed from "frags" and "freemem".  "space" lists
 * free memory areas at our disposal, and is (src, cnt) only.
 */

int
compute_movelist(struct movelist **moves, struct movelist *frags,
		 struct movelist *space)
{
  struct movelist *mv;
  struct movelist *f, **fp, **ep;
  struct movelist *o, **op;
  uintptr_t needbase, needlen, copysrc, copydst, copylen;
  uintptr_t freebase, freelen;

  *moves = NULL;

  if ( setjmp(new_movelist_bail) )
    return -1;			/* malloc() failed */

  tidy_freelist(&frags, &space);

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

	dprintf("O: 0x%08x bytes 	at 0x%08x -> 0x%08x\n",
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
	  return -1;		/* Stuck! */
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
    return -1;			/* Stuck! */

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

  return 0;
}

#ifdef TEST

#include <stdio.h>

int main(int argc, char *argv[])
{
  FILE *f;
  unsigned long d, s, l;
  struct movelist *frags;
  struct movelist **fep = &frags;
  struct movelist *space;
  struct movelist **sep = &space;
  struct movelist *mv, *moves;

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

  if ( compute_movelist(&moves, frags, space) ) {
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
