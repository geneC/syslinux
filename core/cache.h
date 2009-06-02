#ifndef _CACHE_H
#define _CACHE_H

#include "types.h"
#include <com32.h>


#define MAX_CACHE_ENTRIES  0x064 /* I'm not sure it's the max */



/* The cache structure */
struct cache_struct {
        __u32 block;
        struct cache_struct *prev;
        struct cache_struct *next;
        void  *data;
};



/* functions defined in cache.c */
void cache_init(com32sys_t *regs);

void get_cache_block(com32sys_t *regs);

#endif /* cache.h */
