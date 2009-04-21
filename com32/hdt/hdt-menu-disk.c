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

#include "hdt-menu.h"
#include "hdt-util.h"

static void compute_partition_info(int partnb,
				   struct part_entry *ptab,
				   char menu_title_ref[],
				   char menu_title[])
{
	char buffer[56];
	char statbuffer[STATLEN];
	int previous_size, size;
	char previous_unit[3], unit[3]; // GB
	char size_iec[8]; // GiB
	sectors_to_size_dec(previous_unit, &previous_size, unit, &size, ptab->length);
	sectors_to_size(ptab->length, size_iec);

	add_named_menu(menu_title_ref, menu_title, -1);
	set_menu_pos(5, 17);

	snprintf(buffer, sizeof buffer, "Partition # : %d",
		 partnb);
	snprintf(statbuffer, sizeof statbuffer, "Partition #: %d",
		 partnb);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Bootable    : %s",
		 (ptab->active_flag == 0x80) ? "Yes" : "No");
	snprintf(statbuffer, sizeof statbuffer, "Bootable: %s",
		 (ptab->active_flag == 0x80) ? "Yes" : "No");
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Start       : %d",
		 ptab->start_lba);
	snprintf(statbuffer, sizeof statbuffer, "Start: %d",
		 ptab->start_lba);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "End         : %d",
		 ptab->start_lba + ptab->length);
	snprintf(statbuffer, sizeof statbuffer, "End: %d",
		 ptab->start_lba + ptab->length);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Length      : %s/%d %s (%d)",
		 size_iec, size, unit, ptab->length);
	snprintf(statbuffer, sizeof statbuffer, "Length: %s/%d %s (%d)",
		 size_iec, size, unit, ptab->length);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	snprintf(buffer, sizeof buffer, "Id          : %X",
		 ptab->ostype);
	snprintf(statbuffer, sizeof statbuffer, "Id: %X",
		 ptab->ostype);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);

	char *parttype;
	get_label(ptab->ostype, &parttype);
	snprintf(buffer, sizeof buffer, "Type        : %s",
		 parttype);
	snprintf(statbuffer, sizeof statbuffer, "Type: %s",
		 parttype);
	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
	free(parttype);
}

/* Compute the disk submenu */
static int compute_disk_module(struct s_my_menu *menu, int nb_sub_disk_menu,
			       struct driveinfo *d, int disk_number)
{
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];
  char *mbr = NULL;

  snprintf(buffer, sizeof buffer, " Disk <0x%X> ", d[disk_number].disk);
  menu[nb_sub_disk_menu].menu = add_menu(buffer, -1);
  menu[nb_sub_disk_menu].items_count = 0;

  int previous_size, size;
  char previous_unit[3], unit[3]; // GB
  char size_iec[8]; // GiB
  sectors_to_size_dec(previous_unit, &previous_size, unit, &size, d[disk_number].edd_params.sectors);
  sectors_to_size(d[disk_number].edd_params.sectors, size_iec);

  snprintf(buffer, sizeof buffer, "Size          : %s/%d %s (%d %s)", size_iec,
     size, unit, previous_size, previous_unit);
  snprintf(statbuffer, sizeof statbuffer, "Size: %s/%d %s (%d %s)", size_iec, size,
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

	/* Compute disk partitions menus */
	mbr = read_mbr(d[disk_number].disk, NULL);
	if (mbr) {
		struct part_entry *ptab = (struct part_entry *)(mbr + PARTITION_TABLES_OFFSET);
		char menu_title[MENULEN + 1];
		char menu_title_ref[MENULEN + 1];
		/* The calls to add_item need to be done first to draw the main submenu first */
		int submenu_done = 0;
submenu_disk:
		for (int i = 0; i < 4; i++) {
			if (ptab[i].ostype) {
				snprintf(menu_title_ref, sizeof menu_title_ref, "disk_%x_part_%d", d[disk_number].disk, i);
				snprintf(menu_title, sizeof menu_title, "Disk <%X>, Partition %d", d[disk_number].disk, i);
				if (!submenu_done)
					add_item(menu_title, "Partition information (start, end, length, type, ...)",
						 OPT_SUBMENU, menu_title_ref, 0);
				else
					compute_partition_info(i, &ptab[i], menu_title_ref, menu_title);
			}
			/* Now, draw the sub sub menus */
			if (i == 3 && !submenu_done) {
				submenu_done = 1;
				goto submenu_disk;
			}
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
