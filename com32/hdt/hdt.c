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

/*
 * hdt.c
 *
 * An Hardware Detection Tool
 */

#include <stdio.h>
#include <console.h>
#include "hdt.h"
#include "hdt-menu.h"
#include "hdt-cli.h"
#include "hdt-common.h"

int display_line_nb=0;

int main(int argc, char *argv[])
{
  char version_string[256];
  char *arg;

  snprintf(version_string,sizeof version_string,"%s %s by %s",PRODUCT_NAME,VERSION,AUTHOR);

  /* Opening the syslinux console */
  openconsole(&dev_stdcon_r, &dev_ansicon_w);

  clear_screen();
  printf("%s\n",version_string);


  if ((arg = find_argument(argv+1, "nomenu"))) {
	  start_cli_mode(argc, argv);
  } else{
	 return start_menu_mode(version_string);
  }

  return 0;
}
