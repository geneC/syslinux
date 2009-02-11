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
#include <console.h>
#include "com32io.h"
#include "menu.h"
#include "help.h"
#include "passwords.h"
#include "cpuid.h"
#include "dmi/dmi.h"
#include "sys/pci.h"

#define INFLINE 22
#define PWDLINE 3
#define PWDPROMPT 21
#define PWDCOLUMN 60
#define PWDATTR 0x74
#define EDITPROMPT 21

//#define WITH_PCI 1
#define WITH_MENU_DISPLAY 1
unsigned char MAIN_MENU, CPU_MENU, MOBO_MENU, CHASSIS_MENU, BIOS_MENU, SYSTEM_MENU, PCI_MENU;
unsigned char MEMORY_MENU,  MEMORY_SUBMENU[32], BATTERY_MENU;
bool is_dmi_valid=false;

TIMEOUTCODE ontimeout()
{
	  beep();
	    return CODE_WAIT;
}

void keys_handler(t_menusystem *ms, t_menuitem *mi,unsigned int scancode)
{
   char nc;

   if ((scancode >> 8) == F1) { // If scancode of F1
      runhelpsystem(mi->helpid);
   }
   // If user hit TAB, and item is an "executable" item
   // and user has privileges to edit it, edit it in place.
   if (((scancode & 0xFF) == 0x09) && (mi->action == OPT_RUN)) {
//(isallowed(username,"editcmd") || isallowed(username,"root"))) {
     nc = getnumcols();
     // User typed TAB and has permissions to edit command line
     gotoxy(EDITPROMPT,1,ms->menupage);
     csprint("Command line:",0x07);
     editstring(mi->data,ACTIONLEN);
     gotoxy(EDITPROMPT,1,ms->menupage);
     cprint(' ',0x07,nc-1,ms->menupage);
   }
}

int detect_dmi(s_dmi *dmi) {
  if ( ! dmi_iterate() ) {
             printf("No DMI Structure found\n");
             return -1;
  }

  parse_dmitable(dmi);
 return 0;
}

void compute_PCI(unsigned char *menu,struct pci_domain **pci_domain) {
  char buffer[MENULEN];
  char infobar[MENULEN];
  *menu = add_menu(" PCI Devices ",-1);

  struct pci_device *pci_device;
  for_each_pci_func(pci_device, *pci_domain) {
        snprintf(buffer,MENULEN,"%s : %s\n",
		pci_device->dev_info->vendor_name,
		pci_device->dev_info->product_name);
        snprintf(infobar, MENULEN,"%02x:%02x.%01x # %s # ID:%04x:%04x[%04x:%04x] # Kmod:%s\n",
               __pci_bus, __pci_slot, __pci_func,pci_device->dev_info->class_name,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product,pci_device->dev_info->linux_kernel_module);
	add_item(buffer,infobar,OPT_INACTIVE,NULL,0);
  }
}


void compute_battery(unsigned char *menu, s_dmi *dmi) {
  char buffer[MENULEN];
  *menu = add_menu(" Battery ",-1);

  snprintf(buffer,MENULEN,"Vendor          : %s",dmi->battery.manufacturer);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Manufacture Date: %s",dmi->battery.manufacture_date);
  add_item(buffer,"Manufacture Date",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Serial          : %s",dmi->battery.serial);
  add_item(buffer,"Serial",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Name            : %s",dmi->battery.name);
  add_item(buffer,"Name",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Chemistry       : %s",dmi->battery.chemistry);
  add_item(buffer,"Chemistry",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Design Capacity : %s",dmi->battery.design_capacity);
  add_item(buffer,"Design Capacity",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Design Voltage  : %s",dmi->battery.design_voltage);
  add_item(buffer,"Design Voltage",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"SBDS            : %s",dmi->battery.sbds);
  add_item(buffer,"SBDS",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"SBDS Manuf. Date: %s",dmi->battery.sbds_manufacture_date);
  add_item(buffer,"SBDS Manufacture Date",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"SBDS Chemistry  : %s",dmi->battery.sbds_chemistry);
  add_item(buffer,"SBDS Chemistry",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Maximum Error   : %s",dmi->battery.maximum_error);
  add_item(buffer,"Maximum Error (%)",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"OEM Info        : %s",dmi->battery.oem_info);
  add_item(buffer,"OEM Info",OPT_INACTIVE,NULL,0);
}


void compute_memory_module(unsigned char *menu, s_dmi *dmi, int slot_number) {
  int i=slot_number;
  char buffer[MENULEN];
  sprintf(buffer," Module <%d> ",i);
  *menu = add_menu(buffer,-1);

  sprintf(buffer,"Form Factor  : %s",dmi->memory[i].form_factor);
  add_item(buffer,"Form Factor",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Type         : %s",dmi->memory[i].type);
  add_item(buffer,"Memory Type",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Type Details : %s",dmi->memory[i].type_detail);
  add_item(buffer,"Memory Ty^e Details",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Speed        : %s",dmi->memory[i].speed);
  add_item(buffer,"Speed (MHz)",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Size         : %s",dmi->memory[i].size);
  add_item(buffer,"Size",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Device Set   : %s",dmi->memory[i].device_set);
  add_item(buffer,"Device Set",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Device Loc.  : %s",dmi->memory[i].device_locator);
  add_item(buffer,"Device Location",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Bank Locator : %s",dmi->memory[i].bank_locator);
  add_item(buffer,"Bank Location",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Total Width  : %s",dmi->memory[i].total_width);
  add_item(buffer,"Total bit Width",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Data Width   : %s",dmi->memory[i].data_width);
  add_item(buffer,"Data bit Width",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Error        : %s",dmi->memory[i].error);
  add_item(buffer,"Error",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Vendor       : %s",dmi->memory[i].manufacturer);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Serial       : %s",dmi->memory[i].serial);
  add_item(buffer,"Serial Number",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Asset Tag    : %s",dmi->memory[i].asset_tag);
  add_item(buffer,"Asset Tag",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Part Number  : %s",dmi->memory[i].part_number);
  add_item(buffer,"Part Number",OPT_INACTIVE,NULL,0);

}

void compute_motherboard(unsigned char *menu,s_dmi *dmi) {
  char buffer[MENULEN];
  *menu = add_menu(" Motherboard ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",dmi->base_board.manufacturer);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Product   : %s",dmi->base_board.product_name);
  add_item(buffer,"Product Name",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Version   : %s",dmi->base_board.version);
  add_item(buffer,"Version",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Serial    : %s",dmi->base_board.serial);
  add_item(buffer,"Serial Number",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Asset Tag : %s",dmi->base_board.asset_tag);
  add_item(buffer,"Asset Tag",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Location  : %s",dmi->base_board.location);
  add_item(buffer,"Location",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Type      : %s",dmi->base_board.type);
  add_item(buffer,"Type",OPT_INACTIVE,NULL,0);
}

void compute_system(unsigned char *menu,s_dmi *dmi) {
  char buffer[MENULEN];
  *menu = add_menu(" System ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",dmi->system.manufacturer);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Product   : %s",dmi->system.product_name);
  add_item(buffer,"Product Name",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Version   : %s",dmi->system.version);
  add_item(buffer,"Version",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Serial    : %s",dmi->system.serial);
  add_item(buffer,"Serial Number",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"UUID      : %s",dmi->system.uuid);
  add_item(buffer,"UUID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Wakeup    : %s",dmi->system.wakeup_type);
  add_item(buffer,"Wakeup Type",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"SKU Number: %s",dmi->system.sku_number);
  add_item(buffer,"SKU Number",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Family    : %s",dmi->system.family);
  add_item(buffer,"Family",OPT_INACTIVE,NULL,0);
}

void compute_chassis(unsigned char *menu,s_dmi *dmi) {
  char buffer[MENULEN];
  *menu = add_menu(" Chassis ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",dmi->chassis.manufacturer);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Type      : %s",dmi->chassis.type);
  add_item(buffer,"Type",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Version   : %s",dmi->chassis.version);
  add_item(buffer,"Version",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Serial    : %s",dmi->chassis.serial);
  add_item(buffer,"Serial Number",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Asset Tag : %s",dmi->chassis.asset_tag);
  add_item(buffer,"Asset Tag",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Lock      : %s",dmi->chassis.lock);
  add_item(buffer,"Lock",OPT_INACTIVE,NULL,0);
}

void compute_bios(unsigned char *menu,s_dmi *dmi) {
  char buffer[MENULEN];
  *menu = add_menu(" BIOS ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",dmi->bios.vendor);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Version   : %s",dmi->bios.version);
  add_item(buffer,"Version",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Release   : %s",dmi->bios.release_date);
  add_item(buffer,"Release Date",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Bios Rev. : %s",dmi->bios.bios_revision);
  add_item(buffer,"Bios Revision",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Fw.  Rev. : %s",dmi->bios.firmware_revision);
  add_item(buffer,"Firmware Revision",OPT_INACTIVE,NULL,0);
}

void compute_processor(unsigned char *menu,s_cpu *cpu, s_dmi *dmi) {
  char buffer[MENULEN];
  char buffer1[MENULEN];
  *menu = add_menu(" Main Processor ",-1);
  snprintf(buffer,MENULEN,"Vendor    : %s",cpu->vendor);
  add_item(buffer,"Vendor",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Model     : %s",cpu->model);
  add_item(buffer,"Model",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Vendor ID : %d",cpu->vendor_id);
  add_item(buffer,"Vendor ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Family ID : %d",cpu->family);
  add_item(buffer,"Family ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Model  ID : %d",cpu->model_id);
  add_item(buffer,"Model ID",OPT_INACTIVE,NULL,0);
  snprintf(buffer,MENULEN,"Stepping  : %d",cpu->stepping);
  add_item(buffer,"Stepping",OPT_INACTIVE,NULL,0);
  if (is_dmi_valid) {
   snprintf(buffer,MENULEN,"FSB       : %d",dmi->processor.external_clock);
   add_item(buffer,"Front Side Bus (MHz)",OPT_INACTIVE,NULL,0);
   snprintf(buffer,MENULEN,"Cur. Speed: %d",dmi->processor.current_speed);
   add_item(buffer,"Current Speed (MHz)",OPT_INACTIVE,NULL,0);
   snprintf(buffer,MENULEN,"Max Speed : %d",dmi->processor.max_speed);
   add_item(buffer,"Max Speed (MHz)",OPT_INACTIVE,NULL,0);
   snprintf(buffer,MENULEN,"Upgrade   : %s",dmi->processor.upgrade);
   add_item(buffer,"Upgrade",OPT_INACTIVE,NULL,0);
  }

  if (cpu->flags.smp)  snprintf(buffer,MENULEN,"SMP       : Yes");
  else snprintf(buffer,MENULEN,"SMP       : No");
  add_item(buffer,"SMP system",OPT_INACTIVE,NULL,0);

  if (cpu->flags.lm)  snprintf(buffer,MENULEN,"x86_64    : Yes");
  else snprintf(buffer,MENULEN,"X86_64    : No");
  add_item(buffer,"x86_64 compatible processor",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.fpu) strcat(buffer1,"fpu ");
  if (cpu->flags.vme) strcat(buffer1,"vme ");
  if (cpu->flags.de)  strcat(buffer1,"de ");
  if (cpu->flags.pse) strcat(buffer1,"pse ");
  if (cpu->flags.tsc) strcat(buffer1,"tsc ");
  if (cpu->flags.msr) strcat(buffer1,"msr ");
  if (cpu->flags.pae) strcat(buffer1,"pae ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.mce) strcat(buffer1,"mce ");
  if (cpu->flags.cx8) strcat(buffer1,"cx8 ");
  if (cpu->flags.apic) strcat(buffer1,"apic ");
  if (cpu->flags.sep) strcat(buffer1,"sep ");
  if (cpu->flags.mtrr) strcat(buffer1,"mtrr ");
  if (cpu->flags.pge) strcat(buffer1,"pge ");
  if (cpu->flags.mca) strcat(buffer1,"mca ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.cmov) strcat(buffer1,"cmov ");
  if (cpu->flags.pat)  strcat(buffer1,"pat ");
  if (cpu->flags.pse_36) strcat(buffer1,"pse_36 ");
  if (cpu->flags.psn)  strcat(buffer1,"psn ");
  if (cpu->flags.clflsh) strcat(buffer1,"clflsh ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.dts)  strcat(buffer1,"dts ");
  if (cpu->flags.acpi) strcat(buffer1,"acpi ");
  if (cpu->flags.mmx)  strcat(buffer1,"mmx ");
  if (cpu->flags.sse)  strcat(buffer1,"sse ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.sse2) strcat(buffer1,"sse2 ");
  if (cpu->flags.ss)   strcat(buffer1,"ss ");
  if (cpu->flags.htt)  strcat(buffer1,"ht ");
  if (cpu->flags.acc)  strcat(buffer1,"acc ");
  if (cpu->flags.syscall) strcat(buffer1,"syscall ");
  if (cpu->flags.mp)   strcat(buffer1,"mp ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.nx)    strcat(buffer1,"nx ");
  if (cpu->flags.mmxext) strcat(buffer1,"mmxext ");
  if (cpu->flags.lm)     strcat(buffer1,"lm ");
  if (cpu->flags.nowext) strcat(buffer1,"3dnowext ");
  if (cpu->flags.now)    strcat(buffer1,"3dnow! ");
  snprintf(buffer,MENULEN,"Flags     : %s",buffer1);
  add_item(buffer,"Flags",OPT_INACTIVE,NULL,0);

}

void setup_env() {
  openconsole(&dev_stdcon_r, &dev_stdcon_w);
  init_menusystem("Hardware Detection Tool by Erwan Velu");
  set_window_size(1,1,23,78); // Leave some space around

 // Register the menusystem handler
 // reg_handler(HDLR_SCREEN,&msys_handler);
  reg_handler(HDLR_KEYS,&keys_handler);

  // Register the ontimeout handler, with a time out of 10 seconds
  reg_ontimeout(ontimeout,1000,0);
}

void detect_hardware(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain) {
  printf("CPU: Detecting\n");
  detect_cpu(cpu);

  printf("DMI: Detecting Table\n");
  if (detect_dmi(dmi) == 0)
		is_dmi_valid=true;

  printf("PCI: Detecting Devices\n");
  /* Scanning to detect pci buses and devices */
  *pci_domain = pci_scan();

#ifdef WITH_PCI
  printf("PCI: Resolving names\n");
  /* Assigning product & vendor name for each device*/
  get_name_from_pci_ids(*pci_domain);

  printf("PCI: Resolving class names\n");
  /* Assigning class name for each device*/
  get_class_name_from_pci_ids(*pci_domain);

  printf("PCI: Resolving module names\n");
  /* Detecting which kernel module should match each device */
  get_module_name_from_pci_ids(*pci_domain);
#endif
}

void compute_memory(unsigned char *menu, s_dmi *dmi) {
char buffer[MENULEN];
for (int i=0;i<dmi->memory_count;i++) {
  compute_memory_module(&MEMORY_SUBMENU[i],dmi,i);
}

*menu = add_menu(" Modules ",-1);

for (int i=0;i<dmi->memory_count;i++) {
  sprintf(buffer," Module <%d> ",i);
  add_item(buffer,"Memory Module",OPT_SUBMENU,NULL,MEMORY_SUBMENU[i]);
}
}

void compute_submenus(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain) {
if (is_dmi_valid) {
  compute_motherboard(&MOBO_MENU,dmi);
  compute_chassis(&CHASSIS_MENU,dmi);
  compute_system(&SYSTEM_MENU,dmi);
  compute_memory(&MEMORY_MENU,dmi);
  compute_bios(&BIOS_MENU,dmi);
  compute_battery(&BATTERY_MENU,dmi);
}
  compute_processor(&CPU_MENU,cpu,dmi);
#ifdef WITH_PCI
  compute_PCI(&PCI_MENU,pci_domain);
#endif
}

void compute_main_menu() {
  MAIN_MENU = add_menu(" Main Menu ",-1);
  set_item_options(-1,24);
  add_item("<P>rocessor","Main Processor",OPT_SUBMENU,NULL,CPU_MENU);

if (is_dmi_valid) {
  add_item("<M>otherboard","Motherboard",OPT_SUBMENU,NULL,MOBO_MENU);
  add_item("<B>ios","Bios",OPT_SUBMENU,NULL,BIOS_MENU);
  add_item("<C>hassis","Chassis",OPT_SUBMENU,NULL,CHASSIS_MENU);
  add_item("<S>ystem","System",OPT_SUBMENU,NULL,SYSTEM_MENU);
  add_item("<M>emory Modules","Memory Modules",OPT_SUBMENU,NULL,MEMORY_MENU);
  add_item("Ba<t>tery","Battery",OPT_SUBMENU,NULL,BATTERY_MENU);
}
#ifdef WITH_PCI
  add_item("PCI <D>evices","PCI Devices",OPT_SUBMENU,NULL,PCI_MENU);
#endif
}

int main(void)
{
  s_dmi dmi;
  s_cpu cpu;
  struct pci_domain *pci_domain=NULL;

  setup_env();

  detect_hardware(&dmi,&cpu,&pci_domain);

  compute_submenus(&dmi,&cpu,&pci_domain);

  compute_main_menu();

#ifdef WITH_MENU_DISPLAY
  printf("Starting Menu\n");
  showmenus(MAIN_MENU);
#endif

  return 0;
}
