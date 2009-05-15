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

#include <memory.h>
#include "hdt-menu.h"
#define E820MAX 128

/* Compute the e820 submenu */
static void compute_e820(struct s_my_menu *menu)
{
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];

  sprintf(buffer, " e820 Physical RAM map ");
  menu->items_count = 0;
  menu->menu = add_menu(buffer, -1);

  struct e820entry map[E820MAX];
  int  count = 0;
  char type[14];

  detect_memory_e820(map, E820MAX, &count);
  for (int j = 0; j < count; j++) {
	get_type(map[j].type, type, 14);
        snprintf(buffer, sizeof buffer,
		"%016llx - %016llx (%s)",
		map[j].addr, map[j].size,
		remove_spaces(type));
	snprintf(statbuffer, sizeof statbuffer,
		"%016llx - %016llx (%s)",
		map[j].addr, map[j].size,
		remove_spaces(type));
  	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  	menu->items_count++;
  }
}

/* Compute the e801 submenu */
static void compute_e801(struct s_my_menu *menu)
{
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];

  sprintf(buffer, " e801 information ");
  menu->items_count = 0;
  menu->menu = add_menu(buffer, -1);

  int mem_low, mem_high = 0;
  if (detect_memory_e801(&mem_low, &mem_high)) {
        snprintf(buffer, sizeof buffer, "%s", "e801 output is bogus");
        snprintf(statbuffer, sizeof statbuffer, "%s", "e801 output is bogus");
  	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  	menu->items_count++;
  } else {
        snprintf(buffer, sizeof buffer, "%d Kb (%d MiB) - %d Kb (%d MiB)",
                mem_low, mem_low >> 10, mem_high << 6, mem_high >> 4);
        snprintf(statbuffer, sizeof statbuffer, "%d Kb (%d MiB) - %d Kb (%d MiB)",
                mem_low, mem_low >> 10, mem_high << 6, mem_high >> 4);
  	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  }
  menu->items_count++;
}

/* Compute the e88 submenu */
static void compute_e88(struct s_my_menu *menu)
{
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];

  sprintf(buffer, " e88 information ");
  menu->items_count = 0;
  menu->menu = add_menu(buffer, -1);

  int mem_size = 0;
  if (detect_memory_88(&mem_size)) {
        snprintf(buffer, sizeof buffer, "%s", "e88 output is bogus");
        snprintf(statbuffer, sizeof statbuffer, "%s", "e88 output is bogus");
  	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  	menu->items_count++;
  } else {
        snprintf(buffer, sizeof buffer, "%d Kb (%d MiB)",
			mem_size, mem_size >> 10);
        snprintf(statbuffer, sizeof statbuffer, "%d Kb (%d MiB)",
			mem_size, mem_size >> 10);
  	add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  }
  menu->items_count++;
}
/* Compute the Memory submenu */
static void compute_memory_module(struct s_my_menu *menu, s_dmi * dmi,
                                  int slot_number)
{
  int i = slot_number;
  char buffer[MENULEN + 1];
  char statbuffer[STATLEN + 1];

  sprintf(buffer, " Bank <%d> ", i);
  menu->items_count = 0;
  menu->menu = add_menu(buffer, -1);

  snprintf(buffer, sizeof buffer, "Form Factor  : %s",
     dmi->memory[i].form_factor);
  snprintf(statbuffer, sizeof statbuffer, "Form Factor: %s",
     dmi->memory[i].form_factor);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Type         : %s",
     dmi->memory[i].type);
  snprintf(statbuffer, sizeof statbuffer, "Type: %s",
     dmi->memory[i].type);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Type Details : %s",
     dmi->memory[i].type_detail);
  snprintf(statbuffer, sizeof statbuffer, "Type Details: %s",
     dmi->memory[i].type_detail);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Speed        : %s",
     dmi->memory[i].speed);
  snprintf(statbuffer, sizeof statbuffer, "Speed (Mhz): %s",
     dmi->memory[i].speed);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Size         : %s",
     dmi->memory[i].size);
  snprintf(statbuffer, sizeof statbuffer, "Size: %s",
     dmi->memory[i].size);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Device Set   : %s",
     dmi->memory[i].device_set);
  snprintf(statbuffer, sizeof statbuffer, "Device Set: %s",
     dmi->memory[i].device_set);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Device Loc.  : %s",
     dmi->memory[i].device_locator);
  snprintf(statbuffer, sizeof statbuffer, "Device Location: %s",
     dmi->memory[i].device_locator);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Bank Locator : %s",
     dmi->memory[i].bank_locator);
  snprintf(statbuffer, sizeof statbuffer, "Bank Locator: %s",
     dmi->memory[i].bank_locator);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Total Width  : %s",
     dmi->memory[i].total_width);
  snprintf(statbuffer, sizeof statbuffer, "Total bit Width: %s",
     dmi->memory[i].total_width);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Data Width   : %s",
     dmi->memory[i].data_width);
  snprintf(statbuffer, sizeof statbuffer, "Data bit Width: %s",
     dmi->memory[i].data_width);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Error        : %s",
     dmi->memory[i].error);
  snprintf(statbuffer, sizeof statbuffer, "Error: %s",
     dmi->memory[i].error);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Vendor       : %s",
     dmi->memory[i].manufacturer);
  snprintf(statbuffer, sizeof statbuffer, "Vendor: %s",
     dmi->memory[i].manufacturer);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Serial       : %s",
     dmi->memory[i].serial);
  snprintf(statbuffer, sizeof statbuffer, "Serial: %s",
     dmi->memory[i].serial);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Asset Tag    : %s",
     dmi->memory[i].asset_tag);
  snprintf(statbuffer, sizeof statbuffer, "Asset Tag: %s",
     dmi->memory[i].asset_tag);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Part Number  : %s",
     dmi->memory[i].part_number);
  snprintf(buffer, sizeof statbuffer, "Part Number: %s",
     dmi->memory[i].part_number);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

}

/* Compute the Memory menu */
void compute_memory(struct s_hdt_menu *menu, s_dmi * dmi, struct s_hardware *hardware)
{
  char buffer[MENULEN + 1];
  int i=0;
  for (i = 0; i < dmi->memory_count; i++) {
    compute_memory_module(&(menu->memory_sub_menu[i]), dmi, i);
  }

  compute_e820(&(menu->memory_sub_menu[++i]));
  compute_e801(&(menu->memory_sub_menu[++i]));
  compute_e88(&(menu->memory_sub_menu[++i]));

  menu->memory_menu.menu = add_menu(" Memory Banks ", -1);
  menu->memory_menu.items_count = 0;

  for (i = 0; i < dmi->memory_count; i++) {
    snprintf(buffer, sizeof buffer, " Bank <%d> ", i);
    add_item(buffer, "Memory Bank", OPT_SUBMENU, NULL,
       menu->memory_sub_menu[i].menu);
    menu->memory_menu.items_count++;
  }

  add_item("", "", OPT_SEP, "", 0);

  snprintf(buffer, sizeof buffer, " e820 ");
  add_item(buffer, "e820 mapping", OPT_SUBMENU, NULL,
       menu->memory_sub_menu[++i].menu);
  menu->memory_menu.items_count++;

  snprintf(buffer, sizeof buffer, " e801 ");
  add_item(buffer, "e801 information", OPT_SUBMENU, NULL,
       menu->memory_sub_menu[++i].menu);
  menu->memory_menu.items_count++;

  snprintf(buffer, sizeof buffer, " e88 ");
  add_item(buffer, "e88 information", OPT_SUBMENU, NULL,
       menu->memory_sub_menu[++i].menu);
  menu->memory_menu.items_count++;

  add_item("", "", OPT_SEP, "", 0);
  printf("MENU: Memory menu done (%d items)\n",
         menu->memory_menu.items_count);
  add_item("Run Test", "Run Test", OPT_RUN, hardware->memtest_label, 0);
}
