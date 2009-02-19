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

/*
 * hdt.c
 *
 * An Hardware Detection Tool
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include "com32io.h"
#include "menu.h"
#include "help.h"
#include "passwords.h"
#include "dmi/dmi.h"
#include "sys/pci.h"
#include "hdt.h"
#include "hdt-menu.h"
#include "hdt-ata.h"

int nb_pci_devices=0;

/* Detecting if a DMI table exist
 * if yes, let's parse it */
int detect_dmi(s_dmi *dmi) {
  if (dmi_iterate(dmi) == -ENODMITABLE ) {
             printf("No DMI Structure found\n");
             return -ENODMITABLE;
  }

  parse_dmitable(dmi);
 return 0;
}

/* Try to detects disk from port 0x80 to 0xff*/
void detect_disks(struct diskinfo *disk_info) {
 for (int drive = 0x80; drive < 0xff; drive++) {
    if (get_disk_params(drive,disk_info) != 0)
          continue;
    struct diskinfo d=disk_info[drive];
    printf("  DISK 0x%X: %s : %s %s: sectors=%d, s/t=%d head=%d : EDD=%s\n",drive,d.aid.model,d.host_bus_type,d.interface_type, d.sectors, d.sectors_per_track,d.heads,d.edd_version);
 }
}

/* Setup our environement */
void setup_env() {
  char version[255];

  /* Opening the syslinux console */
  openconsole(&dev_stdcon_r, &dev_stdcon_w);

  sprintf(version,"%s %s by %s",PRODUCT_NAME,VERSION,AUTHOR);
  printf("%s\n",version);

  /* Creating the menu */
  init_menusystem(version);
  set_window_size(0,0,24,80); // Leave some space around

 // Register the menusystem handler
 // reg_handler(HDLR_SCREEN,&msys_handler);
  reg_handler(HDLR_KEYS,&keys_handler);

  // Register the ontimeout handler, with a time out of 10 seconds
  reg_ontimeout(ontimeout,1000,0);
}

/* Detect the hardware stuff */
void detect_hardware(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain, struct diskinfo *disk_info) {
  printf("CPU: Detecting\n");
  detect_cpu(cpu);

  printf("DISKS: Detecting\n");
  detect_disks(disk_info);

  printf("DMI: Detecting Table\n");
  if (detect_dmi(dmi) == -ENODMITABLE ) {
   is_dmi_valid=false;
   printf("DMI: ERROR ! Table not found ! \n");
   printf("DMI: Many hardware components will not be detected ! \n");
  } else {
   is_dmi_valid=true;
   printf("DMI: Table found ! (version %d.%d)\n",dmi->dmitable.major_version,dmi->dmitable.minor_version);
  }

#ifdef WITH_PCI
  printf("PCI: Detecting Devices\n");
  /* Scanning to detect pci buses and devices */
  *pci_domain = pci_scan();

  struct pci_device *pci_device;
  for_each_pci_func(pci_device, *pci_domain) {
	  nb_pci_devices++;
  }

  printf("PCI: %d Devices Found\n",nb_pci_devices);

  printf("PCI: Resolving names\n");
  /* Assigning product & vendor name for each device*/
  get_name_from_pci_ids(*pci_domain);

  printf("PCI: Resolving class names\n");
  /* Assigning class name for each device*/
  pci_ids=get_class_name_from_pci_ids(*pci_domain);


  printf("PCI: Resolving module names\n");
  /* Detecting which kernel module should match each device */
  modules_pcimap=get_module_name_from_pci_ids(*pci_domain);
#endif
}

/* Compute Main' Submenus*/
void compute_submenus(struct s_hdt_menu *hdt_menu, s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain, struct diskinfo *disk_info) {
 /* Compute this menus if a DMI table exist */
  if (is_dmi_valid) {
    compute_motherboard(&(hdt_menu->mobo_menu),dmi);
    compute_chassis(&(hdt_menu->chassis_menu),dmi);
    compute_system(&(hdt_menu->system_menu),dmi);
    compute_memory(hdt_menu,dmi);
    compute_bios(&(hdt_menu->bios_menu),dmi);
    compute_battery(&(hdt_menu->battery_menu),dmi);
  }

  compute_processor(&(hdt_menu->cpu_menu),cpu,dmi);
  compute_disks(hdt_menu,disk_info);
#ifdef WITH_PCI
  compute_PCI(hdt_menu,pci_domain);
  compute_kernel(&(hdt_menu->kernel_menu),pci_domain);
#endif
  compute_syslinuxmenu(&(hdt_menu->syslinux_menu));
  compute_aboutmenu(&(hdt_menu->about_menu));
}

/* Compute Main Menu*/
void compute_main_menu(struct s_hdt_menu *hdt_menu) {

  /* Let's count the number of menu we have */
  hdt_menu->total_menu_count=0;
  hdt_menu->main_menu.items_count=0;

  hdt_menu->main_menu.menu = add_menu(" Main Menu ",-1);
  set_item_options(-1,24);

#ifdef WITH_PCI
  add_item("PCI <D>evices","PCI Devices Menu",OPT_SUBMENU,NULL,hdt_menu->pci_menu.menu);
  hdt_menu->main_menu.items_count++;
  hdt_menu->total_menu_count+=hdt_menu->pci_menu.items_count;
#endif
  if (hdt_menu->disk_menu.items_count>0) {
     add_item("<D>isks","Disks Menu",OPT_SUBMENU,NULL,hdt_menu->disk_menu.menu);
     hdt_menu->main_menu.items_count++;
     hdt_menu->total_menu_count+=hdt_menu->disk_menu.items_count;
  }

  if (hdt_menu->memory_menu.items_count>0) {
     add_item("<M>emory Modules","Memory Modules Menu",OPT_SUBMENU,NULL,hdt_menu->memory_menu.menu);
     hdt_menu->main_menu.items_count++;
     hdt_menu->total_menu_count+=hdt_menu->memory_menu.items_count;
  }
  add_item("<P>rocessor","Main Processor Menu",OPT_SUBMENU,NULL,hdt_menu->cpu_menu.menu);
  hdt_menu->main_menu.items_count++;

if (is_dmi_valid) {
  add_item("M<o>therboard","Motherboard Menu",OPT_SUBMENU,NULL,hdt_menu->mobo_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<B>ios","Bios Menu",OPT_SUBMENU,NULL,hdt_menu->bios_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<C>hassis","Chassis Menu",OPT_SUBMENU,NULL,hdt_menu->chassis_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<S>ystem","System Menu",OPT_SUBMENU,NULL,hdt_menu->system_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("Ba<t>tery","Battery Menu",OPT_SUBMENU,NULL,hdt_menu->battery_menu.menu);
  hdt_menu->main_menu.items_count++;
}
  add_item("","",OPT_SEP,"",0);
#ifdef WITH_PCI
  add_item("<K>ernel Modules","Kernel Modules Menu",OPT_SUBMENU,NULL,hdt_menu->kernel_menu.menu);
  hdt_menu->main_menu.items_count++;
#endif
  add_item("<S>yslinux","Syslinux Information Menu",OPT_SUBMENU,NULL,hdt_menu->syslinux_menu.menu);
  hdt_menu->main_menu.items_count++;
  add_item("<A>bout","About Menu",OPT_SUBMENU,NULL,hdt_menu->about_menu.menu);
  hdt_menu->main_menu.items_count++;

  hdt_menu->total_menu_count+=hdt_menu->main_menu.items_count;
}

int main(void)
{
  struct s_hdt_menu hdt_menu;
  s_dmi dmi; /* DMI table */
  s_cpu cpu; /* CPU information */
  struct pci_domain *pci_domain=NULL; /* PCI Devices */
  struct diskinfo disk_info[256];     /* Disk Information*/

  /* Cleaning structures */
  memset(&disk_info,0,sizeof (disk_info));
  memset(&dmi,0,sizeof (dmi));
  memset(&cpu,0,sizeof (cpu));
  memset(&hdt_menu,0,sizeof (hdt_menu));

  /* Setup the environement */
  setup_env();

  /* Detect every kind of hardware */
  detect_hardware(&dmi,&cpu,&pci_domain,disk_info);

  /* Compute all sub menus */
  compute_submenus(&hdt_menu, &dmi,&cpu,&pci_domain,disk_info);

  /* Compute main menu */
  compute_main_menu(&hdt_menu);

#ifdef WITH_MENU_DISPLAY
  t_menuitem * curr;
  char cmd[160];

  printf("Starting Menu (%d menus)\n",hdt_menu.total_menu_count);
  curr=showmenus(hdt_menu.main_menu.menu);
  /* When we exit the menu, do we have something to do */
  if (curr) {
        /* When want to execute something */
        if (curr->action == OPT_RUN)
        {
            strcpy(cmd,curr->data);

	    /* Use specific syslinux call if needed */
	    if (issyslinux())
               runsyslinuxcmd(cmd);
            else csprint(cmd,0x07);
            return 1; // Should not happen when run from SYSLINUX
        }
  }
#endif

  return 0;
}
