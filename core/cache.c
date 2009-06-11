#include "core.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>


/**
 * Each CachePtr contains:
 * - Block pointer
 * - LRU previous pointer
 * - LRU next pointer
 * - Block data buffer address
 * 
 * The cache buffer are pointed to by a cache_head structure.
 */

/**
 * cache_init:
 *
 * Initialize the cache data structres.
 * regs->eax.l stores the block size(in bits not bytes)
 *
 */
void cache_init(struct device *dev, int block_size_shift)
{
    struct cache_struct *prev, *cur;
    char *data = dev->cache_data;
    static __lowmem struct cache_struct cache_head, cache[MAX_CACHE_ENTRIES];
    int i;

    dev->cache_head = &cache_head;
    dev->cache_block_size = 1 << block_size_shift;
    dev->cache_entries = dev->cache_size >> block_size_shift;
    if (dev->cache_entries > MAX_CACHE_ENTRIES)
        dev->cache_entries = MAX_CACHE_ENTRIES;
    
    cache_head.prev = &cache[dev->cache_entries-1];
    cache_head.prev->next = &cache_head;
    prev = &cache_head;
    
    for (i = 0; i < dev->cache_entries; i++) {
        cur = &cache[i];
        cur->block = 0;
        cur->prev  = prev;
        prev->next = cur;
        cur->data  = data;
        data += dev->cache_block_size;
        prev = cur++;
    }
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
 * I just found that I was tring to do a stupid thing!
 * I haven't change the fs code to c, so for now the cache is based
 * on SECTOR SIZE but not block size. While we can fix it easily by 
 * make the block size be the sector size.
 *
 * @return: the data stores at gs:si
 *
 */
struct cache_struct* get_cache_block(struct device *dev, block_t block)
{
    /* let's find it from the end, 'cause the endest is the freshest */
    struct cache_struct *cs = dev->cache_head->prev;
    struct cache_struct *head,  *last;
    int i;

    static int total_read;
    static int missed;
        
#if 0
    printf("we are looking for cache of %d\n", block);
#endif

    if (!block) {
        printf("ERROR: we got a ZERO block number that's not we want!\n");
        return NULL;
    }
    
    /* it's aleardy the freshest, so nothing we need do , just return it */
    if (cs->block == block) 
        goto out;
    
    for (i = 0; i < dev->cache_entries; i ++) {
        if (cs->block == block)
            break;
        else
            cs = cs->prev;
    }
    
    /* missed, so we need to load it */
    if (i == dev->cache_entries) {        
        /* store it at the head of real cache */
        cs = dev->cache_head->next;        
        cs->block = block;
        getoneblk(cs->data, block, dev->cache_block_size);

        missed ++;
    } 
    
    /* remove cs from current position in list */
    cs->prev->next = cs->next;
    cs->next->prev = cs->prev;    
    
    /* add to just before head node */
    last = dev->cache_head->prev;
    head = dev->cache_head;
    
    last->next = cs;
    cs->prev = last;
    head->prev = cs;
    cs->next = head;    
    
 out:
    total_read ++;
#if 0 /* testing how efficiency the cache is */
    if (total_read % 5 == 0) 
        printf("total_read %d\tmissed %d\n", total_read, missed);
#endif

    /* in fact, that would never be happened */
    if ((char *)(cs->data) > (char*)0x100000)
        printf("the buffer addres higher than 1M limit\n");
    
    return cs;
}    


/**
 * Just print the sector, and according the LRU algorithm, 
 * Left most value is the most least secotr, and Right most 
 * value is the most Recent sector. I see it's a Left Right Used
 * (LRU) algorithm; Just kidding:)
 */
void print_cache(struct device *dev)
{
        int i = 0;
        struct cache_struct *cs = dev->cache_head;
        for (; i < dev->cache_entries; i++) {
                cs = cs->next;
                printf("%d(%p) ", cs->block, cs->data);
        }

        printf("\n");
}
