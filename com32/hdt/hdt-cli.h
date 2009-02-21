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

#ifndef DEFINE_HDT_CLI_H
#define DEFINE_HDT_CLI_H
#include <stdio.h>
#include "hdt-common.h"

#define CLI_EXIT "exit"
#define CLI_HELP "help"
#define CLI_SHOW "show"
#define CLI_HDT  "hdt"
#define CLI_PCI  "pci"
#define CLI_DMI  "dmi"
#define CLI_DMI_BASE_BOARD "base_board"
#define CLI_DMI_BATTERY "battery"
#define CLI_DMI_BIOS "bios"
#define CLI_DMI_CHASSIS "chassis"
#define CLI_DMI_MEMORY "memory"
#define CLI_DMI_PROCESSOR "processor"
#define CLI_DMI_SYSTEM "system"
#define CLI_DMI_MODULES "modules"

struct s_cli_mode {
 int mode;
 char prompt[32];
};

enum { EXIT_MODE, HDT_MODE, PCI_MODE, DMI_MODE };

void show_cli_help(struct s_cli_mode *cli_mode);
void start_cli_mode(int argc, char *argv[]);
void main_show(char *item, struct s_hardware *hardware, struct s_cli_mode *cli_mode);
int do_exit(struct s_cli_mode *cli_mode);

//DMI STUFF
void main_show_dmi(struct s_hardware *hardware,struct s_cli_mode *cli_mode);
void handle_dmi_commands(char *cli_line, struct s_cli_mode *cli_mode, struct s_hardware *hardware);
void show_dmi_base_board(struct s_hardware *hardware);
void show_dmi_system(struct s_hardware *hardware);
void show_dmi_bios(struct s_hardware *hardware);
void show_dmi_chassis(struct s_hardware *hardware);
void show_dmi_cpu(struct s_hardware *hardware);
void show_dmi_modules(struct s_hardware *hardware);

//PCI STUFF
void main_show_pci(struct s_hardware *hardware);

#endif
