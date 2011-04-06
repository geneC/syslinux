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

/*
 * Attempt to load a kernel after deciding what type of image it is.
 *
 * We only return from this function if something went wrong loading
 * the the kernel. If we return the caller should call enter_cmdline()
 * so that the user can help us out.
 */
static void load_kernel(const char *kernel)
{
	struct menu_entry *me;
	enum kernel_type type;
	const char *cmdline;
	char *kernel_name;
	int len;

	/* Virtual kernel? */
	me = find_label(kernel);
	if (me) {
		/* XXX we don't handle LOCALBOOT yet */
		execute(me->cmdline, KT_KERNEL);
		/* We shouldn't return */
		goto bad_kernel;
	}

	if (!allowimplicit)
		goto bad_implicit;

	kernel_name = strtok((char *)kernel, COMMAND_DELIM);
	len = strlen(kernel_name);

	if (!strcmp(kernel_name + len - 4, ".c32")) {
		type = KT_COM32;
	} else if (!strcmp(kernel_name + len - 2, ".0")) {
		type = KT_PXE;
	} else if (!strcmp(kernel_name + len - 3, ".bs")) {
		type = KT_BOOT;
	} else if (!strcmp(kernel_name + len - 4, ".img")) {
		type = KT_FDIMAGE;
	} else if (!strcmp(kernel_name + len - 4, ".bin")) {
		type = KT_BOOT;
	} else if (!strcmp(kernel_name + len - 4, ".bss")) {
		type = KT_BSS;
	} else if (!strcmp(kernel_name + len - 4, ".com")
	       || !strcmp(kernel_name + len - 4, ".cbt")) {
		type = KT_COMBOOT;
	}
	/* use KT_KERNEL as default */
	else
		type = KT_KERNEL;

	execute(kernel, type);

bad_implicit:
bad_kernel:
	/*
	 * If we fail to boot the kernel execute the "onerror" command
	 * line.
	 */
	if (onerrorlen) {
		rsprintf(&cmdline, "%s %s", onerror, default_cmd);
		execute(cmdline, KT_COM32);
	}
}

static void enter_cmdline(void)
{
	const char *cmdline;

	/* Enter endless command line prompt, should support "exit" */
	while (1) {
		cmdline = edit_cmdline("syslinux$", 1, NULL, cat_help_file);
		if (!cmdline)
			continue;

		/* return if user only press enter */
		if (cmdline[0] == '\0') {
			printf("\n");
			continue;
		}
		printf("\n");

		load_kernel(cmdline);
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
	if (defaultlevel || noescape) {
		if (defaultlevel) {
			load_kernel(default_cmd); /* Shouldn't return */
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
