#include <linux/list.h>
#include <sys/times.h>
#include <stdbool.h>
#include <core.h>
#include <core-elf.h>
#include "cli.h"
#include "console.h"
#include "com32.h"
#include "menu.h"
#include "config.h"

#include <sys/module.h>

static void enter_cmdline(void)
{
	const char *cmdline;

	/* Enter endless command line prompt, should support "exit" */
	while (1) {
		cmdline = edit_cmdline("syslinux$", 1, NULL, cat_help_file);
		if (!cmdline)
			continue;
		/* feng: give up the aux check here */
		//aux = list_entry(cli_history_head.next, typeof(*aux), list);
		//if (strcmp(aux->command, cmdline)) {
			process_command(cmdline, true);
		//}
	}
}

static void load_kernel(void)
{
	enum kernel_type type;
	const char *cmdline;

	if (defaultlevel == LEVEL_UI)
		type = KT_COM32;
	else
		type = KT_KERNEL;

	execute(default_cmd, type);

	/*
	 * If we fail to boot the kernel execute the "onerror" command
	 * line.
	 */
	if (onerrorlen) {
		rsprintf(&cmdline, "%s %s", onerror, default_cmd);
		execute(cmdline, KT_COM32);
	}
}

static int ldlinux_main(int argc, char **argv)
{
	openconsole(&dev_rawcon_r, &dev_ansiserial_w);

	parse_configs(NULL);

	/* TODO: ADV */
	/* TODO: Check KbdFlags? */

	if (forceprompt)
		goto cmdline;

	/*
	 * Auto boot
	 */
	if (defaultlevel || !noescape) {
		if (defaultlevel) {
			load_kernel();	/* Shouldn't return */
		} else {
			printf("No DEFAULT or UI configuration directive found!\n");

			if (noescape)
				kaboom();
		}
	}

cmdline:
	/* Should never return */
	enter_cmdline();

	return 0;
}
MODULE_MAIN(ldlinux_main);
