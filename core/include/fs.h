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
    void *fs_info;             /* The fs-specific information */
    int blk_bits;              /* block_size = 1 << (blk_bits + SECTOR_SHIFT) */
};

extern struct fs_info *this_fs;

struct dirent;                  /* Directory entry structure */
struct file;
enum fs_flags {
    FS_NODEV   = 1 << 0,
    FS_USEMEM  = 1 << 1,         /* If we need a malloc routine, set it */

 /* 
  * Update the this_inode pointer at each part of path searching. This 
  * flag is just used for FAT and ISO fs for now.
  */
    FS_THISIND = 1 << 2,        
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
    int      (*load_config)();

    struct inode * (*iget_root)(void);
    struct inode * (*iget_current)(void);
    struct inode * (*iget)(char *, struct inode *);
    char * (*follow_symlink)(struct inode *, const char *);

    /* the _dir_ stuff */
    void     (*opendir)(com32sys_t *);
    struct dirent * (*readdir)(struct file *);
};

enum inode_mode {I_FILE, I_DIR, I_SYMLINK};

/* 
 * The inode structure, including the detail file information 
 */
struct inode {
    int          mode;   /* FILE , DIR or SYMLINK */
    uint32_t     size;
    uint32_t     ino;    /* Inode number */
    uint32_t     atime;  /* Access time */
    uint32_t     mtime;  /* Modify time */
    uint32_t     ctime;  /* Create time */
    uint32_t     dtime;  /* Delete time */
    int          blocks; /* How many blocks the file take */
    uint32_t *   data;   /* The block address array where the file stored */
    uint32_t     flags;
    int          blkbits;
    int          blksize;
    uint32_t     file_acl;
};

extern struct inode *this_inode;

struct open_file_t;

struct file {
    struct fs_info *fs;
    union {
	/* For the new universal-path_lookup */
	struct {
	    struct inode *inode;        /* The file-specific information */
	    uint32_t offset;            /* for next read */
	};

	/* For the old searhdir method */
	struct {
	    struct open_file_t *open_file;/* The fs-specific open file struct */
	    uint32_t file_len;
	};
    };
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

static inline void free_inode(struct inode * inode)
{
    if (inode) {
	if (inode->data)
	    free(inode->data);
	free(inode);
    }
}

static inline void malloc_error(char *obj)
{
        printf("Out of memory: can't allocate memory for %s\n", obj);
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
