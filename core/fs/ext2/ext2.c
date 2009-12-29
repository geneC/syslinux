#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include "ext2_fs.h"

/*
 * just like the function strcpy(), except it returns non-zero if overflow.
 * 
 */
static int strecpy(char *dst, const char *src, char *end)
{
    while (*src != '\0')
        *dst++ = *src++;
    *dst = '\0';
    
    if (dst > end)
        return 1;
    else 
        return 0;
}

static void ext2_close_file(struct file *file)
{
    if (file->inode) {
	file->offset = 0;
	free_inode(file->inode);
    }
}

/*
 * get the group's descriptor of group_num
 */
struct ext2_group_desc * ext2_get_group_desc(uint32_t group_num)
{
    struct ext2_sb_info *sbi = EXT2_SB(this_fs);
    
    if (group_num >= sbi->s_groups_count) {
	printf ("ext2_get_group_desc"
		"block_group >= groups_count - "
		"block_group = %d, groups_count = %d",
		group_num, sbi->s_groups_count);
	
	return NULL;
    }        
    
    return sbi->s_group_desc[group_num];
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
static sector_t linsector(struct fs_info *fs, 
			  struct inode *inode, 
			  uint32_t lin_sector)
{
    int blk_bits = fs->block_shift - fs->sector_shift;
    block_t block = bmap(fs, inode, lin_sector >> blk_bits);
    
    return (block << blk_bits) + (lin_sector & ((1 << blk_bits) - 1));
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
    int sec_per_block = 1 << (fs->block_shift - fs->sector_shift);
    struct disk *disk = fs->fs_dev->disk;
    
    if (sector < sec_per_block) {
        ext_cnt = sec_per_block - sector;
        memset(buf, 0, ext_cnt << fs->sector_shift);
        buf += ext_cnt << fs->sector_shift;
    }
    
    sector += ext_cnt;
    sector_cnt -= ext_cnt;
    disk->rdwr_sectors(disk, buf, sector, sector_cnt, 0);
}

/*
 * Get multiple sectors from a file 
 *
 * Alought we have made the buffer data based on block size, 
 * we use sector for implemention; because reading multiple 
 * sectors (then can be multiple blocks) is what the function 
 * do. So, let it be based on sectors.
 *
 */
static uint32_t ext2_getfssec(struct file *file, char *buf,
			      int sectors, bool *have_more)
{
    struct inode *inode = file->inode;
    struct fs_info *fs = file->fs;
    int sector_left, next_sector, sector_idx;
    int frag_start, con_sec_cnt;
    int bytes_read = sectors << fs->sector_shift; 
    uint32_t bytesleft = inode->size - file->offset;
    
    sector_left = (bytesleft + SECTOR_SIZE(fs) - 1) >> fs->sector_shift;
    if (sectors > sector_left)
        sectors = sector_left;
    
    sector_idx = file->offset >> fs->sector_shift;
    while (sectors) {
        /*
         * get the frament
         */
	next_sector = frag_start = linsector(fs, inode, sector_idx);
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
        } while (next_sector == linsector(fs, inode, sector_idx));                
        
#if 0  
        printf("You are reading data stored at sector --0x%x--0x%x\n", 
               frag_start, frag_start + con_sec_cnt -1);
#endif        
        getlinsec_ext(fs, buf, frag_start, con_sec_cnt);
        buf += con_sec_cnt << fs->sector_shift;
    } while(sectors);
    
    if (bytes_read >= bytesleft) { 
        bytes_read = bytesleft;
	*have_more = 0;
    } else {
        *have_more = 1;
    }    
    file->offset += bytes_read;
    
    return bytes_read;
}
   
/*
 * Unlike strncmp, ext2_match_entry returns 1 for success, 0 for failure.
 */
static inline int ext2_match_entry (const char * const name,
                                    struct ext2_dir_entry * de)
{
    if (!de->d_inode)
        return 0;
    if (strlen(name) != de->d_name_len)
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

/*
 * find a dir entry, return it if found, or return NULL.
 */
static struct ext2_dir_entry * 
ext2_find_entry(struct fs_info *fs, struct inode *inode, char *dname)
{
    int index = 0;
    block_t  block;
    uint32_t i = 0;
    struct ext2_dir_entry *de;
    struct cache_struct *cs;
    
    if (!(block = bmap(fs, inode, index++)))
	return NULL;
    cs = get_cache_block(fs->fs_dev, block);
    de = (struct ext2_dir_entry *)cs->data;
    
    while(i < (int)inode->size) {
	if (ext2_match_entry(dname, de))
	    return de;
	i += de->d_rec_len;
	if (i >= (int)inode->size)
	    break;
	if ((char *)de >= (char *)cs->data + BLOCK_SIZE(fs)) {
	    if (!(block = bmap(fs, inode, index++)))
		break;
	    cs = get_cache_block(fs->fs_dev, block);
	    de = (struct ext2_dir_entry *)cs->data;
	    continue;
	}
	
	de = ext2_next_entry(de);
    }
    
    return NULL;
}

static struct ext2_inode * get_inode(int inr)
{
    struct ext2_group_desc *desc;
    struct cache_struct *cs;
    uint32_t inode_group, inode_offset;
    uint32_t block_num, block_off;
    
    inr--;
    inode_group  = inr / EXT2_INODES_PER_GROUP(this_fs);
    inode_offset = inr % EXT2_INODES_PER_GROUP(this_fs);
    desc = ext2_get_group_desc (inode_group);
    if (!desc)
	return NULL;
    
    block_num = desc->bg_inode_table + 
	inode_offset / EXT2_INODES_PER_BLOCK(this_fs);
    block_off = inode_offset % EXT2_INODES_PER_BLOCK(this_fs);
    
    cs = get_cache_block(this_fs->fs_dev, block_num);
    
    return cs->data + block_off * EXT2_SB(this_fs)->s_inode_size;
}

static inline int get_inode_mode(int mode)
{
    mode >>= S_IFSHIFT;
    if (mode == T_IFDIR)
	mode = I_DIR;
    else if (mode == T_IFLNK)
	mode = I_SYMLINK;
    else
	mode = I_FILE; /* we treat others as FILE */        
    return mode;
}

static void fill_inode(struct inode *inode, struct ext2_inode *e_inode)
{     
    inode->mode    = get_inode_mode(e_inode->i_mode);
    inode->size    = e_inode->i_size;
    inode->atime   = e_inode->i_atime;
    inode->ctime   = e_inode->i_ctime;
    inode->mtime   = e_inode->i_mtime;
    inode->dtime   = e_inode->i_dtime;
    inode->blocks  = e_inode->i_blocks;
    inode->flags   = e_inode->i_flags;
    inode->file_acl = e_inode->i_file_acl;
    
    inode->data = malloc(EXT2_N_BLOCKS * sizeof(uint32_t *));
    if (!inode->data) {
	malloc_error("inode data filed");
	return ;
    }
    memcpy(inode->data, e_inode->i_block, EXT2_N_BLOCKS * sizeof(uint32_t *));
}

static struct inode *ext2_iget_by_inr(uint32_t inr)
{
        struct ext2_inode *e_inode;
        struct inode *inode;

        e_inode = get_inode(inr);
        if (!(inode = malloc(sizeof(*inode))))
	    return NULL;
        fill_inode(inode, e_inode);
        inode->ino = inr;
	
        return inode;
}

static struct inode *ext2_iget_root()
{
        return ext2_iget_by_inr(EXT2_ROOT_INO);
}

static struct inode *ext2_iget_current()
{
    extern int CurrentDir;
    
    return ext2_iget_by_inr(CurrentDir);
}

static struct inode *ext2_iget(char *dname, struct inode *parent)
{
        struct ext2_dir_entry *de;
        
        de = ext2_find_entry(this_fs, parent, dname);
        if (!de)
                return NULL;
        
        return ext2_iget_by_inr(de->d_inode);
}


static char * ext2_follow_symlink(struct inode *inode, const char *name_left)
{
    int sec_per_block = 1 << (this_fs->block_shift - this_fs->sector_shift);
    int fast_symlink;
    char *symlink_buf;
    char *p;
    struct cache_struct *cs;
    
    symlink_buf = malloc(BLOCK_SIZE(this_fs));
    if (!symlink_buf) {
	malloc_error("symlink buffer");
	return NULL;
    }
    fast_symlink = (inode->file_acl ? sec_per_block : 0) == inode->blocks;
    if (fast_symlink) {
	memcpy(symlink_buf, inode->data, inode->size);
    } else {
	cs = get_cache_block(this_fs->fs_dev, *(uint32_t *)inode->data);
	memcpy(symlink_buf, cs->data, inode->size);
    }
    p = symlink_buf + inode->size;
    
    if (*name_left)
	*p++ = '/';
    if (strecpy(p, name_left, symlink_buf + BLOCK_SIZE(this_fs))) {
	free(symlink_buf);
	return NULL;
    }
    if(!(p = strdup(symlink_buf)))
	return symlink_buf;
    
    free(symlink_buf);        
    return p;
}

/*
 * Read one directory entry at a time 
 */
static struct dirent * ext2_readdir(struct file *file)
{
    struct fs_info *fs = file->fs;
    struct inode *inode = file->inode;
    struct dirent *dirent;
    struct ext2_dir_entry *de;
    struct cache_struct *cs;
    int index = file->offset >> fs->block_shift;
    block_t block;
    
    if (!(block = bmap(fs, inode, index)))
	return NULL;        
    cs = get_cache_block(fs->fs_dev, block);
    de = (struct ext2_dir_entry *)(cs->data + (file->offset & (BLOCK_SIZE(fs) - 1)));
    
    if (!(dirent = malloc(sizeof(*dirent)))) {
	malloc_error("dirent structure in ext2_readdir");
	return NULL;
    }
    dirent->d_ino = de->d_inode;
    dirent->d_off = file->offset;
    dirent->d_reclen = de->d_rec_len;
    dirent->d_type = de->d_file_type;
    memcpy(dirent->d_name, de->d_name, de->d_name_len);
    dirent->d_name[de->d_name_len] = '\0';
    
    file->offset += de->d_rec_len;  /* Update for next reading */
    
    return dirent;
}

/* Load the config file, return 1 if failed, or 0 */
static int ext2_load_config(void)
{
    char *config_name = "extlinux.conf";
    com32sys_t regs;
    
    strcpy(ConfigName, config_name);
    *(uint32_t *)CurrentDirName = 0x00002f2e;  
    
    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
    call16(core_open, &regs, &regs);

    return !!(regs.eflags.l & EFLAGS_ZF);
}


/*
 * init. the fs meta data, return the block size bits.
 */
static int ext2_fs_init(struct fs_info *fs)
{
    struct disk *disk = fs->fs_dev->disk;
    struct ext2_sb_info *sbi;
    struct ext2_super_block sb;
    int blk_bits;
    int db_count;
    int i;
    int desc_block;
    char *desc_buffer;
    
    /* read the super block */
    disk->rdwr_sectors(disk, &sb, 2, 2, 0);
    
    /* check if it is ext2, since we also support btrfs now */
    if (sb.s_magic != EXT2_SUPER_MAGIC)
	return -1;

    sbi = malloc(sizeof(*sbi));
    if (!sbi) {
	malloc_error("ext2_sb_info structure");
	return -1;
    }
    fs->fs_info = sbi;
    
    if (sb.s_magic != EXT2_SUPER_MAGIC) {
	printf("ext2 mount error: it's not a EXT2/3/4 file system!\n");
	return 0;
    }

    fs->sector_shift = disk->sector_shift;
    fs->block_shift  = sb.s_log_block_size + 10;
    
    sbi->s_inodes_per_group = sb.s_inodes_per_group;
    sbi->s_blocks_per_group = sb.s_blocks_per_group;
    sbi->s_inodes_per_block = BLOCK_SIZE(fs) / sb.s_inode_size;
    if (sb.s_desc_size < sizeof(struct ext2_group_desc))
	sb.s_desc_size = sizeof(struct ext2_group_desc);
    sbi->s_desc_per_block   = BLOCK_SIZE(fs) / sb.s_desc_size;
    sbi->s_groups_count     = (sb.s_blocks_count - sb.s_first_data_block 
			       + EXT2_BLOCKS_PER_GROUP(fs) - 1) 
	                      / EXT2_BLOCKS_PER_GROUP(fs);
    db_count = (sbi->s_groups_count + EXT2_DESC_PER_BLOCK(fs) - 1) /
	        EXT2_DESC_PER_BLOCK(fs);
    sbi->s_inode_size = sb.s_inode_size;
    
    /* read the descpritors */
    desc_block = sb.s_first_data_block + 1;
    desc_buffer = malloc(db_count * BLOCK_SIZE(fs));
    if (!desc_buffer) {
	malloc_error("desc_buffer");
	return -1;
    }    
    blk_bits = fs->block_shift - fs->sector_shift;
    disk->rdwr_sectors(disk, desc_buffer, desc_block << blk_bits, 
		       db_count << blk_bits, 0);
    sbi->s_group_desc = malloc(sizeof(struct ext2_group_desc *) 
			       * sbi->s_groups_count);
    if (!sbi->s_group_desc) {
	malloc_error("sbi->s_group_desc");
	return -1;
    }
    for (i = 0; i < (int)sbi->s_groups_count; i++) {
	sbi->s_group_desc[i] = (struct ext2_group_desc *)desc_buffer;
	desc_buffer += sb.s_desc_size;
    }
    
    return fs->block_shift;
}

const struct fs_ops ext2_fs_ops = {
    .fs_name       = "ext2",
    .fs_flags      = FS_USEMEM,
    .fs_init       = ext2_fs_init,
    .searchdir     = NULL,
    .getfssec      = ext2_getfssec,
    .close_file    = ext2_close_file,
    .mangle_name   = generic_mangle_name,
    .unmangle_name = generic_unmangle_name,
    .load_config   = ext2_load_config,
    .iget_root     = ext2_iget_root,
    .iget_current  = ext2_iget_current,
    .iget          = ext2_iget,
    .follow_symlink = ext2_follow_symlink,
    .readdir       = ext2_readdir
};
