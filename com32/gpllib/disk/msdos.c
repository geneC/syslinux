/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   Some parts borrowed from chain.c32:
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <stdlib.h>
#include <disk/geom.h>
#include <disk/partition.h>
#include <disk/read.h>

static inline int is_extended_partition(struct part_entry *ptab)
{
	return (ptab->ostype == 0x05 ||
		ptab->ostype == 0x0f ||
		ptab->ostype == 0x85);
}
static inline int msdos_magic_present(char *ptab)
{
	return ( *(uint16_t *)(ptab + 0x1fe) == 0xaa55 );
}

/**
 * process_ebr - execute a callback for each partition contained in an ebr
 * @drive_info:	driveinfo struct describing the drive
 * @ptab_root:	part_entry struct describing the root partition (pointing to the ebr)
 * @ebr_seen:	Number of ebr processed
 * @callback:	Callback to execute
 **/
static void process_ebr(struct driveinfo *drive_info, struct part_entry *ptab_root,
		        int ebr_seen,
		        void *callback(struct driveinfo *, struct part_entry *, struct part_entry *, int, int, int),
		        int *error, int offset_root)
{
	/* The ebr is located at the first sector of the extended partition */
	char* ebr = read_sectors(drive_info, ptab_root->start_lba+offset_root, 1, error);
	if (!ebr)
		return;

	/* Check msdos magic signature */
	if(!msdos_magic_present(ebr))
		return;
	ebr_seen += 1;

	struct part_entry *ptab_child = (struct part_entry *)(ebr + PARTITION_TABLES_OFFSET);

	/* First process the data partitions */
	for (int i = 0; i < 4; i++) {
		if (*error)
			return;

		if (ptab_child[i].start_sect > 0) {
			if (is_extended_partition(&ptab_child[i])) {
				continue;
			}

			/* Check for garbage in the 3rd and 4th entries */
			if (i > 2) {
				unsigned int offset = ptab_child->start_lba + ptab_root->start_lba;
				if ( offset + ptab_child->length <= ptab_root->start_lba ||
				     offset >= ptab_root->start_lba + ptab_root->length ) {
					continue;
				}
			}
			callback(drive_info,
				 &ptab_child[i],
				 ptab_root,
				 offset_root,
				 i,
				 ebr_seen);
		}
	}

	/* Now process the extended partitions */
	for (int i = 0; i < 4; i++) {
		if (is_extended_partition(&ptab_child[i])) {
			callback(drive_info,
				 &ptab_child[i],
				 ptab_root,
				 offset_root,
				 i,
				 ebr_seen);
			process_ebr(drive_info, &ptab_child[i], ebr_seen + 1, callback, error, ptab_root->start_lba);
		}
	}
}

/**
 * process_mbr - execute a callback for each partition contained in an {m,e}br
 * @drive_info:	driveinfo struct describing the drive
 * @ptab:	Pointer to the partition table
 * @callback:	Callback to execute
 * @error:	Return the error code (I/O), if needed
 **/
static void process_mbr(struct driveinfo *drive_info, struct part_entry *ptab,
		        void *callback(struct driveinfo *, struct part_entry *, struct part_entry *, int, int, int),
		        int *error)
{
	for (int i = 0; i < 4; i++) {
		if (*error)
			return;

		if (ptab[i].start_sect > 0) {
			if (is_extended_partition(&ptab[i])) {
				callback(drive_info,
					 &ptab[i],
					 ptab,
					 0,
					 i,
					 0);
				process_ebr(drive_info, &ptab[i], 0, callback, error, 0);
			} else
				callback(drive_info,
					 &ptab[i],
					 ptab,
					 0,
					 i,
					 0);
		}
	}
}

/**
 * parse_partition_table - execute a callback for each partition entry
 * @d:		driveinfo struct describing the drive
 * @callback:	Callback to execute
 * @error:	Return the error code (I/O), if needed
 *
 * The signature of the callback should be the following:
 *
 * void callback(struct driveinfo *drive_info,
 *		 struct part_entry *ptab,
 *		 struct part_entry *ptab_root,
 *		 int offset_root,
 *		 int local_partition_number,
 *		 int ebr_seen)
 **/
int parse_partition_table(struct driveinfo *d, void *callback, int *error)
{
	char *mbr = read_mbr(d->disk, error);
	if (!mbr)
		return -1;
	else {
		/* Check msdos magic signature */
		if (!msdos_magic_present(mbr))
			return -1;

		struct part_entry *ptab = (struct part_entry *)(mbr + PARTITION_TABLES_OFFSET);
		process_mbr(d, ptab, callback, error);
		if (*error)
			return -1;
		else
			return 0;
	}
}
