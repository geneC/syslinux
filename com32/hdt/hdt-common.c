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
#include <string.h>
#include <stdio.h>
#include <getkey.h>
#include "syslinux/config.h"
#include "../lib/sys/vesa/vesa.h"
#include "hdt-common.h"
#include "lib-ansi.h"

/* ISOlinux requires a 8.3 format */
void convert_isolinux_filename(char *filename, struct s_hardware *hardware) {
  /* Exit if we are not running ISOLINUX */
  if (hardware->sv->filesystem != SYSLINUX_FS_ISOLINUX) return;
  /* Searching the dot */
  char *dot=strchr(filename,'.');
  /* Exiting if not dot exists in that string */
  if (dot==NULL) return;
  /* Exiting if the extension is 3 char or less */
  if (strlen(dot)<=4) return;

  /* We have an extension bigger than .blah
   * so we have to shorten it to 3*/
  dot[4]='\0';
}

void detect_parameters(const int argc, const char *argv[],
                       struct s_hardware *hardware)
{
  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "modules=", 8)) {
      strncpy(hardware->modules_pcimap_path, argv[i] + 8,
        sizeof(hardware->modules_pcimap_path));
      convert_isolinux_filename(hardware->modules_pcimap_path,hardware);
    } else if (!strncmp(argv[i], "pciids=", 7)) {
      strncpy(hardware->pciids_path, argv[i] + 7,
        sizeof(hardware->pciids_path));
      convert_isolinux_filename(hardware->pciids_path,hardware);
    } else if (!strncmp(argv[i], "memtest=", 8)) {
      strncpy(hardware->memtest_label, argv[i] + 8,
        sizeof(hardware->memtest_label));
      convert_isolinux_filename(hardware->memtest_label,hardware);
    }
  }
}

void detect_syslinux(struct s_hardware *hardware)
{
  hardware->sv = syslinux_version();
  switch (hardware->sv->filesystem) {
  case SYSLINUX_FS_SYSLINUX:
    strlcpy(hardware->syslinux_fs, "SYSlinux", 9);
    break;
  case SYSLINUX_FS_PXELINUX:
    strlcpy(hardware->syslinux_fs, "PXElinux", 9);
    break;
  case SYSLINUX_FS_ISOLINUX:
    strlcpy(hardware->syslinux_fs, "ISOlinux", 9);
    break;
  case SYSLINUX_FS_EXTLINUX:
    strlcpy(hardware->syslinux_fs, "EXTlinux", 9);
    break;
  case SYSLINUX_FS_UNKNOWN:
  default:
    strlcpy(hardware->syslinux_fs, "Unknown Bootloader",
      sizeof hardware->syslinux_fs);
    break;
  }
}

void init_hardware(struct s_hardware *hardware)
{
  hardware->pci_ids_return_code = 0;
  hardware->modules_pcimap_return_code = 0;
  hardware->cpu_detection = false;
  hardware->pci_detection = false;
  hardware->disk_detection = false;
  hardware->disks_count=0;
  hardware->dmi_detection = false;
  hardware->pxe_detection = false;
  hardware->vesa_detection = false;
  hardware->vpd_detection = false;
  hardware->nb_pci_devices = 0;
  hardware->is_dmi_valid = false;
  hardware->is_pxe_valid = false;
  hardware->is_vpd_valid = false;
  hardware->pci_domain = NULL;

  /* Cleaning structures */
  memset(hardware->disk_info, 0, sizeof(hardware->disk_info));
  memset(&hardware->dmi, 0, sizeof(s_dmi));
  memset(&hardware->cpu, 0, sizeof(s_cpu));
  memset(&hardware->pxe, 0, sizeof(struct s_pxe));
  memset(&hardware->vesa, 0, sizeof(struct s_vesa));
  memset(&hardware->vpd, 0, sizeof(s_vpd));
  memset(hardware->syslinux_fs, 0, sizeof hardware->syslinux_fs);
  memset(hardware->pciids_path, 0, sizeof hardware->pciids_path);
  memset(hardware->modules_pcimap_path, 0,
         sizeof hardware->modules_pcimap_path);
  memset(hardware->memtest_label, 0, sizeof hardware->memtest_label);
  strcat(hardware->pciids_path, "pci.ids");
  strcat(hardware->modules_pcimap_path, "modules.pcimap");
  strcat(hardware->memtest_label, "memtest");
}

/*
 * Detecting if a DMI table exist
 * if yes, let's parse it
 */
int detect_dmi(struct s_hardware *hardware)
{
  if (hardware->dmi_detection == true)
    return -1;
  hardware->dmi_detection = true;
  if (dmi_iterate(&hardware->dmi) == -ENODMITABLE) {
    hardware->is_dmi_valid = false;
    return -ENODMITABLE;
  }

  parse_dmitable(&hardware->dmi);
  hardware->is_dmi_valid = true;
  return 0;
}

/**
 * vpd_detection - populate the VPD structure
 *
 * VPD is a structure available on IBM machines.
 * It is documented at:
 *    http://www.pc.ibm.com/qtechinfo/MIGR-45120.html
 * (XXX the page seems to be gone)
 **/
int detect_vpd(struct s_hardware *hardware)
{
	if (hardware->vpd_detection)
		return -1;
	else
		hardware->vpd_detection = true;

	if (vpd_decode(&hardware->vpd) == -ENOVPDTABLE) {
		hardware->is_vpd_valid = false;
		return -ENOVPDTABLE;
	} else {
		hardware->is_vpd_valid = true;
		return 0;
	}
}

/* Detection vesa stuff*/
int detect_vesa(struct s_hardware *hardware) {
  static com32sys_t rm;
  struct vesa_general_info *gi;
  struct vesa_mode_info *mi;
  uint16_t mode, *mode_ptr;
  char *oem_ptr;

  if (hardware->vesa_detection == true) return -1;

  hardware->vesa_detection=true;
  hardware->is_vesa_valid=false;

  /* Allocate space in the bounce buffer for these structures */
  gi = &((struct vesa_info *)__com32.cs_bounce)->gi;
  mi = &((struct vesa_info *)__com32.cs_bounce)->mi;

  gi->signature = VBE2_MAGIC;   /* Get VBE2 extended data */
  rm.eax.w[0] = 0x4F00;         /* Get SVGA general information */
  rm.edi.w[0] = OFFS(gi);
  rm.es      = SEG(gi);
  __intcall(0x10, &rm, &rm);

  if ( rm.eax.w[0] != 0x004F ) {
    return -1;
  };

  mode_ptr = GET_PTR(gi->video_mode_ptr);
  oem_ptr = GET_PTR(gi->oem_vendor_name_ptr);
  strncpy(hardware->vesa.vendor,oem_ptr,sizeof(hardware->vesa.vendor));
  oem_ptr = GET_PTR(gi->oem_product_name_ptr);
  strncpy(hardware->vesa.product,oem_ptr,sizeof(hardware->vesa.product));
  oem_ptr = GET_PTR(gi->oem_product_rev_ptr);
  strncpy(hardware->vesa.product_revision,oem_ptr,sizeof(hardware->vesa.product_revision));

  hardware->vesa.major_version=(gi->version >> 8) & 0xff;
  hardware->vesa.minor_version=gi->version & 0xff;
  hardware->vesa.total_memory=gi->total_memory;
  hardware->vesa.software_rev=gi->oem_software_rev;

  hardware->vesa.vmi_count=0;

  while ((mode = *mode_ptr++) != 0xFFFF) {

    rm.eax.w[0] = 0x4F01;       /* Get SVGA mode information */
    rm.ecx.w[0] = mode;
    rm.edi.w[0] = OFFS(mi);
    rm.es  = SEG(mi);
    __intcall(0x10, &rm, &rm);

    /* Must be a supported mode */
    if ( rm.eax.w[0] != 0x004f )
      continue;

    /* Saving detected values*/
    memcpy(&hardware->vesa.vmi[hardware->vesa.vmi_count].mi, mi,
           sizeof(struct vesa_mode_info));
    hardware->vesa.vmi[hardware->vesa.vmi_count].mode = mode;

    hardware->vesa.vmi_count++;
  }
  hardware->is_vesa_valid = true;
 return 0;
}

/* Try to detect disks from port 0x80 to 0xff */
void detect_disks(struct s_hardware *hardware)
{
  hardware->disk_detection = true;
  for (int drive = 0x80; drive < 0xff; drive++) {
    if (get_disk_params(drive, hardware->disk_info) != 0)
      continue;
    struct diskinfo *d = &hardware->disk_info[drive];
    hardware->disks_count++;
    printf
        ("  DISK 0x%X: %s : %s %s: sectors=%d, s/t=%d head=%d : EDD=%s\n",
         drive, d->aid.model, d->host_bus_type, d->interface_type,
         d->sectors, d->sectors_per_track, d->heads,
         d->edd_version);
  }
}

int detect_pxe(struct s_hardware *hardware)
{
  void *dhcpdata;

  size_t dhcplen;
  t_PXENV_UNDI_GET_NIC_TYPE gnt;

  if (hardware->pxe_detection == true)
    return -1;
  hardware->pxe_detection = true;
  hardware->is_pxe_valid = false;
  memset(&gnt, 0, sizeof(t_PXENV_UNDI_GET_NIC_TYPE));
  memset(&hardware->pxe, 0, sizeof(struct s_pxe));

  /* This code can only work if pxelinux is loaded */
  if (hardware->sv->filesystem != SYSLINUX_FS_PXELINUX) {
    return -1;
  }
// printf("PXE: PXElinux detected\n");
  if (!pxe_get_cached_info
      (PXENV_PACKET_TYPE_DHCP_ACK, &dhcpdata, &dhcplen)) {
    pxe_bootp_t *dhcp = &hardware->pxe.dhcpdata;
    memcpy(&hardware->pxe.dhcpdata, dhcpdata,
           sizeof(hardware->pxe.dhcpdata));
    snprintf(hardware->pxe.mac_addr, sizeof(hardware->pxe.mac_addr),
       "%02x:%02x:%02x:%02x:%02x:%02x", dhcp->CAddr[0],
       dhcp->CAddr[1], dhcp->CAddr[2], dhcp->CAddr[3],
       dhcp->CAddr[4], dhcp->CAddr[5]);

    /* Saving our IP address in a easy format */
    hardware->pxe.ip_addr[0] = hardware->pxe.dhcpdata.yip & 0xff;
    hardware->pxe.ip_addr[1] =
        hardware->pxe.dhcpdata.yip >> 8 & 0xff;
    hardware->pxe.ip_addr[2] =
        hardware->pxe.dhcpdata.yip >> 16 & 0xff;
    hardware->pxe.ip_addr[3] =
        hardware->pxe.dhcpdata.yip >> 24 & 0xff;

    if (!pxe_get_nic_type(&gnt)) {
      switch (gnt.NicType) {
      case PCI_NIC:
        hardware->is_pxe_valid = true;
        hardware->pxe.vendor_id =
            gnt.info.pci.Vendor_ID;
        hardware->pxe.product_id = gnt.info.pci.Dev_ID;
        hardware->pxe.subvendor_id =
            gnt.info.pci.SubVendor_ID;
        hardware->pxe.subproduct_id =
            gnt.info.pci.SubDevice_ID,
            hardware->pxe.rev = gnt.info.pci.Rev;
        hardware->pxe.pci_bus =
            (gnt.info.pci.BusDevFunc >> 8) & 0xff;
        hardware->pxe.pci_dev =
            (gnt.info.pci.BusDevFunc >> 3) & 0x7;
        hardware->pxe.pci_func =
            gnt.info.pci.BusDevFunc & 0x03;
        hardware->pxe.base_class =
            gnt.info.pci.Base_Class;
        hardware->pxe.sub_class =
            gnt.info.pci.Sub_Class;
        hardware->pxe.prog_intf =
            gnt.info.pci.Prog_Intf;
        hardware->pxe.nictype = gnt.NicType;
        break;
      case CardBus_NIC:
        hardware->is_pxe_valid = true;
        hardware->pxe.vendor_id =
            gnt.info.cardbus.Vendor_ID;
        hardware->pxe.product_id =
            gnt.info.cardbus.Dev_ID;
        hardware->pxe.subvendor_id =
            gnt.info.cardbus.SubVendor_ID;
        hardware->pxe.subproduct_id =
            gnt.info.cardbus.SubDevice_ID,
            hardware->pxe.rev = gnt.info.cardbus.Rev;
        hardware->pxe.pci_bus =
            (gnt.info.cardbus.BusDevFunc >> 8) & 0xff;
        hardware->pxe.pci_dev =
            (gnt.info.cardbus.BusDevFunc >> 3) & 0x7;
        hardware->pxe.pci_func =
            gnt.info.cardbus.BusDevFunc & 0x03;
        hardware->pxe.base_class =
            gnt.info.cardbus.Base_Class;
        hardware->pxe.sub_class =
            gnt.info.cardbus.Sub_Class;
        hardware->pxe.prog_intf =
            gnt.info.cardbus.Prog_Intf;
        hardware->pxe.nictype = gnt.NicType;
        break;
      case PnP_NIC:
      default:
        return -1;
        break;
      }
      /* Let's try to find the associated pci device */
      detect_pci(hardware);

      /* The firt pass try to find the exact pci device */
      hardware->pxe.pci_device = NULL;
      hardware->pxe.pci_device_pos = 0;
      struct pci_device *pci_device;
      int pci_number = 0;
      for_each_pci_func(pci_device, hardware->pci_domain) {
        pci_number++;
        if ((__pci_bus == hardware->pxe.pci_bus) &&
            (__pci_slot == hardware->pxe.pci_dev) &&
            (__pci_func == hardware->pxe.pci_func) &&
            (pci_device->vendor ==
             hardware->pxe.vendor_id)
            && (pci_device->product ==
          hardware->pxe.product_id)) {
          hardware->pxe.pci_device = pci_device;
          hardware->pxe.pci_device_pos =
              pci_number;
	      return 0;
        }
      }

      /* If we reach that part, it means the pci device pointed by
       * the pxe rom wasn't found in our list.
       * Let's try to find the device only by its pci ids.
       * The pci device we'll match is maybe not exactly the good one
       * as we can have the same pci id several times.
       * At least, the pci id, the vendor/product will be right.
       * That's clearly a workaround for some weird cases.
       * This should happend very unlikely */
      hardware->pxe.pci_device = NULL;
      hardware->pxe.pci_device_pos = 0;
      pci_number = 0;
      for_each_pci_func(pci_device, hardware->pci_domain) {
        pci_number++;
        if ((pci_device->vendor ==
             hardware->pxe.vendor_id)
            && (pci_device->product ==
          hardware->pxe.product_id)) {
          hardware->pxe.pci_device = pci_device;
          hardware->pxe.pci_device_pos =
              pci_number;
	      return 0;
        }
      }

    }
  }
  return 0;
}

void detect_pci(struct s_hardware *hardware)
{
  if (hardware->pci_detection == true)
    return;
  hardware->pci_detection = true;

  hardware->nb_pci_devices = 0;

  /* Scanning to detect pci buses and devices */
  hardware->pci_domain = pci_scan();

  if (!hardware->pci_domain)
    return;

  /* Gathering addtional information*/
  gather_additional_pci_config(hardware->pci_domain);

  struct pci_device *pci_device;
  for_each_pci_func(pci_device, hardware->pci_domain) {
    hardware->nb_pci_devices++;
  }

  printf("PCI: %d devices detected\n", hardware->nb_pci_devices);
  printf("PCI: Resolving names\n");
  /* Assigning product & vendor name for each device */
  hardware->pci_ids_return_code =
      get_name_from_pci_ids(hardware->pci_domain, hardware->pciids_path);

  printf("PCI: Resolving class names\n");
  /* Assigning class name for each device */
  hardware->pci_ids_return_code =
      get_class_name_from_pci_ids(hardware->pci_domain,
          hardware->pciids_path);

  printf("PCI: Resolving module names\n");
  /* Detecting which kernel module should match each device */
  hardware->modules_pcimap_return_code =
      get_module_name_from_pcimap(hardware->pci_domain,
           hardware->modules_pcimap_path);

  /* We try to detect the pxe stuff to populate the PXE: field of pci devices */
  detect_pxe(hardware);
}

void cpu_detect(struct s_hardware *hardware)
{
  if (hardware->cpu_detection == true)
    return;
  detect_cpu(&hardware->cpu);
  hardware->cpu_detection = true;
}

/*
 * Find the last instance of a particular command line argument
 * (which should include the final =; do not use for boolean arguments)
 */
const char *find_argument(const char **argv, const char *argument)
{
  int la = strlen(argument);
  const char **arg;
  const char *ptr = NULL;

  for (arg = argv; *arg; arg++) {
    if (!memcmp(*arg, argument, la))
      ptr = *arg + la;
  }

  return ptr;
}

void clear_screen(void)
{
  move_cursor_to_next_line();
  disable_utf8();
  set_g1_special_char();
  set_us_g0_charset();
  display_cursor(false);
  clear_entire_screen();
  reset_more_printf();
}

/* remove begining spaces */
char *skip_spaces(char *p)
{
  while (*p && *p <= ' ') {
    p++;
  }

  return p;
}

/* remove trailing & begining spaces */
char *remove_spaces(char *p)
{
  char *save=p;
  p+=strlen(p)-1;
  while (*p && *p <= ' ') {
   *p='\0';
   p--;
  }
  p=save;
  while (*p && *p <= ' ') {
    p++;
  }

  return p;
}

/* delete multiple spaces, one is enough */
char *del_multi_spaces(char *p) {
 /* Saving the original pointer*/
 char *save=p;

 /* Let's parse the complete string
  * As we search for a double spacing
  * we have to be sure then string is
  * long enough to be processed */
 while (*p && *p+1) {

   /* If we have two consecutive spaces*/
   if ((*p == ' ') && (*(p+1) == ' ')) {

    /* Let's copy to the current position
     * the content from the second space*/
    strncpy(p,p+1,strlen(p+1));

    /* The string is 1 char smaller*/
    *(p+strlen(p)-1)='\0';

    /* Don't increment the pointer as we
     * changed the content of the current position*/
    continue;
   }

   /* Nothing as been found, let's see on the next char*/
   p++;
 }
 /* Returning the original pointer*/
 return save;
}

/* Reset the more_printf counter */
void reset_more_printf() {
  display_line_nb=0;
}
