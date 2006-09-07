/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
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
static char *
skipspace(char *p)
{
  while ( *p && *p <= ' ' )
    p++;

  return p;
}

void remove_eol(char *string)
{
 int j = strlen(string); 
 int i = 0;
 for(i = 0; i < j; i++) if(string[i] == '\n') string[i] = 0;
}

int hex_to_int(char *hexa)
{
 int i;
 sscanf(hexa,"%x",&i);
 return i;
}

void get_name_from_pci_ids(s_pci_device *pci_device)
{
  char line[MAX_LINE];
  char *vendor=NULL;
  char vendor_id[5];
  char *product=NULL;
  char product_id[5];
  char sub_product_id[5];
  char sub_vendor_id[5];
  FILE *f;

  f=fopen("pci.ids","r");
  if (!f)
	return;

  strcpy(pci_device->vendor_name,"Unknown");
  strcpy(pci_device->product_name,"Unknown");
  strcpy(vendor_id,"0000");
  strcpy(product_id,"0000");
  strcpy(sub_product_id,"0000");
  strcpy(sub_vendor_id,"0000");
  
 
  while ( fgets(line, sizeof line, f) ) {
    if ((line[0] == '#') || (line[0] == ' ') || (line[0] == 'C') || (line[0] == 10))
	continue;
    if (line[0] != '\t') {
	strncpy(vendor_id,line,4);
	vendor_id[4]=0;
	vendor=strdup(skipspace(strstr(line," ")));
	remove_eol(vendor);
  	strcpy(product_id,"0000");
  	strcpy(sub_product_id,"0000");
  	strcpy(sub_vendor_id,"0000");
	if (strstr(vendor_id,"ffff")) break;
	if (hex_to_int(vendor_id)==pci_device->vendor) strcpy(pci_device->vendor_name,vendor);
    } else if ((line[0] == '\t') && (line[1] != '\t')) {
	product=strdup(skipspace(strstr(line," ")));
	remove_eol(product);
	strncpy(product_id,&line[1],4);
	product_id[4]=0;
  	strcpy(sub_product_id,"0000");
  	strcpy(sub_vendor_id,"0000");
	if ((hex_to_int(vendor_id)==pci_device->vendor) && (hex_to_int(product_id)==pci_device->product)) strcpy(pci_device->product_name,product);
    } else if ((line[0] == '\t') && (line[1] == '\t')) {
	product=skipspace(strstr(line," "));
	product=strdup(skipspace(strstr(product," ")));
	remove_eol(product);
	strncpy(sub_vendor_id,&line[2],4);
	sub_vendor_id[4]=0;
	strncpy(sub_product_id,&line[7],4);
	sub_product_id[4]=0;
	if ((hex_to_int(vendor_id)==pci_device->vendor) && (hex_to_int(product_id)==pci_device->product) && (hex_to_int(sub_product_id)==pci_device->sub_product) && (hex_to_int(sub_vendor_id)==pci_device->sub_vendor)) strcpy(pci_device->product_name,product);
    }
  }
 fclose(f);
}

int pci_scan(s_pci_bus_list *pci_bus_list, s_pci_device_list *pci_device_list)
{
  unsigned int bus, dev, func, maxfunc;
  uint32_t did, sid;
  uint8_t hdrtype, rid;
  pciaddr_t a;
  int cfgtype;

  pci_device_list->count=0;

#ifdef DEBUG
  outl(~0, 0xcf8);
  printf("Poking at port CF8 = %#08x\n", inl(0xcf8));
  outl(0, 0xcf8);
#endif

  cfgtype = pci_set_config_type(PCI_CFG_AUTO);
  (void)cfgtype;

  dprintf("PCI configuration type %d\n", cfgtype);
  printf("Scanning PCI Buses\n");

  for ( bus = 0 ; bus <= 0xff ; bus++ ) {

    dprintf("Probing bus 0x%02x... \n", bus);

    pci_bus_list->pci_bus[bus].id=bus;
    pci_bus_list->pci_bus[bus].pci_device_count=0;
    pci_bus_list->count=0;;
    
    for ( dev = 0 ; dev <= 0x1f ; dev++ ) {
      maxfunc = 0;
      for ( func = 0 ; func <= maxfunc ; func++ ) {
	a = pci_mkaddr(bus, dev, func, 0);

	did = pci_readl(a);

	if ( did == 0xffffffff || did == 0xffff0000 ||
	     did == 0x0000ffff || did == 0x00000000 )
	  continue;

	hdrtype = pci_readb(a + 0x0e);

	if ( hdrtype & 0x80 )
	  maxfunc = 7;		/* Multifunction device */

//	if ( hdrtype & 0x7f )
//	  continue;		/* Ignore bridge devices */

	rid = pci_readb(a + 0x08);
	sid = pci_readl(a + 0x2c);
	s_pci_device *pci_device = &pci_device_list->pci_device[pci_device_list->count];
	pci_device->product=did>>16;
	pci_device->sub_product=sid>>16;
	pci_device->vendor=(did<<16)>>16;
	pci_device->sub_vendor=(sid<<16)>>16;
	pci_device->revision=rid;
	pci_device_list->count++;
	get_name_from_pci_ids(pci_device);
	dprintf("Scanning: BUS %02x DID %08x (%04x:%04x) SID %08x RID %02x\n", bus, did, did>>16, (did<<16)>>16 , sid, rid);
  	/* Adding the detected pci device to the bus*/
	pci_bus_list->pci_bus[bus].pci_device[pci_bus_list->pci_bus[bus].pci_device_count]=pci_device;
	pci_bus_list->pci_bus[bus].pci_device_count++;
      }
    }
  }

  /* Detecting pci buses that have pci devices connected*/
  for (bus=0;bus<=0xff;bus++) {

	if (pci_bus_list->pci_bus[bus].pci_device_count>0) {
		pci_bus_list->count++;
	}
  }
  return 0;
}
