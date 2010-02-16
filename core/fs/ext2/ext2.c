#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include <minmax.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include "ext2_fs.h"

/*
 * get the group's descriptor of group_num
 */
const struct ext2_group_desc *ext2_get_group_desc(struct fs_info *fs,
						  uint32_t group_num)
{
    struct ext2_sb_info *sbi = EXT2_SB(fs);
    uint32_t desc_block, desc_index;
    const struct ext2_group_desc *desc_data_block;

    if (group_num >= sbi->s_groups_count) {
	printf ("ext2_get_group_desc"
		"block_group >= groups_count - "
		"block_group = %d, groups_count = %d",
		group_num, sbi->s_groups_count);

	return NULL;
    }

    desc_block = group_num / sbi->s_desc_per_block;
    desc_index = group_num % sbi->s_desc_per_block;

    desc_block += sbi->s_first_data_block + 1;

    desc_data_block = get_cache(fs->fs_dev, desc_block);
    return &desc_data_block[desc_index];
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
static sector_t linsector(struct inode *inode, uint32_t lin_sector)
{
    struct fs_info *fs = inode->fs;
    int blk_bits = fs->block_shift - fs->sector_shift;
    block_t block = ext2_bmap(inode, lin_sector >> blk_bits);

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
	next_sector = frag_start = linsector(inode, sector_idx);
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

            sector_idx++;
            next_sector++;
        } while (next_sector == linsector(inode, sector_idx));

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
static inline bool ext2_match_entry(const char *name, size_t len,
                                    const struct ext2_dir_entry * de)
{
    if (!de->d_inode)
        return false;
    if (len != de->d_name_len)
	return false;
    return !memcmp(name, de->d_name, len);
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
static const struct ext2_dir_entry *
ext2_find_entry(struct fs_info *fs, struct inode *inode, const char *dname)
{
    block_t index = 0;
    block_t block;
    uint32_t i = 0, offset, maxoffset;
    const struct ext2_dir_entry *de;
    const char *data;
    size_t dname_len = strlen(dname);

    while (i < inode->size) {
	if (!(block = ext2_bmap(inode, index++)))
	    return NULL;
	data = get_cache(fs->fs_dev, block);
	offset = 0;
	maxoffset =  min(BLOCK_SIZE(fs), i-inode->size);

	/* The smallest possible size is 9 bytes */
	while (offset < maxoffset-8) {
	    de = (const struct ext2_dir_entry *)(data + offset);
	    if (de->d_rec_len > maxoffset - offset)
		break;

	    if (ext2_match_entry(dname, dname_len, de))
		return de;

	    offset += de->d_rec_len;
	}
	i += BLOCK_SIZE(fs);
    }

    return NULL;
}

static const struct ext2_inode *
ext2_get_inode(struct fs_info *fs, int inr)
{
    const struct ext2_group_desc *desc;
    const char *data;
    uint32_t inode_group, inode_offset;
    uint32_t block_num, block_off;

    inr--;
    inode_group  = inr / EXT2_INODES_PER_GROUP(fs);
    inode_offset = inr % EXT2_INODES_PER_GROUP(fs);
    desc = ext2_get_group_desc(fs, inode_group);
    if (!desc)
	return NULL;

    block_num = desc->bg_inode_table +
	inode_offset / EXT2_INODES_PER_BLOCK(fs);
    block_off = inode_offset % EXT2_INODES_PER_BLOCK(fs);

    data = get_cache(fs->fs_dev, block_num);

    return (const struct ext2_inode *)
	(data + block_off * EXT2_SB(fs)->s_inode_size);
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

static void fill_inode(struct inode *inode, const struct ext2_inode *e_inode)
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
    memcpy(inode->pvt, e_inode->i_block, EXT2_N_BLOCKS * sizeof(uint32_t *));
}

static struct inode *ext2_iget_by_inr(struct fs_info *fs, uint32_t inr)
{
    const struct ext2_inode *e_inode;
    struct inode *inode;

    e_inode = ext2_get_inode(fs, inr);
    if (!(inode = alloc_inode(fs, inr, EXT2_N_BLOCKS*sizeof(uint32_t *))))
	return NULL;
    fill_inode(inode, e_inode);

    return inode;
}

static struct inode *ext2_iget_root(struct fs_info *fs)
{
    return ext2_iget_by_inr(fs, EXT2_ROOT_INO);
}

static struct inode *ext2_iget(char *dname, struct inode *parent)
{
    const struct ext2_dir_entry *de;
    struct fs_info *fs = parent->fs;

    de = ext2_find_entry(fs, parent, dname);
    if (!de)
	return NULL;
    
    return ext2_iget_by_inr(fs, de->d_inode);
}

int ext2_readlink(struct inode *inode, char *buf)
{
    struct fs_info *fs = inode->fs;
    int sec_per_block = 1 << (fs->block_shift - fs->sector_shift);
    bool fast_symlink;
    const char *data;
    size_t bytes = inode->size;

    if (inode->size > BLOCK_SIZE(fs))
	return -1;		/* Error! */

    fast_symlink = (inode->file_acl ? sec_per_block : 0) == inode->blocks;
    if (fast_symlink) {
	memcpy(buf, inode->pvt, bytes);
    } else {
	data = get_cache(fs->fs_dev, *(uint32_t *)inode->pvt);
	memcpy(buf, data, bytes);
    }

    return bytes;
}

/*
 * Read one directory entry at a time
 */
static struct dirent * ext2_readdir(struct file *file)
{
    struct fs_info *fs = file->fs;
    struct inode *inode = file->inode;
    struct dirent *dirent;
    const struct ext2_dir_entry *de;
    const char *data;
    block_t index = file->offset >> fs->block_shift;
    block_t block;

    if (!(block = ext2_bmap(inode, index)))
	return NULL;

    data = get_cache(fs->fs_dev, block);
    de = (const struct ext2_dir_entry *)
	(data + (file->offset & (BLOCK_SIZE(fs) - 1)));

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

/*
 * init. the fs meta data, return the block size bits.
 */
static int ext2_fs_init(struct fs_info *fs)
{
    struct disk *disk = fs->fs_dev->disk;
    struct ext2_sb_info *sbi;
    struct ext2_super_block sb;

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
    fs->sector_size  = 1 << fs->sector_shift;
    fs->block_size   = 1 << fs->block_shift;

    sbi->s_inodes_per_group = sb.s_inodes_per_group;
    sbi->s_blocks_per_group = sb.s_blocks_per_group;
    sbi->s_inodes_per_block = BLOCK_SIZE(fs) / sb.s_inode_size;
    if (sb.s_desc_size < sizeof(struct ext2_group_desc))
	sb.s_desc_size = sizeof(struct ext2_group_desc);
    sbi->s_desc_per_block   = BLOCK_SIZE(fs) / sb.s_desc_size;
    sbi->s_groups_count     = (sb.s_blocks_count - sb.s_first_data_block
			       + EXT2_BLOCKS_PER_GROUP(fs) - 1)
	                      / EXT2_BLOCKS_PER_GROUP(fs);
    sbi->s_first_data_block = sb.s_first_data_block;
    sbi->s_inode_size = sb.s_inode_size;

    return fs->block_shift;
}

const struct fs_ops ext2_fs_ops = {
    .fs_name       = "ext2",
    .fs_flags      = FS_THISIND | FS_USEMEM,
    .fs_init       = ext2_fs_init,
    .searchdir     = NULL,
    .getfssec      = ext2_getfssec,
    .close_file    = generic_close_file,
    .mangle_name   = generic_mangle_name,
    .unmangle_name = generic_unmangle_name,
    .load_config   = generic_load_config,
    .iget_root     = ext2_iget_root,
    .iget          = ext2_iget,
    .readlink      = ext2_readlink,
    .readdir       = ext2_readdir
};
