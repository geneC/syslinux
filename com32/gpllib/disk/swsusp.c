#include <stdlib.h>
#include <string.h>

#include <disk/swsusp.h>
#include <disk/read.h>
#include <disk/geom.h>

/**
 * swsusp_check - check if a (swap) partition contains the swsusp signature
 * @drive_info:	driveinfo struct describing the disk containing the partition
 * @ptab;	Partition table of the partition
 * @error:	Return the error code on failure
 **/
int swsusp_check(struct driveinfo *drive_info, struct part_entry *ptab, int *error)
{
	struct swsusp_header *header_p;
	int offset;
	int found;

	/* Read first page of the swap device */
	offset = ptab->start_lba;
	header_p = (struct swsusp_header *) read_sectors(drive_info, offset, PAGE_SIZE/SECTOR, error);

	if (!header_p)
		return -1; /* The error code has been stored in `error' */
	else {
		found = !memcmp(SWSUSP_SIG, header_p->sig, 10);
		free(header_p);
		return found;
	}
}
