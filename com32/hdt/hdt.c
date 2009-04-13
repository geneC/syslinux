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
#include <consoles.h>
#include "hdt.h"
#include "hdt-menu.h"
#include "hdt-cli.h"
#include "hdt-common.h"

int display_line_nb = 0;

int main(const int argc, const char *argv[])
{
  char version_string[256];
  const char *arg;
  struct s_hardware hardware;

  snprintf(version_string, sizeof version_string, "%s %s by %s",
           PRODUCT_NAME,VERSION,AUTHOR);

  console_ansi_raw();

  /* Cleaning structures */
  init_hardware(&hardware);

  /* Detecting Syslinux version */
  detect_syslinux(&hardware);

  /* Detecting parameters */
  detect_parameters(argc, argv, &hardware);

  /* Opening the Syslinux console */
//  openconsole(&dev_stdcon_r, &dev_ansicon_w);

  clear_screen();
  printf("%s\n", version_string);

  if ((arg = find_argument(argv + 1, "nomenu")))
    start_cli_mode(&hardware);
  else {
    int return_code = start_menu_mode(&hardware, version_string);

    if (return_code == HDT_RETURN_TO_CLI)
      start_cli_mode(&hardware);
    else
      return return_code;
  }

  return 0;
}
