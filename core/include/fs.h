#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include <com32.h>
#include "core.h"
#include "disk.h"

/*
 * Maximum number of open files.  This is *currently* constrained by the
 * fact that PXE needs to be able to fit all its packet buffers into a
 * 64K segment; this should be fixed by moving the packet buffers to high
 * memory.
 */
#define MAX_OPEN_LG2	5
#define MAX_OPEN	(1 << MAX_OPEN_LG2)

struct fs_info {
    const struct fs_ops *fs_ops;
    struct device *fs_dev;
};

struct open_file_t;		/* Filesystem private structure */

struct file {
    struct open_file_t *open_file; /* Filesystem private data */
    struct fs_info *fs;
    uint32_t file_len;
};

enum fs_flags {
    FS_NODEV = 1,
};

struct fs_ops {
    /* in fact, we use fs_ops structure to find the right fs */
    const char *fs_name;
    enum fs_flags fs_flags;
    
    int      (*fs_init)(struct fs_info *);
    void     (*searchdir)(char *, struct file *);
    uint32_t (*getfssec)(struct file *, char *, int, bool *);
    void     (*close_file)(struct file *);
    void     (*mangle_name)(char *, char *);
    int      (*unmangle_name)(char *, char *);
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
    void* cache_head;
    uint16_t cache_block_size;
    uint16_t cache_entries;
    uint32_t cache_size;
};

#endif /* FS_H */
