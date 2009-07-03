/*
 * malloc.h
 *
 * Internals for the memory allocator
 */

#include <stdint.h>
#include <stddef.h>

/*
 * This is the minimum chunk size we will ask the kernel for; this should
 * be a multiple of the page size on all architectures.
 */
#define MALLOC_CHUNK_SIZE	65536
#define MALLOC_CHUNK_MASK       (MALLOC_CHUNK_SIZE-1)


struct free_arena_header;

/*
 * This structure should be a power of two.  This becomes the
 * alignment unit.
 */
struct arena_header {
    void *tag;
    size_t attrs;			/* Bits 0..1: Type, 2..3: Unused, 4..31: MSB of the size  */
    struct free_arena_header *next, *prev;
};


#define ARENA_TYPE_USED 0x0
#define ARENA_TYPE_FREE 0x1
#define ARENA_TYPE_HEAD 0x2
#ifdef DEBUG_MALLOC
#define ARENA_TYPE_DEAD 0x3
#endif

#define ARENA_SIZE_MASK (~(uintptr_t)(sizeof(struct arena_header)-1))
#define ARENA_TYPE_MASK	((size_t)0x3)

#define ARENA_ALIGN_UP(p)	((char *)(((uintptr_t)(p) + ~ARENA_SIZE_MASK) & ARENA_SIZE_MASK))
#define ARENA_ALIGN_DOWN(p)	((char *)((uintptr_t)(p) & ARENA_SIZE_MASK))

#define ARENA_SIZE_GET(attrs)	((attrs) & ARENA_SIZE_MASK)
#define ARENA_TYPE_GET(attrs)	((attrs) & ARENA_TYPE_MASK)

#define ARENA_SIZE_SET(attrs, size)	\
	((attrs) = ((size) & ARENA_SIZE_MASK) | ((attrs) & ~ARENA_SIZE_MASK))
#define ARENA_TYPE_SET(attrs, type) \
	((attrs) = ((attrs) & ~ARENA_TYPE_MASK) | ((type) & ARENA_TYPE_MASK))

/*
 * This structure should be no more than twice the size of the
 * previous structure.
 */
struct free_arena_header {
    struct arena_header a;
    struct free_arena_header *next_free, *prev_free;
};

extern struct free_arena_header __malloc_head;
void __inject_free_block(struct free_arena_header *ah);
