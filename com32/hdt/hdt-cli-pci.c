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

#include "hdt-cli.h"
#include "hdt-common.h"
#include <stdlib.h>
#include <string.h>

void cli_detect_pci(struct s_hardware *hardware) {
 bool error=false;
 if (hardware->pci_detection==false) {
	 detect_pci(hardware);
	 if (hardware->pci_ids_return_code == -ENOPCIIDS) {
		more_printf("The pci.ids file is missing, device names can't be computed.\n");
		more_printf("Please put one in same dir as hdt\n");
		error=true;
	 }
	 if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
		more_printf("The modules.pcimap file is missing, device names can't be computed.\n");
		more_printf("Please put one in same dir as hdt\n");
		error=true;
	 }
	 if (error == true) {
	   char tempbuf[10];\
	   printf("Press enter to continue\n");\
	   fgets(tempbuf, sizeof(tempbuf), stdin);\
	 }
 }
}

void main_show_pci(struct s_hardware *hardware) {
 int i=1;
 struct pci_device *pci_device;
 char kernel_modules [LINUX_KERNEL_MODULE_SIZE*MAX_KERNEL_MODULES_PER_PCI_DEVICE];
 bool nopciids=false;
 bool nomodulespcimap=false;
 char first_line[81];
 char second_line[81];
 char third_line[81];
 cli_detect_pci(hardware);
 clear_screen();
 more_printf("%d PCI devices detected\n",hardware->nb_pci_devices);

 if (hardware->pci_ids_return_code == -ENOPCIIDS) {
    nopciids=true;
  }

 if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
    nomodulespcimap=true;
 }

 /* For every detected pci device, compute its submenu */
 for_each_pci_func(pci_device, hardware->pci_domain) {
   memset(kernel_modules,0,sizeof kernel_modules);
   for (int kmod=0; kmod<pci_device->dev_info->linux_kernel_module_count;kmod++) {
     if (kmod>0) {
       strncat(kernel_modules," | ",3);
     }
     strncat(kernel_modules, pci_device->dev_info->linux_kernel_module[kmod],LINUX_KERNEL_MODULE_SIZE-1);
   }
   if (pci_device->dev_info->linux_kernel_module_count==0) strlcpy(kernel_modules,"unknown",7);

   if ((nopciids == false) && (nomodulespcimap == false)) {
    snprintf(first_line,sizeof(first_line),"%02d: %02x:%02x.%01x %s %s \n",
               i,__pci_bus, __pci_slot, __pci_func,pci_device->dev_info->vendor_name,
               pci_device->dev_info->product_name);
    snprintf(second_line,sizeof(second_line),"    # %-25s # ID:%04x:%04x[%04x:%04x]\n",
               pci_device->dev_info->class_name,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product);
    snprintf(third_line,sizeof(third_line),  "    # Linux Kernel Module(s): %s \n",kernel_modules);
    more_printf(first_line);
    more_printf(second_line);
    more_printf(third_line);
    more_printf("\n");
   } else if ((nopciids == true) && (nomodulespcimap == true)) {
    more_printf("%02d: %02x:%02x.%01x %04x:%04x [%04x:%04x] \n",
               i,__pci_bus, __pci_slot, __pci_func,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product,kernel_modules);

   } else if ((nopciids == true) && (nomodulespcimap == false)) {
    more_printf("%02d: %02x:%02x.%01x %04x:%04x [%04x:%04x] Kmod:%s\n",
               i,__pci_bus, __pci_slot, __pci_func,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product,kernel_modules,
	       pci_device->sub_product,kernel_modules);
   } else  if ((nopciids == false) && (nomodulespcimap == true)) {
    snprintf(first_line,sizeof(first_line),"%02d: %02x:%02x.%01x %s %s \n",
               i,__pci_bus, __pci_slot, __pci_func,pci_device->dev_info->vendor_name,
               pci_device->dev_info->product_name);
    snprintf(second_line,sizeof(second_line),"    # %-25s # ID:%04x:%04x[%04x:%04x]\n",
               pci_device->dev_info->class_name,
               pci_device->vendor, pci_device->product,
               pci_device->sub_vendor, pci_device->sub_product);
    more_printf(first_line);
    more_printf(second_line);
    more_printf("\n");
   }

 i++;
 }


}
