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

#include "hdt-menu.h"

/* Computing Summary menu */
void compute_summarymenu(struct s_my_menu *menu, struct s_hardware *hardware)
{
  char buffer[SUBMENULEN + 1];
  char statbuffer[STATLEN + 1];
  char bank_number[10];

  menu->menu = add_menu(" Summary ", -1);
  menu->items_count = 0;

  set_menu_pos(SUBMENU_Y, SUBMENU_X);

  snprintf(buffer, sizeof buffer, "CPU Vendor    : %s",
     hardware->cpu.vendor);
  snprintf(statbuffer, sizeof statbuffer, "CPU Vendor: %s",
     hardware->cpu.vendor);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "CPU Model     : %s",
     hardware->cpu.model);
  snprintf(statbuffer, sizeof statbuffer, "CPU Model: %s",
     hardware->cpu.model);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  add_item("", "", OPT_SEP, "", 0);
  if (hardware->is_dmi_valid == true) {

    snprintf(buffer, sizeof buffer, "System Vendor : %s",
       hardware->dmi.system.manufacturer);
    snprintf(statbuffer, sizeof statbuffer, "System Vendor: %s",
       hardware->dmi.system.manufacturer);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    snprintf(buffer, sizeof buffer, "System Product: %s",
       hardware->dmi.system.product_name);
    snprintf(statbuffer, sizeof statbuffer,
       "System Product Name: %s",
       hardware->dmi.system.product_name);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    snprintf(buffer, sizeof buffer, "System Serial : %s",
       hardware->dmi.system.serial);
    snprintf(statbuffer, sizeof statbuffer,
       "System Serial Number: %s",
       hardware->dmi.system.serial);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    add_item("", "", OPT_SEP, "", 0);

    snprintf(buffer, sizeof buffer, "Bios Version  : %s",
       hardware->dmi.bios.version);
    snprintf(statbuffer, sizeof statbuffer, "Bios Version: %s",
       hardware->dmi.bios.version);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    snprintf(buffer, sizeof buffer, "Bios Release  : %s",
       hardware->dmi.bios.release_date);
    snprintf(statbuffer, sizeof statbuffer, "Bios Release Date: %s",
       hardware->dmi.bios.release_date);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    add_item("", "", OPT_SEP, "", 0);

    for (int i = 0; i < hardware->dmi.memory_count; i++) {
      if (hardware->dmi.memory[i].filled == true) {
        /* When discovering the first item, let's clear the screen */
        memset(bank_number, 0, sizeof(bank_number));
        snprintf(bank_number, sizeof(bank_number),
           "%d ", i);
        if (strncmp
            (hardware->dmi.memory[i].size, "Free", 4)) {
          snprintf(buffer, sizeof buffer,
             "Mem bank %02d   : %s %s@%s",
             i,
             hardware->dmi.memory[i].size,
             hardware->dmi.memory[i].type,
             hardware->dmi.memory[i].speed);
          snprintf(statbuffer, sizeof statbuffer,
             "Memory bank %02d   : %s %s@%s",
             i,
             hardware->dmi.memory[i].size,
             hardware->dmi.memory[i].type,
             hardware->dmi.memory[i].speed);
          add_item(buffer, statbuffer,
             OPT_INACTIVE, NULL, 0);
          menu->items_count++;
        }
      }
    }

    add_item("", "", OPT_SEP, "", 0);
  }

  snprintf(buffer, sizeof buffer, "Nb PCI Devices: %d",
     hardware->nb_pci_devices);
  snprintf(statbuffer, sizeof statbuffer, "Number of PCI Devices: %d",
     hardware->nb_pci_devices);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  add_item("", "", OPT_SEP, "", 0);

  if (hardware->is_pxe_valid == true) {
    struct s_pxe *p = &hardware->pxe;

    snprintf(buffer, sizeof buffer, "PXE MAC Address: %s",
       p->mac_addr);
    snprintf(statbuffer, sizeof statbuffer, "PXE MAC Address: %s",
       p->mac_addr);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    snprintf(buffer, sizeof buffer, "PXE IP Address : %d.%d.%d.%d",
       p->ip_addr[0], p->ip_addr[1], p->ip_addr[2],
       p->ip_addr[3]);
    snprintf(statbuffer, sizeof statbuffer,
       "PXE IP Address: %d.%d.%d.%d", p->ip_addr[0],
       p->ip_addr[1], p->ip_addr[2], p->ip_addr[3]);
    add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
    menu->items_count++;

    add_item("", "", OPT_SEP, "", 0);
  }

  if (hardware->modules_pcimap_return_code != -ENOMODULESPCIMAP) {
    bool kmod = false;
    struct pci_device *pci_device;
    char kernel_modules[LINUX_KERNEL_MODULE_SIZE *
            MAX_KERNEL_MODULES_PER_PCI_DEVICE];

    /*
     * For every detected pci device, grab its kernel module to compute
     * this submenu
     */
    for_each_pci_func(pci_device, hardware->pci_domain) {
      memset(kernel_modules, 0, sizeof kernel_modules);
      for (int i = 0;
           i <
           pci_device->dev_info->linux_kernel_module_count;
           i++) {
        if (i > 0) {
          strncat(kernel_modules, " | ", 3);
        }
        strncat(kernel_modules,
          pci_device->dev_info->
          linux_kernel_module[i],
          LINUX_KERNEL_MODULE_SIZE - 1);
      }
      /* No need to add unknown kernel modules */
      if (strlen(kernel_modules) > 0) {
        snprintf(buffer, sizeof buffer, "%s (%s)",
           kernel_modules,
           pci_device->dev_info->class_name);
        snprintf(statbuffer, sizeof statbuffer,
           "%04x:%04x %s : %s\n",
           pci_device->vendor,
           pci_device->product,
           pci_device->dev_info->vendor_name,
           pci_device->dev_info->product_name);

        add_item(buffer, statbuffer, OPT_INACTIVE, NULL,
           0);
        menu->items_count++;
        kmod = true;
      }
    }
    if (kmod == true)
      add_item("", "", OPT_SEP, "", 0);
  }

  printf("MENU: Summary menu done (%d items)\n", menu->items_count);
}
