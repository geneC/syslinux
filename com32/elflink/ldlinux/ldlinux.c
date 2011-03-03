#include <linux/list.h>
#include <sys/times.h>
#include <stdbool.h>
#include <core-elf.h>
#include "cli.h"
#include "console.h"
#include "com32.h"
#include "menu.h"
#include "config.h"

#include <sys/module.h>

static void enter_cmdline(void)
{
	struct cli_command  *aux;
	char *cmdline;

	/* Enter endless command line prompt, should support "exit" */
	while (1) {
		cmdline = edit_cmdline("", 1, NULL, NULL);
		/* feng: give up the aux check here */
		//aux = list_entry(cli_history_head.next, typeof(*aux), list);
		//if (strcmp(aux->command, cmdline)) {
			process_command(cmdline, true);
		//}
	}
}

static int ldlinux_main(int argc, char **argv)
{
	openconsole(&dev_rawcon_r, &dev_ansiserial_w);

	/* Should never return */
	enter_cmdline();

	return 0;
}
MODULE_MAIN(ldlinux_main);
