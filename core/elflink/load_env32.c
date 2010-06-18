#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <console.h>
#include <com32.h>
#include <syslinux/adv.h>
#include <syslinux/config.h>
#include <setjmp.h>
#include <linux/list.h>
#include <netinet/in.h>
#include <sys/cpu.h>

#include <sys/exec.h>
#include <sys/module.h>
#include "common.h"
#include "menu.h"
#include "cli.h"
#include "core-elf.h"

typedef void (*constructor_t) (void);
constructor_t __ctors_start[], __ctors_end[];

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

void enter_cmdline(void)
{
	struct cli_command  *comm, *aux;
	char *cmdline;

	/* Enter endless command line prompt, should support "exit" */
	while (1) {
		cmdline = edit_cmdline("", 1, NULL, NULL);
		/* feng: give up the aux check here */
		//aux = list_entry(cli_history_head.next, typeof(*aux), list);
		//if (strcmp(aux->command, cmdline)) {
			comm = (struct cli_command *)malloc(sizeof(struct cli_command *));
			comm->command =
				(char *)malloc(sizeof(char) * (strlen(cmdline) + 1));
			strcpy(comm->command, cmdline);
			list_add(&(comm->list), &cli_history_head);
			process_command(cmdline);
		//}
	}
}

/* parameter is the config file name if any */
void start_ui(char *config_file)
{
	char *cmdline;
	char *argv[2] = {config_file, NULL};

	mp("enter, config file = %s", config_file);

	parse_configs(argv);
	/* run the default menu if found */
	/*
	if (default_menu) {
		cmdline = default_menu->menu_entries[default_menu->defentry]->cmdline;
		if (*cmdline == '.') {
			while (*cmdline++ != ' ');
		}
		process_command(cmdline);
	}
	*/

	/* try to run a default linux kernel */
	/*
	if (append || globaldefault)
		new_linux_kernel(NULL, NULL);
	*/

	/* Should never return */
	enter_cmdline();
}

/* note to self: do _*NOT*_ use static key word on this function */
void load_env32(com32sys_t * regs)
{
	printf("Starting 32 bit elf module subsystem...\n");
	call_constr();
	openconsole(&dev_rawcon_r, &dev_ansiserial_w);
	INIT_LIST_HEAD(&cli_history_head);

	init_module_subsystem(&core_module);

	start_ui(NULL);
}
