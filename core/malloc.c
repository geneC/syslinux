/*
 * A simple temp malloc for Sysliux project from fstk. For now, just used 
 * in fsc branch, which it's would be easy to remove it when we have a 
 * powerful one, as hpa said this would happen when elflink branch do the
 * work.
 *
 * Copyright (C) 2009 Liu Aleaxander -- All rights reserved. This file
 * may be redistributed under the terms of the GNU Public License.
 */
 

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* The memory managemant structure */
struct mem_struct {
    struct mem_struct *prev;
    int size;
    int free;
};


/* First, assume we just need 64K memory */
static char memory[0x10000];

/* Next free memory address */
static struct mem_struct *next_start = (struct mem_struct *)memory;
static uint32_t mem_end = (uint32_t)(memory + 0x10000);


static inline struct mem_struct *get_next(struct mem_struct *mm)
{
    uint32_t next = (uint32_t)mm + mm->size;
        
    if (next >= mem_end)
	return NULL;
    else
	return (struct mem_struct *)next;
}

/*
 * Here are the _merge_ functions, that merges a adjacent memory region, 
 * from front, or from back, or even merges both. It returns the headest 
 * region mem_struct.
 *
 */

static struct mem_struct *  merge_front(struct mem_struct *mm, 
                                        struct mem_struct *prev)
{
    struct mem_struct *next = get_next(mm);
    
    prev->size += mm->size;
    if (next)
	next->prev = prev;
    return prev;
}

static struct mem_struct *  merge_back(struct mem_struct *mm, 
                                       struct mem_struct *next)
{
    mm->free = 1;    /* Mark it free first */
    mm->size += next->size;
    
    next = get_next(next);
    if (next)
	next->prev = mm;
    return mm;
}

static struct mem_struct *  merge_both(struct mem_struct *mm, 
                                       struct mem_struct *prev, 
                                       struct mem_struct *next)
{
    prev->size += mm->size + next->size;
    
    next = get_next(next);
    if (next)
	next->prev = prev;
    return prev;
}

static inline struct mem_struct *  try_merge_front(struct mem_struct *mm)
{
    mm->free = 1;
    if (mm->prev->free)
	mm = merge_front(mm, mm->prev);
    return mm;
}

static inline struct mem_struct *  try_merge_back(struct mem_struct *mm)
{
    struct mem_struct *next = get_next(mm);
    
    mm->free = 1;
    if (next->free)
	merge_back(mm, next);
    return mm;
}

/* 
 * Here's the main function, malloc, which allocates a memory rigon
 * of size _size_. Returns NULL if failed, or the address newly allocated.
 * 
 */
void *malloc(int size)
{
    struct mem_struct *next = next_start;
    struct mem_struct *good = next, *prev;
    int size_needed = (size + sizeof(struct mem_struct) + 3) & ~3;
    
    while(next) {
	if (next->free && next->size >= size_needed) {
	    good = next;
	    break;
	}
	next = get_next(next);
    }
    if (good->size < size_needed) {
	printf("Out of memory, maybe we need append it\n");
	return NULL;
    } else if (good->size == size_needed) {
	/* 
	 * We just found a right memory that with the exact 
	 * size we want. So we just Mark it _not_free_ here,
	 * and move on the _next_start_ pointer, even though
	 * the next may not be a right next start.
	 */
	good->free = 0;
	next_start = get_next(good);
	goto out;
    } else
	size = good->size;  /* save the total size */
    
    /* 
     * Note: allocate a new memory region will not change 
     * it's prev memory, so we don't need change it here.
     */
    good->free = 0;       /* Mark it not free any more */
    good->size = size_needed;
    
    next = get_next(good);
    if (next) {
	next->size = size - size_needed;
	/* check if it can contain 1 byte allocation at least */
	if (next->size <= (int)sizeof(struct mem_struct)) {
	    good->size = size;     /* restore the original size */
	    next_start = get_next(good);
	    goto out;
	}
        
	next->prev = good;
	next->free = 1;                
	next_start = next; /* Update next_start */
	
	prev = next;
	next = get_next(next);
	if (next)
	    next->prev = prev;
    } else
	next_start = (struct mem_struct *)memory;        
out:
    return (void *)((uint32_t)good + sizeof(struct mem_struct));
}

void free(void *ptr)
{
    struct mem_struct *mm = ptr - sizeof(*mm);
    struct mem_struct *prev = mm->prev;
    struct mem_struct *next = get_next(mm);
    
    if (!prev)
	mm = try_merge_back(mm);
    else if (!next)
	mm = try_merge_front(mm);        
    else if (prev->free && !next->free)
	merge_front(mm, prev);
    else if (!prev->free && next->free)
	merge_back(mm, next);
    else if (prev->free && next->free)
	merge_both(mm, prev, next);
    else
	mm->free = 1;
    
    if (mm < next_start)
	next_start = mm;
}

/* 
 * The debug function
 */
void check_mem(void)
{
    struct mem_struct *next = (struct mem_struct *)memory;
    
    printf("____________\n");
    while (next) {
	printf("%-6d  %s\n", next->size, next->free ? "Free" : "Notf");
	next = get_next(next);
    }
    printf("\n");
}


void mem_init(void)
{
        
    struct mem_struct *first = (struct mem_struct *)memory;
    
    first->prev = NULL;
    first->size = 0x10000;
    first->free = 1;
    
    next_start = first;
}
