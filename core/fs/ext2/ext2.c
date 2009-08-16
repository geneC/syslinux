#include <stdio.h>
#include <string.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include "ext2_fs.h"

#define MAX_SYMLINKS     64
#define SYMLINK_SECTORS  2
static char SymlinkBuf[SYMLINK_SECTORS * SECTOR_SIZE + 64];

/* 
 * File structure, This holds the information for each currently open file 
 */
struct open_file_t {
    uint32_t file_bytesleft;  /* Number of bytes left (0 = free) */
    uint32_t file_sector;     /* Next linear sector to read */
    sector_t file_in_sec;     /* Sector where inode lives */
    uint16_t file_in_off;
    uint16_t file_mode;
    uint32_t pad[3];          /* pad to 2^5 == 0x20 bytes */
};
static struct open_file_t Files[MAX_OPEN];

static struct ext2_inode this_inode;
static struct ext2_super_block sb;

static uint16_t ClustByteShift,  ClustShift;
static uint32_t SecPerClust, ClustSize, ClustMask;
static uint32_t PtrsPerBlock1, PtrsPerBlock2, PtrsPerBlock3;
static int DescPerBlock, InodePerBlock;

/*
 * just like the function strcpy(), except it returns non-zero if overflow.
 * 
 */
static int strecpy(char *dst, char *src, char *end)
{
    while (*src != '\0')
        *dst++ = *src++;
    *dst = '\0';
    
    if (dst > end)
        return 1;
    else 
        return 0;
}


/*
 * Allocate a file structure,  if successful return the file pointer,  or NULL.
 *
 */
static struct open_file_t *allocate_file(void)
{
    struct open_file_t *file = Files;
    int i;
        
    for (i = 0; i < MAX_OPEN; i++) {
        if (file->file_bytesleft == 0) /* found it */
            return file;
        file++;
    }
    
    return NULL; /* not found */
}


/**
 * ext2_close_file:
 *
 * Deallocates a file structure point by FILE
 *
 * @param: file, the file structure we want deallocate
 *
 */
static inline void close_pvt(struct open_file_t *of)
{
    of->file_bytesleft = 0;
}

static void ext2_close_file(struct file *file)
{
    close_pvt(file->open_file);
}

/**
 * get the group's descriptor of group_num
 *
 * @param: group_num, the group number;
 * 
 * @return: the pointer of the group's descriptor
 *
 */ 
static struct ext2_group_desc *
get_group_desc(struct fs_info *fs, uint32_t group_num)
{
    block_t block_num;
    uint32_t offset;
    struct ext2_group_desc *desc;
    struct cache_struct *cs;

    block_num = group_num / DescPerBlock;
    offset = group_num % DescPerBlock;

    block_num += sb.s_first_data_block + 1;
    cs = get_cache_block(fs->fs_dev, block_num);
    desc = (struct ext2_group_desc *)cs->data + offset;

    return desc;
}


/**
 * read the right inode structure to _dst_.
 *
 * @param: inode_offset, the inode offset within a group;
 * @prarm: dst, wher we will store the inode structure;
 * @param: desc, the pointer to the group's descriptor
 * @param: block, a pointer used for retruning the blk number for file structure
 * @param: offset, same as block
 *
 */
static void read_inode(struct fs_info *fs, uint32_t inode_offset, 
                       struct ext2_inode *dst, struct ext2_group_desc *desc,
                       block_t *block, uint32_t *offset)
{
    struct cache_struct *cs;
    struct ext2_inode *inode;
    
    *block  = inode_offset / InodePerBlock + desc->bg_inode_table;
    *offset = inode_offset % InodePerBlock;
    
    cs = get_cache_block(fs->fs_dev, *block);
    
    /* well, in EXT4, the inode structure usually be 256 */
    inode = (struct ext2_inode *)(cs->data + (*offset * (sb.s_inode_size)));
    memcpy(dst, inode, EXT2_GOOD_OLD_INODE_SIZE);
    
    /* for file structure */
    *offset = (inode_offset * sb.s_inode_size) % ClustSize;
}


/**
 * open a file indicated by an inode number in INR
 *
 * @param : inr, the inode number
 * @return: a open_file_t structure pointer
 *          file length in bytes
 *          the first 128 bytes of the inode, stores in ThisInode
 *
 */
static struct open_file_t * 
open_inode(struct fs_info *fs, uint32_t inr, uint32_t *file_len)
{
    struct open_file_t *file;
    struct ext2_group_desc *desc;
        
    uint32_t inode_group, inode_offset;
    block_t block_num;
    uint32_t block_off;    
    
    file = allocate_file();
    if (!file)
        return NULL;
    
    file->file_sector = 0;
    
    inr --;
    inode_group  = inr / sb.s_inodes_per_group;
    
    /* get the group desc */
    desc = get_group_desc(fs, inode_group);
    
    inode_offset = inr % sb.s_inodes_per_group;
    read_inode(fs, inode_offset, &this_inode, desc, &block_num, &block_off);
    
    /* Finally, we need to convet it to sector for now */
    file->file_in_sec = (block_num<<ClustShift) + (block_off>>SECTOR_SHIFT);
    file->file_in_off = block_off & (SECTOR_SIZE - 1);
    file->file_mode = this_inode.i_mode;
    *file_len = file->file_bytesleft = this_inode.i_size;
    
    if (*file_len == 0)
        return NULL;

    return file;
}



static struct ext4_extent_header * 
ext4_find_leaf(struct fs_info *fs, struct ext4_extent_header *eh, block_t block)
{
    struct ext4_extent_idx *index;
    struct cache_struct *cs;
    block_t blk;
    int i;
    
    while (1) {        
        if (eh->eh_magic != EXT4_EXT_MAGIC)
            return NULL;
        
        /* got it */
        if (eh->eh_depth == 0)
            return eh;
        
        index = EXT4_FIRST_INDEX(eh);        
        for (i = 0; i < eh->eh_entries; i++) {
            if (block < index[i].ei_block)
                break;
        }
        if (--i < 0)
            return NULL;
        
        blk = index[i].ei_leaf_hi;
        blk = (blk << 32) + index[i].ei_leaf_lo;
        
        /* read the blk to memeory */
        cs = get_cache_block(fs->fs_dev, blk);
        eh = (struct ext4_extent_header *)(cs->data);
    }
}

/* handle the ext4 extents to get the phsical block number */
static block_t linsector_extent(struct fs_info *fs, block_t block, 
                                struct ext2_inode *inode)
{
    struct ext4_extent_header *leaf;
    struct ext4_extent *ext;
    int i;
    block_t start;
    
    leaf = ext4_find_leaf(fs, (struct ext4_extent_header*)inode->i_block, block);
    if (!leaf) {
        printf("ERROR, extent leaf not found\n");
        return 0;
    }
    
    ext = EXT4_FIRST_EXTENT(leaf);
    for (i = 0; i < leaf->eh_entries; i++) {
        if (block < ext[i].ee_block)
            break;
    }
    if (--i < 0) {
        printf("ERROR, not find the right block\n");
        return 0;
    }    
    
    /* got it */
    block -= ext[i].ee_block;
    if (block >= ext[i].ee_len)
        return 0;
    
    start = ext[i].ee_start_hi;
    start = (start << 32) + ext[i].ee_start_lo;
    
    return start + block;
}


/**
 * linsector_direct:
 * 
 * @param: block, the block index
 * @param: inode, the inode structure
 *
 * @return: the physic block number
 */
static block_t linsector_direct(struct fs_info *fs, uint32_t block, struct ext2_inode *inode)
{
    struct cache_struct *cs;
    
    /* direct blocks */
    if (block < EXT2_NDIR_BLOCKS) 
        return inode->i_block[block];
    

    /* indirect blocks */
    block -= EXT2_NDIR_BLOCKS;
    if (block < PtrsPerBlock1) {
        block_t ind_block = inode->i_block[EXT2_IND_BLOCK];
        cs = get_cache_block(fs->fs_dev, ind_block);
        
        return ((uint32_t *)cs->data)[block];
    }
    
    /* double indirect blocks */
    block -= PtrsPerBlock1;
    if (block < PtrsPerBlock2) {
        block_t dou_block = inode->i_block[EXT2_DIND_BLOCK];
        cs = get_cache_block(fs->fs_dev, dou_block);
        
        dou_block = ((uint32_t *)cs->data)[block / PtrsPerBlock1];
        cs = get_cache_block(fs->fs_dev, dou_block);
        
        return ((uint32_t*)cs->data)[block % PtrsPerBlock1];
    }
    
    /* triple indirect block */
    block -= PtrsPerBlock2;
    if (block < PtrsPerBlock3) {
        block_t tri_block = inode->i_block[EXT2_TIND_BLOCK];
        cs = get_cache_block(fs->fs_dev, tri_block);
        
        tri_block = ((uint32_t *)cs->data)[block / PtrsPerBlock2];
        cs = get_cache_block(fs->fs_dev, tri_block);
        
        tri_block = ((uint32_t *)cs->data)[block % PtrsPerBlock2];
        cs = get_cache_block(fs->fs_dev, tri_block);

        return ((uint32_t*)cs->data)[block % PtrsPerBlock1];
    }
    
    /* File too big, can not handle */
    printf("ERROR, file too big\n");
    return 0;
}


/**
 * linsector:
 *
 * Convert a linear sector index in a file to linear sector number
 *
 * well, alought this function converts a linear sector number to 
 * physic sector number, it uses block cache in the implemention.
 * 
 * @param: lin_sector, the lineral sector index
 * 
 * @return: physic sector number
 */
static sector_t linsector(struct fs_info *fs, uint32_t lin_sector)
{
    uint32_t block = lin_sector >> ClustShift;
    block_t ret;
    struct ext2_inode *inode;

    /* well, this is what I think the variable this_inode used for */
    inode = &this_inode;

    if (inode->i_flags & EXT4_EXTENTS_FLAG)
        ret = linsector_extent(fs, block, inode);
    else
        ret = linsector_direct(fs, block, inode);
    
    if (!ret) {
        printf("ERROR: something error happend at linsector..\n");
        return 0;
    }
    
    /* finally convert it to sector */
    return ((ret << ClustShift) + (lin_sector & ClustMask));
}


/*
 * NOTE! unlike strncmp, ext2_match_entry returns 1 for success, 0 for failure.
 *
 * len <= EXT2_NAME_LEN and de != NULL are guaranteed by caller.
 */
static inline int ext2_match_entry (const char * const name,
                                    struct ext2_dir_entry * de)
{
    if (!de->d_inode)
        return 0;
    return !strncmp(name, de->d_name, de->d_name_len);
}


/*
 * p is at least 6 bytes before the end of page
 */
static inline struct ext2_dir_entry *ext2_next_entry(struct ext2_dir_entry *p)
{
    return (struct ext2_dir_entry *)((char*)p + p->d_rec_len);
}

/**
 * getlinsec_ext:
 *
 * same as getlinsec, except load any sector from the zero
 * block as all zeros; use to load any data derived from
 * n ext2 block pointer, i.e. anything *except the superblock
 *
 */
static void getlinsec_ext(struct fs_info *fs, char *buf, 
                   sector_t sector, int sector_cnt)
{
    int ext_cnt = 0;
    struct disk *disk = fs->fs_dev->disk;
    
    if (sector < SecPerClust) {
        ext_cnt = SecPerClust - sector;
        memset(buf, 0, ext_cnt << SECTOR_SHIFT);
        buf += ext_cnt << SECTOR_SHIFT;
    }
    
    sector += ext_cnt;
    sector_cnt -= ext_cnt;
    disk->rdwr_sectors(disk, buf, sector, sector_cnt, 0);
}

/**
 * getfssec:
 *
 * Get multiple sectors from a file 
 *
 * Alought we have made the buffer data based on block size, 
 * we use sector for implemention; because reading multiple 
 * sectors (then can be multiple blocks) is what the function 
 * do. So, let it be based on sectors.
 *
 * This function can be called from C function, and either from
 * ASM function.
 * 
 * @param: ES:BX(of regs), the buffer to store data
 * @param: DS:SI(of regs), the pointer to open_file_t
 * @param: CX(of regs), number of sectors to read
 *
 * @return: ECX(of regs), number of bytes read
 *
 */
static uint32_t ext2_getfssec(struct file *gfile, char *buf,
			      int sectors, bool *have_more)
{
    int sector_left, next_sector, sector_idx;
    int frag_start, con_sec_cnt;
    int bytes_read = sectors << SECTOR_SHIFT;
    struct open_file_t *file = gfile->open_file;
    struct fs_info *fs = gfile->fs;
    
    sector_left = (file->file_bytesleft + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
    if (sectors > sector_left)
        sectors = sector_left;
    
    while (sectors) {
        /*
         * get the frament
         */
        sector_idx  = file->file_sector;
        next_sector = frag_start = linsector(fs, sector_idx);
        con_sec_cnt = 0;                
        
        /* get the consective sectors count */
        do {            
            con_sec_cnt ++;
            sectors --;
            if (sectors <= 0)
                break;
            
            /* if sectors >= the sectors left in the 64K block, break and read */
            if (sectors >= (((~(uint32_t)buf&0xffff)|((uint32_t)buf&0xffff0000)) + 1))
                break;
            
            sector_idx ++;
            next_sector ++;
        } while (next_sector == linsector(fs, sector_idx));                
        
#if 0   
        printf("You are reading data stored at sector --0x%x--0x%x\n", 
               frag_start, frag_start + con_sec_cnt -1);
#endif        
        getlinsec_ext(fs, buf, frag_start, con_sec_cnt);
        buf += con_sec_cnt << 9;
        file->file_sector += con_sec_cnt;  /* next sector index */
    } while(sectors);
    
    if (bytes_read >= file->file_bytesleft) { 
        bytes_read = file->file_bytesleft;
	*have_more = 0;
    } else {
        *have_more = 1;
    }    
    file->file_bytesleft -= bytes_read;

    return bytes_read;
}

   

/**
 * find_dir_entry:
 *
 * find a dir entry, if find return it or return NULL
 *
 */
static struct ext2_dir_entry* 
find_dir_entry(struct fs_info *fs, struct open_file_t *file, char *filename)
{
    bool have_more;
    char *EndBlock = trackbuf + (SecPerClust << SECTOR_SHIFT);;
    struct ext2_dir_entry *de;
    struct file xfile;

    /* Fake out a VFS file structure */
    xfile.fs        = fs;
    xfile.open_file = file;
    
    /* read a clust at a time */
    ext2_getfssec(&xfile, trackbuf, SecPerClust, &have_more);        
    de = (struct ext2_dir_entry *)trackbuf;        
    
    while (1) {
        if ((char *)de >= (char *)EndBlock) {
            if (!have_more) 
                return NULL;
            ext2_getfssec(&xfile, trackbuf, SecPerClust, &have_more);
            de = (struct ext2_dir_entry *)trackbuf;
        }
        
        /* Zero inode == void entry */
        if (de->d_inode == 0) {
            de = ext2_next_entry(de);       
            continue;
        }
        
        if (ext2_match_entry (filename, de)) {
            filename += de->d_name_len;
            if ((*filename == 0) || (*filename == '/'))
                return de;     /* got it */
            
            /* not match, restore the filename then try next */
            filename -= de->d_name_len;
        }
        
        de = ext2_next_entry(de);
    } 
}


static char *do_symlink(struct fs_info *fs, struct open_file_t *file, 
			uint32_t file_len, char *filename)
{
    int  flag;
    bool have_more;
    
    char *SymlinkTmpBuf = trackbuf;
    char *lnk_end;
    char *SymlinkTmpBufEnd = trackbuf + SYMLINK_SECTORS * SECTOR_SIZE+64;
    struct file xfile;
    xfile.fs = fs;
    xfile.open_file = file;
    
    flag = this_inode.i_file_acl ? SecPerClust : 0;    
    if (this_inode.i_blocks == flag) {
        /* fast symlink */
        close_pvt(file);          /* we've got all we need */
        memcpy(SymlinkTmpBuf, this_inode.i_block, file_len);
        lnk_end = SymlinkTmpBuf + file_len;
        
    } else {                           
        /* slow symlink */
        ext2_getfssec(&xfile, SymlinkTmpBuf, SYMLINK_SECTORS, &have_more);
        lnk_end = SymlinkTmpBuf + file_len;
    }
    
    if (*filename != 0)
        *lnk_end++ = '/';
    
    if (strecpy(lnk_end, filename, SymlinkTmpBufEnd)) 
        return NULL; /* buffer overflow */
    
    /*
     * now copy it to the "real" buffer; we need to have
     * two buffers so we avoid overwriting the tail on 
     * the next copy.
     */
    strcpy(SymlinkBuf, SymlinkTmpBuf);
    
    /* return the new path */
    return SymlinkBuf;
}




/**
 * Search the root directory for a pre-mangle filename in FILENAME.
 *
 * @param: filename, the filename we want to search.
 *
 * @out  : a open_file_t structure pointer, stores in file->open_file
 * @out  : file lenght in bytes, stores in file->file_len
 *
 */
static void ext2_searchdir(char *filename, struct file *file)
{
    extern int CurrentDir;
    
    struct open_file_t *open_file;
    struct ext2_dir_entry *de;
    uint8_t  file_mode;
    uint8_t  SymlinkCtr = MAX_SYMLINKS;        
    uint32_t inr = CurrentDir;
    uint32_t ThisDir = CurrentDir;
    uint32_t file_len;
        
 begin_path:
    while (*filename == '/') { /* Absolute filename */
        inr = EXT2_ROOT_INO;
        filename ++;
    }
 open:
    if ((open_file = open_inode(file->fs, inr, &file_len)) == NULL)
        goto err_noclose;
        
    file_mode = open_file->file_mode >> S_IFSHIFT;
    
    /* It's a file */
    if (file_mode == T_IFREG) {
        if (*filename == '\0')
            goto done;
        else
            goto err;
    } 
    
    
    /* It's a directory */
    if (file_mode == T_IFDIR) {                
        ThisDir = inr;
        
        if (*filename == 0)
            goto err;
        while (*filename == '/')
            filename ++;
        
        de = find_dir_entry(file->fs, open_file, filename);
        if (!de) 
            goto err;
        
        inr = de->d_inode;
        filename += de->d_name_len;
        close_pvt(open_file);
        goto open;
    }    
    
        
    /*
     * It's a symlink.  We have to determine if it's a fast symlink
     * (data stored in the inode) or not (data stored as a regular
     * file.)  Either which way, we start from the directory
     * which we just visited if relative, or from the root directory
     * if absolute, and append any remaining part of the path.
     */
    if (file_mode == T_IFLNK) {
        if (--SymlinkCtr==0 || file_len>=SYMLINK_SECTORS*SECTOR_SIZE)
            goto err;    /* too many links or symlink too long */
        
        filename = do_symlink(file->fs, open_file, file_len, filename);
        if (!filename)    
            goto err_noclose;/* buffer overflow */
        
        inr = ThisDir;
        goto begin_path;     /* we got a new path, so search it again */
    }
    
    /* Otherwise, something bad ... */
 err:
    close_pvt(open_file);
 err_noclose:
    file_len = 0;
    open_file = NULL;
 done:        
    
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

static void ext2_load_config(com32sys_t *regs)
{
    char *config_name = "extlinux.conf";
    
    strcpy(ConfigName, config_name);
    *(uint32_t *)CurrentDirName = 0x00002f2e;  

    regs->edi.w[0] = OFFS_WRT(ConfigName, 0);
    call16(core_open, regs, regs);

#if 0
    printf("the zero flag is %s\n", regs->eax.w[0] ?          \
           "CLEAR, means we found the config file" :
           "SET, menas we didn't find the config file");
#endif
}


/*
 * init. the fs meta data, return the block size bits.
 */
static int ext2_fs_init(struct fs_info *fs)
{
    struct disk *disk = fs->fs_dev->disk;

    /* read the super block */
    disk->rdwr_sectors(disk, &sb, 2, 2, 0);
    
    ClustByteShift = sb.s_log_block_size + 10;
    ClustSize = 1 << ClustByteShift;
    ClustShift = ClustByteShift - SECTOR_SHIFT;
    
    DescPerBlock  = ClustSize >> ext2_group_desc_lg2size;
    InodePerBlock = ClustSize / sb.s_inode_size;
        
    SecPerClust = ClustSize >> SECTOR_SHIFT;
    ClustMask = SecPerClust - 1;
    
    PtrsPerBlock1 = 1 << (ClustByteShift - 2);
    PtrsPerBlock2 = 1 << ((ClustByteShift - 2) * 2);
    PtrsPerBlock3 = 1 << ((ClustByteShift - 2) * 3);
    
    return ClustByteShift;
}

const struct fs_ops ext2_fs_ops = {
    .fs_name       = "ext2",
    .fs_flags      = 0,
    .fs_init       = ext2_fs_init,
    .searchdir     = ext2_searchdir,
    .getfssec      = ext2_getfssec,
    .close_file    = ext2_close_file,
    .mangle_name   = generic_mangle_name,
    .unmangle_name = generic_unmangle_name,
    .load_config   = ext2_load_config
};
