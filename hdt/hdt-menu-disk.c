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

