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
#include "cpuid.h"
#include "dmi/dmi.h"
#include "sys/pci.h"

#define INFLINE 22
#define PWDLINE 3
#define PWDPROMPT 21
#define PWDCOLUMN 60
#define PWDATTR 0x74
#define EDITPROMPT 21

#define WITH_PCI 1
#define WITH_MENU_DISPLAY 1

#define SECTOR 512              /* bytes/sector */

unsigned char MAIN_MENU, CPU_MENU, MOBO_MENU, CHASSIS_MENU, BIOS_MENU, SYSTEM_MENU, PCI_MENU;
unsigned char MEMORY_MENU,  MEMORY_SUBMENU[32], BATTERY_MENU;
bool is_dmi_valid=false;

struct diskinfo {
  int disk;
  int ebios;                    /* EBIOS supported on this disk */
  int cbios;                    /* CHS geometry is valid */
  int head;
  int sect;
  char edd_version[4];
};

/*
 * Get a disk block and return a malloc'd buffer.
 * Uses the disk number and information from disk_info.
 */
struct ebios_dapa {
  uint16_t len;
  uint16_t count;
  uint16_t off;
  uint16_t seg;
  uint64_t lba;
};

// BYTE=8
// WORD=16
// DWORD=32
// QWORD=64
struct device_parameter {
 uint16_t len;
 uint16_t info;
 uint32_t cylinders;
 uint32_t heads;
 uint32_t sectors_per_track;
 uint64_t sectors;
 uint16_t bytes_per_sector;
 uint32_t dpte_pointer;
 uint16_t device_path_information;
 uint8_t  device_path_lenght;
 uint8_t  device_path_reserved;
 uint16_t device_path_reserved_2;
 uint8_t  host_bus_type[4];
 uint8_t  interface_type[8];
 uint64_t interace_path;
 uint64_t device_path[2];
 uint8_t  reserved;
 uint8_t  cheksum;
};

/*
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 */
static int int13_retry(const com32sys_t *inreg, com32sys_t *outreg)
{
  int retry = 6;                /* Number of retries */
  com32sys_t tmpregs;

  if ( !outreg ) outreg = &tmpregs;

  while ( retry-- ) {
    __intcall(0x13, inreg, outreg);
    if ( !(outreg->eflags.l & EFLAGS_CF) )
      return 0;                 /* CF=0, OK */
  }

  return -1;                    /* Error */
}


static void *read_sector(struct diskinfo disk_info, unsigned int lba)
{
  com32sys_t inreg;
  struct ebios_dapa *dapa = __com32.cs_bounce;
  void *buf = (char *)__com32.cs_bounce + SECTOR;
  void *data;

  memset(&inreg, 0, sizeof inreg);

  if ( disk_info.ebios ) {
    dapa->len = sizeof(*dapa);
    dapa->count = 1;            /* 1 sector */
    dapa->off = OFFS(buf);
    dapa->seg = SEG(buf);
    dapa->lba = lba;

    inreg.esi.w[0] = OFFS(dapa);
    inreg.ds       = SEG(dapa);
    inreg.edx.b[0] = disk_info.disk;
    inreg.eax.b[1] = 0x42;      /* Extended read */
  } else {
    unsigned int c, h, s, t;

    if ( !disk_info.cbios ) {
      /* We failed to get the geometry */

      if ( lba )
        return NULL;            /* Can only read MBR */

      s = 1;  h = 0;  c = 0;
    } else {
      s = (lba % disk_info.sect) + 1;
      t = lba / disk_info.sect; /* Track = head*cyl */
      h = t % disk_info.head;
      c = t / disk_info.head;
    }
  if ( s > 63 || h > 256 || c > 1023 )
      return NULL;

    inreg.eax.w[0] = 0x0201;    /* Read one sector */
    inreg.ecx.b[1] = c & 0xff;
    inreg.ecx.b[0] = s + (c >> 6);
    inreg.edx.b[1] = h;
    inreg.edx.b[0] = disk_info.disk;
    inreg.ebx.w[0] = OFFS(buf);
    inreg.es       = SEG(buf);
  }

  if (int13_retry(&inreg, NULL))
    return NULL;

  data = malloc(SECTOR);
  if (data)
    memcpy(data, buf, SECTOR);
  return data;
}


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


static int get_disk_params(int disk, struct diskinfo *disk_info)
{
  static com32sys_t getparm, parm, getebios, ebios, inreg,outreg;
  struct device_parameter *dp = __com32.cs_bounce;

  memset(&inreg, 0, sizeof inreg);

  disk_info[disk].disk = disk;
  disk_info[disk].ebios = disk_info[disk].cbios = 0;

  /* Get EBIOS support */
  getebios.eax.w[0] = 0x4100;
  getebios.ebx.w[0] = 0x55aa;
  getebios.edx.b[0] = disk;
  getebios.eflags.b[0] = 0x3;   /* CF set */

  __intcall(0x13, &getebios, &ebios);

  if ( !(ebios.eflags.l & EFLAGS_CF) &&
       ebios.ebx.w[0] == 0xaa55 &&
       (ebios.ecx.b[0] & 1) ) {
    disk_info[disk].ebios = 1;
    switch(ebios.eax.b[1]) {
	    case 32:  strcpy(disk_info[disk].edd_version,"1.0"); break;
	    case 33:  strcpy(disk_info[disk].edd_version,"1.1"); break;
	    case 48:  strcpy(disk_info[disk].edd_version,"3.0"); break;
	    default:  strcpy(disk_info[disk].edd_version,"0"); break;
    }
  }

  /* Get disk parameters -- really only useful for
     hard disks, but if we have a partitioned floppy
     it's actually our best chance... */
  getparm.eax.b[1] = 0x08;
  getparm.edx.b[0] = disk;

  __intcall(0x13, &getparm, &parm);

  if ( parm.eflags.l & EFLAGS_CF )
    return disk_info[disk].ebios ? 0 : -1;

  disk_info[disk].head = parm.edx.b[1]+1;
  disk_info[disk].sect = parm.ecx.b[0] & 0x3f;
  if ( disk_info[disk].sect == 0 ) {
    disk_info[disk].sect = 1;
  } else {
   disk_info[disk].cbios = 1;        /* Valid geometry */
     }

   inreg.esi.w[0] = OFFS(dp);
   inreg.ds       = SEG(dp);
   inreg.eax.w[0] = 0x4800;
   inreg.edx.b[0] = disk;

   __intcall(0x13, &inreg, &outreg);

   if ( outreg.eflags.l & EFLAGS_CF )
	   printf("Error while detecting disk parameters\n");

   printf("RESULT=0x%X 0x%X 0x%X 0x%X\n",dp->host_bus_type[0],dp->host_bus_type[1],dp->host_bus_type[2],dp->host_bus_type[3]);
   printf("RESULT=0x%X 0x%X 0x%X 0x%X\n",dp->interface_type[0],dp->interface_type[1],dp->interface_type[2],dp->interface_type[3]);
   printf("RESULT= cylindres=%d heads=%d sect=%d bytes_per_sector=%d\n",dp->cylinders, dp->heads,dp->sectors/2/1024,dp->bytes_per_sector);
return 0;
}

int detect_dmi(s_dmi *dmi) {
  if ( ! dmi_iterate() ) {
             printf("No DMI Structure found\n");
             return -1;
  }

  parse_dmitable(dmi);
 return 0;
}

void detect_disks(struct diskinfo *disk_info) {
 char *buf;
 for (int drive = 0x80; drive <= 0xff; drive++) {
    if (get_disk_params(drive,disk_info))
          continue;

    printf("DISK: 0x%X, sector=%d head=%d : EDD=%s\n",drive,disk_info[drive].sect,disk_info[drive].head,disk_info[drive].edd_version);
 }
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

void detect_hardware(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain, struct diskinfo *disk_info) {
  printf("CPU: Detecting\n");
  detect_cpu(cpu);

  printf("DISKS: Detecting\n");
  detect_disks(disk_info);

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
  struct diskinfo disk_info[255];

  setup_env();

  detect_hardware(&dmi,&cpu,&pci_domain,disk_info);

  compute_submenus(&dmi,&cpu,&pci_domain);

  compute_main_menu();

#ifdef WITH_MENU_DISPLAY
  printf("Starting Menu\n");
  showmenus(MAIN_MENU);
#endif

  return 0;
}
