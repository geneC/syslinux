#ifndef FS_H
#define FS_H

#include <com32.h>
#include "core.h"
#include "disk.h"

struct fs_info {
    char *fs_name;
    struct fs_ops *fs_ops;
    struct device *fs_dev;
};

struct file{
    void*    open_file; /* points to the fs-specific open_file_t */
    struct   fs_info *fs;
    uint32_t file_len;
};


struct fs_ops {
    /* in fact, we use fs_ops structure to find the right fs */
    char *fs_name;
    
    int      (*fs_init)(void);
    void     (*searchdir)(char *, struct file *);
    uint32_t (*getfssec)(struct fs_info *, char *, void * , int, int *);
    void     (*mangle_name)(char *, char *);
    void     (*unmangle_name)(void);
    void     (*load_config)(com32sys_t *);
};

enum dev_type {CHS, EDD};

/*
 * Struct device should have all the information about a specific disk
 * structure, and contain either a pointer to the metadata cache or 
 * actually contain the cache itself.
 * 
 * All the information in this case is stuff like BIOS device number, 
 * type of access (CHS, EDD, ...), geometry, partition offset, and 
 * sector size.
 * 
 * It would be usefull and much easier to implement the C version getlinsec
 * later(I have not much time to implement it now, so I will leave it for 
 * a while, maybe a long while).
 */
struct device {
    /* the device numger (in BIOS style ) */
    uint8_t device_number;
    
    /* type of access (CHS or EDD ) */
    uint8_t type;

    /* the sector size, 512B for disk and floppy, 2048B for CD */
    uint16_t sector_size;

    /* the start address of this partition(in sectors) */
    sector_t part_start;
       
    void (*read_sectors)(char *, sector_t, int );

    /* 
     * I think we still need the cache_data filed here, 'cause hpa said 
     * different device has diffrent cache buffer, and the following filed
     * are quite for cache parts. 
     */
    char*    cache_data;
    struct  cache_struct *cache_head;
    uint16_t cache_block_size;
    uint16_t cache_entries;
    uint32_t cache_size;
};



#endif /* FS_H */
