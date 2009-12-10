#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include "fat_fs.h"


static struct inode * new_fat_inode(void)
{
    struct inode *inode = malloc(sizeof(*inode));
    if (!inode) 
	malloc_error("inode structure");		
    memset(inode, 0, sizeof(*inode));
    
    /* 
     * We just need allocate one uint32_t data to store the 
     * first cluster number.
     */
    inode->data = malloc(sizeof(uint32_t));
    if (!inode->data) 
	malloc_error("inode->data");
    
    inode->blksize = 1 << SECTOR_SHIFT;
    
    return inode;
}


static void vfat_close_file(struct file *file)
{
    if (file->inode) {
	file->offset = 0;
	free_inode(file->inode);
    }
}


/*
 * Check for a particular sector in the FAT cache
 */
static struct cache_struct * get_fat_sector(struct fs_info *fs, sector_t sector)
{
    return get_cache_block(fs->fs_dev, FAT_SB(fs)->fat + sector);
}

static uint32_t get_next_cluster(struct fs_info *fs, uint32_t clust_num)
{
    uint32_t next_cluster;
    sector_t fat_sector;
    uint32_t offset;
    int lo, hi;
    struct cache_struct *cs;
    
    switch(FAT_SB(fs)->fat_type) {
    case FAT12:
	fat_sector = (clust_num + clust_num / 2) >> SECTOR_SHIFT;
	cs = get_fat_sector(fs, fat_sector);
	offset = (clust_num * 3 / 2) & ((1 << SECTOR_SHIFT) - 1);
	if (offset == 0x1ff) {
	    /* 
	     * we got the end of the one fat sector, 
	     * but we don't got we have(just one byte, we need two),
	     * so store the low part, then read the next fat
	     * sector, read the high part, then combine it.
	     */
	    lo = *(uint8_t *)(cs->data + offset);
	    cs = get_fat_sector(fs, fat_sector + 1);
	    hi = *(uint8_t *)cs->data;
	    next_cluster = (hi << 8) + lo;
	} else {
	    next_cluster = *(uint16_t *)(cs->data + offset);
	}
	
	if (clust_num & 0x0001)
	    next_cluster >>= 4;         /* cluster number is ODD */
	else
	    next_cluster &= 0x0fff;     /* cluster number is EVEN */
	if (next_cluster > 0x0ff0)
	    goto fail;
	break;
	
    case FAT16:
	fat_sector = clust_num >> (SECTOR_SHIFT - 1);
	offset = clust_num & ((1 << (SECTOR_SHIFT-1)) -1);
	cs = get_fat_sector(fs, fat_sector);
	next_cluster = ((uint16_t *)cs->data)[offset];
	if (next_cluster > 0xfff0)
	    goto fail;
	break;
	
    case FAT32:
	fat_sector = clust_num >> (SECTOR_SHIFT - 2);
	offset = clust_num & ((1 << (SECTOR_SHIFT-2)) -1);
	cs = get_fat_sector(fs, fat_sector);
	next_cluster = ((uint32_t *)cs->data)[offset] & 0x0fffffff;
	if (next_cluster > 0x0ffffff0)
	    goto fail;
	break;
    }
    
    return next_cluster;
    
fail:  
    /* got an unexcepted cluster number, so return ZERO */
    return 0;
}


static sector_t get_next_sector(struct fs_info* fs, uint32_t sector)
{
    sector_t data_area = FAT_SB(fs)->data;
    sector_t data_sector;
    uint32_t cluster;
    
    if (sector < data_area) {
	sector++;
	/* if we reached the end of root area */
	if (sector == data_area)
	    sector = 0; /* return 0 */
	return sector;
    }
    
    data_sector = sector - data_area;
    if ((data_sector + 1) & FAT_SB(fs)->clust_mask)  /* in a cluster */
	return ++sector;
    
    /* get a new cluster */
    cluster = get_next_cluster(fs, (data_sector >> FAT_SB(fs)->clust_shift) + 2);
    if (!cluster ) 
	return 0;
    
    /* return the start of the new cluster */
    sector = ((cluster - 2) << FAT_SB(fs)->clust_shift) + data_area;
    return sector;
}

/*
 * Here comes the place I don't like VFAT fs most; if we need seek
 * the file to the right place, we need get the right sector address
 * from begining everytime! Since it's a kind a signle link list, we
 * need to traver from the head-node to find the right node in that list.
 *
 * What a waste of time!
 */
static sector_t get_the_right_sector(struct file *file)
{
    int i = 0;
    int sector_pos  = file->offset >> SECTOR_SHIFT;
    sector_t sector = *file->inode->data;
    
    for (; i < sector_pos; i++) 
	sector = get_next_sector(file->fs, sector);
    
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
 */
static void __getfssec(struct fs_info *fs, char *buf, 
                       struct file *file, uint32_t sectors)
{
    sector_t curr_sector = get_the_right_sector(file);
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
            if (sectors == 0)
                break;
            
            next_sector = get_next_sector(fs, curr_sector);
            if (!next_sector)
                break;                        
        }while(next_sector == (++curr_sector));
        
#if 0   
        printf("You are reading data stored at sector --0x%x--0x%x\n", 
               frag_start, frag_start + con_sec_cnt -1);
#endif 
                        
        /* do read */
        disk->rdwr_sectors(disk, buf, frag_start, con_sec_cnt, 0);
        buf += con_sec_cnt << SECTOR_SHIFT;/* adjust buffer pointer */
        
        if (!sectors)
            break;
        
        curr_sector = next_sector;
    }
    
}



/**
 * get multiple sectors from a file 
 *
 * @param: buf, the buffer to store the read data
 * @param: file, the file structure pointer
 * @param: sectors, number of sectors wanna read
 * @param: have_more, set one if has more
 *
 * @return: number of bytes read
 *
 */
static uint32_t vfat_getfssec(struct file *file, char *buf, int sectors,
			      bool *have_more)
{
    uint32_t bytes_left = file->inode->size - file->offset;
    uint32_t bytes_read = sectors << SECTOR_SHIFT;
    int sector_left;
    struct fs_info *fs = file->fs;
    
    sector_left = (bytes_left + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
    if (sectors > sector_left)
        sectors = sector_left;
    
    __getfssec(fs, buf, file, sectors);
    
    if (bytes_read >= bytes_left) {
        bytes_read = bytes_left;
        *have_more = 0;
    } else {
        *have_more = 1;
    }    
    file->offset += bytes_read;
    
    return bytes_read;
}

/*
 * Mangle a filename pointed to by src into a buffer pointed to by dst; 
 * ends on encountering any whitespace.
 *
 */
static void vfat_mangle_name(char *dst, const char *src)
{
    char *p = dst;
    char c;
    int i = FILENAME_MAX -1;
    
    /*
     * Copy the filename, converting backslash to slash and
     * collapsing duplicate separators.
     */
    while (not_whitespace(c = *src)) {
        if (c == '\\')
            c = '/';
        
        if (c == '/') {
            if (src[1] == '/' || src[1] == '\\') {
                src++;
                i--;
                continue;
            }
        }        
        i--;
        *dst++ = *src++;
    }

    /* Strip terminal slashes or whitespace */
    while (1) {
        if (dst == p)
            break;
		if (*(dst-1) == '/' && dst-1 == p) /* it's the '/' case */
			break;
        if ((*(dst-1) != '/') && (*(dst-1) != '.'))
            break;
        
        dst--;
        i++;
    }

    i++;
    for (; i > 0; i --)
        *dst++ = '\0';
}

/*
 * Mangle a normal style string to DOS style string.
 */
static void mangle_dos_name(char *mangle_buf, char *src)
{       
    char *dst = mangle_buf;
    int i = 0;
    unsigned char c;        
    
    for (; i < 11; i ++)
	mangle_buf[i] = ' ';
    
    for (i = 0; i < 11; i++) {
	c = *src ++;
	
	if ((c <= ' ') || (c == '/')) 
	    break;
	
	if (c == '.') {
	    dst = &mangle_buf[8];
	    i = 7;
	    continue;
	}
	
	if (c >= 'a' && c <= 'z')
	    c -= 32;
	if ((c == 0xe5) && (i == 11))
	    c = 0x05;
	
	*dst++ = c;
    }
    mangle_buf[11] = '\0';
}


/* try with the biggest long name */
static char long_name[0x40 * 13];
static char entry_name[14];

static void unicode_to_ascii(char *entry_name, uint16_t *unicode_buf)
{
    int i = 0;
    
    for (; i < 13; i++) {
	if (unicode_buf[i] == 0xffff) {
	    entry_name[i] = '\0';
	    return;
	}
	entry_name[i] = (char)unicode_buf[i];
    }
}

/*
 * get the long entry name
 *
 */
static void long_entry_name(struct fat_long_name_entry *dir)
{
    uint16_t unicode_buf[13];
    
    memcpy(unicode_buf,      dir->name1, 5 * 2);
    memcpy(unicode_buf + 5,  dir->name2, 6 * 2);
    memcpy(unicode_buf + 11, dir->name3, 2 * 2);
    
    unicode_to_ascii(entry_name, unicode_buf);    
}


static uint8_t get_checksum(char *dir_name)
{
    int  i;
    uint8_t sum = 0;
    
    for (i = 11; i; i--)
	sum = ((sum & 1) << 7) + (sum >> 1) + *dir_name++;
    return sum;
}


/* compute the first sector number of one dir where the data stores */
static inline sector_t first_sector(struct fat_dir_entry *dir)
{
    struct fat_sb_info *sbi = FAT_SB(this_fs);
    uint32_t first_clust;
    sector_t sector;
    
    first_clust = (dir->first_cluster_high << 16) + dir->first_cluster_low;
    sector = ((first_clust - 2) << sbi->clust_shift) + sbi->data;
    
    return sector;
}

static inline int get_inode_mode(uint8_t attr)
{
    if (attr == FAT_ATTR_DIRECTORY)
	return I_DIR;
    else
	return I_FILE;
}

 
static struct inode *vfat_find_entry(char *dname, struct inode *dir)
{
    struct inode *inode = new_fat_inode();
    struct fat_dir_entry *de;
    struct fat_long_name_entry *long_de;
    struct cache_struct *cs;
    
    char mangled_name[12] = {0, };
    sector_t dir_sector = *dir->data;
    
    uint8_t vfat_init, vfat_next, vfat_csum = 0;
    uint8_t id;
    int slots;
    int entries;
    int checksum;
    
    slots = (strlen(dname) + 12) / 13 ;
    slots |= 0x40;
    vfat_init = vfat_next = slots;
    
    while (1) {
	cs = get_cache_block(this_fs->fs_dev, dir_sector);
	de = (struct fat_dir_entry *)cs->data;
	entries = 1 << (SECTOR_SHIFT - 5);
	
	while(entries--) {
	    if (de->name[0] == 0)
		return NULL;
	    
	    if (de->attr == 0x0f) {
		/*
		 * It's a long name entry.
		 */
		long_de = (struct fat_long_name_entry *)de;
		id = long_de->id;
		if (id != vfat_next)
		    goto not_match;
		
		if (id & 0x40) {
		    /* get the initial checksum value */
		    vfat_csum = long_de->checksum;
		    id &= 0x3f;
		} else {
		    if (long_de->checksum != vfat_csum)
			goto not_match;
		}
		
		vfat_next = --id;
		
		/* got the long entry name */
		long_entry_name(long_de);
		memcpy(long_name + id * 13, entry_name, 13);
		long_name[strlen(dname)] = '\0';
		
		/* 
		 * If we got the last entry, check it.
		 * Or, go on with the next entry.
		 */
		if (id == 0) {
		    if (strcmp(long_name, dname))
			goto not_match;
		}
		
		de++;
		continue;     /* Try the next entry */
	    } else {
		/*
		 * It's a short entry 
		 */
		if (de->attr & 0x08) /* ignore volume labels */
		    goto not_match;
		
		/* If we have a long name match, then vfat_next must be 0 */
		if (vfat_next == 0) {
		    /* 
		     * We already have a VFAT long name match. However, the 
		     * match is only valid if the checksum matches.
		     */
		    checksum = get_checksum(de->name);
		    if (checksum == vfat_csum)
			goto found;  /* Got it */
		} else {
		    if (mangled_name[0] == 0) {
			/* We haven't mangled it, mangle it first. */
			mangle_dos_name(mangled_name, dname);
		    }
		    
		    if (!strncmp(mangled_name, de->name, 11))
			goto found;
		}
	    }
	    
	not_match:
	    vfat_next = vfat_init;
	    
	    de++;
	}
	
	/* Try with the next sector */
	dir_sector = get_next_sector(this_fs, dir_sector);
	if (!dir_sector)
	    return NULL;
    }
    
found:
    inode->size = de->file_size;
    *inode->data = first_sector(de);
    inode->mode = get_inode_mode(de->attr);
    
    return inode;
}

static struct inode *vfat_iget_root(void)
{
    struct inode *inode = new_fat_inode();
    int root_size = FAT_SB(this_fs)->root_size;
    
    inode->size = root_size << SECTOR_SHIFT;
    *inode->data = FAT_SB(this_fs)->root;
    inode->mode = I_DIR;
    
    return inode;
}

static struct inode *vfat_iget(char *dname, struct inode *parent)
{
    return vfat_find_entry(dname, parent);
}

/*
 * The open dir function, just call the searchdir function  directly. 
 * I don't think we need call the mangle_name function first 
 */
void vfat_opendir(com32sys_t *regs)
{
    char *src = MK_PTR(regs->es, regs->esi.w[0]);
    char *dst = MK_PTR(regs->ds, regs->edi.w[0]);
    strcpy(dst, src);
    searchdir(regs);	
}


/* Load the config file, return 1 if failed, or 0 */
static int vfat_load_config(void)
{
    static const char syslinux_cfg1[] = "/boot/syslinux/syslinux.cfg";
    static const char syslinux_cfg2[] = "/syslinux/syslinux.cfg";
    static const char syslinux_cfg3[] = "/syslinux.cfg";
    static const char config_name[] = "syslinux.cfg";    
    const char * const syslinux_cfg[] =
	{ syslinux_cfg1, syslinux_cfg2, syslinux_cfg3 };
    com32sys_t regs;
    int i = 0;

    /* 
     * we use the ConfigName to pass the config path because
     * it is under the address 0xffff
     */
    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
    for (; i < 3; i++) {
        strcpy(ConfigName, syslinux_cfg[i]);
        call16(core_open, &regs, &regs);

        /* if zf flag set, then failed; try another */
        if (! (regs.eflags.l & EFLAGS_ZF))
            break;
    }
    if (i == 3) {
        printf("no config file found\n");
        return 1;  /* no config file */
    }
    
    strcpy(ConfigName, config_name);
    return 0;
}
 
static inline __constfunc uint32_t bsr(uint32_t num)
{
    asm("bsrl %1,%0" : "=r" (num) : "rm" (num));
    return num;
}

/* init. the fs meta data, return the block size in bits */
static int vfat_fs_init(struct fs_info *fs)
{
    struct fat_bpb fat;
    struct fat_sb_info *sbi;
    struct disk *disk = fs->fs_dev->disk;
    int sectors_per_fat;
    uint32_t clust_num;
    sector_t total_sectors;
    
    disk->rdwr_sectors(disk, &fat, 0, 1, 0);
    
    sbi = malloc(sizeof(*sbi));
    if (!sbi)
	malloc_error("fat_sb_info structure");
    fs->fs_info = sbi;
    this_fs = fs;
    
    sectors_per_fat = fat.bxFATsecs ? : fat.u.fat32.bxFATsecs_32;
    total_sectors   = fat.bxSectors ? : fat.bsHugeSectors;
    
    sbi->fat       = fat.bxResSectors;	
    sbi->root      = sbi->fat + sectors_per_fat * fat.bxFATs;
    sbi->root_size = root_dir_size(&fat);
    sbi->data      = sbi->root + sbi->root_size;
    
    sbi->clust_shift      = bsr(fat.bxSecPerClust);
    sbi->clust_byte_shift = sbi->clust_shift + SECTOR_SHIFT;
    sbi->clust_mask       = fat.bxSecPerClust - 1;
    sbi->clust_size       = fat.bxSecPerClust << SECTOR_SHIFT;
    
    clust_num = (total_sectors - sbi->data) >> sbi->clust_shift;
    if (clust_num < 4085)
	sbi->fat_type = FAT12;
    else if (clust_num < 65525)
	sbi->fat_type = FAT16;
    else
	sbi->fat_type = FAT32;
    
    fs->blk_bits = 0;    
    /* for SYSLINUX, the cache is based on sector size */
    return SECTOR_SHIFT;
}
        
const struct fs_ops vfat_fs_ops = {
    .fs_name       = "vfat",
    .fs_flags      = FS_USEMEM | FS_THISIND,
    .fs_init       = vfat_fs_init,
    .searchdir     = NULL,
    .getfssec      = vfat_getfssec,
    .close_file    = vfat_close_file,
    .mangle_name   = vfat_mangle_name,
    .unmangle_name = generic_unmangle_name,
    .load_config   = vfat_load_config,
    .opendir       = vfat_opendir,
    .readdir       = NULL,
    .iget_root     = vfat_iget_root,
    .iget_current  = NULL,
    .iget          = vfat_iget,
};
