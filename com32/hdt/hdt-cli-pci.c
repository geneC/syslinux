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
#include <errno.h>

#include "hdt-cli.h"
#include "hdt-common.h"

void main_show_pci(int argc __unused, char **argv __unused,
		   struct s_hardware *hardware)
{
	cli_detect_pci(hardware);

	more_printf("PCI\n");
	more_printf(" NB Devices   : %d\n", hardware->nb_pci_devices);
}

static void show_pci_device(int argc, char **argv,
			    struct s_hardware *hardware)
{
	int i = 0;
	struct pci_device *pci_device = NULL, *temp_pci_device;
	int pcidev = -1;
	bool nopciids = false;
	bool nomodulespcimap = false;
	char kernel_modules[LINUX_KERNEL_MODULE_SIZE *
			    MAX_KERNEL_MODULES_PER_PCI_DEVICE];
	int bus = 0, slot = 0, func = 0;

	/* Sanitize arguments */
	if (argc <= 0) {
		printf("show device <number>\n");
		return;
	} else
		pcidev = strtol(argv[0], (char **)NULL, 10);

	if (errno == ERANGE) {
		printf("This PCI device number is incorrect\n");
		return;
	}
	if ((pcidev > hardware->nb_pci_devices) || (pcidev <= 0)) {
		printf("PCI device %d doesn't exists\n", pcidev);
		return;
	}
	if (hardware->pci_ids_return_code == -ENOPCIIDS) {
		nopciids = true;
	}
	if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
		nomodulespcimap = true;
	}

	for_each_pci_func(temp_pci_device, hardware->pci_domain) {
		i++;
		if (i == pcidev) {
			bus = __pci_bus;
			slot = __pci_slot;
			func = __pci_func;
			pci_device = temp_pci_device;
		}
	}

	if (pci_device == NULL) {
		printf("We were enabled to find PCI device %d\n", pcidev);
		return;
	}

	memset(kernel_modules, 0, sizeof kernel_modules);
	for (int kmod = 0;
	     kmod < pci_device->dev_info->linux_kernel_module_count; kmod++) {
		if (kmod > 0) {
			strncat(kernel_modules, " | ", 3);
		}
		strncat(kernel_modules,
			pci_device->dev_info->linux_kernel_module[kmod],
			LINUX_KERNEL_MODULE_SIZE - 1);
	}
	if (pci_device->dev_info->linux_kernel_module_count == 0)
		strlcpy(kernel_modules, "unknown", 7);

	printf("PCI Device %d\n", pcidev);

	if (nopciids == false) {
		printf("Vendor Name   : %s\n",
			    pci_device->dev_info->vendor_name);
		printf("Product Name  : %s\n",
			    pci_device->dev_info->product_name);
		printf("Class Name    : %s\n",
			    pci_device->dev_info->class_name);
	}

	if (nomodulespcimap == false) {
		printf("Kernel module : %s\n", kernel_modules);
	}

	printf("Vendor ID     : %04x\n", pci_device->vendor);
	printf("Product ID    : %04x\n", pci_device->product);
	printf("SubVendor ID  : %04x\n", pci_device->sub_vendor);
	printf("SubProduct ID : %04x\n", pci_device->sub_product);
	printf("Class ID      : %02x.%02x.%02x\n", pci_device->class[2],
		    pci_device->class[1], pci_device->class[0]);
	printf("Revision      : %02x\n", pci_device->revision);
	if ((pci_device->dev_info->irq > 0)
	    && (pci_device->dev_info->irq < 255))
		more_printf("IRQ           : %0d\n", pci_device->dev_info->irq);
	printf("Latency       : %0d\n", pci_device->dev_info->latency);
	printf("PCI Bus       : %02d\n", bus);
	printf("PCI Slot      : %02d\n", slot);
	printf("PCI Func      : %02d\n", func);

	if (hardware->is_pxe_valid == true) {
		if ((hardware->pxe.pci_device != NULL)
		    && (hardware->pxe.pci_device == pci_device)) {
			printf("Mac Address   : %s\n", hardware->pxe.mac_addr);
			printf("PXE           : Current boot device\n");
		}
	}
}

static void show_pci_devices(int argc __unused, char **argv __unused,
			     struct s_hardware *hardware)
{
	int i = 1;
	struct pci_device *pci_device;
	char kernel_modules[LINUX_KERNEL_MODULE_SIZE *
			    MAX_KERNEL_MODULES_PER_PCI_DEVICE];
	bool nopciids = false;
	bool nomodulespcimap = false;
	char first_line[81];
	char second_line[81];

	reset_more_printf();
	more_printf("%d PCI devices detected\n", hardware->nb_pci_devices);

	if (hardware->pci_ids_return_code == -ENOPCIIDS) {
		nopciids = true;
	}
	if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
		nomodulespcimap = true;
	}

	/* For every detected pci device, compute its submenu */
	for_each_pci_func(pci_device, hardware->pci_domain) {
		memset(kernel_modules, 0, sizeof kernel_modules);
		for (int kmod = 0;
		     kmod < pci_device->dev_info->linux_kernel_module_count;
		     kmod++) {
			if (kmod > 0) {
				strncat(kernel_modules, " | ", 3);
			}
			strncat(kernel_modules,
				pci_device->dev_info->linux_kernel_module[kmod],
				LINUX_KERNEL_MODULE_SIZE - 1);
		}
		if (pci_device->dev_info->linux_kernel_module_count == 0)
			strlcpy(kernel_modules, "unknown", 7);

		if (nopciids == false) {
			snprintf(first_line, sizeof(first_line),
				 "%02d: %s %s \n", i,
				 pci_device->dev_info->vendor_name,
				 pci_device->dev_info->product_name);
			if (nomodulespcimap == false)
				snprintf(second_line, sizeof(second_line),
					 "    # %-25s # Kmod: %s\n",
					 pci_device->dev_info->class_name,
					 kernel_modules);
			else
				snprintf(second_line, sizeof(second_line),
					 "    # %-25s # ID:%04x:%04x[%04x:%04x]\n",
					 pci_device->dev_info->class_name,
					 pci_device->vendor,
					 pci_device->product,
					 pci_device->sub_vendor,
					 pci_device->sub_product);

			more_printf(first_line);
			more_printf(second_line);
			more_printf("\n");
		} else if (nopciids == true) {
			if (nomodulespcimap == true) {
				more_printf("%02d: %04x:%04x [%04x:%04x] \n",
					    i, pci_device->vendor,
					    pci_device->product,
					    pci_device->sub_vendor,
					    pci_device->sub_product);
			} else {
				more_printf
				    ("%02d: %04x:%04x [%04x:%04x] Kmod:%s\n", i,
				     pci_device->vendor, pci_device->product,
				     pci_device->sub_vendor,
				     pci_device->sub_product, kernel_modules);
			}
		}
		i++;
	}
}

static void show_pci_irq(int argc __unused, char **argv __unused,
			 struct s_hardware *hardware)
{
	struct pci_device *pci_device;
	bool nopciids = false;

	reset_more_printf();
	more_printf("%d PCI devices detected\n", hardware->nb_pci_devices);
	more_printf("IRQ : product\n");
	more_printf("-------------\n");

	if (hardware->pci_ids_return_code == -ENOPCIIDS) {
		nopciids = true;
	}

	/* For every detected pci device, compute its submenu */
	for_each_pci_func(pci_device, hardware->pci_domain) {
		/* Only display valid IRQs */
		if ((pci_device->dev_info->irq > 0)
		    && (pci_device->dev_info->irq < 255)) {
			if (nopciids == false) {
				more_printf("%02d  : %s %s \n",
					    pci_device->dev_info->irq,
					    pci_device->dev_info->vendor_name,
					    pci_device->dev_info->product_name);
			} else {
				more_printf("%02d  : %04x:%04x [%04x:%04x] \n",
					    pci_device->dev_info->irq,
					    pci_device->vendor,
					    pci_device->product,
					    pci_device->sub_vendor,
					    pci_device->sub_product);
			}
		}
	}
}

struct cli_callback_descr list_pci_show_modules[] = {
	{
	 .name = CLI_IRQ,
	 .exec = show_pci_irq,
	 },
	{
	 .name = CLI_PCI_DEVICE,
	 .exec = show_pci_device,
	 },
	{
	 .name = NULL,
	 .exec = NULL,
	 },
};

struct cli_module_descr pci_show_modules = {
	.modules = list_pci_show_modules,
	.default_callback = show_pci_devices,
};

struct cli_mode_descr pci_mode = {
	.mode = PCI_MODE,
	.name = CLI_PCI,
	.default_modules = NULL,
	.show_modules = &pci_show_modules,
	.set_modules = NULL,
};

void cli_detect_pci(struct s_hardware *hardware)
{
	bool error = false;
	if (hardware->pci_detection == false) {
		detect_pci(hardware);
		if (hardware->pci_ids_return_code == -ENOPCIIDS) {
			printf
			    ("The pci.ids file is missing, device names can't be computed.\n");
			printf("Please put one in same dir as hdt\n");
			error = true;
		}
		if (hardware->modules_pcimap_return_code == -ENOMODULESPCIMAP) {
			printf
			    ("The modules.pcimap file is missing, device names can't be computed.\n");
			printf("Please put one in same dir as hdt\n");
			error = true;
		}
		if (error == true) {
			char tempbuf[10];
			printf("Press enter to continue\n");
			fgets(tempbuf, sizeof(tempbuf), stdin);
		}
	}
}
