#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <com32.h>
#include <stdio.h>
#include <sys/dirent.h>
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

#define CURRENTDIR_MAX	FILENAME_MAX

#define BLOCK_SIZE(fs)   ((fs)->block_size)
#define BLOCK_SHIFT(fs)	 ((fs)->block_shift)
#define SECTOR_SIZE(fs)  ((fs)->sector_size)
#define SECTOR_SHIFT(fs) ((fs)->sector_shift)

struct fs_info {
    const struct fs_ops *fs_ops;
    struct device *fs_dev;
    void *fs_info;             /* The fs-specific information */
    int sector_shift, sector_size;
    int block_shift, block_size;
    struct inode *root, *cwd;	   	/* Root and current directories */
    char cwd_name[CURRENTDIR_MAX];	/* Current directory by name */
};

extern struct fs_info *this_fs;

struct dirent;                  /* Directory entry structure */
struct file;
enum fs_flags {
    FS_NODEV   = 1 << 0,
    FS_USEMEM  = 1 << 1,        /* If we need a malloc routine, set it */
    FS_THISIND = 1 << 2,        /* Set cwd based on config file location */
};

struct fs_ops {
    /* in fact, we use fs_ops structure to find the right fs */
    const char *fs_name;
    enum fs_flags fs_flags;
    
    int      (*fs_init)(struct fs_info *);
    void     (*searchdir)(const char *, struct file *);
    uint32_t (*getfssec)(struct file *, char *, int, bool *);
    void     (*close_file)(struct file *);
    void     (*mangle_name)(char *, const char *);
    size_t   (*realpath)(struct fs_info *, char *, const char *, size_t);
    int      (*chdir)(struct fs_info *, const char *);
    int      (*load_config)(void);

    struct inode * (*iget_root)(struct fs_info *);
    struct inode * (*iget)(const char *, struct inode *);
    int	     (*readlink)(struct inode *, char *);

    /* the _dir_ stuff */
    int	     (*readdir)(struct file *, struct dirent *);

    int      (*next_extent)(struct inode *, uint32_t);
};

/*
 * Extent structure: contains the mapping of some chunk of a file
 * that is contiguous on disk.
 */
struct extent {
    sector_t	pstart;		/* Physical start sector */
    uint32_t	lstart;		/* Logical start sector */
    uint32_t	len;		/* Number of contiguous sectors */
};

/* Special sector numbers used for struct extent.pstart */
#define EXTENT_ZERO	((sector_t)-1) /* All-zero extent */
#define EXTENT_VOID	((sector_t)-2) /* Invalid information */

#define EXTENT_SPECIAL(x)	((x) >= EXTENT_VOID)

/* 
 * The inode structure, including the detail file information 
 */
struct inode {
    struct fs_info *fs;	 /* The filesystem this inode is associated with */
    struct inode *parent;	/* Parent directory, if any */
    int		 refcnt;
    int          mode;   /* FILE , DIR or SYMLINK */
    uint32_t     size;
    uint32_t	 blocks; /* How many blocks the file take */
    uint32_t     ino;    /* Inode number */
    uint32_t     atime;  /* Access time */
    uint32_t     mtime;  /* Modify time */
    uint32_t     ctime;  /* Create time */
    uint32_t     dtime;  /* Delete time */
    uint32_t     flags;
    uint32_t     file_acl;
    struct extent this_extent, next_extent;
    char         pvt[0]; /* Private filesystem data */
};

struct file {
    struct fs_info *fs;
    uint32_t offset;            /* for next read */
    struct inode *inode;        /* The file-specific information */
};

/*
 * Struct device contains:
 *     the pointer points to the disk structure,
 *     the cache stuff.
 */
struct cache;

struct device {
    struct disk *disk;

    /* the cache stuff */
    char *cache_data;
    struct cache *cache_head;
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
 * Inode allocator/deallocator
 */
struct inode *alloc_inode(struct fs_info *fs, uint32_t ino, size_t data);
static inline void free_inode(struct inode * inode)
{
    free(inode);
}

static inline struct inode *get_inode(struct inode *inode)
{
    inode->refcnt++;
    return inode;
}

void put_inode(struct inode *inode);

static inline void malloc_error(char *obj)
{
        printf("Out of memory: can't allocate memory for %s\n", obj);
	kaboom();
}

/*
 * File handle conversion functions
 */
extern struct file files[];
static inline uint16_t file_to_handle(struct file *file)
{
    return file ? (file - files)+1 : 0;
}
static inline struct file *handle_to_file(uint16_t handle)
{
    return handle ? &files[handle-1] : NULL;
}

/* fs.c */
void pm_mangle_name(com32sys_t *);
void pm_searchdir(com32sys_t *);
void mangle_name(char *, const char *);
int searchdir(const char *name);
void _close_file(struct file *);
size_t pmapi_read_file(uint16_t *handle, void *buf, size_t sectors);
int open_file(const char *name, struct com32_filedata *filedata);
void pm_open_file(com32sys_t *);
void close_file(uint16_t handle);
void pm_close_file(com32sys_t *);

/* chdir.c */
void pm_realpath(com32sys_t *regs);
size_t realpath(char *dst, const char *src, size_t bufsize);
int chdir(const char *src);

/* readdir.c */
DIR *opendir(const char *pathname);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

/* getcwd.c */
char *getcwd(char *buf, size_t size);

/*
 * Generic functions that filesystem drivers may choose to use
 */

/* mangle.c */
void generic_mangle_name(char *, const char *);

/* loadconfig.c */
int search_config(const char *search_directores[], const char *filenames[]);
int generic_load_config(void);

/* close.c */
void generic_close_file(struct file *file);

/* getfssec.c */
uint32_t generic_getfssec(struct file *file, char *buf,
			  int sectors, bool *have_more);

/* nonextextent.c */
int no_next_extent(struct inode *, uint32_t);

#endif /* FS_H */
