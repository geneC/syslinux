#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <console.h>
#include <dprintf.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <setjmp.h>
#include <linux/list.h>
#include <netinet/in.h>
#include <sys/cpu.h>
#include <core.h>
#include <fcntl.h>
#include <sys/file.h>
#include <fs.h>
#include <ctype.h>
#include <alloca.h>

#include <sys/exec.h>
#include <sys/module.h>
#include "common.h"

#define LDLINUX	"ldlinux.c32"

extern char __dynstr_start[];
extern char __dynstr_len[], __dynsym_len[];
extern char __dynsym_start[];
extern char __got_start[];
extern Elf32_Dyn __dynamic_start[];
extern Elf32_Word __gnu_hash_start[];

struct elf_module core_module = {
    .name		= "(core)",
    .shallow		= true,
    .required		= LIST_HEAD_INIT((core_module.required)),
    .dependants		= LIST_HEAD_INIT((core_module.dependants)),
    .list		= LIST_HEAD_INIT((core_module.list)),
    .module_addr	= (void *)0x0,
    .base_addr		= (Elf32_Addr) 0x0,
    .ghash_table	= __gnu_hash_start,
    .str_table		= __dynstr_start,
    .sym_table		= __dynsym_start,
    .got		= __got_start,
    .dyn_table		= __dynamic_start,
    .strtable_size	= (size_t) __dynstr_len,
    .syment_size	= sizeof(Elf32_Sym),
    .symtable_size	= (size_t) __dynsym_len
};

/*
 * Initializes the module subsystem by taking the core module
 * (preinitialized shallow module) and placing it on top of the
 * modules_head_list.
 */
void init_module_subsystem(struct elf_module *module)
{
    list_add(&module->list, &modules_head);
}

__export int start_ldlinux(int argc, char **argv)
{
	int rv;

again:
	rv = spawn_load(LDLINUX, argc, argv);
	if (rv == EEXIST) {
		/*
		 * If a COM32 module calls execute() we may need to
		 * unload all the modules loaded since ldlinux.c32,
		 * and restart initialisation. This is especially
		 * important for config files.
		 *
		 * But before we do that, try our best to make sure
		 * that spawn_load() is gonna succeed, e.g. that we
		 * can find LDLINUX it in PATH.
		 */
		struct elf_module *ldlinux;
		FILE *f;

		f = findpath(LDLINUX);
		if (!f)
			return ENOENT;

		fclose(f);
		ldlinux = unload_modules_since(LDLINUX);

		/*
		 * Finally unload LDLINUX.
		 *
		 * We'll reload it when we jump to 'again' which will
		 * cause all the initialsation steps to be executed
		 * again.
		 */
		module_unload(ldlinux);
		goto again;
	}

	return rv;
}

/* note to self: do _*NOT*_ use static key word on this function */
void load_env32(com32sys_t * regs __unused)
{
	struct file_info *fp;
	int fd;
	char *argv[] = { LDLINUX, NULL };
	char realname[FILENAME_MAX];

	static const char *search_directories[] = {
		"/boot/isolinux",
		"/isolinux",
		"/boot/syslinux",
		"/syslinux",
		"/",
		NULL
	};

	static const char *filenames[] = {
		LDLINUX,
		NULL
	};

	dprintf("Starting 32 bit elf module subsystem...\n");

	PATH = malloc(strlen(CurrentDirName) + 1);
	if (!PATH) {
		printf("Couldn't allocate memory for PATH\n");
		goto out;
	}

	strcpy(PATH, CurrentDirName);

	init_module_subsystem(&core_module);

	start_ldlinux(1, argv);

	/*
	 * If we failed to load LDLINUX it could be because our
	 * current working directory isn't the install directory. Try
	 * a bit harder to find LDLINUX. If search_dirs() succeeds
	 * in finding LDLINUX it will set the cwd.
	 */
	fd = opendev(&__file_dev, NULL, O_RDONLY);
	if (fd < 0)
		goto out;

	fp = &__file_info[fd];

	if (!search_dirs(&fp->i.fd, search_directories, filenames, realname)) {
		char path[FILENAME_MAX];

		/*
		 * search_dirs() sets the current working directory if
		 * it successfully opens the file. Add the directory
		 * in which we found ldlinux.c32 to PATH.
		 */
		if (!core_getcwd(path, sizeof(path)))
			goto out;

		if (!strlen(PATH)) {
			PATH = realloc(PATH, strlen(path) + 1);
			if (!PATH) {
				printf("Couldn't allocate memory for PATH\n");
				goto out;
			}

			strcpy(PATH, path);
		} else {
			PATH = realloc(PATH, strlen(path) + strlen(PATH) + 2);
			if (!PATH) {
				printf("Couldn't allocate memory for PATH\n");
				goto out;
			}

			strcat(PATH, ":");
			strcat(PATH, path);
		}

		start_ldlinux(1, argv);
	}

out:
	free(PATH);
	writestr("\nFailed to load ldlinux.c32");
}

static const char *__cmdline;
__export const char *com32_cmdline(void)
{
	return __cmdline;
}

__export int create_args_and_load(char *cmdline)
{
	char *p, **argv;
	int argc;
	int i;

	if (!cmdline)
		return -1;

	for (argc = 0, p = cmdline; *p; argc++) {
		/* Find the end of this arg */
		while(*p && !isspace(*p))
			p++;

		/*
		 * Now skip all whitespace between arguments.
		 */
		while (*p && isspace(*p))
			p++;
	}

	/*
	 * Generate a copy of argv on the stack as this is
	 * traditionally where process arguments go.
	 *
	 * argv[0] must be the command name. Remember to allocate
	 * space for the sentinel NULL.
	 */
	argv = alloca((argc + 1) * sizeof(char *));

	for (i = 0, p = cmdline; i < argc; i++) {
		char *start;
		int len = 0;

		start = p;

		/* Find the end of this arg */
		while(*p && !isspace(*p)) {
			p++;
			len++;
		}

		argv[i] = malloc(len + 1);
		strncpy(argv[i], start, len);
		argv[i][len] = '\0';

		/*
		 * Now skip all whitespace between arguments.
		 */
		while (*p && isspace(*p))
			p++;

		/*
		 * Point __cmdline at "argv[1] ... argv[argc-1]"
		 */
		if (i == 0)
			__cmdline = p;
	}

	/* NUL-terminate */
	argv[argc] = NULL;

	return spawn_load(argv[0], argc, argv);
}

void pm_env32_run(com32sys_t *regs)
{
	char *cmdline;

	cmdline = MK_PTR(regs->es, regs->ebx.w[0]);
	if (create_args_and_load(cmdline) < 0)
		printf("Failed to run com32 module\n");
}
