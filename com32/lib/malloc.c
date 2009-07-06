/*
 * malloc.c
 *
 * Very simple linked-list based malloc()/free().
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <com32.h>
#include <syslinux/memscan.h>
#include "init.h"
#include "malloc.h"

/**
 * The global tag used to label all the new allocations
 */
void *__malloc_global_tag = NULL;

struct free_arena_header __malloc_head =
{
  {
    NULL,
    ARENA_TYPE_HEAD,
    &__malloc_head,
    &__malloc_head
  },
    &__malloc_head,
    &__malloc_head
};

/* This is extern so it can be overridden by the user application */
extern size_t __stack_size;
extern void *__mem_end;		/* Produced after argv parsing */

static inline size_t sp(void)
{
    size_t sp;
    asm volatile ("movl %%esp,%0":"=rm" (sp));
    return sp;
}

#define E820_MEM_MAX 0xfff00000	/* 4 GB - 1 MB */

static int consider_memory_area(void *dummy, addr_t start,
				addr_t len, bool valid)
{
    struct free_arena_header *fp;
    addr_t end;

    (void)dummy;

    if (valid && start < E820_MEM_MAX) {
	if (len > E820_MEM_MAX - start)
	    len = E820_MEM_MAX - start;

	end = start + len;

	if (end > __com32.cs_memsize) {
	    if (start <= __com32.cs_memsize) {
		start = __com32.cs_memsize;
		len = end - start;
	    }

	    if (len >= 2 * sizeof(struct arena_header)) {
		fp = (struct free_arena_header *)start;
		ARENA_SIZE_SET(fp->a.attrs, len);
		__inject_free_block(fp);
	    }
	}
    }

    return 0;
}

static void __constructor init_memory_arena(void)
{
    struct free_arena_header *fp;
    size_t start, total_space;

    start = (size_t) ARENA_ALIGN_UP(__mem_end);
    total_space = sp() - start;

    if (__stack_size == 0 || __stack_size > total_space >> 1)
	__stack_size = total_space >> 1;	/* Half for the stack, half for the heap... */

    if (total_space < __stack_size + 4 * sizeof(struct arena_header))
	__stack_size = total_space - 4 * sizeof(struct arena_header);

    fp = (struct free_arena_header *)start;
    ARENA_TYPE_SET(fp->a.attrs, ARENA_TYPE_FREE);
    ARENA_SIZE_SET(fp->a.attrs, total_space - __stack_size);
    fp->a.tag = NULL;

    __inject_free_block(fp);

    /* Scan the memory map to look for other suitable regions */
    if (!__com32.cs_memsize)
	return;			/* Old Syslinux core, can't do this... */

    syslinux_scan_memory(consider_memory_area, NULL);
}

static void *__malloc_from_block(struct free_arena_header *fp, size_t size)
{
    size_t fsize;
    struct free_arena_header *nfp, *na;

    fsize = ARENA_SIZE_GET(fp->a.attrs);

    /* We need the 2* to account for the larger requirements of a free block */
    if ( fsize >= size+2*sizeof(struct arena_header) ) {
        /* Bigger block than required -- split block */
        nfp = (struct free_arena_header *)((char *)fp + size);
        na = fp->a.next;

        ARENA_TYPE_SET(nfp->a.attrs, ARENA_TYPE_FREE);
        ARENA_SIZE_SET(nfp->a.attrs, fsize-size);
        nfp->a.tag = NULL;
        ARENA_TYPE_SET(fp->a.attrs, ARENA_TYPE_USED);
        ARENA_SIZE_SET(fp->a.attrs, size);
        fp->a.tag = __malloc_global_tag;

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
        fp->a.tag = __malloc_global_tag;

        /* Remove from free chain */
        fp->next_free->prev_free = fp->prev_free;
        fp->prev_free->next_free = fp->next_free;
    }

    return (void *)(&fp->a + 1);
}

void *malloc(size_t size)
{
    struct free_arena_header *fp;

    if (size == 0)
	return NULL;

    /* Add the obligatory arena header, and round up */
    size = (size + 2 * sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

    for ( fp = __malloc_head.next_free ; ARENA_TYPE_GET(fp->a.attrs) != ARENA_TYPE_HEAD ;
	  fp = fp->next_free ) {
        if ( ARENA_SIZE_GET(fp->a.attrs) >= size ) {
           /* Found fit -- allocate out of this block */
           return __malloc_from_block(fp, size);
        }
    }

    /* Nothing found... need to request a block from the kernel */
    return NULL;		/* No kernel to get stuff from */
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	struct free_arena_header *fp, *nfp;
	uintptr_t align_mask, align_addr;

	if (size == 0 || memptr == NULL) {
		return EINVAL;
	}

	if ((alignment & (alignment - 1)) != 0)
		return EINVAL;

#if 0
	// POSIX says to refuse alignments smaller than sizeof(void*)
	if (alignment % sizeof(void*) != 0)
		return EINVAL;
#endif

	// The arena allocator can't handle alignments smaller than this
	if (alignment < sizeof(struct arena_header)) {
		alignment = sizeof(struct arena_header);
	}
	align_mask = ~(uintptr_t)(alignment - 1);

	// Round up
	size = (size + sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

	*memptr = NULL;

	for (fp = __malloc_head.next_free; ARENA_TYPE_GET(fp->a.attrs) != ARENA_TYPE_HEAD;
		fp = fp->next_free) {

		if (ARENA_SIZE_GET(fp->a.attrs) <= size)
			continue;

		align_addr = (uintptr_t)fp;

		// Ensure the alignment leaves some space before for the header
		if (align_addr % alignment == 0) {
			align_addr += alignment;
		} else {
			align_addr = (align_addr + alignment - 1) & align_mask;
		}
		if (align_addr - (uintptr_t)fp == 2*sizeof(struct arena_header))
			align_addr += alignment;

		// See if now we have enough space
		if (align_addr + size > (uintptr_t)fp + ARENA_SIZE_GET(fp->a.attrs))
			continue;

		// We have a winner...
		if (align_addr - (uintptr_t)fp > sizeof(struct arena_header)) {
			// We must split the block before the alignment point
			nfp = (struct free_arena_header*)(align_addr - sizeof(struct arena_header));
			ARENA_TYPE_SET(nfp->a.attrs, ARENA_TYPE_FREE);
			ARENA_SIZE_SET(nfp->a.attrs,
					ARENA_SIZE_GET(fp->a.attrs) - ((uintptr_t)nfp - (uintptr_t)fp));
			nfp->a.tag = NULL;
			nfp->a.prev = fp;
			nfp->a.next = fp->a.next;
			nfp->prev_free = fp;
			nfp->next_free = fp->next_free;

			nfp->a.next->a.prev = nfp;
			nfp->next_free->prev_free = nfp;

			ARENA_SIZE_SET(fp->a.attrs, (uintptr_t)nfp - (uintptr_t)fp);

			fp->a.next = nfp;
			fp->next_free = nfp;

			*memptr = __malloc_from_block(nfp, size + sizeof(struct arena_header));
		} else {
			*memptr = __malloc_from_block(fp, size + sizeof(struct arena_header));
		}
		break;
	}

	if (*memptr == NULL)
		return ENOMEM;

	return 0;
}

void *__mem_get_tag(void *memptr) {
	struct arena_header *ah = (struct arena_header *)memptr - 1;
	return ah->tag;
}

void __mem_set_tag(void *memptr, void *tag) {
	struct arena_header *ah = (struct arena_header *)memptr - 1;
	ah->tag = tag;
}
