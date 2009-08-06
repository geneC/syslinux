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

#include <disk/errno_disk.h>
#include <disk/geom.h>
#include <disk/read.h>
#include <disk/error.h>
#include <disk/swsusp.h>
#include <disk/msdos.h>

#include "hdt-cli.h"
#include "hdt-common.h"
#include "hdt-util.h"

/**
 * show_partition_information - print information about a partition
 * @ptab:	part_entry describing the partition
 * @i:		Partition number (UI purposes only)
 * @ptab_root:	part_entry describing the root partition (extended only)
 * @drive_info:	driveinfo struct describing the drive on which the partition
 *		is
 *
 * Note on offsets (from hpa, see chain.c32):
 *
 *  To make things extra confusing: data partition offsets are relative to where
 *  the data partition record is stored, whereas extended partition offsets
 *  are relative to the beginning of the extended partition all the way back
 *  at the MBR... but still not absolute!
 **/
static void show_partition_information(struct driveinfo *drive_info,
				       struct part_entry *ptab,
				       int partition_offset,
				       int nb_partitions_seen)
{
	char size[8];
	char *parttype;
	int error = 0;
	char error_buffer[MAX_DISK_ERRNO];
	unsigned int start, end;

	int i = nb_partitions_seen;

	start = partition_offset;
	end = start + ptab->length;

	if (ptab->length > 0)
		sectors_to_size(ptab->length, size);
	else
		memset(size, 0, sizeof size);

	if (i == 1)
		more_printf("   #  B       Start         End    Size Id Type\n");

	get_label(ptab->ostype, &parttype);
	more_printf("  %2d  %s %11d %11d %s %02X %s",
		    i, (ptab->active_flag == 0x80) ? "x" : " ",
		    start,
		    end,
		    size,
		    ptab->ostype, parttype);

	/* Extra info */
	if (ptab->ostype == 0x82 && swsusp_check(drive_info, ptab)) {
		more_printf("%s", " (Swsusp sig. detected)");
	} else if (error) {
		get_error(&error_buffer);
		more_printf("%s\n", error_buffer);
		free(error_buffer);
	}

	more_printf("\n");

	free(parttype);
}

void main_show_disk(int argc, char **argv,
		    struct s_hardware *hardware)
{
	reset_more_printf();
	if (!argc) {
		more_printf("Which disk?\n");
		return;
	}

	int drive = strtol(argv[0], (char**) NULL, 16);

	if (drive < 0x80 || drive >= 0xff) {
		more_printf("Invalid disk: %d.\n", drive);
		return;
	}

	int i = drive - 0x80;
	struct driveinfo *d = &hardware->disk_info[i];
	char error_buffer[MAX_DISK_ERRNO];
	char disk_size[8];

	detect_disks(hardware);
	if (!hardware->disk_info[i].cbios)
		return; /* Invalid geometry */

	if ((int) d->edd_params.sectors > 0)
		sectors_to_size((int) d->edd_params.sectors, disk_size);
	else
		memset(disk_size, 0, sizeof disk_size);

	more_printf("DISK 0x%X:\n", d->disk);
	more_printf("  C/H/S: %d cylinders, %d heads, %d sectors/track\n",
		d->legacy_max_cylinder + 1, d->legacy_max_head + 1,
		d->legacy_sectors_per_track);
	more_printf("  EDD:   Version: %X\n", d->edd_version);
	more_printf("         Size: %s, %d bytes/sector, %d sectors/track\n",
		disk_size,
		(int) d->edd_params.bytes_per_sector,
		(int) d->edd_params.sectors_per_track);
	more_printf("         Host bus: %s, Interface type: %s\n\n",
		remove_spaces(d->edd_params.host_bus_type),
		remove_spaces(d->edd_params.interface_type));

	if (parse_partition_table(d, &show_partition_information) == -1) {
		get_error(&error_buffer);
		more_printf("%s\n", error_buffer);
		free(error_buffer);
	}
}

void main_show_disks(int argc __unused, char **argv __unused,
		     struct s_hardware *hardware)
{
	reset_more_printf();
	detect_disks(hardware);

	for (int drive = 0x80; drive < 0xff; drive++) {
		char buf[5] = "";
		sprintf(buf, "0x%x", drive);
		char *argv[1] = { buf };
		main_show_disk(1, argv, hardware);
	}
}

void disks_summary(int argc __unused, char** argv __unused,
		   struct s_hardware *hardware)
{
	int i = -1;

	detect_disks(hardware);

	for (int drive = 0x80; drive < 0xff; drive++) {
		i++;
		if (!hardware->disk_info[i].cbios)
			continue; /* Invalid geometry */
		struct driveinfo *d = &hardware->disk_info[i];
		char disk_size[8];

		if ((int) d->edd_params.sectors > 0)
			sectors_to_size((int) d->edd_params.sectors, disk_size);
		else
			memset(disk_size, 0, sizeof disk_size);

		more_printf("DISK 0x%X:\n", d->disk);
		more_printf("  C/H/S: %d cylinders, %d heads, %d sectors/track\n",
			d->legacy_max_cylinder + 1, d->legacy_max_head + 1,
			d->legacy_sectors_per_track);
		more_printf("  EDD:   Version: %X, size: %s\n", d->edd_version,
			disk_size);
		more_printf("         Host bus: %s, Interface type: %s\n\n",
			remove_spaces(d->edd_params.host_bus_type),
			remove_spaces(d->edd_params.interface_type));
	}
}

struct cli_callback_descr list_disk_show_modules[] = {
	{
		.name = "disks",
		.exec = main_show_disks,
	},
	{
		.name = "disk",
		.exec = main_show_disk,
	},
	{
		.name = NULL,
		.exec = NULL,
	},
};


struct cli_module_descr disk_show_modules = {
	.modules = list_disk_show_modules,
	.default_callback = disks_summary,
};

struct cli_mode_descr disk_mode = {
	.mode = DISK_MODE,
	.name = CLI_DISK,
	.default_modules = NULL,
	.show_modules = &disk_show_modules,
	.set_modules = NULL,
};
