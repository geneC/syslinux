/*
 * free.c
 *
 * Very simple linked-list based malloc()/free().
 */

#include <stdlib.h>
#include "malloc.h"

static struct free_arena_header *__free_block(struct free_arena_header *ah)
{
    struct free_arena_header *pah, *nah;

    pah = ah->a.prev;
    nah = ah->a.next;
    if (pah->a.type == ARENA_TYPE_FREE &&
	(char *)pah + pah->a.size == (char *)ah) {
	/* Coalesce into the previous block */
	pah->a.size += ah->a.size;
	pah->a.next = nah;
	nah->a.prev = pah;

#ifdef DEBUG_MALLOC
	ah->a.type = ARENA_TYPE_DEAD;
#endif

	ah = pah;
	pah = ah->a.prev;
    } else {
	/* Need to add this block to the free chain */
	ah->a.type = ARENA_TYPE_FREE;

	ah->next_free = __malloc_head.next_free;
	ah->prev_free = &__malloc_head;
	__malloc_head.next_free = ah;
	ah->next_free->prev_free = ah;
    }

    /* In either of the previous cases, we might be able to merge
       with the subsequent block... */
    if (nah->a.type == ARENA_TYPE_FREE &&
	(char *)ah + ah->a.size == (char *)nah) {
	ah->a.size += nah->a.size;

	/* Remove the old block from the chains */
	nah->next_free->prev_free = nah->prev_free;
	nah->prev_free->next_free = nah->next_free;
	ah->a.next = nah->a.next;
	nah->a.next->a.prev = ah;

#ifdef DEBUG_MALLOC
	nah->a.type = ARENA_TYPE_DEAD;
#endif
    }

    /* Return the block that contains the called block */
    return ah;
}

/*
 * This is used to insert a block which is not previously on the
 * free list.  Only the a.size field of the arena header is assumed
 * to be valid.
 */
void __inject_free_block(struct free_arena_header *ah)
{
    struct free_arena_header *nah;
    size_t a_end = (size_t) ah + ah->a.size;
    size_t n_end;

    for (nah = __malloc_head.a.next; nah->a.type != ARENA_TYPE_HEAD;
	 nah = nah->a.next) {
	n_end = (size_t) nah + nah->a.size;

	/* Is nah entirely beyond this block? */
	if ((size_t) nah >= a_end)
	    break;

	/* Is this block entirely beyond nah? */
	if ((size_t) ah >= n_end)
	    continue;

	/* Otherwise we have some sort of overlap - reject this block */
	return;
    }

    /* Now, nah should point to the successor block */
    ah->a.next = nah;
    ah->a.prev = nah->a.prev;
    nah->a.prev = ah;
    ah->a.prev->a.next = ah;

    __free_block(ah);
}

void free(void *ptr)
{
    struct free_arena_header *ah;

    if (!ptr)
	return;

    ah = (struct free_arena_header *)
	((struct arena_header *)ptr - 1);

#ifdef DEBUG_MALLOC
    assert(ah->a.type == ARENA_TYPE_USED);
#endif

    __free_block(ah);

    /* Here we could insert code to return memory to the system. */
}
