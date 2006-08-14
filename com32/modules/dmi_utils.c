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

#include "stdio.h"
#include "dmi/dmi.h"

void display_bios_characteristics(s_dmi *dmi) {
int i;
  for (i=0;i<=BIOS_CHAR_NB_ELEMENTS; i++) {
        if (((bool *)(& dmi->bios.characteristics))[i] == true) {
               moreprintf("\t\t%s\n", bios_charac_strings[i]);
                }
  }
  for (i=0;i<=BIOS_CHAR_X1_NB_ELEMENTS; i++) {
        if (((bool *)(& dmi->bios.characteristics_x1))[i] == true) {
               moreprintf("\t\t%s\n", bios_charac_x1_strings[i]);
                }
  }

  for (i=0;i<=BIOS_CHAR_X2_NB_ELEMENTS; i++) {
        if (((bool *)(& dmi->bios.characteristics_x2))[i] == true) {
               moreprintf("\t\t%s\n", bios_charac_x2_strings[i]);
                }
  }
}

void display_base_board_features(s_dmi *dmi) {
int i;
  for (i=0;i<=BASE_BOARD_NB_ELEMENTS; i++) {
        if (((bool *)(& dmi->base_board.features))[i] == true) {
               moreprintf("\t\t%s\n", base_board_features_strings[i]);
                }
  }
}

void display_processor_flags(s_dmi *dmi) {
int i;
  for (i=0;i<=PROCESSOR_FLAGS_ELEMENTS; i++) {
        if (((bool *)(& dmi->processor.cpu_flags))[i] == true) {
               moreprintf("\t\t%s\n", cpu_flags_strings[i]);
                }
  }
}
