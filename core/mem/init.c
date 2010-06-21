#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "malloc.h"

#include <stdio.h>

struct free_arena_header __core_malloc_head[NHEAP];

//static __hugebss char main_heap[128 << 10];
/* change it to 32M */
static __hugebss char main_heap[32 << 20];
extern char __lowmem_heap[];

void mem_init(void)
{
    struct free_arena_header *fp;
    int i;
    uint16_t *bios_free_mem = (uint16_t *)0x413;

    /* Initialize the head nodes */
    fp = &__core_malloc_head[0];
    for (i = 0 ; i < NHEAP ; i++) {
	fp->a.next = fp->a.prev = fp->next_free = fp->prev_free = fp;
	fp->a.attrs = ARENA_TYPE_HEAD | (i << ARENA_HEAP_POS);
	fp->a.tag = MALLOC_HEAD;
	fp++;
    }

    /* Initialize the main heap */
    fp = (struct free_arena_header *)main_heap;
    fp->a.attrs = ARENA_TYPE_USED | (HEAP_MAIN << ARENA_HEAP_POS);
    ARENA_SIZE_SET(fp->a.attrs, sizeof main_heap);
    __inject_free_block(fp);

	//mp("__lowmem_heap = 0x%p bios_free = 0x%p", __lowmem_heap, *bios_free_mem);
    /* Initialize the lowmem heap */
    fp = (struct free_arena_header *)__lowmem_heap;
    fp->a.attrs = ARENA_TYPE_USED | (HEAP_LOWMEM << ARENA_HEAP_POS);
    ARENA_SIZE_SET(fp->a.attrs, (*bios_free_mem << 10) - (uintptr_t)fp);
    __inject_free_block(fp);
}
