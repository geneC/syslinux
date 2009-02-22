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

void set_mode(struct s_cli_mode *cli_mode, cli_mode_t mode, struct s_hardware *hardware) {
 switch (mode) {
  case EXIT_MODE:
        cli_mode->mode=mode;
        snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s:", CLI_EXIT);
        break;

  case HDT_MODE:
        cli_mode->mode=mode;
        snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s:", CLI_HDT);
        break;

  case PCI_MODE:
	cli_mode->mode=mode;
	snprintf(cli_mode->prompt,sizeof(cli_mode->prompt),"%s:", CLI_PCI);
	if (!hardware->pci_detection)
          cli_detect_pci(hardware);
	break;

  case DMI_MODE:
	if (!hardware->dmi_detection)
          detect_dmi(hardware);
	if (!hardware->is_dmi_valid) {
          printf("No valid DMI table found, exiting.\n");
          break;
        }
	cli_mode->mode=mode;
	snprintf(cli_mode->prompt,sizeof(cli_mode->prompt),"%s:",CLI_DMI);
	break;
 }
}

void handle_hdt_commands(char *cli_line, struct s_cli_mode *cli_mode, struct s_hardware *hardware) {
    /* hdt cli mode specific commands */
    if ( !strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1) ) {
	   main_show(strstr(cli_line,"show")+ sizeof (CLI_SHOW), hardware,cli_mode);
	   return;
    }
}

/* Code that manage the cli mode */
void start_cli_mode(int argc, char *argv[]) {
 char cli_line[256];
 struct s_hardware hardware;
 struct s_cli_mode cli_mode;

 /* Cleaning structures */
 init_hardware(&hardware);

 set_mode(&cli_mode,HDT_MODE,&hardware);

 printf("Entering CLI mode\n");

 for (;;) {
  memset(cli_line,0,sizeof cli_line);
  printf("%s",cli_mode.prompt);

  fgets(cli_line, sizeof cli_line, stdin);
  cli_line[strlen(cli_line)-1]='\0';
    /* We use sizeof BLAH - 1 to remove the last \0 */

    if ( !strncmp(cli_line, CLI_EXIT, sizeof(CLI_EXIT) - 1) ) {
	   int mode=do_exit(&cli_mode);
	   if (mode ==  EXIT_MODE)
		   return;
	   set_mode(&cli_mode,mode,&hardware);
	   continue;
    }

    if ( !strncmp(cli_line, CLI_HELP, sizeof(CLI_HELP) - 1) ) {
           show_cli_help(&cli_mode);
	   continue;
    }

    if ( !strncmp(cli_line, CLI_PCI, sizeof(CLI_PCI) - 1) ) {
	   set_mode(&cli_mode,PCI_MODE,&hardware);
	   continue;
    }
    if ( !strncmp(cli_line, CLI_CLEAR, sizeof(CLI_CLEAR) - 1) ) {
	   clear_screen();
	   continue;
    }
    if ( !strncmp(cli_line, CLI_DMI, sizeof(CLI_DMI) - 1) ) {
	   set_mode(&cli_mode,DMI_MODE,&hardware);
	   continue;
    }
    /* All commands before that line are common for all cli modes
     * the following will be specific for every mode */
    switch(cli_mode.mode) {
     case DMI_MODE: handle_dmi_commands(cli_line,&cli_mode, &hardware); break;
     case PCI_MODE: handle_pci_commands(cli_line,&cli_mode, &hardware); break;
     case HDT_MODE: handle_hdt_commands(cli_line,&cli_mode, &hardware); break;
     case EXIT_MODE: break; /* should not happend */
    }
 }
}

int do_exit(struct s_cli_mode *cli_mode) {
 switch (cli_mode->mode) {
  case HDT_MODE: return EXIT_MODE;
  case PCI_MODE: return HDT_MODE;
  case DMI_MODE: return HDT_MODE;
  case EXIT_MODE: return EXIT_MODE; /* should not happend */
 }
return HDT_MODE;
}

void show_cli_help(struct s_cli_mode *cli_mode) {
switch (cli_mode->mode) {
	case HDT_MODE:
		printf("Available commands are : %s %s %s %s %s %s\n",CLI_CLEAR, CLI_EXIT,CLI_HELP,CLI_SHOW, CLI_PCI, CLI_DMI);
		break;
	case PCI_MODE:
		printf("Available commands are : %s %s %s %s\n",CLI_CLEAR, CLI_EXIT, CLI_HELP, CLI_SHOW);
		break;
	case DMI_MODE:
		printf("Available commands are : %s %s %s %s\n",CLI_CLEAR, CLI_EXIT, CLI_HELP, CLI_SHOW);
		break;
	case EXIT_MODE: /* Should not happend*/
		break;
}
}

void main_show(char *item, struct s_hardware *hardware, struct s_cli_mode *cli_mode) {
 if (!strncmp(item,CLI_PCI, sizeof (CLI_PCI))) main_show_pci(hardware);
 if (!strncmp(item,CLI_DMI, sizeof (CLI_DMI))) main_show_dmi(hardware,cli_mode);
}
