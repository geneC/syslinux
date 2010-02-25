/*
 * malloc.c
 *
 * Very simple linked-list based malloc()/free().
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dprintf.h>
#include "malloc.h"

static void *__malloc_from_block(struct free_arena_header *fp,
				 size_t size, malloc_tag_t tag)
{
    size_t fsize;
    struct free_arena_header *nfp, *na;
    unsigned int heap = ARENA_HEAP_GET(fp->a.attrs);

    fsize = ARENA_SIZE_GET(fp->a.attrs);

    /* We need the 2* to account for the larger requirements of a free block */
    if ( fsize >= size+2*sizeof(struct arena_header) ) {
        /* Bigger block than required -- split block */
        nfp = (struct free_arena_header *)((char *)fp + size);
        na = fp->a.next;

        ARENA_TYPE_SET(nfp->a.attrs, ARENA_TYPE_FREE);
	ARENA_HEAP_SET(nfp->a.attrs, heap);
        ARENA_SIZE_SET(nfp->a.attrs, fsize-size);
        nfp->a.tag = MALLOC_FREE;
        ARENA_TYPE_SET(fp->a.attrs, ARENA_TYPE_USED);
        ARENA_SIZE_SET(fp->a.attrs, size);
        fp->a.tag = tag;

        /* Insert into all-block chain */
        nfp->a.prev = fp;
        nfp->a.next = na;
        na->a.prev = nfp;
        fp->a.next = nfp;

        /* Replace current block on free chain */
        nfp->next_free = fp->next_free;
        nfp->prev_free = fp->prev_free;
        fp->next_free->prev_free = nfp;
        fp->prev_free->next_free = nfp;
    } else {
        /* Allocate the whole block */
        ARENA_TYPE_SET(fp->a.attrs, ARENA_TYPE_USED);
        fp->a.tag = tag;

        /* Remove from free chain */
        fp->next_free->prev_free = fp->prev_free;
        fp->prev_free->next_free = fp->next_free;
    }

    return (void *)(&fp->a + 1);
}

static void *_malloc(size_t size, enum heap heap, malloc_tag_t tag)
{
    struct free_arena_header *fp;
    struct free_arena_header *head = &__malloc_head[heap];
    void *p = NULL;

    dprintf("_malloc(%zu, %u, %u) @ %p = ",
	    size, heap, tag, __builtin_return_address(0));

    if (size) {
	/* Add the obligatory arena header, and round up */
	size = (size + 2 * sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

	for ( fp = head->next_free ; fp != head ; fp = fp->next_free ) {
	    if ( ARENA_SIZE_GET(fp->a.attrs) >= size ) {
		/* Found fit -- allocate out of this block */
		p = __malloc_from_block(fp, size, tag);
		break;
	    }
        }
    }

    dprintf("%p\n", p);
    return p;
}

void *malloc(size_t size)
{
    return _malloc(size, HEAP_MAIN, MALLOC_CORE);
}

void *lmalloc(size_t size)
{
    return _malloc(size, HEAP_LOWMEM, MALLOC_CORE);
}

void *pmapi_lmalloc(size_t size)
{
    return _malloc(size, HEAP_LOWMEM, MALLOC_MODULE);
}
