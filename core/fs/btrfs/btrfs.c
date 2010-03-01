/*
 * btrfs.c -- readonly btrfs support for syslinux
 * Some data structures are derivated from btrfs-tools-0.19 ctree.h
 * Copyright 2009 Intel Corporation; author: alek.du@intel.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 * Boston MA 02111-1307, USA; either version 2 of the License, or
 * (at your option) any later version; incorporated herein by reference.
 *
 */

#include <stdio.h>
#include <string.h>
#include <cache.h>
#include <core.h>
#include <disk.h>
#include <fs.h>
#include <sys/stat.h>
#include "btrfs.h"

/* compare function used for bin_search */
typedef int (*cmp_func)(void *ptr1, void *ptr2);

/* simple but useful bin search, used for chunk search and btree search */
static int bin_search(void *ptr, int item_size, void *cmp_item, cmp_func func,
			      int min, int max, int *slot)
{
	int low = min;
	int high = max;
	int mid;
	int ret;
	unsigned long offset;
	void *item;

	while (low < high) {
		mid = (low + high) / 2;
		offset = mid * item_size;

		item = ptr + offset;
		ret = func(item, cmp_item);

		if (ret < 0)
			low = mid + 1;
		else if (ret > 0)
			high = mid;
		else {
			*slot = mid;
			return 0;
		}
	}
	*slot = low;
	return 1;
}

static int cache_ready;
static struct fs_info *fs;
static struct btrfs_chunk_map chunk_map;
static struct btrfs_super_block sb;
/* used for small chunk read for btrfs_read */
#define RAW_BUF_SIZE 4096
static u8 raw_buf[RAW_BUF_SIZE];
static u64 fs_tree;

static int btrfs_comp_chunk_map(struct btrfs_chunk_map_item *m1,
				struct btrfs_chunk_map_item *m2)
{
	if (m1->logical > m2->logical)
		return 1;
	if (m1->logical < m2->logical)
		return -1;
	return 0;
}

/* insert a new chunk mapping item */
static void insert_map(struct btrfs_chunk_map_item *item)
{
	int ret;
	int slot;
	int i;

	if (chunk_map.map == NULL) { /* first item */
		chunk_map.map_length = BTRFS_MAX_CHUNK_ENTRIES;
		chunk_map.map = (struct btrfs_chunk_map_item *)
			malloc(chunk_map.map_length * sizeof(*chunk_map.map));
		chunk_map.map[0] = *item;
		chunk_map.cur_length = 1;
		return;
	}
	ret = bin_search(chunk_map.map, sizeof(*item), item,
			(cmp_func)btrfs_comp_chunk_map, 0,
			chunk_map.cur_length, &slot);
	if (ret == 0)/* already in map */
		return;
	if (chunk_map.cur_length == BTRFS_MAX_CHUNK_ENTRIES) {
		/* should be impossible */
		printf("too many chunk items\n");
		return;
	}
	for (i = chunk_map.cur_length; i > slot; i--)
		chunk_map.map[i] = chunk_map.map[i-1];
	chunk_map.map[slot] = *item;
	chunk_map.cur_length++;
}

/*
 * from sys_chunk_array or chunk_tree, we can convert a logical address to
 * a physical address we can not support multi device case yet
 */
static u64 logical_physical(u64 logical)
{
	struct btrfs_chunk_map_item item;
	int slot, ret;

	item.logical = logical;
	ret = bin_search(chunk_map.map, sizeof(*chunk_map.map), &item,
			(cmp_func)btrfs_comp_chunk_map, 0,
			chunk_map.cur_length, &slot);
	if (ret == 0)
		slot++;
	else if (slot == 0)
		return -1;
	if (logical >=
		chunk_map.map[slot-1].logical + chunk_map.map[slot-1].length)
		return -1;
	return chunk_map.map[slot-1].physical + logical -
			chunk_map.map[slot-1].logical;
}

/* raw read from disk, offset and count are bytes */
static int raw_read(char *buf, u64 offset, u64 count)
{
	struct disk *disk = fs->fs_dev->disk;
	size_t max = RAW_BUF_SIZE >> disk->sector_shift;
	size_t off, cnt, done, total;
	sector_t sec;

	total = count;
	while (count > 0) {
		sec = offset >> disk->sector_shift;
		off = offset - (sec << disk->sector_shift);
		done = disk->rdwr_sectors(disk, raw_buf, sec, max, 0);
		if (done == 0)/* no data */
			break;
		cnt = (done << disk->sector_shift) - off;
		if (cnt > count)
			cnt = count;
		memcpy(buf, raw_buf + off, cnt);
		count -= cnt;
		buf += cnt;
		offset += cnt;
		if (done != max)/* no enough sectors */
			break;
	}
	return total - count;
}

/* cache read from disk, offset and count are bytes */
static int cache_read(char *buf, u64 offset, u64 count)
{
	const char *cd;
	size_t block_size = fs->fs_dev->cache_block_size;
	size_t off, cnt, total;
	block_t block;

	total = count;
	while (count > 0) {
		block = offset / block_size;
		off = offset % block_size;
		cd = get_cache(fs->fs_dev, block);
		if (!cd)
			break;
		cnt = block_size - off;
		if (cnt > count)
			cnt = count;
		memcpy(buf, cd + off, cnt);
		count -= cnt;
		buf += cnt;
		offset += cnt;
	}
	return total - count;
}

static int btrfs_read(char *buf, u64 offset, u64 count)
{
	if (cache_ready)
		return cache_read(buf, offset, count);
	return raw_read(buf, offset, count);
}

/* btrfs has several super block mirrors, need to calculate their location */
static inline u64 btrfs_sb_offset(int mirror)
{
	u64 start = 16 * 1024;
	if (mirror)
		return start << (BTRFS_SUPER_MIRROR_SHIFT * mirror);
	return BTRFS_SUPER_INFO_OFFSET;
}

/* find the most recent super block */
static void btrfs_read_super_block(void)
{
	int i;
	int ret;
	u8 fsid[BTRFS_FSID_SIZE];
	u64 offset;
	u64 transid = 0;
	struct btrfs_super_block buf;

	/* find most recent super block */
	for (i = 0; i < BTRFS_SUPER_MIRROR_MAX; i++) {
		offset = btrfs_sb_offset(i);
		ret = btrfs_read((char *)&buf, offset, sizeof(buf));
		if (ret < sizeof(buf))
			break;

		if (buf.bytenr != offset ||
		    strncmp((char *)(&buf.magic), BTRFS_MAGIC,
			    sizeof(buf.magic)))
			continue;

		if (i == 0)
			memcpy(fsid, buf.fsid, sizeof(fsid));
		else if (memcmp(fsid, buf.fsid, sizeof(fsid)))
			continue;

		if (buf.generation > transid) {
			memcpy(&sb, &buf, sizeof(sb));
			transid = buf.generation;
		}
	}
}

static inline unsigned long btrfs_chunk_item_size(int num_stripes)
{
	return sizeof(struct btrfs_chunk) +
		sizeof(struct btrfs_stripe) * (num_stripes - 1);
}

static void clear_path(struct btrfs_path *path)
{
	memset(path, 0, sizeof(*path));
}

static int btrfs_comp_keys(struct btrfs_disk_key *k1, struct btrfs_disk_key *k2)
{
	if (k1->objectid > k2->objectid)
		return 1;
	if (k1->objectid < k2->objectid)
		return -1;
	if (k1->type > k2->type)
		return 1;
	if (k1->type < k2->type)
		return -1;
	if (k1->offset > k2->offset)
		return 1;
	if (k1->offset < k2->offset)
		return -1;
	return 0;
}

/* compare keys but ignore offset, is useful to enumerate all same kind keys */
static int btrfs_comp_keys_type(struct btrfs_disk_key *k1,
					struct btrfs_disk_key *k2)
{
	if (k1->objectid > k2->objectid)
		return 1;
	if (k1->objectid < k2->objectid)
		return -1;
	if (k1->type > k2->type)
		return 1;
	if (k1->type < k2->type)
		return -1;
	return 0;
}

/* seach tree directly on disk ... */
static int search_tree(u64 loffset, struct btrfs_disk_key *key,
				struct btrfs_path *path)
{
	u8 buf[BTRFS_MAX_LEAF_SIZE];
	struct btrfs_header *header = (struct btrfs_header *)buf;
	struct btrfs_node *node = (struct btrfs_node *)buf;
	struct btrfs_leaf *leaf = (struct btrfs_leaf *)buf;
	int slot, ret;
	u64 offset;

	offset = logical_physical(loffset);
	btrfs_read((char *)header, offset, sizeof(*header));
	if (header->level) {/*node*/
		btrfs_read((char *)&node->ptrs[0], offset + sizeof(*header),
			sb.nodesize - sizeof(*header));
		path->itemsnr[header->level] = header->nritems;
		path->offsets[header->level] = loffset;
		ret = bin_search(&node->ptrs[0], sizeof(struct btrfs_key_ptr),
			key, (cmp_func)btrfs_comp_keys,
			path->slots[header->level], header->nritems, &slot);
		if (ret && slot > path->slots[header->level])
			slot--;
		path->slots[header->level] = slot;
		ret = search_tree(node->ptrs[slot].blockptr, key, path);
	} else {/*leaf*/
		btrfs_read((char *)&leaf->items, offset + sizeof(*header),
			sb.leafsize - sizeof(*header));
		path->itemsnr[header->level] = header->nritems;
		path->offsets[0] = loffset;
		ret = bin_search(&leaf->items[0], sizeof(struct btrfs_item),
			key, (cmp_func)btrfs_comp_keys, path->slots[0],
			header->nritems, &slot);
		if (ret && slot > path->slots[header->level])
			slot--;
		path->slots[0] = slot;
		path->item = leaf->items[slot];
		btrfs_read((char *)&path->data,
			offset + sizeof(*header) + leaf->items[slot].offset,
			leaf->items[slot].size);
	}
	return ret;
}

/* return 0 if leaf found */
static int next_leaf(struct btrfs_disk_key *key, struct btrfs_path *path)
{
	int slot;
	int level = 1;

	while (level < BTRFS_MAX_LEVEL) {
		if (!path->itemsnr[level]) /* no more nodes */
			return 1;
		slot = path->slots[level] + 1;
		if (slot >= path->itemsnr[level]) {
			level++;
			continue;;
		}
		path->slots[level] = slot;
		path->slots[level-1] = 0; /* reset low level slots info */
		search_tree(path->offsets[level], key, path);
		break;
	}
	if (level == BTRFS_MAX_LEVEL)
		return 1;
	return 0;
}

/* return 0 if slot found */
static int next_slot(struct btrfs_disk_key *key, struct btrfs_path *path)
{
	int slot;

	if (!path->itemsnr[0])
		return 1;
	slot = path->slots[0] + 1;
	if (slot >= path->itemsnr[0])
		return 1;
	path->slots[0] = slot;
	search_tree(path->offsets[0], key, path);
	return 0;
}

/*
 * read chunk_array in super block
 */
static void btrfs_read_sys_chunk_array(void)
{
	struct btrfs_chunk_map_item item;
	struct btrfs_disk_key *key;
	struct btrfs_chunk *chunk;
	int cur;

	/* read chunk array in superblock */
	cur = 0;
	while (cur < sb.sys_chunk_array_size) {
		key = (struct btrfs_disk_key *)(sb.sys_chunk_array + cur);
		cur += sizeof(*key);
		chunk = (struct btrfs_chunk *)(sb.sys_chunk_array + cur);
		cur += btrfs_chunk_item_size(chunk->num_stripes);
		/* insert to mapping table, ignore multi stripes */
		item.logical = key->offset;
		item.length = chunk->length;
		item.devid = chunk->stripe.devid;
		item.physical = chunk->stripe.offset;/*ignore other stripes */
		insert_map(&item);
	}
}

/* read chunk items from chunk_tree and insert them to chunk map */
static void btrfs_read_chunk_tree(void)
{
	struct btrfs_disk_key search_key;
	struct btrfs_chunk *chunk;
	struct btrfs_chunk_map_item item;
	struct btrfs_path path;

	if (!(sb.flags & BTRFS_SUPER_FLAG_METADUMP)) {
		if (sb.num_devices > 1)
			printf("warning: only support single device btrfs\n");
		/* read chunk from chunk_tree */
		search_key.objectid = BTRFS_FIRST_CHUNK_TREE_OBJECTID;
		search_key.type = BTRFS_CHUNK_ITEM_KEY;
		search_key.offset = 0;
		clear_path(&path);
		search_tree(sb.chunk_root, &search_key, &path);
		do {
			do {
				if (btrfs_comp_keys_type(&search_key,
							&path.item.key))
					break;
				chunk = (struct btrfs_chunk *)(path.data);
				/* insert to mapping table, ignore stripes */
				item.logical = path.item.key.offset;
				item.length = chunk->length;
				item.devid = chunk->stripe.devid;
				item.physical = chunk->stripe.offset;
				insert_map(&item);
			} while (!next_slot(&search_key, &path));
			if (btrfs_comp_keys_type(&search_key, &path.item.key))
				break;
		} while (!next_leaf(&search_key, &path));
	}
}

static inline u64 btrfs_name_hash(const char *name, int len)
{
	return btrfs_crc32c((u32)~1, name, len);
}

static inline int get_inode_mode(int mode)
{
	if (S_ISLNK(mode))
		return I_SYMLINK;
	if (S_ISDIR(mode))
		return  I_DIR;
	return I_FILE;
}

static struct inode *btrfs_iget_by_inr(struct fs_info *fs, u64 inr)
{
	struct inode *inode;
	struct btrfs_inode_item inode_item;
	struct btrfs_disk_key search_key;
	struct btrfs_path path;
	int ret;

	/* FIXME: some BTRFS inode member are u64, while our logical inode
           is u32, we may need change them to u64 later */
	search_key.objectid = inr;
	search_key.type = BTRFS_INODE_ITEM_KEY;
	search_key.offset = 0;
	clear_path(&path);
	ret = search_tree(fs_tree, &search_key, &path);
	if (ret)
		return NULL;
	inode_item = *(struct btrfs_inode_item *)path.data;
	if (!(inode = alloc_inode(fs, inr, sizeof(struct btrfs_pvt_inode))))
		return NULL;
	inode->ino = inr;
	inode->size = inode_item.size;
	inode->mode = get_inode_mode(inode_item.mode);

	if (inode->mode == I_FILE) {
		struct btrfs_file_extent_item extent_item;
		u64 offset;

		/* get file_extent_item */
		search_key.type = BTRFS_EXTENT_DATA_KEY;
		search_key.offset = 0;
		clear_path(&path);
		ret = search_tree(fs_tree, &search_key, &path);
		if (ret)
			return NULL; /* impossible */
		extent_item = *(struct btrfs_file_extent_item *)path.data;
		if (extent_item.type == BTRFS_FILE_EXTENT_INLINE)/* inline file */
			offset = path.offsets[0] + sizeof(struct btrfs_header)
				+ path.item.offset
				+ offsetof(struct btrfs_file_extent_item, disk_bytenr);
		else
			offset = extent_item.disk_bytenr;
		PVT(inode)->offset = offset;
	}
	return inode;
}

static struct inode *btrfs_iget_root(struct fs_info *fs)
{
	/* BTRFS_FIRST_CHUNK_TREE_OBJECTID(256) actually is first OBJECTID for FS_TREE */
	return btrfs_iget_by_inr(fs, BTRFS_FIRST_CHUNK_TREE_OBJECTID);
}

static struct inode *btrfs_iget(const char *name, struct inode *parent)
{
	struct fs_info *fs = parent->fs;
	struct btrfs_disk_key search_key;
	struct btrfs_path path;
	struct btrfs_dir_item dir_item;
	int ret;

	search_key.objectid = parent->ino;
	search_key.type = BTRFS_DIR_ITEM_KEY;
	search_key.offset = btrfs_name_hash(name, strlen(name));
	clear_path(&path);
	ret = search_tree(fs_tree, &search_key, &path);
	if (ret)
		return NULL;
	dir_item = *(struct btrfs_dir_item *)path.data;

	return btrfs_iget_by_inr(fs, dir_item.location.objectid);
}

static int btrfs_readlink(struct inode *inode, char *buf)
{
	btrfs_read(buf, logical_physical(PVT(inode)->offset), inode->size);
	buf[inode->size] = '\0';
	return inode->size;
}

static uint32_t btrfs_getfssec(struct file *file, char *buf, int sectors,
					bool *have_more)
{
	struct inode *inode = file->inode;
	struct disk *disk = fs->fs_dev->disk;
	u32 sec_shift = fs->fs_dev->disk->sector_shift;
	u32 phy = logical_physical(PVT(inode)->offset + file->offset);
	u32 sec = phy >> sec_shift;
	u32 off = phy - (sec << sec_shift);
	u32 remain = file->file_len - file->offset;
	u32 remain_sec = (remain + (1 << sec_shift) - 1) >> sec_shift;
	u32 size;

	if (sectors > remain_sec)
		sectors = remain_sec;
	/* btrfs extent is continus */
	disk->rdwr_sectors(disk, buf, sec, sectors, 0);
	size = sectors << sec_shift;
	if (size > remain)
		size = remain;
	file->offset += size;
	*have_more = remain - size;

	if (off)/* inline file is not started with sector boundary */
		memcpy(buf, buf + off, size);

	return size;
}

static void btrfs_get_fs_tree(void)
{
	struct btrfs_disk_key search_key;
	struct btrfs_path path;
	struct btrfs_root_item *tree;
	bool subvol_ok = false;

	/* check if subvol is filled by installer */
	if (*SubvolName) {
		search_key.objectid = BTRFS_FS_TREE_OBJECTID;
		search_key.type = BTRFS_ROOT_REF_KEY;
		search_key.offset = 0;
		clear_path(&path);
		if (search_tree(sb.root, &search_key, &path))
			next_slot(&search_key, &path);
		do {
			do {
				struct btrfs_root_ref *ref;

				if (btrfs_comp_keys_type(&search_key,
							&path.item.key))
					break;
				ref = (struct btrfs_root_ref *)path.data;
				if (!strcmp((char*)(ref + 1), SubvolName)) {
					subvol_ok = true;
					break;
				}
			} while (!next_slot(&search_key, &path));
			if (subvol_ok)
				break;
			if (btrfs_comp_keys_type(&search_key, &path.item.key))
				break;
		} while (!next_leaf(&search_key, &path));
		if (!subvol_ok) /* should be impossible */
			printf("no subvol found!\n");
	}
	/* find fs_tree from tree_root */
	if (subvol_ok)
		search_key.objectid = path.item.key.offset;
	else /* "default" volume */
		search_key.objectid = BTRFS_FS_TREE_OBJECTID;
	search_key.type = BTRFS_ROOT_ITEM_KEY;
	search_key.offset = -1;
	clear_path(&path);
	search_tree(sb.root, &search_key, &path);
	tree = (struct btrfs_root_item *)path.data;
	fs_tree = tree->bytenr;
}

/* init. the fs meta data, return the block size shift bits. */
static int btrfs_fs_init(struct fs_info *_fs)
{
	struct disk *disk = fs->fs_dev->disk;
    
	btrfs_init_crc32c();

	fs->sector_shift = disk->sector_shift;
	fs->sector_size  = 1 << fs->sector_shift;
	fs->block_shift  = BTRFS_BLOCK_SHIFT;
	fs->block_size   = 1 << fs->block_shift;

	fs = _fs;
	btrfs_read_super_block();
	if (strncmp((char *)(&sb.magic), BTRFS_MAGIC, sizeof(sb.magic)))
		return -1;
	btrfs_read_sys_chunk_array();
	btrfs_read_chunk_tree();
	btrfs_get_fs_tree();
	cache_ready = 1;

	/* Initialize the block cache */
	cache_init(fs->fs_dev, fs->block_shift);

	return fs->block_shift;
}

const struct fs_ops btrfs_fs_ops = {
    .fs_name       = "btrfs",
    .fs_flags      = 0,
    .fs_init       = btrfs_fs_init,
    .iget_root     = btrfs_iget_root,
    .iget          = btrfs_iget,
    .readlink      = btrfs_readlink,
    .getfssec      = btrfs_getfssec,
    .close_file    = generic_close_file,
    .mangle_name   = generic_mangle_name,
    .load_config   = generic_load_config
};
