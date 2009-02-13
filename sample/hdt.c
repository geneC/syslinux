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

enum {
        ATA_ID_FW_REV           = 23,
	ATA_ID_PROD             = 27,
        ATA_ID_FW_REV_LEN       = 8,
        ATA_ID_PROD_LEN         = 40,
};

unsigned char MAIN_MENU, CPU_MENU, MOBO_MENU, CHASSIS_MENU, BIOS_MENU, SYSTEM_MENU, PCI_MENU, KERNEL_MENU;
unsigned char MEMORY_MENU,  MEMORY_SUBMENU[32], DISK_MENU, DISK_SUBMENU[32], BATTERY_MENU;
int nb_sub_disk_menu=0;
bool is_dmi_valid=false;

#define ATTR_PACKED __attribute__((packed))

struct ata_identify_device {
  unsigned short words000_009[10];
  unsigned char  serial_no[20];
  unsigned short words020_022[3];
  unsigned char  fw_rev[8];
  unsigned char  model[40];
  unsigned short words047_079[33];
  unsigned short major_rev_num;
  unsigned short minor_rev_num;
  unsigned short command_set_1;
  unsigned short command_set_2;
  unsigned short command_set_extension;
  unsigned short cfs_enable_1;
  unsigned short word086;
  unsigned short csf_default;
  unsigned short words088_255[168];
} ATTR_PACKED;

struct diskinfo {
  int disk;
  int ebios;                    /* EBIOS supported on this disk */
  int cbios;                    /* CHS geometry is valid */
  int heads;
  int sectors_per_track;
  int sectors;
  int cylinders;
  char edd_version[4];
  struct ata_identify_device aid; /* IDENTIFY xxx DEVICE data */
  char host_bus_type[5];
  char interface_type[9];
  char interface_port;
} ATTR_PACKED;

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
} ATTR_PACKED;

/**
 *      ata_id_string - Convert IDENTIFY DEVICE page into string
 *      @id: IDENTIFY DEVICE results we will examine
 *      @s: string into which data is output
 *      @ofs: offset into identify device page
 *      @len: length of string to return. must be an even number.
 *
 *      The strings in the IDENTIFY DEVICE page are broken up into
 *      16-bit chunks.  Run through the string, and output each
 *      8-bit chunk linearly, regardless of platform.
 *
 *      LOCKING:
 *      caller.
 */

void ata_id_string(const uint16_t *id, unsigned char *s,
                   unsigned int ofs, unsigned int len)
{
        unsigned int c;

        while (len > 0) {
                c = id[ofs] >> 8;
                *s = c;
                s++;

                c = id[ofs] & 0xff;
                *s = c;
                s++;

                ofs++;
                len -= 2;
        }
}

/**
 *      ata_id_c_string - Convert IDENTIFY DEVICE page into C string
 *      @id: IDENTIFY DEVICE results we will examine
 *      @s: string into which data is output
 *      @ofs: offset into identify device page
 *      @len: length of string to return. must be an odd number.
 *
 *      This function is identical to ata_id_string except that it
 *      trims trailing spaces and terminates the resulting string with
 *      null.  @len must be actual maximum length (even number) + 1.
 *
 *      LOCKING:
 *      caller.
 */
void ata_id_c_string(const uint16_t *id, unsigned char *s,
                     unsigned int ofs, unsigned int len)
{
        unsigned char *p;

        //WARN_ON(!(len & 1));

        ata_id_string(id, s, ofs, len - 1);

        p = s + strnlen(s, len - 1);
        while (p > s && p[-1] == ' ')
                p--;
        *p = '\0';
}


static void printregs(const com32sys_t *r)
{
  printf("eflags = %08x  ds = %04x  es = %04x  fs = %04x  gs = %04x\n"
         "eax = %08x  ebx = %08x  ecx = %08x  edx = %08x\n"
         "ebp = %08x  esi = %08x  edi = %08x  esp = %08x\n",
         r->eflags.l, r->ds, r->es, r->fs, r->gs,
         r->eax.l, r->ebx.l, r->ecx.l, r->edx.l,
         r->ebp.l, r->esi.l, r->edi.l, r->_unused_esp.l);
}


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

TIMEOUTCODE ontimeout()
{
	 // beep();
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
  //char buffer[255];
  struct device_parameter dp;
//  struct ata_identify_device aid;

  disk_info[disk].disk = disk;
  disk_info[disk].ebios = disk_info[disk].cbios = 0;

   memset(&getebios, 0, sizeof (com32sys_t));
   memset(&ebios, 0, sizeof (com32sys_t));
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
  memset(&getparm, 0, sizeof (com32sys_t));
  memset(&parm, 0, sizeof (com32sys_t));
  getparm.eax.b[1] = 0x08;
  getparm.edx.b[0] = disk;

  __intcall(0x13, &getparm, &parm);

  if ( parm.eflags.l & EFLAGS_CF )
    return disk_info[disk].ebios ? 0 : -1;

  disk_info[disk].heads = parm.edx.b[1]+1;
  disk_info[disk].sectors_per_track = parm.ecx.b[0] & 0x3f;
  if ( disk_info[disk].sectors_per_track == 0 ) {
    disk_info[disk].sectors_per_track = 1;
  } else {
   disk_info[disk].cbios = 1;        /* Valid geometry */
     }

   memset(&dp, 0, sizeof(struct device_parameter));
   //FIXME: memset to 0 make it fails
//   memset(__com32.cs_bounce, 0, sizeof(struct device_parameter));
   memset(&inreg, 0, sizeof(com32sys_t));

   inreg.esi.w[0] = OFFS(__com32.cs_bounce);
   inreg.ds       = SEG(__com32.cs_bounce);
   inreg.eax.w[0] = 0x4800;
   inreg.edx.b[0] = disk;

   __intcall(0x13, &inreg, &outreg);

  memcpy(&dp, __com32.cs_bounce, sizeof (struct device_parameter));

   if ( outreg.eflags.l & EFLAGS_CF) {
	   printf("Disk 0x%X doesn't supports EDD 3.0\n",disk);
//	   return -1;
  }

   sprintf(disk_info[disk].host_bus_type,"%c%c%c%c",dp.host_bus_type[0],dp.host_bus_type[1],dp.host_bus_type[2],dp.host_bus_type[3]);
   sprintf(disk_info[disk].interface_type,"%c%c%c%c%c%c%c%c",dp.interface_type[0],dp.interface_type[1],dp.interface_type[2],dp.interface_type[3],dp.interface_type[4],dp.interface_type[5],dp.interface_type[6],dp.interface_type[7]);
   disk_info[disk].sectors=dp.sectors;
   disk_info[disk].cylinders=dp.cylinders;
   //FIXME: we have to find a way to grab the model & fw
   sprintf(disk_info[disk].aid.model,"0x%X",disk);
   sprintf(disk_info[disk].aid.fw_rev,"%s","N/A");
   sprintf(disk_info[disk].aid.serial_no,"%s","N/A");

  /*
   memset(__com32.cs_bounce, 0, sizeof(struct device_parameter));
   memset(&aid, 0, sizeof(struct ata_identify_device));
   memset(&inreg, 0, sizeof inreg);
   inreg.ebx.w[0] = OFFS(__com32.cs_bounce+1024);
   inreg.es       = SEG(__com32.cs_bounce+1024);
   inreg.eax.w[0] = 0x2500;
   inreg.edx.b[0] = disk;

  __intcall(0x13,&inreg, &outreg);

  memcpy(&aid, __com32.cs_bounce, sizeof (struct ata_identify_device));

  if ( outreg.eflags.l & EFLAGS_CF) {
	   printf("Disk 0x%X: Failed to Identify Device\n",disk);
	   //FIXME
	   return 0;
  }

//   ata_id_c_string(aid, disk_info[disk].fwrev, ATA_ID_FW_REV, sizeof(disk_info[disk].fwrev));
//   ata_id_c_string(aid, disk_info[disk].model, ATA_ID_PROD,  sizeof(disk_info[disk].model));

  char buff[sizeof(struct ata_identify_device)];
  memcpy(buff,&aid, sizeof (struct ata_identify_device));
  for (int j=0;j<sizeof(struct ata_identify_device);j++)
     printf ("model=|%c|\n",buff[j]);
    printf ("Disk 0x%X : %s %s %s\n",disk, aid.model, aid.fw_rev,aid.serial_no);
*/
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
 for (int drive = 0x80; drive <= 0xff; drive++) {
    if (get_disk_params(drive,disk_info))
          continue;
    struct diskinfo d=disk_info[drive];
    printf("  DISK 0x%X: %s %s: sectors=%d, sector/track=%d head=%d : EDD=%s\n",drive,d.host_bus_type,d.interface_type, d.sectors, d.sectors_per_track,d.heads,d.edd_version);
 }
}

void compute_PCI(unsigned char *menu,struct pci_domain **pci_domain) {
  char buffer[MENULEN];
  char infobar[STATLEN];
  *menu = add_menu(" PCI Devices ",-1);

  struct pci_device *pci_device;
  for_each_pci_func(pci_device, *pci_domain) {
        snprintf(buffer,59,"%s : %s\n",
		pci_device->dev_info->vendor_name,
		pci_device->dev_info->product_name);
        snprintf(infobar, MENULEN,"%02x:%02x.%01x # %s # ID:%04x:%04x[%04x:%04x] # Kmod:%s\n",
               __pci_bus, __pci_slot, __pci_func,pci_device->dev_info->class_name,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product,pci_device->dev_info->linux_kernel_module);
	add_item(buffer,infobar,OPT_INACTIVE,NULL,0);
  }
}

void compute_KERNEL(unsigned char *menu,struct pci_domain **pci_domain) {
  char buffer[MENULEN];
  char infobar[MENULEN];

  *menu = add_menu(" Kernel Modules ",-1);
  struct pci_device *pci_device;
  for_each_pci_func(pci_device, *pci_domain) {
	if (strcmp("unknown",pci_device->dev_info->linux_kernel_module)!=0) {
         snprintf(buffer,MENULEN,"%s",pci_device->dev_info->linux_kernel_module);
	 snprintf(infobar, MENULEN,"%04x:%04x %s : %s\n",
               pci_device->vendor, pci_device->product,
		pci_device->dev_info->vendor_name,
                pci_device->dev_info->product_name);

	 add_item(buffer,infobar,OPT_INACTIVE,NULL,0);
	}
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


void compute_disk_module(unsigned char *menu, struct diskinfo *disk_info, int disk_number) {
  char buffer[MENULEN];
  struct diskinfo d = disk_info[disk_number];
  if (strlen(d.aid.model)<=0) return;

   sprintf(buffer," Disk <%d> ",nb_sub_disk_menu);
  *menu = add_menu(buffer,-1);

  sprintf(buffer,"Model        : %s",d.aid.model);
  add_item(buffer,"Model",OPT_INACTIVE,NULL,0);

  // Compute device size
  char previous_unit[3],unit[3]; //GB
  int previous_size,size = d.sectors/2; // Converting to bytes
  strcpy(unit,"KB");
  strcpy(previous_unit,unit);
  previous_size=size;
  if (size>1000) {
     size=size/1000;
     strcpy(unit,"MB");
     if (size>1000) {
       previous_size=size;
       size=size/1000;
       strcpy(previous_unit,unit);
       strcpy(unit,"GB");
       if (size>1000) {
        previous_size=size;
        size=size/1000;
        strcpy(previous_unit,unit);
        strcpy(unit,"TB");
       }
     }
  }

  sprintf(buffer,"Size         : %d %s (%d %s)",size,unit,previous_size,previous_unit);
  add_item(buffer,"Size",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Firmware Rev.: %s",d.aid.fw_rev);
  add_item(buffer,"Firmware Revision",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Serial Number: %s",d.aid.serial_no);
  add_item(buffer,"Serial Number",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Interface    : %s",d.interface_type);
  add_item(buffer,"Interface Type",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Host Bus     : %s",d.host_bus_type);
  add_item(buffer,"Host Bus Type",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Sectors      : %d",d.sectors);
  add_item(buffer,"Sectors",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Heads        : %d",d.heads);
  add_item(buffer,"Heads",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Cylinders    : %d",d.cylinders);
  add_item(buffer,"Cylinders",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Sectors/Track: %d",d.sectors_per_track);
  add_item(buffer,"Sectors per Track",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"Port         : 0x%X",disk_number);
  add_item(buffer,"Port",OPT_INACTIVE,NULL,0);

  sprintf(buffer,"EDD Version  : %s",d.edd_version);
  add_item(buffer,"EDD Version",OPT_INACTIVE,NULL,0);

  nb_sub_disk_menu++;
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
  init_menusystem("Hardware Detection Tool Version 0.1.0 by Erwan Velu");
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

#ifdef WITH_PCI
  printf("PCI: Detecting Devices\n");
  /* Scanning to detect pci buses and devices */
  *pci_domain = pci_scan();


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
add_item("Run Test","Run Test",OPT_RUN,"memtest",0);
}

void compute_disks(unsigned char *menu, struct diskinfo *disk_info) {
char buffer[MENULEN];
nb_sub_disk_menu=0;

for (int i=0;i<0xff;i++) {
  compute_disk_module(&DISK_SUBMENU[nb_sub_disk_menu],disk_info,i);
}

*menu = add_menu(" Disks ",-1);

for (int i=0;i<nb_sub_disk_menu;i++) {
  sprintf(buffer," Disk <%d> ",i);
  add_item(buffer,"Disk",OPT_SUBMENU,NULL,DISK_SUBMENU[i]);
}
}

void compute_submenus(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain, struct diskinfo *disk_info) {
if (is_dmi_valid) {
  compute_motherboard(&MOBO_MENU,dmi);
  compute_chassis(&CHASSIS_MENU,dmi);
  compute_system(&SYSTEM_MENU,dmi);
  compute_memory(&MEMORY_MENU,dmi);
  compute_bios(&BIOS_MENU,dmi);
  compute_battery(&BATTERY_MENU,dmi);
}
  compute_processor(&CPU_MENU,cpu,dmi);
  compute_disks(&DISK_MENU,disk_info);
#ifdef WITH_PCI
  compute_PCI(&PCI_MENU,pci_domain);
  compute_KERNEL(&KERNEL_MENU,pci_domain);
#endif
}

void compute_main_menu() {
  MAIN_MENU = add_menu(" Main Menu ",-1);
  set_item_options(-1,24);
 if (nb_sub_disk_menu>0)
 add_item("<D>isks","Disks",OPT_SUBMENU,NULL,DISK_MENU);
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
  add_item("","",OPT_SEP,"",0);
  add_item("<K>ernel modules","Kernel Modules",OPT_SUBMENU,NULL,KERNEL_MENU);
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

  compute_submenus(&dmi,&cpu,&pci_domain,disk_info);

  compute_main_menu();

#ifdef WITH_MENU_DISPLAY
  t_menuitem * curr;
  char cmd[160];

  printf("Starting Menu\n");
  curr=showmenus(MAIN_MENU);
  if (curr) {
        if (curr->action == OPT_RUN)
        {
            strcpy(cmd,curr->data);

	     if (issyslinux())
               runsyslinuxcmd(cmd);
            else csprint(cmd,0x07);
            return 1; // Should not happen when run from SYSLINUX
        }
  }
#endif

  return 0;
}
