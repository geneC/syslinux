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
#include <getkey.h>
#include "hdt-cli.h"
#include "hdt-common.h"

#define MAX_MODES 1
struct commands_mode *list_modes[] = {
	&dmi_mode,
};

static void set_mode(struct s_cli *cli, cli_mode_t mode,
		     struct s_hardware *hardware)
{
	switch (mode) {
	case EXIT_MODE:
		cli->mode = mode;
		break;

	case HDT_MODE:
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_HDT);
		break;

	case PXE_MODE:
		if (hardware->sv->filesystem != SYSLINUX_FS_PXELINUX) {
			more_printf("You are not currently using PXELINUX\n");
			break;
		}
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_PXE);
		break;

	case KERNEL_MODE:
		detect_pci(hardware);
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_KERNEL);
		break;

	case SYSLINUX_MODE:
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ",
			 CLI_SYSLINUX);
		break;

	case VESA_MODE:
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_VESA);
		break;

	case PCI_MODE:
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_PCI);
		if (!hardware->pci_detection)
			cli_detect_pci(hardware);
		break;

	case CPU_MODE:
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_CPU);
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
		cli->mode = mode;
		snprintf(cli->prompt, sizeof(cli->prompt), "%s> ", CLI_DMI);
		break;
	}
}

static void handle_hdt_commands(char *cli_line, struct s_hardware *hardware)
{
	/* hdt cli mode specific commands */
	if (!strncmp(cli_line, CLI_SHOW, sizeof(CLI_SHOW) - 1)) {
		main_show(strstr(cli_line, "show") + sizeof(CLI_SHOW),
			  hardware);
		return;
	}
}

static void show_cli_help(struct s_cli *cli)
{
	switch (cli->mode) {
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
	case EXIT_MODE:	/* Should not happen */
		break;
	}
}

static void exec_command(char *command, struct s_cli *cli,
			 struct s_hardware *hardware)
{
	/* We use sizeof BLAH - 1 to remove the last \0 */
//  command[strlen(command) - 1] = '\0';

	if (!strncmp(command, CLI_EXIT, sizeof(CLI_EXIT) - 1)) {
		int mode = do_exit(cli);
		set_mode(cli, mode, hardware);
		return;
	}

	if (!strncmp(command, CLI_HELP, sizeof(CLI_HELP) - 1)) {
		show_cli_help(cli);
		return;
	}

	if (!strncmp(command, CLI_PCI, sizeof(CLI_PCI) - 1)) {
		set_mode(cli, PCI_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_CLEAR, sizeof(CLI_CLEAR) - 1)) {
		clear_screen();
		return;
	}

	if (!strncmp(command, CLI_CPU, sizeof(CLI_CPU) - 1)) {
		set_mode(cli, CPU_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_DMI, sizeof(CLI_DMI) - 1)) {
		set_mode(cli, DMI_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_PXE, sizeof(CLI_PXE) - 1)) {
		set_mode(cli, PXE_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_KERNEL, sizeof(CLI_KERNEL) - 1)) {
		set_mode(cli, KERNEL_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_SYSLINUX, sizeof(CLI_SYSLINUX) - 1)) {
		set_mode(cli, SYSLINUX_MODE, hardware);
		return;
	}

	if (!strncmp(command, CLI_VESA, sizeof(CLI_VESA) - 1)) {
		set_mode(cli, VESA_MODE, hardware);
		return;
	}

	/*
	 * All commands before that line are common for all cli modes.
	 * The following will be specific for every mode.
	 */

	int modes_iter = 0, modules_iter = 0;

	/* Find the mode selected */
	while (modes_iter < MAX_MODES &&
	       list_modes[modes_iter]->mode != cli->mode)
		modes_iter++;

	if (modes_iter != MAX_MODES) {
		struct commands_mode *current_mode = list_modes[modes_iter];

		/*
		 * Find the type of command.
		 *
		 * The syntax of the cli is the following:
		 *    <type of command> <module on which to operate> <args>
		 * e.g.
		 *    dmi> show system
		 *    dmi> show bank 1
		 *    dmi> show memory 0 1
		 *    pci> show device 12
		 */
		if (!strncmp(command, CLI_SHOW, sizeof(CLI_SHOW) - 1)) {
			int module_len = 0, args_len = 0;
			int argc = 0, args_iter = 0, argc_iter = 0;
			char *module = NULL, *args = NULL, *args_cpy = NULL;
			char **argv = NULL;

			/* Get the module name and args */
			while (strncmp
			       (command + sizeof(CLI_SHOW) + module_len,
				CLI_SPACE, 1))
				module_len++;

			/* cli_line is filled with \0 when initialized */
			while (strncmp
			       (command + sizeof(CLI_SHOW) + module_len + 1 +
				args_len, "\0", 1))
				args_len++;

			module = malloc(module_len + 1);
			strncpy(module, command + sizeof(CLI_SHOW), module_len);
			module[module_len] = '\0';

			/* Skip arguments handling if none is supplied */
			if (!args_len)
				goto find_callback;

			args = malloc(args_len + 1);
			strncpy(args,
				command + sizeof(CLI_SHOW) + module_len + 1,
				args_len);
			args[args_len] = '\0';

			/* Compute the number of arguments */
			args_cpy = args;
 read_argument:
			args_iter = 0;
			while (args_iter < args_len
			       && strncmp(args_cpy + args_iter, CLI_SPACE, 1))
				args_iter++;
			argc++;
			args_iter++;
			args_cpy += args_iter;
			args_len -= args_iter;
			if (args_len > 0)
				goto read_argument;

			/* Transform the arguments string into an array */
			char *result = NULL;
			argv = malloc(argc * sizeof(char *));
			result = strtok(args, CLI_SPACE);
			while (result != NULL) {
				argv[argc_iter] = result;
				argc_iter++;
				result = strtok(NULL, CLI_SPACE);
			}

 find_callback:
			/* Find the callback to execute */
			while (modules_iter <
			       current_mode->show_modules->nb_modules
			       && strncmp(module,
					  current_mode->show_modules->
					  modules[modules_iter].name,
					  module_len + 1) != 0)
				modules_iter++;

			if (modules_iter !=
			    current_mode->show_modules->nb_modules) {
				struct commands_module current_module =
				    current_mode->show_modules->
				    modules[modules_iter];
				/* Execute the callback */
				current_module.exec(argc, argv, hardware);
			} else
				printf("Module %s unknown.\n", module);
			/* XXX Add a default help option for empty commands */

			free(module);
			if (args_len) {
				free(args);
				free(argv);
			}
		}
		/* Handle here other keywords such as 'set', ... */
	} else
		printf("Mode '%s' unknown.\n", command);

	/* Legacy cli */
	switch (cli->mode) {
	case PCI_MODE:
		handle_pci_commands(command, hardware);
		break;
	case HDT_MODE:
		handle_hdt_commands(command, hardware);
		break;
	case CPU_MODE:
		handle_cpu_commands(command, hardware);
		break;
	case PXE_MODE:
		handle_pxe_commands(command, hardware);
		break;
	case VESA_MODE:
		handle_vesa_commands(command, hardware);
		break;
	case SYSLINUX_MODE:
		handle_syslinux_commands(command, hardware);
		break;
	case KERNEL_MODE:
		handle_kernel_commands(command, hardware);
		break;
	case EXIT_MODE:
		break;		/* should not happen */
	}
}

static void reset_prompt(struct s_cli *cli)
{
	/* No need to display the prompt if we exit */
	if (cli->mode != EXIT_MODE) {
		printf("%s", cli->prompt);
		/* Reset the line */
		memset(cli->input, '\0', MAX_LINE_SIZE);
		cli->cursor_pos = 0;
	}
}

/* Code that manages the cli mode */
void start_cli_mode(struct s_hardware *hardware)
{
	struct s_cli cli;
	int current_key = 0;
	char temp_command[MAX_LINE_SIZE];
	set_mode(&cli, HDT_MODE, hardware);

	printf("Entering CLI mode\n");

	/* Display the cursor */
	fputs("\033[?25h", stdout);
	reset_prompt(&cli);
	while (cli.mode != EXIT_MODE) {

		//fgets(cli_line, sizeof cli_line, stdin);
		current_key = get_key(stdin, 0);
		switch (current_key) {

			/* clear until then end of line */
		case KEY_CTRL('k'):
			/* Clear the end of the line */
			fputs("\033[0K", stdout);
			memset(&cli.input[cli.cursor_pos], 0,
			       strlen(cli.input) - cli.cursor_pos);
			break;

			/* quit current line */
		case KEY_CTRL('c'):
			more_printf("\n");
			reset_prompt(&cli);
			break;
		case KEY_TAB:
			break;

			/* Let's move to left */
		case KEY_LEFT:
			if (cli.cursor_pos > 0) {
				fputs("\033[1D", stdout);
				cli.cursor_pos--;
			}
			break;

			/* Let's move to right */
		case KEY_RIGHT:
			if (cli.cursor_pos < (int)strlen(cli.input)) {
				fputs("\033[1C", stdout);
				cli.cursor_pos++;
			}
			break;

			/* Returning at the begining of the line */
		case KEY_CTRL('e'):
		case KEY_END:
			/* Calling with a 0 value will make the cursor move */
			/* So, let's move the cursor only if needed */
			if ((strlen(cli.input) - cli.cursor_pos) > 0) {
				memset(temp_command, 0, sizeof(temp_command));
				sprintf(temp_command, "\033[%dC",
					strlen(cli.input) - cli.cursor_pos);
				/* Return to the begining of line */
				fputs(temp_command, stdout);
				cli.cursor_pos = strlen(cli.input);
			}
			break;

			/* Returning at the begining of the line */
		case KEY_CTRL('a'):
		case KEY_HOME:
			/* Calling with a 0 value will make the cursor move */
			/* So, let's move the cursor only if needed */
			if (cli.cursor_pos > 0) {
				memset(temp_command, 0, sizeof(temp_command));
				sprintf(temp_command, "\033[%dD",
					cli.cursor_pos);
				/* Return to the begining of line */
				fputs(temp_command, stdout);
				cli.cursor_pos = 0;
			}
			break;

		case KEY_ENTER:
			more_printf("\n");
			exec_command(skipspace(cli.input), &cli, hardware);
			reset_prompt(&cli);
			break;
		case KEY_BACKSPACE:
			/* Don't delete prompt */
			if (cli.cursor_pos == 0)
				break;

			for (int c = cli.cursor_pos - 1;
			     c < (int)strlen(cli.input) - 1; c++)
				cli.input[c] = cli.input[c + 1];
			cli.input[strlen(cli.input) - 1] = '\0';

			/* Get one char back */
			fputs("\033[1D", stdout);
			/* Clear the end of the line */
			fputs("\033[0K", stdout);

			/* Print the resulting buffer */
			printf("%s", cli.input + cli.cursor_pos - 1);

			/* Realing to the place we were */
			memset(temp_command, 0, sizeof(temp_command));
			sprintf(temp_command, "\033[%dD",
				strlen(cli.input + cli.cursor_pos - 1));
			fputs(temp_command, stdout);
			fputs("\033[1C", stdout);
			/* Don't decrement the position unless
			 * if we are at then end of the line*/
			if (cli.cursor_pos > (int)strlen(cli.input))
				cli.cursor_pos--;
			break;
		case KEY_F1:
			more_printf("\n");
			exec_command(CLI_HELP, &cli, hardware);
			reset_prompt(&cli);
			break;
		default:
			/* Prevent overflow */
			if (cli.cursor_pos > MAX_LINE_SIZE - 2)
				break;
			/* If we aren't at the end of the input line, let's insert */
			if (cli.cursor_pos < (int)strlen(cli.input)) {
				char key[2];
				int trailing_chars =
				    strlen(cli.input) - cli.cursor_pos;
				memset(temp_command, 0, sizeof(temp_command));
				strncpy(temp_command, cli.input,
					cli.cursor_pos);
				sprintf(key, "%c", current_key);
				strncat(temp_command, key, 1);
				strncat(temp_command,
					cli.input + cli.cursor_pos,
					trailing_chars);
				memset(cli.input, 0, sizeof(cli.input));
				snprintf(cli.input, sizeof(cli.input), "%s",
					 temp_command);

				/* Clear the end of the line */
				fputs("\033[0K", stdout);

				/* Print the resulting buffer */
				printf("%s", cli.input + cli.cursor_pos);
				sprintf(temp_command, "\033[%dD",
					trailing_chars);
				/* Return where we must put the new char */
				fputs(temp_command, stdout);

			} else {
				putchar(current_key);
				cli.input[cli.cursor_pos] = current_key;
			}
			cli.cursor_pos++;
			break;
		}
	}
}

int do_exit(struct s_cli *cli)
{
	switch (cli->mode) {
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
		return EXIT_MODE;	/* should not happen */
	}
	return HDT_MODE;
}

static void main_show_summary(struct s_hardware *hardware)
{
	detect_pci(hardware);	/* pxe is detected in the pci */
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

		int argc = 2;
		char *argv[2] = { "0", "0" };
		show_dmi_memory_modules(argc, argv, hardware);
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
