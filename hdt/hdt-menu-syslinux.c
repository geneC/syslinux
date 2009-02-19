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

#include "syslinux/config.h"
#include "hdt-menu.h"

/* Computing Syslinux menu*/
void compute_syslinuxmenu(struct s_my_menu *menu) {
  char syslinux_fs[22];
  char syslinux_fs_menu[24];
  char buffer[SUBMENULEN+1];
  char statbuffer[STATLEN+1];
  const struct syslinux_version *sv;


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
  menu->menu = add_menu(syslinux_fs_menu,-1);
  menu->items_count=0;
  set_menu_pos(SUBMENU_Y,SUBMENU_X);

  snprintf(buffer, sizeof buffer, "Bootloader : %s", syslinux_fs);
  snprintf(statbuffer, sizeof statbuffer, "Bootloader: %s", syslinux_fs);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Version    : %s", sv->version_string+2);
  snprintf(statbuffer, sizeof statbuffer, "Version: %s", sv->version_string+2);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Version    : %u",sv->version);
  snprintf(statbuffer, sizeof statbuffer, "Version: %u",sv->version);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Max API    : %u",sv->max_api);
  snprintf(statbuffer, sizeof statbuffer, "Max API: %u",sv->max_api);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  menu->items_count++;

  add_item("","",OPT_SEP,"",0);

  snprintf(buffer, sizeof buffer, "%s", sv->copyright_string+1);
  snprintf(statbuffer, sizeof statbuffer, "%s", sv->copyright_string+1);
  add_item(buffer,statbuffer,OPT_INACTIVE,NULL,0);
  menu->items_count++;

  printf("MENU: Syslinux menu done (%d items)\n",menu->items_count);
}

