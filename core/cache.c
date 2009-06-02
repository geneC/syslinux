#include "core.h"
#include "cache.h"

#include <stdio.h>
#include <string.h>


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
static int cache_block_size;
static int cache_entries;

/**
 * cache_init:
 *
 * Initialize the cache data structres.
 * regs->eax.l stores the block size(in bits not bytes)
 *
 */
void cache_init(com32sys_t * regs)
{
        struct cache_struct *prev, *cur;
        char *data = core_cache_buf;
        int block_size_shift = regs->eax.l;
        int i;

        cache_block_size = 1 << block_size_shift;
        cache_entries = sizeof(core_cache_buf) >> block_size_shift;
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
                data += cache_block_size;
                prev = cur++;
        }
}


void getoneblk(char *buf, uint32_t block, int block_size)
{
        int sec_per_block = block_size >> 9; /* 512==sector size */
        com32sys_t regs;
        
        memset(&regs, 0, sizeof(regs) );

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
 * I just found that I was tring to do a stupid thing!
 * I haven't change the fs code to c, so for now the cache is based
 * on SECTOR SIZE but not block size. While we can fix it easily by 
 * make the block size be the sector size.
 *
 * @return: the data stores at gs:si
 *
 */
void get_cache_block(com32sys_t * regs)
{
    /* let's find it from the end, 'cause the endest is the freshest */
    struct cache_struct *cs = cache_head.prev;
    struct cache_struct *head,  *last;
    uint32_t block = regs->eax.l;
    int i;

    static int total_read;
    static int missed;
        
#if 0
    printf("we are looking for cache of %d\n", block);
#endif

    if ( !block ) {
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

        missed ++;
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

        total_read ++;

#if 0 /* testing how efficiency the cache is */
    if ( total_read % 5 == 0 ) 
        printf("total_read %d\tmissed %d\n", total_read, missed);
#endif

    /* in fact, that would never be happened */
    if ( (char *)(cs->data) > (char*)0x100000 )
        printf("the buffer addres higher than 1M limit\n\r");
    
    regs->gs = SEG(cs->data);
    regs->esi.w[0]= OFFS(cs->data);
}    
