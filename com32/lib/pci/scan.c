/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006-2007 Erwan Velu - All Rights Reserved
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
 * ----------------------------------------------------------------------- */

/*
 * pci.c
 *
 * A module to extract pci informations
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <sys/pci.h>
#include <com32.h>
#include <stdbool.h>

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

#define MAX_LINE 512

/* searching the next char that is not a space */
static char *skipspace(char *p)
{
  while (*p && *p <= ' ')
    p++;

  return p;
}

/* removing any \n found in a string */
static void remove_eol(char *string)
{
 int j = strlen(string);
 int i = 0;
 for(i = 0; i < j; i++) if(string[i] == '\n') string[i] = 0;
}

/* converting a hexa string into its numerical value*/
static int hex_to_int(char *hexa)
{
  return strtoul(hexa, NULL, 16);
}

/* Try to match any pci device to the appropriate kernel module */
/* it uses the modules.pcimap from the boot device*/
void get_module_name_from_pci_ids(struct pci_device_list *pci_device_list)
{
  char line[MAX_LINE];
  char module_name[21]; // the module name field is 21 char long
  char delims[]=" ";    // colums are separated by spaces
  char vendor_id[16];
  char product_id[16];
  char sub_vendor_id[16];
  char sub_product_id[16];
  FILE *f;
  int pci_dev;

  /* Intializing the linux_kernel_module for each pci device to "unknow" */
  /* adding a pci_dev_info member if needed*/
  for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
    struct pci_device *pci_device = &(pci_device_list->pci_device[pci_dev]);

    /* initialize the pci_dev_info structure if it doesn't exist yet. */
    if (! pci_device->pci_dev_info) {
      pci_device->pci_dev_info = calloc(1,sizeof *pci_device->pci_dev_info);

      if (!pci_device->pci_dev_info) {
	printf("Can't allocate memory\n");
	return;
      }
    }
    strlcpy(pci_device->pci_dev_info->linux_kernel_module,"unknown",7);
  }

  /* Opening the modules.pcimap (ofa linux kernel) from the boot device*/
  f=fopen("modules.pcimap","r");
  if (!f)
    return;

  strcpy(vendor_id,"0000");
  strcpy(product_id,"0000");
  strcpy(sub_product_id,"0000");
  strcpy(sub_vendor_id,"0000");

  /* for each line we found in the modules.pcimap*/
  while ( fgets(line, sizeof line, f) ) {
    /*skipping unecessary lines */
    if ((line[0] == '#') || (line[0] == ' ') || (line[0] == 10))
        continue;

    char *result = NULL;
    int field=0;

    /* looking for the next field */
    result = strtok(line, delims);
    while( result != NULL ) {
       /* if the column is larger than 1 char */
       /* multiple spaces generates some empty fields*/
       if (strlen(result)>1) {
	 switch (field) {
	 case 0:strcpy(module_name,result); break;
	 case 1:strcpy(vendor_id,result); break;
	 case 2:strcpy(product_id,result); break;
	 case 3:strcpy(sub_vendor_id,result); break;
	 case 4:strcpy(sub_product_id,result); break;
	 }
	 field++;
       }
       /* Searching the next field*/
       result = strtok( NULL, delims );
   }
    /* if a pci_device match an entry, fill the linux_kernel_module with
       the appropriate kernel module */
    for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
      struct pci_device *pci_device =
	&pci_device_list->pci_device[pci_dev];

      if (hex_to_int(vendor_id) == pci_device->vendor &&
	  hex_to_int(product_id) == pci_device->product &&
	  (hex_to_int(sub_product_id) & pci_device->sub_product)
	  == pci_device->sub_product &&
	  (hex_to_int(sub_vendor_id) & pci_device->sub_vendor)
	  == pci_device->sub_vendor)
	strcpy(pci_device->pci_dev_info->linux_kernel_module,
	       module_name);
    }
  }
 fclose(f);
}

/* Try to match any pci device to the appropriate vendor and product name */
/* it uses the pci.ids from the boot device*/
void get_name_from_pci_ids(struct pci_device_list *pci_device_list)
{
  char line[MAX_LINE];
  char vendor[255];
  char vendor_id[5];
  char product[255];
  char product_id[5];
  char sub_product_id[5];
  char sub_vendor_id[5];
  FILE *f;
  int pci_dev;

 /* Intializing the vendor/product name for each pci device to "unknow" */
 /* adding a pci_dev_info member if needed*/
 for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
    struct pci_device *pci_device = &pci_device_list->pci_device[pci_dev];

    /* initialize the pci_dev_info structure if it doesn't exist yet. */
    if (! pci_device->pci_dev_info) {
      pci_device->pci_dev_info = calloc(1,sizeof *pci_device->pci_dev_info);

      if (!pci_device->pci_dev_info) {
	printf("Can't allocate memory\n");
	return;
      }
    }

    strlcpy(pci_device->pci_dev_info->vendor_name,"unknown",7);
    strlcpy(pci_device->pci_dev_info->product_name,"unknown",7);
  }

  /* Opening the pci.ids from the boot device*/
  f=fopen("pci.ids","r");
  if (!f)
        return;
  strcpy(vendor_id,"0000");
  strcpy(product_id,"0000");
  strcpy(sub_product_id,"0000");
  strcpy(sub_vendor_id,"0000");


  /* for each line we found in the pci.ids*/
  while ( fgets(line, sizeof line, f) ) {
    /* Skipping uncessary lines */
    if ((line[0] == '#') || (line[0] == ' ') || (line[0] == 'C') ||
	(line[0] == 10))
        continue;
    /* If the line doesn't start with a tab, it means that's a vendor id */
    if (line[0] != '\t') {

	/* the 4th first chars are the vendor_id */
        strlcpy(vendor_id,line,4);

	/* the vendor name is the next field*/
        vendor_id[4]=0;
        strlcpy(vendor,skipspace(strstr(line," ")),255);

        remove_eol(vendor);
	/* init product_id, sub_product and sub_vendor */
        strcpy(product_id,"0000");
        strcpy(sub_product_id,"0000");
        strcpy(sub_vendor_id,"0000");

	/* ffff is an invalid vendor id */
	if (strstr(vendor_id,"ffff")) break;
	/* assign the vendor_name to any matching pci device*/
	for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
	  struct pci_device *pci_device =
	    &pci_device_list->pci_device[pci_dev];

	  if (hex_to_int(vendor_id) == pci_device->vendor)
	    strlcpy(pci_device->pci_dev_info->vendor_name,vendor,255);
	}
    /* if we have a tab + a char, it means this is a product id */
    } else if ((line[0] == '\t') && (line[1] != '\t')) {

	/* the product name the second field */
        strlcpy(product,skipspace(strstr(line," ")),255);
        remove_eol(product);

	/* the product id is first field */
	strlcpy(product_id,&line[1],4);
        product_id[4]=0;

	/* init sub_product and sub_vendor */
        strcpy(sub_product_id,"0000");
        strcpy(sub_vendor_id,"0000");

	/* assign the product_name to any matching pci device*/
	for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
	  struct pci_device *pci_device =
	    &pci_device_list->pci_device[pci_dev];
	  if (hex_to_int(vendor_id) == pci_device->vendor &&
	      hex_to_int(product_id) == pci_device->product)
	    strlcpy(pci_device->pci_dev_info->product_name,product,255);
	}

    /* if we have two tabs, it means this is a sub product */
    } else if ((line[0] == '\t') && (line[1] == '\t')) {

      /* the product name is last field */
      strlcpy(product,skipspace(strstr(line," ")),255);
      strlcpy(product,skipspace(strstr(product," ")),255);
      remove_eol(product);

      /* the sub_vendor id is first field */
      strlcpy(sub_vendor_id,&line[2],4);
      sub_vendor_id[4]=0;

      /* the sub_vendor id is second field */
      strlcpy(sub_product_id,&line[7],4);
      sub_product_id[4]=0;

      /* assign the product_name to any matching pci device*/
      for (pci_dev=0; pci_dev < pci_device_list->count; pci_dev++) {
	struct pci_device *pci_device =
	  &pci_device_list->pci_device[pci_dev];

	if (hex_to_int(vendor_id) == pci_device->vendor &&
	    hex_to_int(product_id) == pci_device->product &&
	    hex_to_int(sub_product_id) == pci_device->sub_product &&
	    hex_to_int(sub_vendor_id) == pci_device->sub_vendor)
	  strlcpy(pci_device->pci_dev_info->product_name,product,255);
      }
    }
  }
  fclose(f);
}

/* searching if any pcidevice match our query */
struct match *find_pci_device(struct pci_device_list * pci_device_list,
			      struct match *list)
{
  int pci_dev;
  uint32_t did, sid;
  struct match *m;
  /* for all matches we have to search */
  for (m = list; m; m = m->next) {
	  /* for each pci device we know */
    for (pci_dev = 0; pci_dev < pci_device_list->count; pci_dev++) {
      struct pci_device *pci_device =
	&pci_device_list->pci_device[pci_dev];

      /* sid & did are the easiest way to compare devices */
      /* they are made of vendor/product subvendor/subproduct ids */
      sid =
	((pci_device->sub_product) << 16 | (pci_device->
					    sub_vendor));
      did = ((pci_device->product << 16) | (pci_device->vendor));

      /*if the current device match */
      if (((did ^ m->did) & m->did_mask) == 0 &&
	  ((sid ^ m->sid) & m->sid_mask) == 0 &&
	  pci_device->revision >= m->rid_min
	  && pci_device->revision <= m->rid_max) {
	dprintf("PCI Match: Vendor=%04x Product=%04x Sub_vendor=%04x Sub_Product=%04x Release=%02x\n",
		pci_device->vendor, pci_device->product,
		pci_device->sub_vendor,
		pci_device->sub_product,
		pci_device->revision);
	/* returning the matched pci device */
	return m;
      }
    }
  }
  return NULL;
}

/* scanning the pci bus to find pci devices */
int pci_scan(struct pci_bus_list * pci_bus_list, struct pci_device_list * pci_device_list)
{
  unsigned int bus, dev, func, maxfunc;
  uint32_t did, sid;
  uint8_t hdrtype, rid;
  pciaddr_t a;
  int cfgtype;

  pci_device_list->count = 0;

#ifdef DEBUG
  outl(~0, 0xcf8);
  printf("Poking at port CF8 = %#08x\n", inl(0xcf8));
  outl(0, 0xcf8);
#endif

  cfgtype = pci_set_config_type(PCI_CFG_AUTO);
  (void)cfgtype;

  dprintf("PCI configuration type %d\n", cfgtype);
  dprintf("Scanning PCI Buses\n");

  /* We try to detect 256 buses */
  for (bus = 0; bus < MAX_PCI_BUSES; bus++) {

    dprintf("Probing bus 0x%02x... \n", bus);

    pci_bus_list->pci_bus[bus].id = bus;
    pci_bus_list->pci_bus[bus].pci_device_count = 0;
    pci_bus_list->count = 0;;

    for (dev = 0; dev < MAX_PCI_DEVICES ; dev++) {
      maxfunc = 1;		/* Assume a single-function device */
      for (func = 0; func < maxfunc; func++) {
	struct pci_device *pci_device =
 	  &pci_device_list->pci_device[pci_device_list->count];

	a = pci_mkaddr(bus, dev, func, 0);

	did = pci_readl(a);

	if (did == 0xffffffff || did == 0xffff0000 ||
	    did == 0x0000ffff || did == 0x00000000)
	  continue;

	hdrtype = pci_readb(a + 0x0e);

	if (hdrtype & 0x80)
	  maxfunc = MAX_PCI_FUNC; /* Multifunction device */

	rid = pci_readb(a + 0x08);
	sid = pci_readl(a + 0x2c);

	pci_device->addr = a;
	pci_device->product = did >> 16;
	pci_device->sub_product = sid >> 16;
	pci_device->vendor = (did << 16) >> 16;
	pci_device->sub_vendor = (sid << 16) >> 16;
	pci_device->revision = rid;
	pci_device_list->count++;
	pci_device++;

	dprintf
	  ("Scanning: BUS %02x DID %08x (%04x:%04x) SID %08x RID %02x\n",
	   bus, did, did >> 16, (did << 16) >> 16,
	   sid, rid);
	/* Adding the detected pci device to the bus */
	pci_bus_list->pci_bus[bus].
	  pci_device[pci_bus_list->pci_bus[bus].
		     pci_device_count] = pci_device;
	pci_bus_list->pci_bus[bus].pci_device_count++;
      }
    }
  }

  /* Detecting pci buses that have pci devices connected */
  for (bus = 0; bus < MAX_PCI_BUSES; bus++) {
    if (pci_bus_list->pci_bus[bus].pci_device_count > 0) {
      pci_bus_list->count++;
    }
  }
  return 0;
}
