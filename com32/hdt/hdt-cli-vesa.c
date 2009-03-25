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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

void main_show_vesa(struct s_hardware *hardware) {
  detect_vesa(hardware);
  if (hardware->is_vesa_valid==false) {
    more_printf("No VESA BIOS detected\n");
    return;
  }
  more_printf("VESA\n");
  more_printf(" Vesa version : %d.%d\n",hardware->vesa.major_version, hardware->vesa.minor_version);
  more_printf(" Vendor       : %s\n",hardware->vesa.vendor);
  more_printf(" Product      : %s\n",hardware->vesa.product);
  more_printf(" Product rev. : %s\n",hardware->vesa.product_revision);
  more_printf(" Software rev.: %d\n",hardware->vesa.software_rev);
  more_printf(" Memory (KB)  : %d\n",hardware->vesa.total_memory*64);
  more_printf(" Modes        : %d\n",hardware->vesa.vmi_count);
}

void show_vesa_modes(struct s_hardware *hardware) {
  detect_vesa(hardware);
  if (hardware->is_vesa_valid==false) {
    more_printf("No VESA BIOS detected\n");
    return;
  }
  clear_screen();
  more_printf(" ResH. x ResV x Bits : vga= : Vesa Mode\n",hardware->vesa.vmi_count);
  more_printf("----------------------------------------\n",hardware->vesa.vmi_count);

  for (int i=0;i<hardware->vesa.vmi_count;i++) {
    struct vesa_mode_info *mi=&hardware->vesa.vmi[i].mi;
    more_printf("%5u %5u    %3u     %3d     0x%04x\n",
                 mi->h_res, mi->v_res, mi->bpp, hardware->vesa.vmi[i].mode+0x200,hardware->vesa.vmi[i].mode);
  }
}

static void show_vesa_help() {
 more_printf("Show supports the following commands : %s %s\n",CLI_SHOW_LIST, CLI_MODES);
}

static void vesa_show(char *item, struct s_hardware *hardware) {
 if ( !strncmp(item, CLI_SHOW_LIST, sizeof(CLI_SHOW_LIST) - 1) ) {
   main_show_vesa(hardware);
   return;
 }
 if ( !strncmp(item, CLI_MODES, sizeof(CLI_MODES) - 1) ) {
   show_vesa_modes(hardware);
   return;
 }
 show_vesa_help();
}

void handle_vesa_commands(char *cli_line, struct s_hardware *hardware) {
 if ( !strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1) ) {
    vesa_show(strstr(cli_line,"show")+ sizeof(CLI_SHOW), hardware);
    return;
 }
}

