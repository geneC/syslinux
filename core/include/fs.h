#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include <com32.h>
#include "core.h"
#include "disk.h"

/* I don't know it's right or not */
#define USE_CACHE(device_num) (device_num >= 0x00 && device_num < 0xfe)


struct fs_info {
    char *fs_name;
    struct fs_ops *fs_ops;
    struct device *fs_dev;
};

struct file {
    void*    open_file; /* points to the fs-specific open_file_t */
    struct   fs_info *fs;
    uint32_t file_len;
};


struct fs_ops {
    /* in fact, we use fs_ops structure to find the right fs */
    char *fs_name;
    
    int      (*fs_init)(struct fs_info *);
    void     (*searchdir)(char *, struct file *);
    uint32_t (*getfssec)(struct fs_info *, char *, void * , int, int *);
    void     (*mangle_name)(char *, char *);
    void     (*unmangle_name)(void);
    void     (*load_config)(com32sys_t *);
};

enum dev_type {CHS, EDD};
    
/*
 * Struct device contains:
 *     the pointer points to the disk structure,
 *     the cache stuff.
 */
struct device {
    struct disk *disk;

    /* the cache stuff */
    char* cache_data;
    struct cache_struct* cache_head;
    uint16_t cache_block_size;
    uint16_t cache_entries;
    uint32_t cache_size;
};

#endif /* FS_H */
