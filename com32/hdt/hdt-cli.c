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
#include <stdlib.h>
#include <string.h>

/* Code that manage the cli mode */
void start_cli_mode(int argc, char *argv[]) {
 char cli_line[256];
 struct s_hardware hardware;

  /* Cleaning structures */
 init_hardware(&hardware);

 printf("Entering CLI mode\n");
 for (;;) {
  memset(cli_line,0,sizeof cli_line);
  printf("hdt:");
  fgets(cli_line, sizeof cli_line, stdin);
    /* We use sizeof BLAH - 2 to remove the return char */
    if ( !strncmp(cli_line, CLI_EXIT, sizeof CLI_EXIT - 2 ) )
                  break;
    if ( !strncmp(cli_line, CLI_HELP, sizeof CLI_HELP - 2) )
                  show_cli_help();
 }
}


void show_cli_help() {
 printf("Available commands are : %s %s\n",CLI_EXIT,CLI_HELP);
}

