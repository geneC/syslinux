#include <linux/list.h>
#include <sys/times.h>
#include <fcntl.h>
#include <stdbool.h>
#include <core.h>
#include <fs.h>
#include "cli.h"
#include "console.h"
#include "com32.h"
#include "menu.h"
#include "config.h"
#include "syslinux/adv.h"

#include <sys/module.h>

struct file_ext {
	const char *name;
	enum kernel_type type;
};

static const struct file_ext file_extensions[] = {
	{ ".com", KT_COMBOOT },
	{ ".cbt", KT_COMBOOT },
	{ ".c32", KT_COM32 },
	{ ".img", KT_FDIMAGE },
	{ ".bss", KT_BSS },
	{ ".bin", KT_BOOT },
	{ ".bs", KT_BOOT },
	{ ".0", KT_PXE },
	{ NULL, KT_NONE },
};

/*
 * Return a pointer to one byte after the last character of the
 * command.
 */
static inline const char *find_command(const char *str)
{
	const char *p;

	p = str;
	while (*p && !my_isspace(*p))
		p++;
	return p;
}

static enum kernel_type parse_kernel_type(char *kernel)
{
	const struct file_ext *ext;
	const char *p;
	int len;

	/* Find the end of the command */
	p = find_command(kernel);
	len = p - kernel;

	for (ext = file_extensions; ext->name; ext++) {
		int elen = strlen(ext->name);

		if (!strncmp(kernel + len - elen, ext->name, elen))
			return ext->type;
	}

	/* use KT_KERNEL as default */
	return KT_KERNEL;
}

/*
 * Returns the kernel name with file extension if one wasn't present.
 */
static const char *get_extension(const char *kernel)
{
	const struct file_ext *ext;
	const char *p;
	int len;

	/* Find the end of the command */
	p = find_command(kernel);
	len = p - kernel;

	for (ext = file_extensions; ext->name; ext++) {
		char *str;
		int elen = strlen(ext->name);
		int fd;

		str = malloc(len + elen + 1);

		strncpy(str, kernel, len);
		strncpy(str + len, ext->name, elen);
		str[len + elen] = '\0';

		fd = searchdir(str);
		free(str);

		if (fd >= 0)
			return ext->name;
	}

	return NULL;
}

static const char *apply_extension(const char *kernel, const char *ext)
{
	const char *p;
	char *k;
	int len = strlen(kernel);
	int elen = strlen(ext);

	k = malloc(len + elen + 1);
	if (!k)
		return NULL;

	p = find_command(kernel);

	len = p - kernel;

	/* Copy just the kernel name */
	memcpy(k, kernel, len);

	/* Append the extension */
	memcpy(k + len, ext, elen);

	/* Copy the rest of the command line */
	strcpy(k + len + elen, p);

	k[len + elen] = '\0';

	return k;
}

/*
 * Attempt to load a kernel after deciding what type of image it is.
 *
 * We only return from this function if something went wrong loading
 * the the kernel. If we return the caller should call enter_cmdline()
 * so that the user can help us out.
 */
static void load_kernel(const char *command_line)
{
	struct menu_entry *me;
	enum kernel_type type;
	const char *cmdline;
	const char *kernel;

	kernel = strdup(command_line);
	if (!kernel)
		goto bad_kernel;

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
	if (type == KT_KERNEL) {
		const char *ext;

		/*
		 * Automatically lookup the extension if one wasn't
		 * supplied by the user.
		 */
		ext = get_extension(kernel);
		if (ext) {
			const char *k;

			k = apply_extension(kernel, ext);
			if (!k)
				goto bad_kernel;

			free((void *)kernel);
			kernel = k;

			type = parse_kernel_type(kernel);
		}
	}

	execute(kernel, type);
	free((void *)kernel);

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

int main(int argc, char **argv)
{
	com32sys_t ireg, oreg;
	uint8_t *adv;
	int count = 0;
	char *config_argv[2] = { NULL, NULL };

	openconsole(&dev_rawcon_r, &dev_ansiserial_w);

	if (ConfigName[0])
		config_argv[0] = ConfigName;

	parse_configs(config_argv);

	__syslinux_init();
	adv = syslinux_getadv(ADV_BOOTONCE, &count);
	if (adv && count) {
		/*
		 * We apparently have a boot-once set; clear it and
		 * then execute the boot-once.
		 */
		uint8_t *src, *dst, *cmdline;
		int i;

		src = adv;
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
