#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "malloc.h"

#include <stdio.h>
#include <syslinux/memscan.h>

struct free_arena_header __core_malloc_head[NHEAP];

static __hugebss char main_heap[128 << 10];
/* change it to 32M */
//static __hugebss char main_heap[32 << 20];
extern char __lowmem_heap[];
extern char free_high_memory[];

#define E820_MEM_MAX 0xfff00000	/* 4 GB - 1 MB */
int scan_highmem_area(void *data, addr_t start, addr_t len, bool is_ram)
{
	struct free_arena_header *fp;
	addr_t end;

	mp("start = %x, len = %x, is_ram = %d\n", start, len, is_ram);

	if (start < 0x100000 || start > E820_MEM_MAX
			     || !is_ram)
		return 0;

	if (start < __com32.cs_memsize)
		start = __com32.cs_memsize;
	if (len > E820_MEM_MAX - start)
		len = E820_MEM_MAX - start;
	end = start + len;

	if (len >= 2 * sizeof(struct arena_header)) {
		fp = (struct free_arena_header *)start;
		fp->a.attrs = ARENA_TYPE_USED | (HEAP_MAIN << ARENA_HEAP_POS);
		ARENA_SIZE_SET(fp->a.attrs, len);
		mp("will inject a block start:0x%x size 0x%x", start, len);
		__inject_free_block(fp);
	}

	__com32.cs_memsize = start + len; /* update the HighMemSize */
	return 0;
}

void mem_init(void)
{
	struct free_arena_header *fp;
	int i;
	uint16_t *bios_free_mem = (uint16_t *)0x413;

	mp("enter");

	/* Initialize the head nodes */
	fp = &__core_malloc_head[0];
	for (i = 0 ; i < NHEAP ; i++) {
	fp->a.next = fp->a.prev = fp->next_free = fp->prev_free = fp;
	fp->a.attrs = ARENA_TYPE_HEAD | (i << ARENA_HEAP_POS);
	fp->a.tag = MALLOC_HEAD;
	fp++;
	}

	//mp("__lowmem_heap = 0x%p bios_free = 0x%p", __lowmem_heap, *bios_free_mem);
	/* Initialize the lowmem heap */
	fp = (struct free_arena_header *)__lowmem_heap;
	fp->a.attrs = ARENA_TYPE_USED | (HEAP_LOWMEM << ARENA_HEAP_POS);
	ARENA_SIZE_SET(fp->a.attrs, (*bios_free_mem << 10) - (uintptr_t)fp);
	__inject_free_block(fp);

	/* Initialize the main heap */
	/*
	fp = (struct free_arena_header *)main_heap;
	fp->a.attrs = ARENA_TYPE_USED | (HEAP_MAIN << ARENA_HEAP_POS);
	ARENA_SIZE_SET(fp->a.attrs, sizeof main_heap);
	__inject_free_block(fp);
	*/

	__com32.cs_memsize = free_high_memory;
	syslinux_scan_memory(scan_highmem_area, NULL);
}
