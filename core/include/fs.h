#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
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

#define FILENAME_MAX_LG2 8
#define FILENAME_MAX     (1 << FILENAME_MAX_LG2)

struct fs_info {
    const struct fs_ops *fs_ops;
    struct device *fs_dev;
};

struct open_file_t;		/* Filesystem private structure */
struct dirent;          /* Directory entry structure */

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
    void     (*mangle_name)(char *, const char *);
    char *   (*unmangle_name)(char *, const char *);
    void     (*load_config)(com32sys_t *);

	/* the _dir_ stuff */
	void     (*opendir)(com32sys_t *);
	struct dirent * (*readdir)(struct file *);
};

enum dev_type {CHS, EDD};

/*
 * Generic functions that filesystem drivers may choose to use
 */
void generic_mangle_name(char *, const char *);
#define generic_unmangle_name stpcpy

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

/*
 * Our definition of "not whitespace"
 */
static inline bool not_whitespace(char c)
{
  return (unsigned char)c > ' ';
}

/* 
 * functions
 */
void mangle_name(com32sys_t *);
void searchdir(com32sys_t *);
void _close_file(struct file *);
inline uint16_t file_to_handle(struct file *);
inline struct file *handle_to_file(uint16_t);

#endif /* FS_H */
