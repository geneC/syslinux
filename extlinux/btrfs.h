#ifndef _BTRFS_H_
#define _BTRFS_H_

#define BTRFS_SUPER_MAGIC 0x9123683E
#define BTRFS_SUPER_INFO_OFFSET (64 * 1024)
#define BTRFS_SUPER_INFO_SIZE 4096
#define BTRFS_MAGIC "_BHRfS_M"
#define BTRFS_CSUM_SIZE 32
#define BTRFS_FSID_SIZE 16

struct btrfs_super_block {
	unsigned char csum[BTRFS_CSUM_SIZE];
	/* the first 3 fields must match struct btrfs_header */
	unsigned char fsid[BTRFS_FSID_SIZE];    /* FS specific uuid */
	u64 bytenr; /* this block number */
	u64 flags;

	/* allowed to be different from the btrfs_header from here own down */
	u64 magic;
} __attribute__ ((__packed__));

#endif
