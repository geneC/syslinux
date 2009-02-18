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
#include "syslinux/config.h"

#define PRODUCT_NAME "Hardware Detection Tool"
#define AUTHOR "Erwan Velu"
#define CONTACT "erwan(dot)velu(point)free(dot)fr"
#define VERSION "0.1.5"

#define EDITPROMPT 21

#define SUBMENULEN 46
#define WITH_PCI 1
#define WITH_MENU_DISPLAY 1

#define SUBMENU_Y 3
#define SUBMENU_X 29

unsigned char MAIN_MENU, CPU_MENU, MOBO_MENU, CHASSIS_MENU, BIOS_MENU, SYSTEM_MENU, PCI_MENU, KERNEL_MENU;
unsigned char MEMORY_MENU,  MEMORY_SUBMENU[32], DISK_MENU, DISK_SUBMENU[32], PCI_SUBMENU[128],BATTERY_MENU;
unsigned char SYSLINUX_MENU, ABOUT_MENU;
int nb_sub_disk_menu=0;
int nb_pci_devices=0;
bool is_dmi_valid=false;
int menu_count=0;
int pci_ids=0;
int modules_pcimap=0;

#define ATTR_PACKED __attribute__((packed))

/* Useless stuff until I manage how to send ata packets */
#ifdef ATA
enum {
        ATA_ID_FW_REV           = 23,
	ATA_ID_PROD             = 27,
        ATA_ID_FW_REV_LEN       = 8,
        ATA_ID_PROD_LEN         = 40,
};
#endif

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

#ifdef ATA
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
#endif

/* Display CPU registers for debugging purposes */
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

/* In the menu system, what to do on keyboard timeout */
TIMEOUTCODE ontimeout()
{
	 // beep();
	    return CODE_WAIT;
}

/* Keyboard handler for the menu system */
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

/* Try to get information for a given disk*/
static int get_disk_params(int disk, struct diskinfo *disk_info)
{
  static com32sys_t getparm, parm, getebios, ebios, inreg,outreg;
  struct device_parameter dp;
#ifdef ATA
  struct ata_identify_device aid;
#endif

  disk_info[disk].disk = disk;
  disk_info[disk].ebios = disk_info[disk].cbios = 0;

  /* Sending int 13h func 41h to query EBIOS information*/
  memset(&getebios, 0, sizeof (com32sys_t));
  memset(&ebios, 0, sizeof (com32sys_t));

  /* Get EBIOS support */
  getebios.eax.w[0] = 0x4100;
  getebios.ebx.w[0] = 0x55aa;
  getebios.edx.b[0] = disk;
  getebios.eflags.b[0] = 0x3;   /* CF set */

  __intcall(0x13, &getebios, &ebios);

  /* Detecting EDD support */
  if ( !(ebios.eflags.l & EFLAGS_CF) &&
       ebios.ebx.w[0] == 0xaa55 &&
       (ebios.ecx.b[0] & 1) ) {
    disk_info[disk].ebios = 1;
    switch(ebios.eax.b[1]) {
	    case 32:  strlcpy(disk_info[disk].edd_version,"1.0",3); break;
	    case 33:  strlcpy(disk_info[disk].edd_version,"1.1",3); break;
	    case 48:  strlcpy(disk_info[disk].edd_version,"3.0",3); break;
	    default:  strlcpy(disk_info[disk].edd_version,"0",1); break;
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

/* FIXME: memset to 0 make it fails
 * memset(__com32.cs_bounce, 0, sizeof(struct device_pairameter)); */
   memset(&dp, 0, sizeof(struct device_parameter));
   memset(&inreg, 0, sizeof(com32sys_t));

   /* Requesting Extended Read Drive Parameters via int13h func 48h*/
   inreg.esi.w[0] = OFFS(__com32.cs_bounce);
   inreg.ds       = SEG(__com32.cs_bounce);
   inreg.eax.w[0] = 0x4800;
   inreg.edx.b[0] = disk;

   __intcall(0x13, &inreg, &outreg);

  /* Saving bounce buffer before anything corrupt it */
  memcpy(&dp, __com32.cs_bounce, sizeof (struct device_parameter));

   if ( outreg.eflags.l & EFLAGS_CF) {
	   printf("Disk 0x%X doesn't supports EDD 3.0\n",disk);
	   return -1;
  }

   /* Copying result to the disk_info structure
    * host_bus_type, interface_type, sectors & cylinders */
   sprintf(disk_info[disk].host_bus_type,"%c%c%c%c",dp.host_bus_type[0],dp.host_bus_type[1],dp.host_bus_type[2],dp.host_bus_type[3]);
   sprintf(disk_info[disk].interface_type,"%c%c%c%c%c%c%c%c",dp.interface_type[0],dp.interface_type[1],dp.interface_type[2],dp.interface_type[3],dp.interface_type[4],dp.interface_type[5],dp.interface_type[6],dp.interface_type[7]);
   disk_info[disk].sectors=dp.sectors;
   disk_info[disk].cylinders=dp.cylinders;

   /*FIXME: we have to find a way to grab the model & fw
    * We do put dummy data until we found a solution */
   sprintf(disk_info[disk].aid.model,"0x%X",disk);
   sprintf(disk_info[disk].aid.fw_rev,"%s","N/A");
   sprintf(disk_info[disk].aid.serial_no,"%s","N/A");

  /* Useless stuff before I figure how to send ata packets */
#ifdef ATA
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
#endif

return 0;
}

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
 for (int drive = 0x80; drive <= 0xff; drive++) {
    if (get_disk_params(drive,disk_info))
          continue;
    struct diskinfo d=disk_info[drive];
    printf("  DISK 0x%X: %s %s: sectors=%d, sector/track=%d head=%d : EDD=%s\n",drive,d.host_bus_type,d.interface_type, d.sectors, d.sectors_per_track,d.heads,d.edd_version);
 }
}

/* Dynamic submenu for the pci devices */
void compute_pci_device(unsigned char *menu,struct pci_device *pci_device,int pci_bus, int pci_slot, int pci_func) {
  char buffer[56];
  char statbuffer[STATLEN];
  char kernel_modules [LINUX_KERNEL_MODULE_SIZE*MAX_KERNEL_MODULES_PER_PCI_DEVICE];

  *menu = add_menu(" Details ",-1);
   menu_count++;
   set_menu_pos(5,17);

   snprintf(buffer,sizeof buffer,"Vendor  : %s",pci_device->dev_info->vendor_name);
   snprintf(statbuffer,sizeof statbuffer,"Vendor Name: %s",pci_device->dev_info->vendor_name);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Product : %s",pci_device->dev_info->product_name);
   snprintf(statbuffer,sizeof statbuffer,"Product Name  %s",pci_device->dev_info->product_name);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Class   : %s",pci_device->dev_info->class_name);
   snprintf(statbuffer,sizeof statbuffer,"Class Name: %s",pci_device->dev_info->class_name);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Location: %02x:%02x.%01x",pci_bus, pci_slot, pci_func);
   snprintf(statbuffer,sizeof statbuffer,"Location on the PCI Bus: %02x:%02x.%01x",pci_bus, pci_slot, pci_func);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"PCI ID  : %04x:%04x[%04x:%04x]",pci_device->vendor, pci_device->product,pci_device->sub_vendor, pci_device->sub_product);
   snprintf(statbuffer,sizeof statbuffer,"vendor:product[sub_vendor:sub_product] : %04x:%04x[%04x:%04x]",pci_device->vendor, pci_device->product,pci_device->sub_vendor, pci_device->sub_product);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   if (pci_device->dev_info->linux_kernel_module_count>1) {
    for (int i=0; i<pci_device->dev_info->linux_kernel_module_count;i++) {
      if (i>0) {
        strncat(kernel_modules," | ",3);
      }
      strncat(kernel_modules, pci_device->dev_info->linux_kernel_module[i],LINUX_KERNEL_MODULE_SIZE-1);
    }
    snprintf(buffer,sizeof buffer,"Modules : %s",kernel_modules);
    snprintf(statbuffer,sizeof statbuffer,"Kernel Modules: %s",kernel_modules);
   } else {
    snprintf(buffer,sizeof buffer,"Module  : %s",pci_device->dev_info->linux_kernel_module[0]);
    snprintf(statbuffer,sizeof statbuffer,"Kernel Module: %s",pci_device->dev_info->linux_kernel_module[0]);
   }
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

}

/* Main PCI Menu*/
int compute_PCI(unsigned char *menu, struct pci_domain **pci_domain) {
 int i=0;
 char menuname[255][MENULEN+1];
 char infobar[255][STATLEN+1];
 struct pci_device *pci_device;
 char kernel_modules [LINUX_KERNEL_MODULE_SIZE*MAX_KERNEL_MODULES_PER_PCI_DEVICE];

 printf("MENU: Computing PCI menu\n");

 /* For every detected pci device, compute its submenu */
 for_each_pci_func(pci_device, *pci_domain) {
   memset(kernel_modules,0,sizeof kernel_modules);
   for (int i=0; i<pci_device->dev_info->linux_kernel_module_count;i++) {
     if (i>0) {
       strncat(kernel_modules," | ",3);
     }
     strncat(kernel_modules, pci_device->dev_info->linux_kernel_module[i],LINUX_KERNEL_MODULE_SIZE-1);
   }
   if (pci_device->dev_info->linux_kernel_module_count==0) strlcpy(kernel_modules,"unknown",7);

   compute_pci_device(&PCI_SUBMENU[i],pci_device,__pci_bus,__pci_slot,__pci_func);
   snprintf(menuname[i],59,"%s|%s",pci_device->dev_info->vendor_name,pci_device->dev_info->product_name);
   snprintf(infobar[i], STATLEN,"%02x:%02x.%01x # %s # ID:%04x:%04x[%04x:%04x] # Kmod:%s\n",
               __pci_bus, __pci_slot, __pci_func,pci_device->dev_info->class_name,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product,kernel_modules);
   i++;
 }

 *menu = add_menu(" PCI Devices ",-1);
  menu_count++;
 if (pci_ids == -ENOPCIIDS) {
    add_item("The pci.ids file is missing","Missing pci.ids file",OPT_INACTIVE,NULL,0);
    add_item("PCI Device names  can't be computed.","Missing pci.ids file",OPT_INACTIVE,NULL,0);
    add_item("Please put one in same dir as hdt","Missing pci.ids file",OPT_INACTIVE,NULL,0);
    add_item("","",OPT_SEP,"",0);
  }
 for (int j=0;j<i;j++) {
  add_item(menuname[j],infobar[j],OPT_SUBMENU,NULL,PCI_SUBMENU[j]);
 }
return 0;
}

/* Main Kernel Menu*/
void compute_KERNEL(unsigned char *menu,struct pci_domain **pci_domain) {
  char buffer[SUBMENULEN+1];
  char infobar[STATLEN+1];
  char kernel_modules [LINUX_KERNEL_MODULE_SIZE*MAX_KERNEL_MODULES_PER_PCI_DEVICE];
  struct pci_device *pci_device;

  *menu = add_menu(" Kernel Modules ",-1);
  menu_count++;
  printf("MENU: Computing Kernel menu\n");
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  if (modules_pcimap == -ENOMODULESPCIMAP) {
    add_item("The modules.pcimap file is missing","Missing modules.pcimap file",OPT_INACTIVE,NULL,0);
    add_item("Kernel modules can't be computed.","Missing modules.pcimap file",OPT_INACTIVE,NULL,0);
    add_item("Please put one in same dir as hdt","Missing modules.pcimap file",OPT_INACTIVE,NULL,0);
    add_item("","",OPT_SEP,"",0);
  } else  {
   /* For every detected pci device, grab its kernel module to compute this submenu */
   for_each_pci_func(pci_device, *pci_domain) {
	memset(kernel_modules,0,sizeof kernel_modules);
	for (int i=0; i<pci_device->dev_info->linux_kernel_module_count;i++) {
	  if (i>0) {
	   strncat(kernel_modules," | ",3);
          }
          strncat(kernel_modules, pci_device->dev_info->linux_kernel_module[i],LINUX_KERNEL_MODULE_SIZE-1);
        }

	/* No need to add unknown kernel modules*/
	if (strlen(kernel_modules)>0) {
         snprintf(buffer,sizeof buffer,"%s (%s)",kernel_modules, pci_device->dev_info->class_name);
	 snprintf(infobar, sizeof infobar,"%04x:%04x %s : %s\n",
               pci_device->vendor, pci_device->product,
		pci_device->dev_info->vendor_name,
                pci_device->dev_info->product_name);

	 add_item(buffer,infobar,OPT_INACTIVE,NULL,0);
	}
    }
  }
}

/* Main Battery Menu*/
void compute_battery(unsigned char *menu, s_dmi *dmi) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  *menu = add_menu(" Battery ",-1);
  menu_count++;
  printf("MENU: Computing Battery menu\n");
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer, sizeof buffer,"Vendor          : %s",dmi->battery.manufacturer);
  snprintf(statbuffer, sizeof statbuffer,"Vendor: %s",dmi->battery.manufacturer);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Manufacture Date: %s",dmi->battery.manufacture_date);
  snprintf(statbuffer, sizeof statbuffer,"Manufacture Date: %s",dmi->battery.manufacture_date);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Serial          : %s",dmi->battery.serial);
  snprintf(statbuffer, sizeof statbuffer,"Serial: %s",dmi->battery.serial);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Name            : %s",dmi->battery.name);
  snprintf(statbuffer, sizeof statbuffer,"Name: %s",dmi->battery.name);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Chemistry       : %s",dmi->battery.chemistry);
  snprintf(statbuffer, sizeof statbuffer,"Chemistry: %s",dmi->battery.chemistry);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Design Capacity : %s",dmi->battery.design_capacity);
  snprintf(statbuffer, sizeof statbuffer,"Design Capacity: %s",dmi->battery.design_capacity);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Design Voltage  : %s",dmi->battery.design_voltage);
  snprintf(statbuffer, sizeof statbuffer,"Design Voltage : %s",dmi->battery.design_voltage);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"SBDS            : %s",dmi->battery.sbds);
  snprintf(statbuffer, sizeof statbuffer,"SBDS: %s",dmi->battery.sbds);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"SBDS Manuf. Date: %s",dmi->battery.sbds_manufacture_date);
  snprintf(statbuffer, sizeof statbuffer,"SBDS Manufacture Date: %s",dmi->battery.sbds_manufacture_date);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"SBDS Chemistry  : %s",dmi->battery.sbds_chemistry);
  snprintf(statbuffer, sizeof statbuffer,"SBDS Chemistry : %s",dmi->battery.sbds_chemistry);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Maximum Error   : %s",dmi->battery.maximum_error);
  snprintf(statbuffer, sizeof statbuffer,"Maximum Error (%) : %s",dmi->battery.maximum_error);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"OEM Info        : %s",dmi->battery.oem_info);
  snprintf(statbuffer, sizeof statbuffer,"OEM Info: %s",dmi->battery.oem_info);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute the disk submenu */
void compute_disk_module(unsigned char *menu, struct diskinfo *d,int disk_number) {
  char buffer[MENULEN+1];
  char statbuffer[STATLEN+1];

  /* No need to add no existing devices*/
  if (strlen(d->aid.model)<=0) return;

  snprintf(buffer,sizeof buffer," Disk <%d> ",nb_sub_disk_menu);
  *menu = add_menu(buffer,-1);
  menu_count++;

  snprintf(buffer,sizeof buffer,"Model        : %s",d->aid.model);
  snprintf(statbuffer,sizeof statbuffer,"Model: %s",d->aid.model);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  /* Compute device size */
  char previous_unit[3],unit[3]; //GB
  int previous_size,size = d->sectors/2; // Converting to bytes
  strlcpy(unit,"KB",2);
  strlcpy(previous_unit,unit,2);
  previous_size=size;
  if (size>1000) {
     size=size/1000;
     strlcpy(unit,"MB",2);
     if (size>1000) {
       previous_size=size;
       size=size/1000;
       strlcpy(previous_unit,unit,2);
       strlcpy(unit,"GB",2);
       if (size>1000) {
        previous_size=size;
        size=size/1000;
        strlcpy(previous_unit,unit,2);
        strlcpy(unit,"TB",2);
       }
     }
  }

  snprintf(buffer,sizeof buffer,"Size         : %d %s (%d %s)",size,unit,previous_size,previous_unit);
  snprintf(statbuffer, sizeof statbuffer, "Size: %d %s (%d %s)",size,unit,previous_size,previous_unit);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Firmware Rev.: %s",d->aid.fw_rev);
  snprintf(statbuffer,sizeof statbuffer,"Firmware Revision: %s",d->aid.fw_rev);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Serial Number: %s",d->aid.serial_no);
  snprintf(statbuffer,sizeof statbuffer,"Serial Number: %s",d->aid.serial_no);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Interface    : %s",d->interface_type);
  snprintf(statbuffer,sizeof statbuffer,"Interface: %s",d->interface_type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Host Bus     : %s",d->host_bus_type);
  snprintf(statbuffer,sizeof statbuffer,"Host Bus Type: %s",d->host_bus_type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer, "Sectors      : %d",d->sectors);
  snprintf(statbuffer,sizeof statbuffer, "Sectors: %d",d->sectors);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Heads        : %d",d->heads);
  snprintf(statbuffer,sizeof statbuffer,"Heads: %d",d->heads);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer,"Cylinders    : %d",d->cylinders);
  snprintf(statbuffer, sizeof statbuffer,"Cylinders: %d",d->cylinders);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer, "Sectors/Track: %d",d->sectors_per_track);
  snprintf(statbuffer,sizeof statbuffer, "Sectors per Track: %d",d->sectors_per_track);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Port         : 0x%X",disk_number);
  snprintf(statbuffer,sizeof statbuffer,"Port: 0x%X",disk_number);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"EDD Version  : %s",d->edd_version);
  snprintf(statbuffer,sizeof statbuffer,"EDD Version: %s",d->edd_version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  nb_sub_disk_menu++;
}

/* Compute the memory submenu */
void compute_memory_module(unsigned char *menu, s_dmi *dmi, int slot_number) {
  int i=slot_number;
  char buffer[MENULEN+1];
  char statbuffer[STATLEN+1];

  sprintf(buffer," Module <%d> ",i);
  *menu = add_menu(buffer,-1);
  menu_count++;

  snprintf(buffer,sizeof buffer,"Form Factor  : %s",dmi->memory[i].form_factor);
  snprintf(statbuffer,sizeof statbuffer,"Form Factor: %s",dmi->memory[i].form_factor);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Type         : %s",dmi->memory[i].type);
  snprintf(statbuffer,sizeof statbuffer,"Type: %s",dmi->memory[i].type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Type Details : %s",dmi->memory[i].type_detail);
  snprintf(statbuffer,sizeof statbuffer,"Type Details: %s",dmi->memory[i].type_detail);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Speed        : %s",dmi->memory[i].speed);
  snprintf(statbuffer,sizeof statbuffer,"Speed (Mhz): %s",dmi->memory[i].speed);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Size         : %s",dmi->memory[i].size);
  snprintf(statbuffer,sizeof statbuffer,"Size: %s",dmi->memory[i].size);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Device Set   : %s",dmi->memory[i].device_set);
  snprintf(statbuffer,sizeof statbuffer,"Device Set: %s",dmi->memory[i].device_set);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Device Loc.  : %s",dmi->memory[i].device_locator);
  snprintf(statbuffer,sizeof statbuffer,"Device Location: %s",dmi->memory[i].device_locator);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Bank Locator : %s",dmi->memory[i].bank_locator);
  snprintf(statbuffer,sizeof statbuffer,"Bank Locator: %s",dmi->memory[i].bank_locator);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Total Width  : %s",dmi->memory[i].total_width);
  snprintf(statbuffer,sizeof statbuffer,"Total bit Width: %s",dmi->memory[i].total_width);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Data Width   : %s",dmi->memory[i].data_width);
  snprintf(statbuffer,sizeof statbuffer,"Data bit Width: %s",dmi->memory[i].data_width);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Error        : %s",dmi->memory[i].error);
  snprintf(statbuffer,sizeof statbuffer,"Error: %s",dmi->memory[i].error);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Vendor       : %s",dmi->memory[i].manufacturer);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",dmi->memory[i].manufacturer);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Serial       : %s",dmi->memory[i].serial);
  snprintf(statbuffer,sizeof statbuffer,"Serial: %s",dmi->memory[i].serial);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Asset Tag    : %s",dmi->memory[i].asset_tag);
  snprintf(statbuffer,sizeof statbuffer,"Asset Tag: %s",dmi->memory[i].asset_tag);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Part Number  : %s",dmi->memory[i].part_number);
  snprintf(buffer,sizeof statbuffer,"Part Number: %s",dmi->memory[i].part_number);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

}

/* Compute Motherboard main menu */
void compute_motherboard(unsigned char *menu,s_dmi *dmi) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  printf("MENU: Computing motherboard menu\n");
  *menu = add_menu(" Motherboard ",-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer,sizeof buffer,"Vendor    : %s",dmi->base_board.manufacturer);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",dmi->base_board.manufacturer);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Product   : %s",dmi->base_board.product_name);
  snprintf(statbuffer,sizeof statbuffer,"Product Name: %s",dmi->base_board.product_name);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Version   : %s",dmi->base_board.version);
  snprintf(statbuffer,sizeof statbuffer,"Version: %s",dmi->base_board.version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Serial    : %s",dmi->base_board.serial);
  snprintf(statbuffer,sizeof statbuffer,"Serial Number: %s",dmi->base_board.serial);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Asset Tag : %s",dmi->base_board.asset_tag);
  snprintf(statbuffer,sizeof statbuffer,"Asset Tag: %s",dmi->base_board.asset_tag);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Location  : %s",dmi->base_board.location);
  snprintf(statbuffer,sizeof statbuffer,"Location: %s",dmi->base_board.location);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Type      : %s",dmi->base_board.type);
  snprintf(statbuffer,sizeof statbuffer,"Type: %s",dmi->base_board.type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute System main menu */
void compute_system(unsigned char *menu,s_dmi *dmi) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  printf("MENU: Computing system menu\n");
  *menu = add_menu(" System ",-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer,sizeof buffer,"Vendor    : %s",dmi->system.manufacturer);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",dmi->system.manufacturer);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Product   : %s",dmi->system.product_name);
  snprintf(statbuffer,sizeof statbuffer,"Product Name: %s",dmi->system.product_name);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Version   : %s",dmi->system.version);
  snprintf(statbuffer,sizeof statbuffer,"Version: %s",dmi->system.version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Serial    : %s",dmi->system.serial);
  snprintf(statbuffer,sizeof statbuffer,"Serial Number: %s",dmi->system.serial);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"UUID      : %s",dmi->system.uuid);
  snprintf(statbuffer,sizeof statbuffer,"UUID: %s",dmi->system.uuid);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Wakeup    : %s",dmi->system.wakeup_type);
  snprintf(statbuffer,sizeof statbuffer,"Wakeup Type: %s",dmi->system.wakeup_type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"SKU Number: %s",dmi->system.sku_number);
  snprintf(statbuffer,sizeof statbuffer,"SKU Number: %s",dmi->system.sku_number);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Family    : %s",dmi->system.family);
  snprintf(statbuffer,sizeof statbuffer,"Family: %s",dmi->system.family);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute Chassis menu */
void compute_chassis(unsigned char *menu,s_dmi *dmi) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  printf("MENU: Computing chassis menu\n");
  *menu = add_menu(" Chassis ",-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer,sizeof buffer,"Vendor    : %s",dmi->chassis.manufacturer);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",dmi->chassis.manufacturer);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Type      : %s",dmi->chassis.type);
  snprintf(statbuffer,sizeof statbuffer,"Type: %s",dmi->chassis.type);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Version   : %s",dmi->chassis.version);
  snprintf(statbuffer,sizeof statbuffer,"Version: %s",dmi->chassis.version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Serial    : %s",dmi->chassis.serial);
  snprintf(statbuffer,sizeof statbuffer,"Serial Number: %s",dmi->chassis.serial);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Asset Tag : %s",dmi->chassis.asset_tag);
  snprintf(statbuffer,sizeof statbuffer,"Asset Tag: %s",dmi->chassis.asset_tag);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Lock      : %s",dmi->chassis.lock);
  snprintf(statbuffer,sizeof statbuffer,"Lock: %s",dmi->chassis.lock);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute BIOS menu */
void compute_bios(unsigned char *menu,s_dmi *dmi) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  *menu = add_menu(" BIOS ",-1);
  menu_count++;
  printf("MENU: Computing BIOS menu\n");
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer,sizeof buffer,"Vendor    : %s",dmi->bios.vendor);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",dmi->bios.vendor);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Version   : %s",dmi->bios.version);
  snprintf(statbuffer,sizeof statbuffer,"Version: %s",dmi->bios.version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Release   : %s",dmi->bios.release_date);
  snprintf(statbuffer,sizeof statbuffer,"Release Date: %s",dmi->bios.release_date);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Bios Rev. : %s",dmi->bios.bios_revision);
  snprintf(statbuffer,sizeof statbuffer,"Bios Revision: %s",dmi->bios.bios_revision);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Fw.  Rev. : %s",dmi->bios.firmware_revision);
  snprintf(statbuffer,sizeof statbuffer,"Firmware Revision : %s",dmi->bios.firmware_revision);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute Processor menu */
void compute_processor(unsigned char *menu,s_cpu *cpu, s_dmi *dmi) {
  char buffer[MENULEN+1];
  char buffer1[MENULEN+1];
  char statbuffer[STATLEN+1];

  printf("MENU: Computing Processor menu\n");
  *menu = add_menu(" Main Processor ",-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer,sizeof buffer,"Vendor    : %s",cpu->vendor);
  snprintf(statbuffer,sizeof statbuffer,"Vendor: %s",cpu->vendor);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Model     : %s",cpu->model);
  snprintf(statbuffer,sizeof statbuffer,"Model: %s",cpu->model);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Vendor ID : %d",cpu->vendor_id);
  snprintf(statbuffer,sizeof statbuffer,"Vendor ID: %d",cpu->vendor_id);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Family ID : %d",cpu->family);
  snprintf(statbuffer,sizeof statbuffer,"Family ID: %d",cpu->family);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Model  ID : %d",cpu->model_id);
  snprintf(statbuffer,sizeof statbuffer,"Model  ID: %d",cpu->model_id);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer,sizeof buffer,"Stepping  : %d",cpu->stepping);
  snprintf(statbuffer,sizeof statbuffer,"Stepping: %d",cpu->stepping);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  if (is_dmi_valid) {
   snprintf(buffer,sizeof buffer,"FSB       : %d",dmi->processor.external_clock);
   snprintf(statbuffer,sizeof statbuffer,"Front Side Bus (MHz): %d",dmi->processor.external_clock);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Cur. Speed: %d",dmi->processor.current_speed);
   snprintf(statbuffer,sizeof statbuffer,"Current Speed (MHz): %d",dmi->processor.current_speed);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Max Speed : %d",dmi->processor.max_speed);
   snprintf(statbuffer,sizeof statbuffer,"Max Speed (MHz): %d",dmi->processor.max_speed);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

   snprintf(buffer,sizeof buffer,"Upgrade   : %s",dmi->processor.upgrade);
   snprintf(statbuffer,sizeof statbuffer,"Upgrade: %s",dmi->processor.upgrade);
   add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  }

  if (cpu->flags.smp) {
	  snprintf(buffer,sizeof buffer,"SMP       : Yes");
	  snprintf(statbuffer,sizeof statbuffer,"SMP: Yes");
  }
  else {
	  snprintf(buffer,sizeof buffer,"SMP       : No");
	  snprintf(statbuffer,sizeof statbuffer,"SMP: No");
  }
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  if (cpu->flags.lm) {
	  snprintf(buffer,sizeof buffer,"x86_64    : Yes");
	  snprintf(statbuffer,sizeof statbuffer,"x86_64 compatible processor: Yes");
  }
  else {
	  snprintf(buffer,sizeof buffer,"X86_64    : No");
	  snprintf(statbuffer,sizeof statbuffer,"X86_64 compatible processor: No");
  }
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.fpu) strcat(buffer1,"fpu ");
  if (cpu->flags.vme) strcat(buffer1,"vme ");
  if (cpu->flags.de)  strcat(buffer1,"de ");
  if (cpu->flags.pse) strcat(buffer1,"pse ");
  if (cpu->flags.tsc) strcat(buffer1,"tsc ");
  if (cpu->flags.msr) strcat(buffer1,"msr ");
  if (cpu->flags.pae) strcat(buffer1,"pae ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.mce) strcat(buffer1,"mce ");
  if (cpu->flags.cx8) strcat(buffer1,"cx8 ");
  if (cpu->flags.apic) strcat(buffer1,"apic ");
  if (cpu->flags.sep) strcat(buffer1,"sep ");
  if (cpu->flags.mtrr) strcat(buffer1,"mtrr ");
  if (cpu->flags.pge) strcat(buffer1,"pge ");
  if (cpu->flags.mca) strcat(buffer1,"mca ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.cmov) strcat(buffer1,"cmov ");
  if (cpu->flags.pat)  strcat(buffer1,"pat ");
  if (cpu->flags.pse_36) strcat(buffer1,"pse_36 ");
  if (cpu->flags.psn)  strcat(buffer1,"psn ");
  if (cpu->flags.clflsh) strcat(buffer1,"clflsh ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.dts)  strcat(buffer1,"dts ");
  if (cpu->flags.acpi) strcat(buffer1,"acpi ");
  if (cpu->flags.mmx)  strcat(buffer1,"mmx ");
  if (cpu->flags.sse)  strcat(buffer1,"sse ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.sse2) strcat(buffer1,"sse2 ");
  if (cpu->flags.ss)   strcat(buffer1,"ss ");
  if (cpu->flags.htt)  strcat(buffer1,"ht ");
  if (cpu->flags.acc)  strcat(buffer1,"acc ");
  if (cpu->flags.syscall) strcat(buffer1,"syscall ");
  if (cpu->flags.mp)   strcat(buffer1,"mp ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  buffer1[0]='\0';
  if (cpu->flags.nx)    strcat(buffer1,"nx ");
  if (cpu->flags.mmxext) strcat(buffer1,"mmxext ");
  if (cpu->flags.lm)     strcat(buffer1,"lm ");
  if (cpu->flags.nowext) strcat(buffer1,"3dnowext ");
  if (cpu->flags.now)    strcat(buffer1,"3dnow! ");
  snprintf(buffer,sizeof buffer,"Flags     : %s",buffer1);
  snprintf(statbuffer,sizeof statbuffer,"Flags: %s",buffer1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

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

/* Compute the Memory Menu*/
void compute_memory(unsigned char *menu, s_dmi *dmi) {
 char buffer[MENULEN+1];
 printf("MENU: Computing Memory menu\n");
 for (int i=0;i<dmi->memory_count;i++) {
   compute_memory_module(&MEMORY_SUBMENU[i],dmi,i);
 }

 *menu = add_menu(" Modules ",-1);
  menu_count++;

 for (int i=0;i<dmi->memory_count;i++) {
  snprintf(buffer,sizeof buffer," Module <%d> ",i);
  add_item(buffer,"Memory Module",OPT_SUBMENU,NULL,MEMORY_SUBMENU[i]);
 }
 add_item("Run Test","Run Test",OPT_RUN,"memtest",0);
}

/* Compute the Disk Menu*/
void compute_disks(unsigned char *menu, struct diskinfo *disk_info) {
  char buffer[MENULEN+1];
  nb_sub_disk_menu=0;
  printf("MENU: Computing Disks menu\n");
  for (int i=0;i<0xff;i++) {
     compute_disk_module(&DISK_SUBMENU[nb_sub_disk_menu],&disk_info[i],i);
  }

  *menu = add_menu(" Disks ",-1);
  menu_count++;

  for (int i=0;i<nb_sub_disk_menu;i++) {
    snprintf(buffer,sizeof buffer," Disk <%d> ",i);
    add_item(buffer,"Disk",OPT_SUBMENU,NULL,DISK_SUBMENU[i]);
  }
}

/* Computing About menu*/
void compute_aboutmenu(unsigned char *menu) {
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];

  *menu = add_menu(" About ",-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  printf("MENU: Computing About menu\n");

  snprintf(buffer, sizeof buffer, "Product : %s", PRODUCT_NAME);
  snprintf(statbuffer, sizeof statbuffer, "Product : %s", PRODUCT_NAME);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Version : %s", VERSION);
  snprintf(statbuffer, sizeof statbuffer, "Version : %s", VERSION);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Author  : %s", AUTHOR);
  snprintf(statbuffer, sizeof statbuffer, "Author  : %s", AUTHOR);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Contact : %s", CONTACT);
  snprintf(statbuffer, sizeof statbuffer, "Contact : %s", CONTACT);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

}

/* Computing Syslinux menu*/
void compute_syslinuxmenu(unsigned char *menu) {
  char syslinux_fs[22];
  char syslinux_fs_menu[24];
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  const struct syslinux_version *sv;

  printf("MENU: Computing Syslinux menu\n");

  memset(syslinux_fs,0,sizeof syslinux_fs);
  memset(syslinux_fs_menu,0,sizeof syslinux_fs_menu);

  sv = syslinux_version();
  switch(sv->filesystem) {
	case SYSLINUX_FS_SYSLINUX: strlcpy(syslinux_fs,"SYSlinux",9); break;
	case SYSLINUX_FS_PXELINUX: strlcpy(syslinux_fs,"PXElinux",9); break;
	case SYSLINUX_FS_ISOLINUX: strlcpy(syslinux_fs,"ISOlinux",9); break;
	case SYSLINUX_FS_EXTLINUX: strlcpy(syslinux_fs,"EXTlinux",9); break;
	case SYSLINUX_FS_UNKNOWN:
	default: strlcpy(syslinux_fs,"Unknown Bootloader",sizeof syslinux_fs); break;
  }
  snprintf(syslinux_fs_menu,sizeof syslinux_fs_menu," %s ",syslinux_fs);
  *menu = add_menu(syslinux_fs_menu,-1);
  menu_count++;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer, sizeof buffer, "Bootloader : %s", syslinux_fs);
  snprintf(statbuffer, sizeof statbuffer, "Bootloader: %s", syslinux_fs);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Version    : %s", sv->version_string+2);
  snprintf(statbuffer, sizeof statbuffer, "Version: %s", sv->version_string+2);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Version    : %u",sv->version);
  snprintf(statbuffer, sizeof statbuffer, "Version: %u",sv->version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  snprintf(buffer, sizeof buffer, "Max API    : %u",sv->max_api);
  snprintf(statbuffer, sizeof statbuffer, "Max API: %u",sv->max_api);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);

  add_item("","",OPT_SEP,"",0);

  snprintf(buffer, sizeof buffer, "%s", sv->copyright_string+1);
  snprintf(statbuffer, sizeof statbuffer, "%s", sv->copyright_string+1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
}

/* Compute Main' Submenus*/
void compute_submenus(s_dmi *dmi, s_cpu *cpu, struct pci_domain **pci_domain, struct diskinfo *disk_info) {
 /* Compute this menus if a DMI table exist */
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
  compute_syslinuxmenu(&SYSLINUX_MENU);
  compute_aboutmenu(&ABOUT_MENU);
}

/* Compute Main Menu*/
void compute_main_menu() {
  MAIN_MENU = add_menu(" Main Menu ",-1);
  menu_count++;
  set_item_options(-1,24);

#ifdef WITH_PCI
  add_item("PCI <D>evices","PCI Devices Menu",OPT_SUBMENU,NULL,PCI_MENU);
#endif
  if (nb_sub_disk_menu>0)
    add_item("<D>isks","Disks Menu",OPT_SUBMENU,NULL,DISK_MENU);
  add_item("<M>emory Modules","Memory Modules Menu",OPT_SUBMENU,NULL,MEMORY_MENU);
  add_item("<P>rocessor","Main Processor Menu",OPT_SUBMENU,NULL,CPU_MENU);

if (is_dmi_valid) {
  add_item("M<o>therboard","Motherboard Menu",OPT_SUBMENU,NULL,MOBO_MENU);
  add_item("<B>ios","Bios Menu",OPT_SUBMENU,NULL,BIOS_MENU);
  add_item("<C>hassis","Chassis Menu",OPT_SUBMENU,NULL,CHASSIS_MENU);
  add_item("<S>ystem","System Menu",OPT_SUBMENU,NULL,SYSTEM_MENU);
  add_item("Ba<t>tery","Battery Menu",OPT_SUBMENU,NULL,BATTERY_MENU);
}
  add_item("","",OPT_SEP,"",0);
#ifdef WITH_PCI
  add_item("<K>ernel Modules","Kernel Modules Menu",OPT_SUBMENU,NULL,KERNEL_MENU);
#endif
  add_item("<S>yslinux","Syslinux Information Menu",OPT_SUBMENU,NULL,SYSLINUX_MENU);
  add_item("<A>bout","About Menu",OPT_SUBMENU,NULL,ABOUT_MENU);
}

int main(void)
{
  s_dmi dmi; /* DMI table */
  s_cpu cpu; /* CPU information */
  struct pci_domain *pci_domain=NULL; /* PCI Devices */
  struct diskinfo disk_info[256];     /* Disk Information*/

  /* Setup the environement */
  setup_env();

  /* Detect every kind of hardware */
  detect_hardware(&dmi,&cpu,&pci_domain,disk_info);

  /* Compute all sub menus */
  compute_submenus(&dmi,&cpu,&pci_domain,disk_info);

  /* Compute main menu */
  compute_main_menu();

#ifdef WITH_MENU_DISPLAY
  t_menuitem * curr;
  char cmd[160];

  printf("Starting Menu (%d menus)\n",menu_count);
  curr=showmenus(MAIN_MENU);
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
