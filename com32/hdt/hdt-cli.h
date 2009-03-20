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
#include <getkey.h>

#include "hdt-common.h"

#define MAX_LINE_SIZE 256

#define CLI_SPACE " "
#define CLI_LF "\n"
#define CLI_CLEAR "clear"
#define CLI_EXIT "exit"
#define CLI_HELP "help"
#define CLI_SHOW "show"
#define CLI_HDT  "hdt"
#define CLI_PCI  "pci"
#define CLI_PXE  "pxe"
#define CLI_KERNEL "kernel"
#define CLI_SYSLINUX "syslinux"
#define CLI_VESA "vesa"
#define CLI_SUMMARY "summary"
#define CLI_COMMANDS "commands"
#define CLI_DMI  "dmi"
#define CLI_CPU  "cpu"
#define CLI_SHOW_LIST "list"
#define CLI_IRQ "irq"
#define CLI_MODES "modes"

typedef enum {
  EXIT_MODE,
  HDT_MODE,
  PCI_MODE,
  DMI_MODE,
  CPU_MODE,
  PXE_MODE,
  KERNEL_MODE,
  SYSLINUX_MODE,
  VESA_MODE,
} cli_mode_t;

#define PROMPT_SIZE 32
#define MAX_HISTORY_SIZE 32
struct s_cli {
  cli_mode_t mode;
  char prompt[PROMPT_SIZE];
  char input[MAX_LINE_SIZE];
  int cursor_pos;
  char history[MAX_HISTORY_SIZE][MAX_LINE_SIZE];
  int history_pos;
  int max_history_pos;
};

/* A command-line command */
struct commands_mode {
  const unsigned int mode;
  struct commands_module_descr* show_modules;
  /* Future: set? */
};

struct commands_module {
  const char *name;
  void ( * exec ) ( int argc, char** argv, struct s_hardware *hardware );
};

/* Describe 'show', 'set', ... commands in a module */
struct commands_module_descr {
  struct commands_module* modules;
  const int nb_modules;
};

struct commands_mode dmi_mode;

void start_cli_mode(struct s_hardware *hardware);
void main_show(char *item, struct s_hardware *hardware);
int do_exit(struct s_cli *cli);

// DMI STUFF
#define CLI_DMI_BASE_BOARD "base_board"
#define CLI_DMI_BATTERY "battery"
#define CLI_DMI_BIOS "bios"
#define CLI_DMI_CHASSIS "chassis"
#define CLI_DMI_MEMORY "memory"
#define CLI_DMI_MEMORY_BANK "bank"
#define CLI_DMI_PROCESSOR "cpu"
#define CLI_DMI_SYSTEM "system"
#define CLI_DMI_LIST CLI_SHOW_LIST
#define CLI_DMI_MAX_MODULES 9
void main_show_dmi(struct s_hardware *hardware);
void handle_dmi_commands(char *cli_line, struct s_hardware *hardware);
void show_dmi_memory_modules(int argc, char** argv, struct s_hardware *hardware);

// PCI STUFF
#define CLI_PCI_DEVICE "device"
void main_show_pci(struct s_hardware *hardware);
void handle_pci_commands(char *cli_line, struct s_hardware *hardware);
void cli_detect_pci(struct s_hardware *hardware);

// CPU STUFF
void main_show_cpu(struct s_hardware *hardware);
void handle_cpu_commands(char *cli_line, struct s_hardware *hardware);

// PXE STUFF
void main_show_pxe(struct s_hardware *hardware);
void handle_pxe_commands(char *cli_line, struct s_hardware *hardware);

// KERNEL STUFF
void main_show_kernel(struct s_hardware *hardware);
void handle_kernel_commands(char *cli_line, struct s_hardware *hardware);

// SYSLINUX STUFF
void main_show_syslinux(struct s_hardware *hardware);
void handle_syslinux_commands(char *cli_line, struct s_hardware *hardware);

// VESA STUFF
void main_show_vesa(struct s_hardware *hardware);
void handle_vesa_commands(char *cli_line, struct s_hardware *hardware);
#endif
