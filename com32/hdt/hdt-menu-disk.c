/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
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

#include <stdlib.h>
#include <disk/geom.h>
#include <disk/read.h>
#include <disk/partition.h>
#include <disk/error.h>
#include <disk/msdos.h>
#include <disk/swsusp.h>

#include "hdt-menu.h"
#include "hdt-util.h"

static int dn;

static void show_partition_information(struct driveinfo *drive_info,
                                       struct part_entry *ptab,
                                       struct part_entry *ptab_root,
                                       int offset_root, int data_partitions_seen,
                                       int ebr_seen)
{
	char menu_title[MENULEN + 1];
	char menu_title_ref[MENULEN + 1];
	/* Useless code to prevent warnings */
	ptab=ptab; ptab_root=ptab_root;offset_root=offset_root;

        int i = 1 + ebr_seen * 4 + data_partitions_seen;

	memset(menu_title,0,sizeof menu_title);
	memset(menu_title_ref,0,sizeof menu_title_ref);
	snprintf(menu_title_ref, sizeof menu_title_ref, "disk_%x_part_%d", 
			drive_info[dn].disk, i);
	snprintf(menu_title, sizeof menu_title, "Partition %d", i);

	add_item(menu_title, "Partition information (start, end, length, type, ...)",
			OPT_SUBMENU, menu_title_ref, 0);

}
/**
 * compute_partition_information - print information about a partition
 * @ptab:       part_entry describing the partition
 * @i:          Partition number (UI purposes only)
 * @ptab_root:  part_entry describing the root partition (extended only)
 * @drive_info: driveinfo struct describing the drive on which the partition
 *              is
 *
 * Note on offsets (from hpa, see chain.c32):
 *
 *  To make things extra confusing: data partition offsets are relative to where
 *  the data partition record is stored, whereas extended partition offsets
 *  are relative to the beginning of the extended partition all the way back
 *  at the MBR... but still not absolute!
 **/
static void compute_partition_information(struct driveinfo *drive_info,
                                       struct part_entry *ptab,
                                       struct part_entry *ptab_root,
                                       int offset_root, int data_partitions_seen,
                                       int ebr_seen)
{
        char size[8];
        char *parttype;
        int error = 0;
        char *error_buffer;
        unsigned int start, end;
  	char buffer[SUBMENULEN+1];
  	char statbuffer[STATLEN+1];
	char menu_title[MENULEN + 1];
	char menu_title_ref[MENULEN + 1];

        int i = 1 + ebr_seen * 4 + data_partitions_seen;

	memset(buffer,0,sizeof buffer);
	memset(statbuffer,0,sizeof statbuffer);
	memset(menu_title,0,sizeof menu_title);
	memset(menu_title_ref,0,sizeof menu_title_ref);
	snprintf(menu_title_ref, sizeof menu_title_ref, "disk_%x_part_%d", drive_info[dn].disk, i);
	snprintf(menu_title, sizeof menu_title, "Partition %d", i);

   	add_named_menu(menu_title_ref,menu_title,-1);
   	set_menu_pos(SUBMENU_Y,SUBMENU_X);

	start = ptab->start_lba + ptab_root->start_lba + offset_root;
        end = (ptab->start_lba + ptab_root->start_lba) + ptab->length + offset_root;

        if (ptab->length > 0)
                sectors_to_size(ptab->length, size);
        else
                memset(size, 0, sizeof size);

        get_label(ptab->ostype, &parttype);

	snprintf(buffer, sizeof buffer, "Size        : %s)",
		 remove_spaces(size));
	snprintf(statbuffer, sizeof statbuffer, "Size : %s)",
		 remove_spaces(size));
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Type        : %s",
		 parttype);
	snprintf(statbuffer, sizeof statbuffer, "Type: %s",
		 parttype);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Bootable    : %s",
		 (ptab->active_flag == 0x80) ? "Yes" : "No");
	snprintf(statbuffer, sizeof statbuffer, "Bootable: %s",
		 (ptab->active_flag == 0x80) ? "Yes" : "No");
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Start       : %d",
		 start);
	snprintf(statbuffer, sizeof statbuffer, "Start: %d",
		 start);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "End         : %d",
		 end);
	snprintf(statbuffer, sizeof statbuffer, "End: %d",
		 end);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Id          : %X",
		 ptab->ostype);
	snprintf(statbuffer, sizeof statbuffer, "Id: %X",
		 ptab->ostype);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);


	free(parttype);

        /* Extra info */
        if (ptab->ostype == 0x82 && swsusp_check(drive_info, ptab, &error)) {
		snprintf(buffer, sizeof buffer, "%s","Swsusp sig  : detected");
		snprintf(statbuffer, sizeof statbuffer, "%s","Swsusp sig  : detected");
		add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
        } else if (error) {
                get_error(error, &error_buffer);
		snprintf(buffer, sizeof buffer, "%s",error_buffer);
		snprintf(statbuffer, sizeof statbuffer, "%s",error_buffer);
		add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
                free(error_buffer);
        }

}

/* Compute the disk submenu */
static int compute_disk_module(struct s_my_menu *menu, int nb_sub_disk_menu,
			       struct driveinfo *d, int disk_number)
{
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];

  snprintf(buffer, sizeof buffer, " Disk <0x%X> ", d[disk_number].disk);
  menu[nb_sub_disk_menu].menu = add_menu(buffer, -1);
  menu[nb_sub_disk_menu].items_count = 0;

  int previous_size, size;
  char previous_unit[3], unit[3]; // GB
  char size_iec[8]; // GiB
  sectors_to_size_dec(previous_unit, &previous_size, unit, &size, d[disk_number].edd_params.sectors);
  sectors_to_size(d[disk_number].edd_params.sectors, size_iec);

  snprintf(buffer, sizeof buffer, "Size          : %s/%d %s (%d %s)", remove_spaces(size_iec),
     size, unit, previous_size, previous_unit);
  snprintf(statbuffer, sizeof statbuffer, "Size: %s/%d %s (%d %s)", remove_spaces(size_iec), size,
     unit, previous_size, previous_unit);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Interface     : %s",
     d[disk_number].edd_params.interface_type);
  snprintf(statbuffer, sizeof statbuffer, "Interface: %s",
     d[disk_number].edd_params.interface_type);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Host Bus      : %s",
     d[disk_number].edd_params.host_bus_type);
  snprintf(statbuffer, sizeof statbuffer, "Host Bus Type: %s",
     d[disk_number].edd_params.host_bus_type);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Sectors       : %d",
     (int) d[disk_number].edd_params.sectors);
  snprintf(statbuffer, sizeof statbuffer, "Sectors: %d",
     (int) d[disk_number].edd_params.sectors);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Heads         : %d",
     d[disk_number].legacy_max_head + 1);
  snprintf(statbuffer, sizeof statbuffer, "Heads: %d",
     d[disk_number].legacy_max_head + 1);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Cylinders     : %d",
     d[disk_number].legacy_max_cylinder + 1);
  snprintf(statbuffer, sizeof statbuffer, "Cylinders: %d",
     d[disk_number].legacy_max_cylinder + 1);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Sectors/Track : %d",
     d[disk_number].legacy_sectors_per_track);
  snprintf(statbuffer, sizeof statbuffer, "Sectors per Track: %d",
     d[disk_number].legacy_sectors_per_track);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "Drive number  : 0x%X", d[disk_number].disk);
  snprintf(statbuffer, sizeof statbuffer, "Drive number: 0x%X", d[disk_number].disk);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  snprintf(buffer, sizeof buffer, "EDD Version   : %X",
     d[disk_number].edd_version);
  snprintf(statbuffer, sizeof statbuffer, "EDD Version: %X",
     d[disk_number].edd_version);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu[nb_sub_disk_menu].items_count++;

  add_sep();
  dn=disk_number;

  int error;
  parse_partition_table(d, &show_partition_information, &error);
  if (parse_partition_table(d, &compute_partition_information, &error)) {
        if (error) {
  	   char *error_buffer;
           get_error(error, &error_buffer);
	   snprintf(buffer, sizeof buffer, "I/O error   : %s", error_buffer);
	   snprintf(statbuffer, sizeof statbuffer, "I/O error   : %s", error_buffer);
  	   add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  	   menu[nb_sub_disk_menu].items_count++;
           free(error_buffer);
        } else {
	   snprintf(buffer, sizeof buffer, "An unknown error occured");
	   snprintf(statbuffer, sizeof statbuffer, "An unknown error occured");
  	   add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  	   menu[nb_sub_disk_menu].items_count++;
        }
  }

  return 0;
}

/* Compute the Disks menu */
void compute_disks(struct s_hdt_menu *menu, struct driveinfo *disk_info, struct s_hardware *hardware)
{
  char buffer[MENULEN + 1];
  int nb_sub_disk_menu = 0;

  /* No need to compute that menu if no disks were detected */
  menu->disk_menu.items_count = 0;
  if (hardware->disks_count == 0) return;

  for (int i = 0; i < hardware->disks_count; i++) {
    if (!hardware->disk_info[i].cbios)
      continue; /* Invalid geometry */
    compute_disk_module
        ((struct s_my_menu*) &(menu->disk_sub_menu), nb_sub_disk_menu, disk_info,
         i);
    nb_sub_disk_menu++;
  }

  menu->disk_menu.menu = add_menu(" Disks ", -1);

  for (int i = 0; i < nb_sub_disk_menu; i++) {
    snprintf(buffer, sizeof buffer, " Disk <%d> ", i+1);
    add_item(buffer, "Disk", OPT_SUBMENU, NULL,
       menu->disk_sub_menu[i].menu);
    menu->disk_menu.items_count++;
  }
  printf("MENU: Disks menu done (%d items)\n",
         menu->disk_menu.items_count);
}
