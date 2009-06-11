#ifndef _CACHE_H
#define _CACHE_H

#include <stdint.h>
#include <com32.h>
#include "disk.h"
#include "fs.h"

#define MAX_CACHE_ENTRIES  0x10 /* I find even this is enough:) */

/* The cache structure */
struct cache_struct {
        block_t block;
        struct cache_struct *prev;
        struct cache_struct *next;
        void  *data;
};


/* functions defined in cache.c */
void cache_init(struct device *, int);
struct cache_struct* get_cache_block(struct device *, block_t);
void print_cache(struct device *);

#endif /* cache.h */
