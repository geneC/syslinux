#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "core.h"
#include "disk.h"
#include "fat_fs.h"
#include "fs.h"


#define FILENAME_MAX_LG2 8
#define FILENAME_MAX     (1 << FILENAME_MAX_LG2)
#define MAX_OPEN_LG2     6
#define MAX_OPEN         (1 << MAX_OPEN_LG2)
#define ROOT_DIR_WORD    0x002f

/* file structure. This holds the information for each currently open file */
struct open_file_t {
        uint32_t file_sector;    /* sector pointer ( 0 = structure free ) */
        uint32_t file_bytesleft; /* number of bytes left */
        uint32_t file_left;      /* number of sectors left */
        uint32_t pad;            /* padding */
};

extern char Files[MAX_OPEN * sizeof(struct open_file_t)];
extern char trackbuf[8192];


/* the fat bpb data */
struct fat_bpb fat;
int FATType = 0;

/* generic information about FAT fs */
sector_t FAT;            /* Location of (first) FAT */
sector_t RootDirArea;    /* Location of root directory area */
sector_t RootDir;        /* Location of root directory proper */
sector_t DataArea;       /* Location of data area */
uint32_t TotalSectors;   /* Total number of sectors */
uint32_t ClustSize;      /* Bytes/cluster */
uint32_t ClustMask;      /* Sector/cluster - 1 */
uint8_t  CopySuper;      /* Distinguish .bs versus .bss */
uint8_t  DriveNumber;    /* BIOS drive number */
uint8_t  ClustShift;     /* Shift count for sectors/cluster */
uint8_t  ClustByteShift; /* Shift count for bytes/cluster */

int CurrentDir;
int PrevDir;

/* used for long name entry */
char MangleBuf[12];
char entry_name[14];

/* try with the biggest long name */
char long_name[0x40 * 13];
char *NameStart;
int  NameLen;

/* do this for readdir, because it called from asm and don't know the fs structure */
struct fs_info *this_fs = NULL;


/**
 * allocate_file:
 * 
 * Allocate a file structure
 *
 * @return: if successful return the file pointer, or return NULL
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
 * alloc_fill_dir:
 *
 * Allocate then fill a file structure for a directory starting in
 * sector SECTOR. if successful, return the pointer of filled file
 * structure, or return NULL.
 *
 */
void alloc_fill_dir(com32sys_t *regs)
{
    sector_t sector = regs->eax.l;
    struct open_file_t *file;
    
    file = allocate_file();
    if ( !file ) {
        regs->esi.w[0] = 0;
        return;
    }
    
    file->file_sector = sector; /* current sector */
    file->file_bytesleft = 0;   /* current offset */
    file->file_left = sector;   /* beginning sector */
    
    regs->esi.w[0] = OFFS_WRT(file, 0);
}


/* Deallocates a file structure */
static void close_file(struct open_file_t *file)
{
    if ( file )
        file->file_sector = 0;
}



/* Deallocates a directory structure */
/***********
void close_dir(struct fat_dir_entry *dir)
{
    if ( dir )
        *(uint32_t*)dir = 0;
}
***********/


/**
 * getfatsector:
 *
 * check for a particular sector in the FAT cache.
 *
 */
struct cache_struct *getfatsector(struct fs_info *fs, sector_t sector)
{
    return get_cache_block(fs->fs_dev, FAT + sector);
}


/**
 * nextcluster: 
 *
 * Advance a cluster pointer in clust_num to the next cluster
 * pointer at in the FAT tables. CF = 0 on return if end of file.
 *
 * @param: clust_num;
 *
 * @return: the next cluster number
 *
 */
static uint32_t nextcluster(struct fs_info *fs, uint32_t clust_num)
{
    uint32_t next_cluster;
    sector_t fat_sector;
    uint32_t offset;
    int lo, hi;
    struct cache_struct *cs;
            
    switch(FATType) {
    case FAT12:
        fat_sector = (clust_num + clust_num / 2) >> SECTOR_SHIFT;
        cs = getfatsector(fs, fat_sector);
        offset = (clust_num * 3 / 2) & ( SECTOR_SIZE -1 );
        if ( offset == 0x1ff ) {
            /* 
             * we got the end of the one fat sector, 
             * but we don't got we have(just one byte, we need two),
             * so store the low part, then read the next fat
             * sector, read the high part, then combine it.
             */
            lo = *(uint8_t *)(cs->data + offset);
            cs = getfatsector(fs, fat_sector + 1);
            hi = *(uint8_t *)cs->data;
            next_cluster = (hi << 8) + lo;
        } else 
            next_cluster = *(uint16_t *)(cs->data + offset);
        
        if ( clust_num & 0x0001 )
            next_cluster >>= 4;         /* cluster number is ODD */
        else
            next_cluster &= 0x0fff;     /* cluster number is EVEN */
        if ( next_cluster > 0x0ff0 )
            goto fail;
        break;
        
    case FAT16:
        fat_sector = clust_num >> (SECTOR_SHIFT - 1);
        offset = clust_num & ( (1 << (SECTOR_SHIFT-1)) -1);
        cs = getfatsector(fs, fat_sector);
        next_cluster = ((uint16_t *)cs->data)[offset];
        if ( next_cluster > 0xfff0 )
            goto fail;
        break;
        
    case FAT32:
        fat_sector = clust_num >> (SECTOR_SHIFT - 2);
        offset = clust_num & ( (1 << (SECTOR_SHIFT-2)) -1);
        cs = getfatsector(fs, fat_sector);
        next_cluster = ((uint32_t *)cs->data)[offset] & 0x0fffffff;
        if ( next_cluster > 0x0ffffff0 )
            goto fail;
        break;
    }
    
    return next_cluster;
    
 fail:  
    /* got an unexcepted cluster number, so return ZERO */
    return 0;
}



/**
 * nextsector:
 * 
 * given a sector  on input, return the next sector of the 
 * same filesystem object, which may be the root directory or a 
 * cluster chain. Returns EOF.
 *
 */
static sector_t nextsector(struct fs_info *fs, sector_t sector)
{
    sector_t data_sector;
    uint32_t cluster;
    
    if ( sector < DataArea ) {
        sector ++;
        /* if we reached the end of root area */
        if ( sector == DataArea ) 
            sector = 0; /* return 0 */
        return sector;
    }
    
    data_sector = sector - DataArea;
    if ( (data_sector+1) & ClustMask )      /* in a cluster */
        return (++sector);
    
    /* got a new cluster */
    cluster = nextcluster(fs, (data_sector >> ClustShift) + 2);
    if ( !cluster  ) 
        return 0;
    
    /* return the start of the new cluster */
    sector = ( (cluster - 2) << ClustShift ) + DataArea;
    return sector;
}





/**
 * __getfssec:
 *
 * get multiple sectors from a file
 *
 * This routine makes sure the subransfers do not cross a 64K boundary
 * and will correct the situation if it does, UNLESS *sectos* cross
 * 64K boundaries.
 *
 * @param: buf
 * @param: file structure
 * @param: sectors
 *
 */
static void __getfssec(struct fs_info *fs, char *buf, struct open_file_t *file, uint32_t sectors)
{
    sector_t curr_sector = file->file_sector;
    sector_t frag_start , next_sector;
    uint32_t con_sec_cnt;
    struct disk *disk = fs->fs_dev->disk;
    
    while (sectors) {
        /* get fragment */
        con_sec_cnt = 0;
        frag_start = curr_sector;
        
        do {
            /* get consective sector  count */
            con_sec_cnt ++;
            sectors --;
            if ( sectors == 0 )
                break;
            
            next_sector = nextsector(fs, curr_sector);
            if ( !next_sector )
                break;                        
        }while( next_sector == (++curr_sector) );
        
#if 0   /* Debug message */
        printf("You are reading stores at sector --0x%x--0x%x\n", 
               frag_start, frag_start + con_sec_cnt -1);
#endif  
                        
        /* do read */
        disk->rdwr_sectors(disk, (void *)buf, frag_start, con_sec_cnt, 0);
        buf += con_sec_cnt << 9;/* adjust buffer pointer */
        
        if ( !sectors )
            break;
        //curr_sector --;         /* this is the last sector actually read */
        curr_sector = next_sector;
    }
    
    /* update the file_sector filed for the next read */
    file->file_sector = nextsector(fs, curr_sector);        
}



/**
 * getfssec:
 *
 * get multiple sectors from a file 
 *
 *
 * @param: buf
 * @param: file
 * @param: sectors
 * @param: have_more
 *
 * @return: number of bytes read
 *
 */
uint32_t vfat_getfssec(struct fs_info *fs, char *buf, 
                  void *open_file, int sectors, int *have_more)
{
    uint32_t bytes_read = sectors << SECTOR_SHIFT;
    struct open_file_t *file = (struct open_file_t *)open_file;
    
    if ( sectors > file->file_left )
        sectors = file->file_left;
    
    __getfssec(fs, buf, file, sectors);
    
    if ( bytes_read >= file->file_bytesleft ) {
        bytes_read = file->file_bytesleft;
        *have_more = 0;
    } else
        *have_more = 1;    
    file->file_bytesleft -= bytes_read;
    file->file_left -= sectors;
    
    return bytes_read;
}

/**
 * mangle_name:
 * 
 * Mangle a filename pointed to by src into a buffer pointed to by dst; 
 * ends on encountering any whitespace.
 *
 */
void vfat_mangle_name(char *dst, char *src)
{
    char *p = dst;
    int i = FILENAME_MAX -1;
    
    while(*src > ' ') {
        if ( *src == '\\' )
            *src = '/';
        
        if (*src == '/') {
            if (*(src+1) == '/') {
                src ++;
                i --;
                continue;
            }
        }        
        i --;
        *dst++ = *src++;
    }

    while (1) {
        if (dst == p)
            break;        
        if ((*(dst-1) != '/') && (*(dst-1) != '.'))
            break;
        
        dst --;
        i ++;
    }

    i ++;
    for (; i > 0; i --)
        *dst++ = '\0';
}
 

/**
 * mangle_dos_name:
 *
 * Mangle a dos filename component pointed to by FILENAME
 * into MangleBuf; ends on encountering any whitespace or 
 * slash.
 *
 * WARNING: saves pointers into the buffer for longname matchs!
 *
 * @param: filename
 * @param: MangleBuf
 *
 */
/**
 * for now, it can't handle this case:
 * xyxzxyxjfdkfjdjf.txt as it will just output the first 11 chars
 * but not care the dot char at the later, so I think we need do 
 * this, but it seems that the SYSLINUX doesn't do it, so I will
 * make it stay as what it was orignal.
 *
 */
static void mangle_dos_name(char *MangleBuf, char *filename)
{
       
    char *dst = MangleBuf;
    char *src = filename;
    int i = 0;
    unsigned char c;        
    
    NameStart = filename;
    
    for (; i < 11; i ++)
        MangleBuf[i] = ' ';
    
    for (i = 0; i < 11; i++) {
        c = *src ++;
        
        if ( (c <= ' ') || (c == '/') ) 
            break;
        
        if ( c == '.' ) {
            dst = &MangleBuf[8];
            i = 7;
            continue;
        }
        
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if ( (c == 0xe5) && (i == 11) )
            c = 0x05;
        
        *dst++ = c;
    }
    MangleBuf[12] = '\0';
    
    while( (*src != '/') && (*src > ' ') )
        src ++;
    
    NameLen = src - filename;
}



static void unicode_to_ascii(char *entry_name, uint16_t *unicode_buf)
{
    int i = 0;
    
    for (; i < 13; i++) {
        if ( unicode_buf[i] == 0xffff ) {
            entry_name[i] = '\0';
            return;
        }
        entry_name[i] = (char)unicode_buf[i];
    }
}

/**
 * long_entry_name:
 *
 * get the long entry name
 *
 */
static void long_entry_name(struct fat_long_name_entry *dir)
{
    uint16_t unicode_buf[13];
    
    memcpy(unicode_buf,     dir->name1, 5 * 2);
    memcpy(unicode_buf + 5, dir->name2, 6 * 2);
    memcpy(unicode_buf + 11,dir->name3, 2 * 2);
    
    unicode_to_ascii(entry_name, unicode_buf);
    
}


static uint8_t get_checksum(char *dir_name)
{
    int  i;
    uint8_t sum=0;
    
    for (i=11; i; i--)
        sum = ((sum & 1) << 7) + (sum >> 1) + *dir_name++;
    return sum;
}

/* compute the first sector number of one dir where the data stores */
static inline sector_t first_sector(struct fat_dir_entry *dir)
{
    uint32_t first_clust, sector;
    
    first_clust = (dir->first_cluster_high << 16) + dir->first_cluster_low;
    sector = ((first_clust - 2) << ClustShift) + DataArea;
    
    return sector;
}


/**
 * search_dos_dir:
 *
 * search a specific directory for a pre-mangled filename in
 * MangleBuf, in the directory starting in sector SECTOR
 *
 * NOTE: This file considers finding a zero-length file an
 * error.  This is so we don't have to deal with that special
 * case elsewhere in the program (most loops have the test
 * at the end).
 *
 * @param: MangleBuf
 * @param: dir_sector, directory sector
 *
 * @out:  file pointer
 * @out:  file length (MAY BE ZERO!)
 * @out:  file attribute
 * @out:  dh, clobbered.
 *
 */
static struct open_file_t* search_dos_dir(struct fs_info *fs, char *MangleBuf, 
                                   uint32_t dir_sector, uint32_t *file_len, uint8_t *attr)
{
    struct open_file_t*  file;
    struct cache_struct* cs;
    struct fat_dir_entry *dir;
    struct fat_long_name_entry *long_dir;
    
    uint8_t  VFATInit, VFATNext, VFATCsum;
    uint8_t  id;
    uint32_t slots;
    uint32_t entries;
    int checksum;
        
    file = allocate_file();
    if ( !file )
                return NULL;
    
    /*
     * Compute the value of a possible VFAT longname
     * "last" entry (which, of coures, comes first ...)
     */
    slots = (NameLen + 12) / 13;
    slots |= 0x40;    
    VFATInit = slots;
    VFATNext = slots;    
    
    do {
        cs = get_cache_block(fs->fs_dev, dir_sector);
        dir = (struct fat_dir_entry *)cs->data;
        entries = SECTOR_SIZE / 32;
        
        /* scan all the entries in a sector */
        do {
            if ( dir->name[0] == 0 )
                return NULL;    /* Hit directory high water mark */
            
            if ( dir->attr == 0x0f ) {
                /* it's a long name entry */
                long_dir = (struct fat_long_name_entry *)dir;
                id = long_dir->id;
                if ( id !=VFATNext )
                    goto not_match;
                
                if ( id & 0x40 ) {
                    /*get the initial checksum value*/
                    VFATCsum = long_dir->checksum;
                } else {
                    if ( long_dir->checksum != VFATCsum )
                        goto not_match;
                }
                
                id &= 0x3f;
                VFATNext = --id;
                
                /* got the long entry name */
                long_entry_name(long_dir);
                memcpy(long_name + id * 13, entry_name, 13);
                
                /* 
                 * if we got the last entry?
                 * if so, check it, or go on with the next entry
                 */
                if ( id == 0 ) {
                    if ( strcmp(long_name, NameStart) )
                        goto not_match;
                }

                goto next_entry;
                
            } else {
                /* it's a short entry */
                if ( dir->attr & 0x08 )     /* ingore volume labels */
                    goto not_match;
                
                
                /* If we have a long name match, then VFATNext must be 0 */
                if ( !VFATNext )  {  
                    /*
                     * we already have a VFAT long name match, however,
                     * the match is only valid if the checksum matchs.
                     */
                    checksum = get_checksum(dir->name);
                    if ( checksum == VFATCsum )
                        goto found;        /* got a match on long name */
                    
                } else { 
                    if ( strncmp(MangleBuf, dir->name, 11) == 0 )
                        goto found;                                       
                }
            }
            
        not_match:/* find it again */
            VFATNext = VFATInit;
                
        next_entry:
            dir ++;
            
        }while ( --entries );
        
        dir_sector = nextsector(fs, dir_sector);
        
    }while ( dir_sector ); /* scan another secotr */

 found:
    *file_len = file->file_bytesleft = dir->file_size;
    file->file_sector = first_sector(dir);
    *attr = dir->attr;
    
    return file;
}



/**
 * searchdir:
 * 
 * open a file
 *
 * @param: filename, the file we wanna open
 * @param: file_len, to return the file length
 *
 * @return: return the file structure on successful, or NULL.
 *
 */
void vfat_searchdir(char *filename, struct file *file)
{
    sector_t dir_sector;
    uint32_t file_len = 0;
    uint8_t  attr = 0;
    char  *p;        
    struct open_file_t *open_file = NULL;

    if (file->fs != this_fs)
        this_fs = file->fs;
    
    dir_sector = CurrentDir;
    if ( *filename == '/' ) {
        dir_sector = RootDir;
        filename ++;
    }
    
    while ( *filename ) {
        p = filename;
        
        /* try to find the end */
        while ( (*p > ' ') && (*p != '/') )
            p ++;
        
        if (filename == p)
            //return NULL;
            goto fail;

        PrevDir = dir_sector;
        
        mangle_dos_name(MangleBuf, filename);
        open_file = search_dos_dir(file->fs, MangleBuf, dir_sector, &file_len, &attr);
        if (! open_file) 
            goto fail;
        
        if ( *p != '/' )   /* we got a file */                        
            break;
        
        if ( (attr & 0x10) == 0 ) /* subdirectory */
            //return NULL;
            goto fail;
        
        dir_sector = open_file->file_sector;
        close_file(open_file);
        
        filename = p + 1; /* search again */                
    }
    
    if ( (attr & 0x18) || (file_len == 0) ) {
    fail:
        file_len = 0;
        open_file = NULL;
    } else {
        open_file->file_bytesleft = file_len;
        open_file->file_left = ( file_len + SECTOR_SIZE -1 ) >> SECTOR_SHIFT;
    }

    file->file_len = file_len;
    file->open_file = (void *)open_file;
}




/**
 * readdir:
 *
 * read one file from a directory
 *
 * returns the file's name in the filename string buffer
 *
 * @param: filename
 * @param: file
 *
 */
void readdir(com32sys_t *regs)/*
                                struct fs_info *fs, struct open_file_t* dir_file,
                                char* filename, uint32_t *file_len, uint8_t *attr)
                              */
{
    uint32_t sector, sec_off;       
    
    /* make it to be 1 to check if we have met a long name entry before */
    uint8_t  id = 1;
    uint8_t  init_id, next_id;
    uint8_t  entries_left;  
    int i;

    char *filename = (char *)MK_PTR(regs->es, regs->edi.w[0]);
    struct open_file_t *dir_file = (struct open_file_t *)MK_PTR(regs->ds, regs->esi.w[0]);
    
    struct cache_struct  *cs;
    struct fat_dir_entry *dir;
    struct fat_long_name_entry *long_dir;
    struct open_file_t file;
    
    sector  = dir_file->file_sector;
    sec_off = dir_file->file_bytesleft;
    if ( !sector )
        goto fail;
    
    entries_left = (SECTOR_SIZE - sec_off) >> 5;
    cs = get_cache_block(this_fs->fs_dev, sector);
    dir = (struct fat_dir_entry *)(cs->data + sec_off);/* resume last position in sector */
    
    while ( 1 ) {
        if ( dir->name[0] == 0 )
            goto fail;                
        
        if  ( dir->attr == FAT_ATTR_LONG_NAME ) {
            /* it's a long name */
            long_dir = (struct fat_long_name_entry *)dir;
            
            if ( long_dir->id & 0x40 )  {
                init_id = id = long_dir->id & 0x3f;
                id--;
            } else {
                next_id = (long_dir->id & 0x3f) - 1;
                id--;            
                if ( id != next_id )
                    goto next_entry;
            }
            
            long_entry_name(long_dir);
            memcpy(filename + id * 13, entry_name, 13);
            
            
            /* 
             * we need go on with the next entry 
             * and we will fall through to next entry
             */
            
        } else {
            /* it's a short entry */
            
            if ( !id ) /* we got a long name match */
                break;
            
            if ( dir->attr & FAT_ATTR_VOLUME_ID ) 
                goto next_entry;
            
            for( i = 0; i < 8; i ++) {
                if ( dir->name[i] == ' ' )
                    break;
                *filename++ = dir->name[i];
            }
            
            *filename++ = '.';
            
            for ( i = 8; i < 11; i ++) {
                if ( dir->name[i] == ' ' )
                    break;
                *filename ++ = dir->name[i];
            }
            
            /* check if we have got an extention */
            if ( ! *(filename - 1) ) 
                *(filename -2) = '\0';
            else
                *filename = '\0';      
            
            break;
        }
        
    next_entry:
        dir ++;
        entries_left --;
        
        if ( !entries_left ) {
            sector = nextsector(this_fs, sector);
            if ( !sector )
                goto fail;
            cs = get_cache_block(this_fs->fs_dev, sector);
            dir = (struct fat_dir_entry *)cs->data;
        }
    }
    
        /* finally , we get what we want */
    entries_left --;
    if ( !entries_left ) {
        sector = nextsector(this_fs, sector);
        if ( !sector )
            goto fail;
    }

    file.file_sector = sector;
    file.file_bytesleft = (SECTOR_SIZE - (entries_left << DIRENT_SHIFT) ) & 0xffff;
    
    regs->eax.l = dir->file_size;
    regs->ebx.l = first_sector(dir);
    regs->edx.b[0] = dir->attr;
    
    return;
    
 fail:
    //close_dir(dir);
    regs->eax.l = 0;
}

void vfat_load_config(com32sys_t *regs)
{
    char syslinux_cfg1[] = "/boot/syslinux/syslinux.cfg";
    char syslinux_cfg2[] = "/syslinux/syslinux.cfg";
    char syslinux_cfg3[] = "/syslinux.cfg";
    char config_name[] = "syslinux.cfg";
    

    char *syslinux_cfg[]= {syslinux_cfg1, syslinux_cfg2, syslinux_cfg3};
    com32sys_t oregs;
    int i = 0;
    
    *(uint16_t *)CurrentDirName = ROOT_DIR_WORD;
    CurrentDir = RootDir;

    /* 
     * we use the ConfigName to pass the config path because
     * it is under the address 0xffff
     */
    regs->edi.w[0] = OFFS_WRT(ConfigName, 0);
    for (; i < 3; i++) {
        strcpy(ConfigName, syslinux_cfg[i]);
        memset(&oregs, 0, sizeof oregs);
        call16(core_open, regs, &oregs);

        /* if zf flag set, then failed; try another */
        if (! (oregs.eflags.l & EFLAGS_ZF))
            break;
    }
    if ( i == 3 ) {
        printf("no config file found\n");
        return;  /* no config file */
    }

    strcpy(ConfigName, config_name);
    strcpy(CurrentDirName, syslinux_cfg[i]);
    CurrentDirName[strlen(syslinux_cfg[i])-strlen(config_name)] = '\0';
    CurrentDir = PrevDir;
}
 
static inline void bsr(uint8_t *res, int num)
{
        int i = 0;
        while (num >>= 1)
                i ++;
        *res = i;
}

/* init. the fs meta data, return the block size in bits */
int vfat_fs_init(struct fs_info *fs)
{
    int   sectors_per_fat; 
    uint32_t clust_num;
    int RootDirSize;
    struct disk *disk = fs->fs_dev->disk;
    
    /* get the fat bpb information */
    disk->rdwr_sectors(disk, (void *)&fat, 0, 1, 0);
    
    TotalSectors = fat.bxSectors ? : fat.bsHugeSectors;
    FAT = fat.bxResSectors;
    
    sectors_per_fat = fat.bxFATsecs ? : fat.u.fat32.bxFATsecs_32;
    RootDir = RootDirArea = FAT + sectors_per_fat * fat.bxFATs;
    RootDirSize = (fat.bxRootDirEnts+SECTOR_SIZE/32-1) >> (SECTOR_SHIFT-5);
    DataArea = RootDirArea + RootDirSize;
    
    bsr(&ClustShift, fat.bxSecPerClust);
    ClustByteShift = ClustShift + SECTOR_SHIFT;
    ClustMask = fat.bxSecPerClust - 1;
    ClustSize = fat.bxSecPerClust << SECTOR_SHIFT;    
        
    clust_num = (TotalSectors - DataArea) >> ClustShift;
    if ( clust_num < 4085 )
        FATType = FAT12;
    else if ( clust_num < 65525 )
        FATType = FAT16;
    else 
        FATType = FAT32;
    
    /* for SYSLINUX, the cache is based on sector size */
    return SECTOR_SHIFT;
}
        
const struct fs_ops vfat_fs_ops = {
    .fs_name       = "vfat",
    .fs_init       = vfat_fs_init,
    .searchdir     = vfat_searchdir,
    .getfssec      = vfat_getfssec,
    .mangle_name   = vfat_mangle_name,
    .unmangle_name = NULL,
    .load_config   = vfat_load_config
};
