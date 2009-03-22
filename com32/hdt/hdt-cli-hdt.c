/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer - All Rights Reserved
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslinux/config.h>

#include "hdt-cli.h"
#include "hdt-common.h"

/**
 * cli_clear_screen - clear (erase) the entire screen
 **/
static void cli_clear_screen(int argc __unused, char** argv __unused,
			     struct s_hardware *hardware __unused)
{
	clear_screen();
}

/**
 * cli_set_mode - set the mode of the cli, in the cli
 *
 * The mode number must be supplied in argv, position 0.
 **/
static void cli_set_mode(int argc, char **argv,
			 struct s_hardware *hardware)
{
	cli_mode_t new_mode;

	if (argc <= 0) {
		printf("Which mode?\n");
		return;
	}

	/*
	 * Note! argv[0] is a string representing the mode, we need the
	 * equivalent cli_mode_t to pass it to set_mode.
	 */
	new_mode = mode_s_to_mode_t(argv[0]);
	set_mode(new_mode, hardware);
}

/**
 * do_exit - shared helper to exit a mode
 **/
static void do_exit(int argc __unused, char** argv __unused,
		    struct s_hardware *hardware)
{
	int new_mode = HDT_MODE;

	switch (hdt_cli.mode) {
	case HDT_MODE:
		new_mode = EXIT_MODE;
		break;
	default:
		new_mode = HDT_MODE;
		break;
	}

	dprintf("CLI DEBUG: Switching from mode %d to mode %d\n", hdt_cli.mode,
								  new_mode);
	set_mode(new_mode, hardware);
}

/**
 * show_cli_help - shared helper to show available commands
 **/
static void show_cli_help(int argc __unused, char** argv __unused,
			  struct s_hardware *hardware __unused)
{
	int j;
	struct cli_mode_descr *current_mode;
	struct cli_callback_descr* associated_module = NULL;

	find_cli_mode_descr(hdt_cli.mode, &current_mode);

	printf("Available commands are:\n");

	/* List first default modules of the mode */
	if (current_mode->default_modules != NULL ) {
		for (j = 0; j < current_mode->default_modules->nb_modules; j++) {
			printf("%s ",
			       current_mode->default_modules->modules[j].name);
		}
	}

	/* List secondly the show modules of the mode */
	if (current_mode->show_modules != NULL ) {
		more_printf("show commands:\n");
		for (j = 0; j < current_mode->show_modules->nb_modules; j++) {
			printf("     %s\n",
			       current_mode->show_modules->modules[j].name);
		}
	}

	/* List finally the default modules of the hdt mode */
	if (current_mode->mode != hdt_mode.mode &&
	    hdt_mode.default_modules != NULL ) {
		for (j = 0; j < hdt_mode.default_modules->nb_modules; j++) {
			/*
			 * Any default command that is present in hdt mode but
			 * not in the current mode is available. A default
			 * command can be redefined in the current mode though.
			 * This next call test this use case: if it is
			 * overwritten, do not print it again.
			 */
			find_cli_callback_descr(hdt_mode.default_modules->modules[j].name,
					     current_mode->default_modules,
					     &associated_module);
			if (associated_module == NULL)
				printf("%s ",
				       hdt_mode.default_modules->modules[j].name);
		}
	}
	printf("\n");
}

/* Default hdt mode */
struct cli_callback_descr list_hdt_default_modules[] = {
	{
		.name = CLI_CLEAR,
		.exec = cli_clear_screen,
	},
	{
		.name = CLI_EXIT,
		.exec = do_exit,
	},
	{
		.name = CLI_HELP,
		.exec = show_cli_help,
	},
};

struct cli_callback_descr list_hdt_set_modules[] = {
	{
		.name = CLI_MODE,
		.exec = cli_set_mode,
	},
};

struct cli_module_descr hdt_default_modules = {
	.modules = list_hdt_default_modules,
	.nb_modules = 3,
};

struct cli_module_descr hdt_set_modules = {
	.modules = list_hdt_set_modules,
	.nb_modules = 1,
};

struct cli_mode_descr hdt_mode = {
	.mode = HDT_MODE,
	.name = CLI_HDT,
	.default_modules = &hdt_default_modules,
	.show_modules = NULL,
	.set_modules = &hdt_set_modules,
};
