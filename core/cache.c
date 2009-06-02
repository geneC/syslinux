#include "core.h"

#include "cache.h"
#include <stdio.h>


/*
 * Each CachePtr contains:
 * - Block pointer
 * - LRU previous pointer
 * - LRU next pointer
 * - Block data buffer address
 * 
 * The cache buffer are pointed to by a cache_head structure.
 */


static struct cache_struct cache_head, cache[MAX_CACHE_ENTRIES];
static __u8 cache_data[65536];
static int cache_block_size;
static int cache_entries;

/**
 * cache_init:
 *
 * Initialize the cache data structres.
 * regs->eax.l stores the block size
 *
 */
void cache_init(com32sys_t * regs)
{
        struct cache_struct *prev, *cur;
        __u8 *data = cache_data;
        int block_size = regs->eax.l;
        int i;

        cache_block_size = block_size;
        cache_entries = sizeof cache_data / block_size;
        if (cache_entries > MAX_CACHE_ENTRIES)
                cache_entries = MAX_CACHE_ENTRIES;
        
        cache_head.prev = &cache[cache_entries-1];
        cache_head.prev->next = &cache_head;
        prev = &cache_head;
        
        for (i = 0; i < cache_entries; i++) {
                cur = &cache[i];
                cur->block = 0;
                cur->prev  = prev;
                prev->next = cur;
                cur->data  = data;
                data += block_size;
                prev = cur++;
        }
}


extern void getlinsec(void);

void getoneblk(char *buf, __u32 block, int block_size)
{
        int sec_per_block = block_size / 512; /* 512==sector size */
        com32sys_t regs;
        
        

        regs.eax.l = block * sec_per_block;
        regs.ebp.l = sec_per_block;
        regs.es = SEG(buf);
        regs.ebx.w[0] = OFFS(buf);

        call16(getlinsec, &regs, NULL);
}



/**
 * get_cache_block:
 *
 * Check for a particular BLOCK in the block cache, 
 * and if it is already there, just do nothing and return;
 * otherwise load it and updata the relative cache
 * structre with data pointer.
 *
 * it's a test version for my start of merging extlinux into core.
 * and after I have figured out how to handle the relations between
 * rm and pm, c and asm, we call call it from C file, so no need 
 * com32sys_t *regs any more.
 *
 * @return: the data stores at gs:si
 *
 */
void get_cache_block(com32sys_t * regs)
{
    /* let's find it from the end, 'cause the endest is the freshest */
    struct cache_struct *cs = cache_head.prev;
    struct cache_struct *head,  *last;
    __u32 block = regs->eax.l;
    int i;
    
    if ( !block ) {
        extern void myputs(const char *);
        myputs("ERROR: we got a ZERO block number that's not we want!\n");
        return;
    }
    
    /* it's aleardy the freshest, so nothing we need do , just return it */
    if ( cs->block == block ) 
        goto out;
    
    for ( i = 0; i < cache_entries; i ++ ) {
        if ( cs->block == block )
            break;
        else
            cs = cs->prev;
    }
    
    if ( i == cache_entries ) {
        /* missed, so we need to load it */
        
        /* store it at the head of real cache */
        cs = cache_head.next;
        
        cs->block = block;
        getoneblk(cs->data, block, cache_block_size);
    }
    
    /* remove cs from current position in list */
    cs->prev->next = cs->next;
    cs->next->prev = cs->prev;
    
    
    /* add to just before head node */
    last = cache_head.prev;
    head = &cache_head;
    
    last->next = cs;
    cs->prev = last;
    head->prev = cs;
    cs->next = head;
    
 out:
    regs->gs = SEG(cs->data);
    regs->esi.w[0]= OFFS(cs->data);
}
