/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <disk/geom.h>
#include <disk/read.h>
#include <disk/util.h>

#include "hdt-cli.h"
#include "hdt-common.h"

static void process_br(struct driveinfo *drive_info, struct part_entry *ptab, int start);

/**
 * show_partition_information - print information about a partition
 * @ptab:	part_entry describing the partition
 * @i:		Partition number (UI purposes only)
 *
 * Note on offsets (from hpa, see chain.c32):
 *
 *  To make things extra confusing: data partition offsets are relative to where
 *  the data partition record is stored, whereas extended partition offsets
 *  are relative to the beginning of the extended partition all the way back
 *  at the MBR... but still not absolute!
 **/
static void show_partition_information(struct part_entry *ptab, int i)
{
	char *parttype;
	get_label(ptab->ostype, &parttype);
	more_printf("  %d  %s %8d %8d %8d %02X %s\n",
		    i, (ptab->active_flag == 0x80) ? " x " : "   ",
		    ptab->start_lba,
		    ptab->start_lba + ptab->length, ptab->length,
		    ptab->ostype, parttype);
	free(parttype);
}

/**
 * process_ebr - print information for partitions contained in an ebr
 * @drive_info:	driveinfo struct describing the drive
 * @ptab_root:	part_entry struct describing the root partition (pointing to the ebr)
 * @ebr_seen:	Number of ebr processed (UI purposes only)
 **/
static void process_ebr(struct driveinfo *drive_info, struct part_entry *ptab_root,
		        int ebr_seen)
{
	/* The ebr is located at the first sector of the extended partition */
	char* ebr = read_sectors(drive_info, ptab_root->start_lba, 1);
	if (!ebr) {
		more_printf("Unable to read the ebr.");
		return;
	}

	struct part_entry *ptab_child = (struct part_entry *)(ebr + PARTITION_TABLES_OFFSET);
	return process_br(drive_info, ptab_child, ebr_seen);
}

/**
 * process_br - print information for partitions contained in an {m,e}br
 * @drive_info:	driveinfo struct describing the drive
 * @ptab_root:	part_entry struct describing the root partition
 *		(pointing to the {m,e}br)
 * @ebr_seen:	Number of ebr processed (UI purposes only)
 **/
static void process_br(struct driveinfo *drive_info, struct part_entry *ptab,
		       int ebr_seen)
{
	for (int i = 0; i < 4; i++) {
		if (ptab[i].ostype) {
			show_partition_information(&ptab[i], ebr_seen * 4 + i + 1);

			/* 3 types for extended partitions */
			if ( ptab[i].ostype == 0x05 ||
			     ptab[i].ostype == 0x0f ||
			     ptab[i].ostype == 0x85)
				process_ebr(drive_info, &ptab[i], ebr_seen + 1);
		}
	}
}

void main_show_disk(int argc __unused, char **argv __unused,
		    struct s_hardware *hardware)
{
	int i = -1;

	detect_disks(hardware);

	for (int drive = 0x80; drive < 0xff; drive++) {
		i++;
		if (!hardware->disk_info[i].cbios)
			continue; /* Invalid geometry */
		struct driveinfo *d = &hardware->disk_info[i];

		more_printf("DISK 0x%X:\n", d->disk);
		more_printf("  C/H/S: %d heads, %d cylinders\n",
			d->legacy_max_head + 1, d->legacy_max_cylinder + 1);
		more_printf("         %d sectors/track, %d drives\n",
			d->legacy_sectors_per_track, d->legacy_max_drive);
		more_printf("  EDD:   ebios=%d, EDD version: %X\n",
			d->ebios, d->edd_version);
		more_printf("         %d heads, %d cylinders\n",
			(int) d->edd_params.heads, (int) d->edd_params.cylinders);
		more_printf("         %d sectors, %d bytes/sector, %d sectors/track\n",
			(int) d->edd_params.sectors, (int) d->edd_params.bytes_per_sector,
			(int) d->edd_params.sectors_per_track);
		more_printf("         Host bus: %s, Interface type: %s\n\n",
			d->edd_params.host_bus_type, d->edd_params.interface_type);

		char *mbr = read_mbr(d->disk);
		if (!mbr) {
			more_printf("Unable to read the mbr.");
			continue;
		}

		more_printf("  # Boot    Start      End   Blocks Id Type\n");
		struct part_entry *ptab = (struct part_entry *)(mbr + PARTITION_TABLES_OFFSET);
		process_br(d, ptab, 0);
	}
}

struct cli_module_descr disk_show_modules = {
	.modules = NULL,
	.default_callback = main_show_disk,
};

struct cli_mode_descr disk_mode = {
	.mode = DISK_MODE,
	.name = CLI_DISK,
	.default_modules = NULL,
	.show_modules = &disk_show_modules,
	.set_modules = NULL,
};
