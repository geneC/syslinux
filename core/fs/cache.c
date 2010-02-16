/*
 * core/cache.c: A simple LRU-based cache implementation.
 *
 */

#include <stdio.h>
#include <string.h>
#include "core.h"
#include "cache.h"


/*
 * Initialize the cache data structres. the _block_size_shift_ specify
 * the block size, which is 512 byte for FAT fs of the current 
 * implementation since the block(cluster) size in FAT is a bit big.
 *
 */
void cache_init(struct device *dev, int block_size_shift)
{
    struct cache *prev, *cur;
    char *data = dev->cache_data;
    struct cache *cache_head, *cache;
    int i;

    dev->cache_block_size = 1 << block_size_shift;
    dev->cache_entries = dev->cache_size >> block_size_shift;

    if (dev->cache_size < dev->cache_block_size + 2*sizeof(struct cache)) {
	dev->cache_head = NULL;
	return;			/* Cache unusably small */
    }

    while ((dev->cache_entries << block_size_shift) +
	   ((dev->cache_entries+1) * sizeof(struct cache))
	   > dev->cache_size)
	dev->cache_entries--;

    cache_head = (struct cache *)
	(data + (dev->cache_entries << block_size_shift));
    cache = cache_head + 1;

    cache_head->prev  = &cache[dev->cache_entries-1];
    cache_head->next->prev = cache_head;
    cache_head->block = (block_t)-1;
    cache_head->data  = NULL;

    prev = cache_head;
    
    for (i = 0; i < dev->cache_entries; i++) {
        cur = &cache[i];
        cur->data  = data;
        cur->block = (block_t)-1;
        cur->prev  = prev;
        prev->next = cur;
        data += dev->cache_block_size;
        prev = cur++;
    }
}

/*
 * Lock a block permanently in the cache
 */
void cache_lock_block(struct cache *cs)
{
    cs->prev->next = cs->next;
    cs->next->prev = cs->prev;

    cs->next = cs->prev = NULL;
}

/*
 * Check for a particular BLOCK in the block cache, 
 * and if it is already there, just do nothing and return;
 * otherwise pick a victim block and update the LRU link.
 */
struct cache *_get_cache_block(struct device *dev, block_t block)
{
    struct cache *head = dev->cache_head;
    struct cache *cs;
    int i;

    cs = dev->cache_head + 1;

    for (i = 0; i < dev->cache_entries; i++) {
	if (cs->block == block)
	    goto found;
	cs++;
    }
    
    /* Not found, pick a victim */
    cs = head->next;

found:
    /* Move to the end of the LRU chain, unless the block is already locked */
    if (cs->next) {
	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	
	cs->prev = head->prev;
	head->prev->next = cs;
	cs->next = head;
	head->prev = cs;
    }

    return cs;
}    

/*
 * Check for a particular BLOCK in the block cache, 
 * and if it is already there, just do nothing and return;
 * otherwise load it from disk and update the LRU link.
 * Return the data pointer.
 */
const void *get_cache(struct device *dev, block_t block)
{
    struct cache *cs;

    cs = _get_cache_block(dev, block);
    if (cs->block != block) {
	cs->block = block;
        getoneblk(dev->disk, cs->data, block, dev->cache_block_size);
    }

    return cs->data;
}
