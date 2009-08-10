#include <stdio.h>
#include <string.h>
//#include "cache.h"
#include "core.h"
#include "disk.h"
#include "iso9660_fs.h"
#include "fs.h"

#define DEBUG 1

#define FILENAME_MAX_LG2 8
#define FILENAME_MAX     (1 << FILENAME_MAX_LG2)
#define MAX_OPEN_LG2     6
#define MAX_OPEN         (1 << MAX_OPEN_LG2)
#define ISO_SECTOR_SHIFT 11
#define ISO_SECTOR_SIZE  (1 << ISO_SECTOR_SHIFT)
#define ROOT_DIR_WORD    0x002f
#define TRACKBUF_SIZE    8192


struct open_file_t {
        sector_t file_sector;
        uint32_t file_bytesleft;
        uint32_t file_left;
};

static struct open_file_t __bss16 Files[MAX_OPEN];

struct dir_t {
        uint32_t dir_lba;        /* Directory start (LBA) */
        uint32_t dir_len;        /* Length in bytes */
        uint32_t dir_clust;      /* Length in clusters */
};
static struct dir_t RootDir;
static struct dir_t CurrentDir;

static uint16_t BufSafe = TRACKBUF_SIZE >> ISO_SECTOR_SHIFT;

static char ISOFileName[64];      /* ISO filename canonicalizatin buffer */
static char *ISOFileNameEnd = &ISOFileName[64];

/* 
 * use to store the block shift, since we treat the hd-mode as 512 bytes
 * sector size, 2048 bytes block size. we still treat the cdrom as 2048  
 * bytes sector size and also the block size.
 */
static int block_shift;

/**
 * allocate_file:
 *
 * allocate a file structure
 *
 */
static struct open_file_t *allocate_file()
{
    struct open_file_t *file;
    int i = 0;
    
    file = (struct open_file_t *)Files;
    for (; i < MAX_OPEN; i ++ ) {
        if ( file->file_sector == 0 ) /* found it */
            return file;
        file ++;
    }
    
    return NULL; /* not found */
}
  

/**
 * close_file:
 * 
 * Deallocates a file structure 
 *
 */
static void close_file(struct open_file_t *file)
{
    if (file)
        file->file_sector = 0;
}

/**
 * mangle_name:
 *
 * Mangle a filename pointed to by src into a buffer pointed
 * to by dst; ends on encountering any whitespace.
 * dst is preserved.
 *
 * This verifies that a filename is < FilENAME_MAX characters, 
 * doesn't contain whitespace, zero-pads the output buffer,
 * and removes trailing dots and redumndant slashes, so "repe
 * cmpsb" can do a compare, and the path-searching routine gets
 * a bit of an easier job.
 *
 */
static void iso_mangle_name(char *dst, char *src)
{
    char *p = dst;
    int i = FILENAME_MAX - 1;
    
    while ( *src > ' ' ) {
        if ( *src == '/' ) {
            if ( *(src+1) == '/' ) {
                i --;
                src ++;
                continue;
            }
        }
        
        *dst++ = *src ++;
        i --;
    }
    
    while ( 1 ) {
        if ( dst == p )
            break;
        
        if ( (*(dst-1) != '.') && (*(dst-1) != '/') ) 
            break;
        
        dst --;
        i ++;
    }
    
    i ++;
    for (; i > 0; i -- )
        *dst++ = '\0';
}
    

/*
 * Does the opposite of mangle_name; converts a DOS-mangled
 * filename to the conventional representation.  This is 
 * needed for the BOOT_IMAGE= parameter for the kernel.
 *
 * it returns the lenght of the filename.
 */
static int iso_unmangle_name(char *dst, char *src)
{
    strcpy(dst, src);
    return strlen(src);
}

/**
 * compare the names si and di and report if they are
 * equal from an ISO 9600 perspective. 
 *
 * @param: de_name, the name from the file system.
 * @param: len, the length of de_name, and will return the real name of the de_name
 *              ';' and other terminates excluded.
 * @param: file_name, the name we want to check, is expected to end with a null
 *
 * @return: 1 on match, or 0.
 *
 */
static int iso_compare_names(char *de_name, int *len, char *file_name)
{        
    char *p  = ISOFileName;
    char c1, c2;
    
    int i = 0;
    
    while ( (i < *len) && *de_name && (*de_name != ';') && (p < ISOFileNameEnd - 1) ) {
        *p++ = *de_name++;
        i++;
    }
    
    /* Remove terminal dots */
    while ( *(p-1) == '.' ) {
        if ( *len <= 2 )
            break;
        
        if ( p <= ISOFileName )
            break;
        p --;
        i--;
    }
    
    if ( i <= 0 )
        return 0;
    
    *p = '\0';
    
    /* return the 'real' length of de_name */
    *len = i;
    
    p = ISOFileName;
    
    /* i is the 'real' name length of file_name */
    while ( i ) {
        c1 = *p++;
        c2 = *file_name++;
        
        if ( (c1 == 0) && (c2 == 0) )
            return 1; /* success */
        
        else if ( (c1 == 0) || ( c2 == 0 ) )
            return 0;
        
        c1 |= 0x20;
        c2 |= 0x20;          /* convert to lower case */
        if ( c1 != c2 )
            return 0;
        i --;
    }
    
    return 1;
}

static inline int cdrom_read_sectors(struct disk *disk, void *buf, int block, int count)
{
    /* changed those to _sector_ */
    block <<= block_shift;
    count <<= block_shift;
    return disk->rdwr_sectors(disk, buf, block, count, 0);
}

/**
 * iso_getfssec:
 *
 * Get multiple clusters from a file, given the file pointer.
 *
 * @param: buf
 * @param: file, the address of the open file structure
 * @param: sectors, how many we want to read at once 
 * @param: have_more, to indicate if we have reach the end of the file
 *
 */
static uint32_t iso_getfssec(struct fs_info *fs, char *buf, 
                      void *open_file, int sectors, int *have_more)
{
    uint32_t bytes_read = sectors << ISO_SECTOR_SHIFT;
    struct open_file_t *file = (struct open_file_t *)open_file;
    struct disk *disk = fs->fs_dev->disk;
    
    if ( sectors > file->file_left )
        sectors = file->file_left;
    
    cdrom_read_sectors(disk, buf, file->file_sector, sectors);
    
    file->file_sector += sectors;
    file->file_left   -= sectors;
    
    if ( bytes_read >= file->file_bytesleft ) {
        bytes_read = file->file_bytesleft;
        *have_more = 0;
    } else
        *have_more = 1;
    file->file_bytesleft -= bytes_read;
    
    return bytes_read;
}



/**
 * do_search_dir:
 *
 * find a file or directory with name within the _dir_ directory.
 * 
 * the return value will tell us what we find, it's a file or dir?
 * on 1 be dir, 2 be file, 0 be error.
 *
 * res will return the result.
 *
 */
static int do_search_dir(struct fs_info *fs, struct dir_t *dir, 
                  char *name, uint32_t *file_len, void **res)
{
    struct open_file_t *file;
    struct iso_dir_entry *de;
    struct iso_dir_entry tmpde;
    
    uint32_t offset = 0;  /* let's start it with the start */
    uint32_t file_pos = 0;
    char *de_name;
    int de_len;
    int de_name_len;
    int have_more;
    
    file = allocate_file();
    if ( !file )
        return 0;
    
    file->file_left = dir->dir_clust;
    file->file_sector = dir->dir_lba;
    
    iso_getfssec(fs, trackbuf, file, BufSafe, &have_more);
    de = (struct iso_dir_entry *)trackbuf;
    
    while ( file_pos < dir->dir_len ) {
        int found = 0;
        
        if ( (char *)de >= (char *)(trackbuf + TRACKBUF_SIZE) ) {
            if ( !have_more ) 
                return 0;
            
            iso_getfssec(fs, trackbuf, file, BufSafe, &have_more);
            offset = 0;
        }
        
        de = (struct iso_dir_entry *) (trackbuf + offset);
        
        de_len = de->length;
        
        if ( de_len == 0) {
            offset = file_pos = (file_pos+ISO_SECTOR_SIZE) & ~(ISO_SECTOR_SIZE-1);
            continue;
        }
        
        
        offset += de_len;
        
        /* Make sure we have a full directory entry */
        if ( offset >= TRACKBUF_SIZE ) {
            int slop = TRACKBUF_SIZE - offset + de_len;
            memcpy(&tmpde, de, slop);
            offset &= TRACKBUF_SIZE - 1;
            file->file_sector ++;
            if ( offset ) {
                if ( !have_more ) 
                    return 0;
                iso_getfssec(fs, trackbuf, file, BufSafe, &have_more);
                memcpy((void*)&tmpde + slop, trackbuf, offset);
            }
            de = &tmpde;
        }
        
        if ( de_len < 33 ) {
            printf("Corrutped directory entry in sector %d\n", file->file_sector);
            return 0;
        }
        
        de_name_len = de->name_len;
        de_name = (char *)((void *)de + 0x21);
        
        
        if ( (de_name_len == 1) && (*de_name == 0) ) {
            found = iso_compare_names(".", &de_name_len, name);
            
        } else if ( (de_name_len == 1) && (*de_name == 1) ) {
            de_name_len = 2;
            found = iso_compare_names("..", &de_name_len, name);
            
        } else 
            found = iso_compare_names(de_name, &de_name_len, name);
        
        if (found)
            break;
        
        file_pos += de_len;
    }
    
    if ( file_pos >= dir->dir_len ) 
        return 0; /* not found */
    

    if ( *(name+de_name_len) && (*(name+de_name_len) != '/' ) ) {
        printf("Something wrong happened during searching file %s\n", name);
        
        *res = NULL;
        return 0;
    }
    
    if ( de->flags & 0x02 ) {
        /* it's a directory */        
        dir = &CurrentDir;        
        dir->dir_lba = *(uint32_t *)de->extent;
        dir->dir_len = *(uint32_t *)de->size;
        dir->dir_clust = (dir->dir_len + ISO_SECTOR_SIZE - 1) >> ISO_SECTOR_SHIFT;
        
        *file_len = dir->dir_len;
        *res = dir;
        
        /* we can close it now */
        close_file(file); 
                
        /* Mark we got a directory */
        return 1;        
    } else {
        /* it's a file */
        file->file_sector    = *(uint32_t *)de->extent;
        file->file_bytesleft = *(uint32_t *)de->size;
        file->file_left = (file->file_bytesleft + ISO_SECTOR_SIZE - 1) >> ISO_SECTOR_SHIFT;

        *file_len = file->file_bytesleft;
        *res = file;
        
        /* Mark we got a file */
        return 2;
    }    
}


/**
 * iso_searchdir:
 *
 * open a file
 *
 * searchdir_iso is a special entry point for ISOLINUX only. In addition
 * to the above, searchdir_iso passes a file flag mask in AL. This is 
 * useful for searching for directories.
 *
 * well, it's not like the searchidr function in EXT fs or FAT fs; it also
 * can read a diretory.(Just thought of mine, liu)
 *
 */
static void iso_searchdir(char *filename, struct file *file)
{
    struct open_file_t *open_file = NULL;
    struct dir_t *dir;
    uint32_t file_len = 0;
    int ret;
    void *res;
        
    dir = &CurrentDir;
    if ( *filename == '/' ) {
        dir = &RootDir;
        filename ++;
    }

    while ( *filename ) {
        ret = do_search_dir(file->fs, dir, filename, &file_len, &res);
        if ( ret == 1 )
            dir = (struct dir_t *)res;
        else if ( ret == 2 )
            break;
        else 
            goto err;
       
        /* find the end */
        while ( *filename && (*filename != '/') )
            filename ++;
        
        /* skip the slash */
        while ( *filename && (*filename == '/') )
            filename++;   
    }
 
    /* well , we need recheck it , becuase it can be a directory */
    if ( ret == 2 ) {
        open_file = (struct open_file_t *)res;
        goto found;
    } else {
        open_file = allocate_file();
        if ( !open_file )
            goto err;
        
        open_file->file_sector = dir->dir_lba;
        open_file->file_bytesleft = dir->dir_len;
        open_file->file_left = (dir->dir_len + ISO_SECTOR_SIZE - 1) >> ISO_SECTOR_SHIFT;
        goto found;
    }
 err:
    close_file(open_file);
    file_len = 0;
    open_file = NULL;
 
 found:
    file->file_len = file_len;
    file->open_file = (void*)open_file;

#if 0
    if (open_file) {
        printf("file bytesleft: %d\n", open_file->file_bytesleft);
        printf("file sector   : %d\n", open_file->file_sector);
        printf("file in sector: %d\n", open_file->file_in_sec);
        printf("file offsector: %d\n", open_file->file_in_off);
    }
#endif
}

static void iso_load_config(com32sys_t *regs)
{
    char *config_name = "isolinux.cfg";
    com32sys_t out_regs;
    
    strcpy(ConfigName, config_name);
    
    regs->edi.w[0] = OFFS_WRT(ConfigName, 0);
    memset(&out_regs, 0, sizeof out_regs);
    call16(core_open, regs, &out_regs);
}


static int iso_fs_init(struct fs_info *fs)
{
    char *iso_dir;
    char *boot_dir  = "/boot/isolinux";
    char *isolinux_dir = "/isolinux";
    int len;
    int bi_pvd = 16;   
    struct file file;
    struct open_file_t *open_file;
    struct disk *disk = fs->fs_dev->disk;
    
    block_shift = ISO_SECTOR_SHIFT - disk->sector_shift;
    cdrom_read_sectors(disk, trackbuf, bi_pvd, 1);
    CurrentDir.dir_lba = RootDir.dir_lba = *(uint32_t *)(trackbuf + 156 + 2);
    
#ifdef DEBUG
    printf("Root directory at LBA = 0x%x\n", RootDir.dir_lba);
#endif
    
    CurrentDir.dir_len = RootDir.dir_len = *(uint32_t*)(trackbuf + 156 + 10);
    CurrentDir.dir_clust = RootDir.dir_clust = (RootDir.dir_len + ISO_SECTOR_SIZE - 1) >> ISO_SECTOR_SHIFT;
    
    /*
     * Look for an isolinux directory, and if found,
     * make it the current directory instead of the
     * root directory.
     *
     * Also copy the name of the directory to CurrrentDirName
     */
    *(uint16_t *)CurrentDirName = ROOT_DIR_WORD;
    
    iso_dir = boot_dir;
    file.fs = fs;
    iso_searchdir(boot_dir, &file);         /* search for /boot/isolinux */
    if ( !file.file_len ) {
        iso_dir = isolinux_dir;
        iso_searchdir(isolinux_dir, &file); /* search for /isolinux */
        if ( !file.file_len ) {
            printf("No isolinux directory found!\n");
            return 0;
        }            
    }
    
    strcpy(CurrentDirName, iso_dir);
    len = strlen(CurrentDirName);
    CurrentDirName[len]    = '/';
    CurrentDirName[len+1]  = '\0';
    
    open_file = (struct open_file_t *)file.open_file;
    CurrentDir.dir_len    = open_file->file_bytesleft;
    CurrentDir.dir_clust  = open_file->file_left;
    CurrentDir.dir_lba    = open_file->file_sector;
    close_file(open_file);
    
#ifdef DEBUG
    printf("isolinux directory at LBA = 0x%x\n", CurrentDir.dir_lba);
#endif  
      
    /* we do not use cache for now, so we can just return 0 */
    return 0;
}


const struct fs_ops iso_fs_ops = {
    .fs_name       = "iso",
    .fs_init       = iso_fs_init,
    .searchdir     = iso_searchdir,
    .getfssec      = iso_getfssec,
    .mangle_name   = iso_mangle_name,
    .unmangle_name = iso_unmangle_name,
    .load_config   = iso_load_config
};
