/*
 * malloc.h
 *
 * Internals for the memory allocator
 */

#include <stdint.h>
#include <stddef.h>
#include "core.h"

/*
 * This is a temporary hack.  In Syslinux 5 this will be a pointer to
 * the owner module.
 */
typedef size_t malloc_tag_t;
enum malloc_owner {
    MALLOC_FREE,
    MALLOC_HEAD,
    MALLOC_CORE,
    MALLOC_MODULE,
};

struct free_arena_header;

/*
 * This structure should be a power of two.  This becomes the
 * alignment unit.
 */
struct arena_header {
    malloc_tag_t tag;
    size_t attrs;			/* Bits 0..1:  Type
					        2..3:  Heap,
						4..31: MSB of the size  */
    struct free_arena_header *next, *prev;
};

enum arena_type {
    ARENA_TYPE_USED = 0,
    ARENA_TYPE_FREE = 1,
    ARENA_TYPE_HEAD = 2,
    ARENA_TYPE_DEAD = 3,
};
enum heap {
    HEAP_MAIN,
    HEAP_LOWMEM,
    NHEAP
};

#define ARENA_SIZE_MASK (~(uintptr_t)(sizeof(struct arena_header)-1))
#define ARENA_HEAP_MASK ((size_t)0xc)
#define ARENA_HEAP_POS	2
#define ARENA_TYPE_MASK	((size_t)0x3)

#define ARENA_ALIGN_UP(p)	((char *)(((uintptr_t)(p) + ~ARENA_SIZE_MASK) \
					  & ARENA_SIZE_MASK))
#define ARENA_ALIGN_DOWN(p)	((char *)((uintptr_t)(p) & ARENA_SIZE_MASK))

#define ARENA_SIZE_GET(attrs)	((attrs) & ARENA_SIZE_MASK)
#define ARENA_HEAP_GET(attrs)	(((attrs) & ARENA_HEAP_MASK) >> ARENA_HEAP_POS)
#define ARENA_TYPE_GET(attrs)	((attrs) & ARENA_TYPE_MASK)

#define ARENA_SIZE_SET(attrs, size)	\
	((attrs) = ((size) & ARENA_SIZE_MASK) | ((attrs) & ~ARENA_SIZE_MASK))
#define ARENA_HEAP_SET(attrs, heap)	\
	((attrs) = (((heap) << ARENA_HEAP_POS) & ARENA_HEAP_MASK) | \
	 ((attrs) & ~ARENA_HEAP_MASK))
#define ARENA_TYPE_SET(attrs, type) \
	((attrs) = ((attrs) & ~ARENA_TYPE_MASK) | \
	 ((type) & ARENA_TYPE_MASK))

/*
 * This structure should be no more than twice the size of the
 * previous structure.
 */
struct free_arena_header {
    struct arena_header a;
    struct free_arena_header *next_free, *prev_free;
    size_t _pad[2];		/* Pad to 2*sizeof(struct arena_header) */
};

extern struct free_arena_header __malloc_head[NHEAP];
void __inject_free_block(struct free_arena_header *ah);
