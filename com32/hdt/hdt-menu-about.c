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

/* Computing About menu*/
void compute_aboutmenu(struct s_my_menu *menu)
{
  char buffer[SUBMENULEN + 1];
  char statbuffer[STATLEN + 1];

  menu->menu = add_menu(" About ", -1);
  menu->items_count = 0;

  set_menu_pos(SUBMENU_Y, SUBMENU_X);

  snprintf(buffer, sizeof buffer, "Product     : %s", PRODUCT_NAME);
  snprintf(statbuffer, sizeof statbuffer, "Product : %s", PRODUCT_NAME);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Version     : %s", VERSION);
  snprintf(statbuffer, sizeof statbuffer, "Version : %s", VERSION);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Author      : %s", AUTHOR);
  snprintf(statbuffer, sizeof statbuffer, "Author  : %s", AUTHOR);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  snprintf(buffer, sizeof buffer, "Contact     : %s", CONTACT);
  snprintf(statbuffer, sizeof statbuffer, "Contact : %s", CONTACT);
  add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
  menu->items_count++;

  char *contributors[NB_CONTRIBUTORS] = CONTRIBUTORS;
  for (int c=0; c<NB_CONTRIBUTORS; c++) {
   snprintf(buffer, sizeof buffer, "Contributor : %s", contributors[c]);
   snprintf(statbuffer, sizeof statbuffer, "Contributor : %s", contributors[c]);
   add_item(buffer, statbuffer, OPT_INACTIVE, NULL, 0);
   menu->items_count++;
  }

  printf("MENU: About menu done (%d items)\n", menu->items_count);
}
