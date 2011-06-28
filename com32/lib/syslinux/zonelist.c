/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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
 * zonelist.c
 *
 * Deal with syslinux_memmap's, which are data structures designed to
 * hold memory maps.  A zonelist is a sorted linked list of memory
 * ranges, with the guarantee that no two adjacent blocks have the
 * same range type.  Additionally, all unspecified memory have a range
 * type of zero.
 */

#include <stdlib.h>
#include <syslinux/align.h>
#include <syslinux/movebits.h>
#include <dprintf.h>

/*
 * Create an empty syslinux_memmap list.
 */
struct syslinux_memmap *syslinux_init_memmap(void)
{
    struct syslinux_memmap *sp, *ep;

    sp = malloc(sizeof(*sp));
    if (!sp)
	return NULL;

    ep = malloc(sizeof(*ep));
    if (!ep) {
	free(sp);
	return NULL;
    }

    sp->start = 0;
    sp->type = SMT_UNDEFINED;
    sp->next = ep;

    ep->start = 0;		/* Wrap around... */
    ep->type = SMT_END;		/* End of chain */
    ep->next = NULL;

    return sp;
}

/*
 * Add an item to a syslinux_memmap list, potentially overwriting
 * what is already there.
 */
int syslinux_add_memmap(struct syslinux_memmap **list,
			addr_t start, addr_t len,
			enum syslinux_memmap_types type)
{
    addr_t last;
    struct syslinux_memmap *mp, **mpp;
    struct syslinux_memmap *range;
    enum syslinux_memmap_types oldtype;

    dprintf("Input memmap:\n");
    syslinux_dump_memmap(*list);

    /* Remove this to make len == 0 mean all of memory */
    if (len == 0)
	return 0;

    /* Last byte -- to avoid rollover */
    last = start + len - 1;

    mpp = list;
    oldtype = SMT_END;		/* Impossible value */
    while (mp = *mpp, start > mp->start && mp->type != SMT_END) {
	oldtype = mp->type;
	mpp = &mp->next;
    }

    if (start < mp->start || mp->type == SMT_END) {
	if (type != oldtype) {
	    /* Splice in a new start token */
	    range = malloc(sizeof(*range));
	    if (!range)
		return -1;

	    range->start = start;
	    range->type = type;
	    *mpp = range;
	    range->next = mp;
	    mpp = &range->next;
	}
    } else {
	/* mp is exactly aligned with the start of our region */
	if (type != oldtype) {
	    /* Reclaim this entry as our own boundary marker */
	    oldtype = mp->type;
	    mp->type = type;
	    mpp = &mp->next;
	}
    }

    while (mp = *mpp, last > mp->start - 1) {
	oldtype = mp->type;
	*mpp = mp->next;
	free(mp);
    }

    if (last < mp->start - 1) {
	if (oldtype != type) {
	    /* Need a new end token */
	    range = malloc(sizeof(*range));
	    if (!range)
		return -1;

	    range->start = last + 1;
	    range->type = oldtype;
	    *mpp = range;
	    range->next = mp;
	}
    } else {
	if (mp->type == type) {
	    /* Merge this region with the following one */
	    *mpp = mp->next;
	    free(mp);
	}
    }

    dprintf("After adding (%#x,%#x,%d):\n", start, len, type);
    syslinux_dump_memmap(*list);

    return 0;
}

/*
 * Verify what type a certain memory region is.  This function returns
 * SMT_ERROR if the memory region has multiple types.
 */
enum syslinux_memmap_types syslinux_memmap_type(struct syslinux_memmap *list,
						addr_t start, addr_t len)
{
    addr_t last, llast;

    last = start + len - 1;

    while (list->type != SMT_END) {
	llast = list->next->start - 1;
	if (list->start <= start) {
	    if (llast >= last)
		return list->type;	/* Region has a well-defined type */
	    else if (llast >= start)
		return SMT_ERROR;	/* Crosses region boundary */
	}
	list = list->next;
    }

    return SMT_ERROR;		/* Internal error? */
}

/*
 * Find the largest zone of a specific type.  Returns -1 on failure.
 */
int syslinux_memmap_largest(struct syslinux_memmap *list,
			    enum syslinux_memmap_types type,
			    addr_t * start, addr_t * len)
{
    addr_t size, best_size = 0;
    struct syslinux_memmap *best = NULL;

    while (list->type != SMT_END) {
	size = list->next->start - list->start;

	if (list->type == type && size > best_size) {
	    best = list;
	    best_size = size;
	}

	list = list->next;
    }

    if (!best)
	return -1;

    *start = best->start;
    *len = best_size;

    return 0;
}

/*
 * Find the first (lowest address) zone of a specific type and of
 * a certain minimum size, with an optional starting address.
 * The input values of start and len are used as minima.
 */
int syslinux_memmap_find(struct syslinux_memmap *list,
			 enum syslinux_memmap_types type,
			 addr_t * start, addr_t * len, addr_t align)
{
    addr_t min_start = *start;
    addr_t min_len = *len;

    while (list->type != SMT_END) {
	if (list->type == type) {
	    addr_t xstart, xlen;
	    xstart = min_start > list->start ? min_start : list->start;
	    xstart = ALIGN_UP(xstart, align);

	    if (xstart < list->next->start) {
		xlen = list->next->start - xstart;
		if (xlen >= min_len) {
		    *start = xstart;
		    *len = xlen;
		    return 0;
		}
	    }
	}
	list = list->next;
    }

    return -1;			/* Not found */
}

/*
 * Free a zonelist.
 */
void syslinux_free_memmap(struct syslinux_memmap *list)
{
    struct syslinux_memmap *ml;

    while (list) {
	ml = list;
	list = list->next;
	free(ml);
    }
}

/*
 * Duplicate a zonelist.  Returns NULL on failure.
 */
struct syslinux_memmap *syslinux_dup_memmap(struct syslinux_memmap *list)
{
    struct syslinux_memmap *newlist = NULL, **nlp = &newlist;
    struct syslinux_memmap *ml;

    while (list) {
	ml = malloc(sizeof(*ml));
	if (!ml) {
	    syslinux_free_memmap(newlist);
	    return NULL;
	}
	ml->start = list->start;
	ml->type = list->type;
	ml->next = NULL;
	*nlp = ml;
	nlp = &ml->next;

	list = list->next;
    }

    return newlist;
}
