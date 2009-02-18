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

#ifndef DEFINE_HDT_MENU_H
#define DEFINE_HDT_MENU_H
#include <stdio.h>
#include "menu.h"
#include "cpuid.h"
#include "sys/pci.h"
#include "dmi/dmi.h"
#include "hdt-ata.h"

#define EDITPROMPT 21

#define SUBMENULEN 46

#define SUBMENU_Y 3
#define SUBMENU_X 29
unsigned char MAIN_MENU, CPU_MENU, MOBO_MENU, CHASSIS_MENU, BIOS_MENU, SYSTEM_MENU, PCI_MENU, KERNEL_MENU;
unsigned char MEMORY_MENU,  MEMORY_SUBMENU[32], DISK_MENU, DISK_SUBMENU[32], PCI_SUBMENU[128],BATTERY_MENU;
unsigned char SYSLINUX_MENU, ABOUT_MENU;

static int menu_count=0;

TIMEOUTCODE ontimeout();
void keys_handler(t_menusystem *ms, t_menuitem *mi,unsigned int scancode);

// PCI Stuff
static int pci_ids=0;
void compute_pci_device(unsigned char *menu,struct pci_device *pci_device,int pci_bus, int pci_slot, int pci_func);
int compute_PCI(unsigned char *menu, struct pci_domain **pci_domain);

// KERNEL Stuff
static int modules_pcimap=0;
void compute_KERNEL(unsigned char *menu,struct pci_domain **pci_domain);

// Disk Stuff
static int nb_sub_disk_menu=0;
void compute_disk_module(unsigned char *menu, struct diskinfo *d,int disk_number);
void compute_disks(unsigned char *menu, struct diskinfo *disk_info);

// DMI Stuff
void compute_motherboard(unsigned char *menu,s_dmi *dmi);
void compute_battery(unsigned char *menu, s_dmi *dmi);
void compute_system(unsigned char *menu,s_dmi *dmi);
void compute_chassis(unsigned char *menu,s_dmi *dmi);
void compute_bios(unsigned char *menu,s_dmi *dmi);
void compute_memory(unsigned char *menu, s_dmi *dmi);
void compute_memory_module(unsigned char *menu, s_dmi *dmi, int slot_number);

// Processor Stuff
static bool is_dmi_valid=false;
void compute_processor(unsigned char *menu,s_cpu *cpu, s_dmi *dmi);

// Syslinux stuff
void compute_syslinuxmenu(unsigned char *menu);

// About menu
void compute_aboutmenu(unsigned char *menu);
#endif
