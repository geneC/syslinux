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

#if __SIZEOF_POINTER__ == 4
typedef Elf32_Dyn	Elf_Dyn;
typedef Elf32_Word	Elf_Word;
typedef Elf32_Addr	Elf_Addr;
#define ELF_SYMENT_SIZE	sizeof(Elf32_Sym)
#define ELF_MOD_SYS	"32 bit"
#elif __SIZEOF_POINTER__ == 8
typedef Elf64_Dyn	Elf_Dyn;
typedef Elf64_Word	Elf_Word;
typedef Elf64_Addr	Elf_Addr;
#define ELF_SYMENT_SIZE	sizeof(Elf64_Sym)
#define ELF_MOD_SYS	"64 bit"
#else
#error "unsupported architecture"
#endif
typedef void (*constructor_t) (void);
constructor_t __ctors_start[], __ctors_end[];

extern char __dynstr_start[];
extern char __dynstr_end[], __dynsym_end[];
extern char __dynsym_start[];
extern char __got_start[];
extern Elf_Dyn __dynamic_start[];
extern Elf_Word __gnu_hash_start[];
extern char __module_start[];

struct elf_module core_module = {
    .name		= "(core)",
    .shallow		= true,
    .required		= LIST_HEAD_INIT((core_module.required)),
    .dependants		= LIST_HEAD_INIT((core_module.dependants)),
    .list		= LIST_HEAD_INIT((core_module.list)),
    .module_addr	= (void *)0x0,
    .ghash_table	= __gnu_hash_start,
    .str_table		= __dynstr_start,
    .sym_table		= __dynsym_start,
    .got		= __got_start,
    .dyn_table		= __dynamic_start,
    .syment_size	= ELF_SYMENT_SIZE,
};

/*
	Initializes the module subsystem by taking the core module ( shallow module ) and placing
	it on top of the modules_head_list. Since the core module is initialized when declared
	we technically don't need the exec_init() and module_load_shallow() procedures
*/
void init_module_subsystem(struct elf_module *module)
{
    list_add(&module->list, &modules_head);
}

/* call_constr: initializes sme things related */
static void call_constr(void)
{
	constructor_t *p;

	for (p = __ctors_start; p < __ctors_end; p++)
		(*p) ();
}

/* note to self: do _*NOT*_ use static key word on this function */
void load_env32(com32sys_t * regs)
{
	struct file_info *fp;
	int fd;
	char *argv[] = { LDLINUX, NULL };
	size_t size;

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

	dprintf("Starting %s elf module subsystem...\n", ELF_MOD_SYS);
	call_constr();

	size = (size_t)__dynstr_end - (size_t)__dynstr_start;
	core_module.strtable_size = size;
	size = (size_t)__dynsym_end - (size_t)__dynsym_start;
	core_module.symtable_size = size;
	core_module.base_addr = (Elf_Addr)__module_start;

	init_module_subsystem(&core_module);

	spawn_load(LDLINUX, 1, argv);

	/*
	 * If we failed to load LDLINUX it could be because our
	 * current working directory isn't the install directory. Try
	 * a bit harder to find LDLINUX. If search_config() succeeds
	 * in finding LDLINUX it will set the cwd.
	 */
	fd = opendev(&__file_dev, NULL, O_RDONLY);
	if (fd < 0)
		return;

	fp = &__file_info[fd];

	if (!search_config(&fp->i.fd, search_directories, filenames))
		spawn_load(LDLINUX, 1, argv);
}

int create_args_and_load(char *cmdline)
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
	 * argv[0] must be the command name.
	 */
	argv = alloca(argc * sizeof(char *));

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
