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
