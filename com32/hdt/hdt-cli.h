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

#define DEBUG 0
#if DEBUG
# define dprintf printf
#else
# define dprintf(f, ...) ((void)0)
#endif

/* Declare a variable or data structure as unused. */
#define __unused __attribute__ (( unused ))

#define MAX_LINE_SIZE 256

#define CLI_SPACE " "
#define CLI_LF "\n"
#define CLI_CLEAR "clear"
#define CLI_EXIT "exit"
#define CLI_HELP "help"
#define CLI_SHOW "show"
#define CLI_SET "set"
#define CLI_MODE "mode"
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
	INVALID_MODE,
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

struct s_cli {
  cli_mode_t mode;
  char prompt[32];
  char input[MAX_LINE_SIZE];
  int cursor_pos;
};
struct s_cli hdt_cli;

/* Describe a cli mode */
struct cli_mode_descr {
	const unsigned int mode;
	const char* name;
	/* Handle 1-token commands */
	struct cli_module_descr* default_modules;
	/* Handle show <module> <args> */
	struct cli_module_descr* show_modules;
	/* Handle set <module> <args> */
	struct cli_module_descr* set_modules;
};

/* Describe a subset of commands in a module (default, show, set, ...) */
struct cli_module_descr {
	struct cli_callback_descr* modules;
	const int nb_modules;
};

/* Describe a callback (belongs to a mode and a module) */
struct cli_callback_descr {
	const char *name;
	void ( * exec ) ( int argc, char** argv, struct s_hardware *hardware );
};

/* List of implemented modes */
#define MAX_MODES 2
struct cli_mode_descr *list_modes[MAX_MODES];
struct cli_mode_descr hdt_mode;
struct cli_mode_descr dmi_mode;

/* cli helpers */
void find_cli_mode_descr(cli_mode_t mode, struct cli_mode_descr **mode_found);
void find_cli_callback_descr(const char *module_name,
			     struct cli_module_descr *modules_list,
			     struct cli_callback_descr **module_found);
cli_mode_t mode_s_to_mode_t(char *name);

void set_mode(cli_mode_t mode, struct s_hardware *hardware);
void start_cli_mode(struct s_hardware *hardware);
void main_show(char *item, struct s_hardware *hardware);

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
