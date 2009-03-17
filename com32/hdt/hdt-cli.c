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

#include <stdlib.h>
#include <string.h>
#include <syslinux/config.h>

#include "hdt-cli.h"
#include "hdt-common.h"

static void set_mode(struct s_cli_mode *cli_mode, cli_mode_t mode,
                     struct s_hardware *hardware)
{
  switch (mode) {
  case EXIT_MODE:
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
             CLI_EXIT);
    break;

  case HDT_MODE:
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
             CLI_HDT);
    break;

  case PXE_MODE:
    if (hardware->sv->filesystem != SYSLINUX_FS_PXELINUX) {
      more_printf("You are not currently using PXELINUX\n");
      break;
    }
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
       CLI_PXE);
    break;

  case KERNEL_MODE:
    detect_pci(hardware);
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
       CLI_KERNEL);
    break;

  case SYSLINUX_MODE:
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
       CLI_SYSLINUX);
    break;

  case VESA_MODE:
    cli_mode->mode=mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ", CLI_VESA);
    break;

  case PCI_MODE:
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
       CLI_PCI);
    if (!hardware->pci_detection)
      cli_detect_pci(hardware);
    break;

  case CPU_MODE:
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
       CLI_CPU);
    if (!hardware->dmi_detection)
      detect_dmi(hardware);
    if (!hardware->cpu_detection)
      cpu_detect(hardware);
    break;

  case DMI_MODE:
    detect_dmi(hardware);
    if (!hardware->is_dmi_valid) {
      more_printf("No valid DMI table found, exiting.\n");
      break;
    }
    cli_mode->mode = mode;
    snprintf(cli_mode->prompt, sizeof(cli_mode->prompt), "%s> ",
             CLI_DMI);
    break;
  }
}

static void handle_hdt_commands(char *cli_line, struct s_hardware *hardware)
{
  /* hdt cli mode specific commands */
  if (!strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1)) {
    main_show(strstr(cli_line, "show") + sizeof(CLI_SHOW), hardware);
    return;
  }
}

static void show_cli_help(struct s_cli_mode *cli_mode)
{
  switch (cli_mode->mode) {
  case HDT_MODE:
    more_printf("Available commands are :\n");
    more_printf
        ("%s %s %s %s %s %s %s %s %s %s %s\n",
         CLI_CLEAR, CLI_EXIT, CLI_HELP, CLI_SHOW, CLI_PCI, CLI_DMI,
         CLI_PXE, CLI_KERNEL, CLI_CPU, CLI_SYSLINUX, CLI_VESA);
    break;
  case SYSLINUX_MODE:
  case KERNEL_MODE:
  case PXE_MODE:
  case VESA_MODE:
  case CPU_MODE:
  case PCI_MODE:
  case DMI_MODE:
    printf("Available commands are : %s %s %s %s\n",
           CLI_CLEAR, CLI_EXIT, CLI_HELP, CLI_SHOW);
    break;
  case EXIT_MODE: /* Should not happen */
    break;
  }
}

/* Code that manages the cli mode */
void start_cli_mode(struct s_hardware *hardware)
{
  char cli_line[256];
  struct s_cli_mode cli_mode;

  set_mode(&cli_mode, HDT_MODE, hardware);

  printf("Entering CLI mode\n");

  for (;;) {
    memset(cli_line, 0, sizeof cli_line);
    more_printf("%s", cli_mode.prompt);

    fgets(cli_line, sizeof cli_line, stdin);
    /* We use sizeof BLAH - 1 to remove the last \0 */
    cli_line[strlen(cli_line) - 1] = '\0';

    if (!strncmp(cli_line, CLI_EXIT, sizeof(CLI_EXIT) - 1)) {
      int mode = do_exit(&cli_mode);
      if (mode == EXIT_MODE)
        return;
      set_mode(&cli_mode, mode, hardware);
      continue;
    }

    if (!strncmp(cli_line, CLI_HELP, sizeof(CLI_HELP) - 1)) {
      show_cli_help(&cli_mode);
      continue;
    }
    if (!strncmp(cli_line, CLI_PCI, sizeof(CLI_PCI) - 1)) {
      set_mode(&cli_mode, PCI_MODE, hardware);
      continue;
    }
    if (!strncmp(cli_line, CLI_CLEAR, sizeof(CLI_CLEAR) - 1)) {
      clear_screen();
      continue;
    }
    if (!strncmp(cli_line, CLI_CPU, sizeof(CLI_CPU) - 1)) {
      set_mode(&cli_mode, CPU_MODE, hardware);
      continue;
    }
    if (!strncmp(cli_line, CLI_DMI, sizeof(CLI_DMI) - 1)) {
      set_mode(&cli_mode, DMI_MODE, hardware);
      continue;
    }
    if (!strncmp(cli_line, CLI_PXE, sizeof(CLI_PXE) - 1)) {
      set_mode(&cli_mode, PXE_MODE, hardware);
      continue;
    }
    if (!strncmp(cli_line, CLI_KERNEL, sizeof(CLI_KERNEL) - 1)) {
      set_mode(&cli_mode, KERNEL_MODE, hardware);
      continue;
    }
    if (!strncmp(cli_line, CLI_SYSLINUX, sizeof(CLI_SYSLINUX) - 1)) {
      set_mode(&cli_mode, SYSLINUX_MODE, hardware);
      continue;
    }
    if ( !strncmp(cli_line, CLI_VESA, sizeof(CLI_VESA) - 1) ) {
      set_mode(&cli_mode,VESA_MODE,hardware);
      continue;
    }

    /*
     * All commands before that line are common for all cli modes.
     * The following will be specific for every mode.
     */
    switch (cli_mode.mode) {
    case DMI_MODE:
      handle_dmi_commands(cli_line, hardware);
      break;
    case PCI_MODE:
      handle_pci_commands(cli_line, hardware);
      break;
    case HDT_MODE:
      handle_hdt_commands(cli_line, hardware);
      break;
    case CPU_MODE:
      handle_cpu_commands(cli_line, hardware);
      break;
    case PXE_MODE:
      handle_pxe_commands(cli_line, hardware);
      break;
    case VESA_MODE:
      handle_vesa_commands(cli_line, hardware);
      break;
    case SYSLINUX_MODE:
      handle_syslinux_commands(cli_line, hardware);
      break;
    case KERNEL_MODE:
      handle_kernel_commands(cli_line, hardware);
      break;
    case EXIT_MODE:
      break;  /* should not happen */
    }
  }
}

int do_exit(struct s_cli_mode *cli_mode)
{
  switch (cli_mode->mode) {
  case HDT_MODE:
    return EXIT_MODE;
  case KERNEL_MODE:
  case PXE_MODE:
  case SYSLINUX_MODE:
  case PCI_MODE:
  case DMI_MODE:
  case VESA_MODE:
  case CPU_MODE:
    return HDT_MODE;
  case EXIT_MODE:
    return EXIT_MODE; /* should not happen */
  }
  return HDT_MODE;
}

static void main_show_summary(struct s_hardware *hardware)
{
  detect_pci(hardware); /* pxe is detected in the pci */
  detect_dmi(hardware);
  cpu_detect(hardware);
  clear_screen();
  main_show_cpu(hardware);
  if (hardware->is_dmi_valid) {
    more_printf("System\n");
    more_printf(" Manufacturer : %s\n",
          hardware->dmi.system.manufacturer);
    more_printf(" Product Name : %s\n",
          hardware->dmi.system.product_name);
    more_printf(" Serial       : %s\n",
          hardware->dmi.system.serial);
    more_printf("Bios\n");
    more_printf(" Version      : %s\n", hardware->dmi.bios.version);
    more_printf(" Release      : %s\n",
          hardware->dmi.bios.release_date);
    show_dmi_memory_modules(hardware, false, false);
  }
  main_show_pci(hardware);

  if (hardware->is_pxe_valid)
    main_show_pxe(hardware);

  main_show_kernel(hardware);
}

void show_main_help(struct s_hardware *hardware)
{
  more_printf("Show supports the following commands : \n");
  more_printf(" %s\n", CLI_SUMMARY);
  more_printf(" %s\n", CLI_PCI);
  more_printf(" %s\n", CLI_DMI);
  more_printf(" %s\n", CLI_CPU);
  more_printf(" %s\n", CLI_KERNEL);
  more_printf(" %s\n", CLI_SYSLINUX);
  more_printf(" %s\n", CLI_VESA);
  if (hardware->sv->filesystem == SYSLINUX_FS_PXELINUX)
    more_printf(" %s\n", CLI_PXE);
}

void main_show(char *item, struct s_hardware *hardware)
{
  if (!strncmp(item, CLI_SUMMARY, sizeof(CLI_SUMMARY))) {
    main_show_summary(hardware);
    return;
  }
  if (!strncmp(item, CLI_PCI, sizeof(CLI_PCI))) {
    main_show_pci(hardware);
    return;
  }
  if (!strncmp(item, CLI_DMI, sizeof(CLI_DMI))) {
    main_show_dmi(hardware);
    return;
  }
  if (!strncmp(item, CLI_CPU, sizeof(CLI_CPU))) {
    main_show_cpu(hardware);
    return;
  }
  if (!strncmp(item, CLI_PXE, sizeof(CLI_PXE))) {
    main_show_pxe(hardware);
    return;
  }
  if (!strncmp(item, CLI_SYSLINUX, sizeof(CLI_SYSLINUX))) {
    main_show_syslinux(hardware);
    return;
  }
  if (!strncmp(item, CLI_KERNEL, sizeof(CLI_KERNEL))) {
    main_show_kernel(hardware);
    return;
  }
  if (!strncmp(item, CLI_VESA, sizeof(CLI_VESA))) {
    main_show_vesa(hardware);
    return;
  }
  show_main_help(hardware);
}
