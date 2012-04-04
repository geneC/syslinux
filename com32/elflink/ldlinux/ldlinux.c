#include <linux/list.h>
#include <sys/times.h>
#include <stdbool.h>
#include <string.h>
#include <core.h>
#include "cli.h"
#include "console.h"
#include "com32.h"
#include "menu.h"
#include "config.h"
#include "syslinux/adv.h"

#include <sys/module.h>

static enum kernel_type parse_kernel_type(const char *kernel)
{
	enum kernel_type type;
	const char *p;
	int len;

	/* Find the end of the command */
	p = kernel;
	while (*p && !my_isspace(*p))
		p++;

	len = p - kernel;

	if (!strncmp(kernel + len - 4, ".c32", 4)) {
		type = KT_COM32;
	} else if (!strncmp(kernel + len - 2, ".0", 2)) {
		type = KT_PXE;
	} else if (!strncmp(kernel + len - 3, ".bs", 3)) {
		type = KT_BOOT;
	} else if (!strncmp(kernel + len - 4, ".img", 4)) {
		type = KT_FDIMAGE;
	} else if (!strncmp(kernel + len - 4, ".bin", 4)) {
		type = KT_BOOT;
	} else if (!strncmp(kernel + len - 4, ".bss", 4)) {
		type = KT_BSS;
	} else if (!strncmp(kernel + len - 4, ".com", 4) ||
		   !strncmp(kernel + len - 4, ".cbt", 4)) {
		type = KT_COMBOOT;
	}
	/* use KT_KERNEL as default */
	else
		type = KT_KERNEL;

	return type;
}

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

	/* Virtual kernel? */
	me = find_label(kernel);
	if (me) {
		type = parse_kernel_type(me->cmdline);

		/* cmdline contains type specifier */
		if (me->cmdline[0] == '.')
			type = KT_NONE;

		execute(me->cmdline, type);
		/* We shouldn't return */
		goto bad_kernel;
	}

	if (!allowimplicit)
		goto bad_implicit;

	type = parse_kernel_type(kernel);
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

int main(int argc __unused, char **argv __unused)
{
	const void *adv;
	size_t count = 0;
	char *config_argv[2] = { NULL, NULL };

	openconsole(&dev_rawcon_r, &dev_ansiserial_w);

	if (ConfigName[0])
		config_argv[0] = ConfigName;

	parse_configs(config_argv);

	adv = syslinux_getadv(ADV_BOOTONCE, &count);
	if (adv && count) {
		/*
		 * We apparently have a boot-once set; clear it and
		 * then execute the boot-once.
		 */
		const char *cmdline;
		char *src, *dst;
		size_t i;

		src = (char *)adv;
		cmdline = dst = malloc(count + 1);
		if (!dst) {
			printf("Failed to allocate memory for ADV\n");
			goto cmdline;
		}

		for (i = 0; i < count; i++)
			*dst++ = *src++;
		*dst = '\0';	/* Null-terminate */

		/* Clear the boot-once data from the ADV */
		if (!syslinux_setadv(ADV_BOOTONCE, 0, NULL))
			syslinux_adv_write();

		load_kernel(cmdline); /* Shouldn't return */
		goto cmdline;
	}

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
