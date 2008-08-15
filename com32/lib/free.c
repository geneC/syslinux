/*
 * free.c
 *
 * Very simple linked-list based malloc()/free().
 */

#include <stdlib.h>
#include "malloc.h"

static struct free_arena_header *
__free_block(struct free_arena_header *ah)
{
  struct free_arena_header *pah, *nah;

  pah = ah->a.prev;
  nah = ah->a.next;
  if ( ARENA_TYPE_GET(pah->a.attrs) == ARENA_TYPE_FREE &&
       (char *)pah+ARENA_SIZE_GET(pah->a.attrs) == (char *)ah ) {
    /* Coalesce into the previous block */
    ARENA_SIZE_SET(pah->a.attrs, ARENA_SIZE_GET(pah->a.attrs) +
    		ARENA_SIZE_GET(ah->a.attrs));
    pah->a.next = nah;
    nah->a.prev = pah;

#ifdef DEBUG_MALLOC
    ARENA_TYPE_SET(ah->a.attrs, ARENA_TYPE_DEAD);
#endif

    ah = pah;
    pah = ah->a.prev;
  } else {
    /* Need to add this block to the free chain */
    ARENA_TYPE_SET(ah->a.attrs, ARENA_TYPE_FREE);
    ah->a.tag = NULL;

    ah->next_free = __malloc_head.next_free;
    ah->prev_free = &__malloc_head;
    __malloc_head.next_free = ah;
    ah->next_free->prev_free = ah;
  }

  /* In either of the previous cases, we might be able to merge
     with the subsequent block... */
  if ( ARENA_TYPE_GET(nah->a.attrs) == ARENA_TYPE_FREE &&
       (char *)ah+ARENA_SIZE_GET(ah->a.attrs) == (char *)nah ) {
    ARENA_SIZE_SET(ah->a.attrs, ARENA_SIZE_GET(ah->a.attrs) +
    		ARENA_SIZE_GET(nah->a.attrs));

    /* Remove the old block from the chains */
    nah->next_free->prev_free = nah->prev_free;
    nah->prev_free->next_free = nah->next_free;
    ah->a.next = nah->a.next;
    nah->a.next->a.prev = ah;

#ifdef DEBUG_MALLOC
    ARENA_TYPE_SET(nah->a.attrs, ARENA_TYPE_DEAD);
#endif
  }

  /* Return the block that contains the called block */
  return ah;
}

void free(void *ptr)
{
  struct free_arena_header *ah;

  if ( !ptr )
    return;

  ah = (struct free_arena_header *)
    ((struct arena_header *)ptr - 1);

#ifdef DEBUG_MALLOC
  assert( ARENA_TYPE_GET(ah->a.attrs) == ARENA_TYPE_USED );
#endif

  __free_block(ah);

  /* Here we could insert code to return memory to the system. */
}

void __free_tagged(void *tag) {
	struct free_arena_header *fp, *nfp;

	for (fp = __malloc_head.a.next, nfp = fp->a.next;
			ARENA_TYPE_GET(fp->a.attrs) != ARENA_TYPE_HEAD;
			fp = nfp, nfp = fp->a.next) {

		if (ARENA_TYPE_GET(fp->a.attrs) == ARENA_TYPE_USED &&
				fp->a.tag == tag) {
			// Free this block
			__free_block(fp);
		}
	}
}
