/*
 * realloc.c
 */

#include <stdlib.h>
#include <string.h>
#include <minmax.h>

#include "malloc.h"

void *realloc(void *ptr, size_t size)
{
  struct free_arena_header *ah, *nah;
  void *newptr;
  size_t newsize, oldsize, xsize;

  if ( !ptr )
    return malloc(size);

  if ( size == 0 ) {
    free(ptr);
    return NULL;
  }

  ah = (struct free_arena_header *)
    ((struct arena_header *)ptr - 1);

  /* Actual size of the old block */
  oldsize = ARENA_SIZE_GET(ah->a.attrs);

  /* Add the obligatory arena header, and round up */
  newsize = (size+2*sizeof(struct arena_header)-1) & ARENA_SIZE_MASK;

  if ( oldsize >= newsize && newsize >= (oldsize >> 2) &&
       oldsize-newsize < 4096 ) {
    /* This allocation is close enough already. */
    return ptr;
  } else {
    xsize = oldsize;

    nah = ah->a.next;
    if ((char *)nah == (char *)ah + ARENA_SIZE_GET(ah->a.attrs) &&
	ARENA_TYPE_GET(nah->a.attrs) == ARENA_TYPE_FREE &&
	oldsize + ARENA_SIZE_GET(nah->a.attrs) >= newsize) {
      /* Merge in subsequent free block */
      ah->a.next = nah->a.next;
      ah->a.next->a.prev = ah;
      nah->next_free->prev_free = nah->prev_free;
      nah->prev_free->next_free = nah->next_free;
      xsize = (ARENA_SIZE_SET(ah->a.attrs, ARENA_SIZE_GET(ah->a.attrs) +
    		  ARENA_SIZE_GET(nah->a.attrs)));
    }

    if (xsize >= newsize) {
      /* We can reallocate in place */
      if (xsize >= newsize + 2*sizeof(struct arena_header)) {
	/* Residual free block at end */
	nah = (struct free_arena_header *)((char *)ah + newsize);
	ARENA_TYPE_SET(nah->a.attrs, ARENA_TYPE_FREE);
	ARENA_SIZE_SET(nah->a.attrs, xsize - newsize);
	nah->a.tag = NULL;
	ARENA_SIZE_SET(ah->a.attrs, newsize);

	/* Insert into block list */
	nah->a.next = ah->a.next;
	ah->a.next = nah;
	nah->a.next->a.prev = nah;
	nah->a.prev = ah;

	/* Insert into free list */
	if (newsize > oldsize) {
	  /* Hack: this free block is in the path of a memory object
	     which has already been grown at least once.  As such, put
	     it at the *end* of the freelist instead of the beginning;
	     trying to save it for future realloc()s of the same block. */
	  nah->prev_free = __malloc_head.prev_free;
	  nah->next_free = &__malloc_head;
	  __malloc_head.prev_free = nah;
	  nah->prev_free->next_free = nah;
	} else {
	  nah->next_free = __malloc_head.next_free;
	  nah->prev_free = &__malloc_head;
	  __malloc_head.next_free = nah;
	  nah->next_free->prev_free = nah;
	}
      }
      /* otherwise, use up the whole block */
      return ptr;
    } else {
      /* Last resort: need to allocate a new block and copy */
      oldsize -= sizeof(struct arena_header);
      newptr = malloc(size);
      if (newptr) {
	memcpy(newptr, ptr, min(size,oldsize));
	/* Retain tag from the old block */
	__mem_set_tag(newptr, __mem_get_tag(ptr));
	free(ptr);
      }
      return newptr;
    }
  }
}
