#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include <sys/module.h>

#define INFO_PRINT(fmt, args...)	printf("[COM32] " fmt, ##args)

#define KLIBC_NAME			"klibc.dyn"
#define ROOT_NAME			"_root_.dyn"

#define ELF_DIRECTORY		"/dyn/"

static struct elf_module    *mod_root;

int modules_com32_setup() {
	int res;

	res = modules_init();

	if (res != 0)
		return res;

	////////////////////////////////////////
	// Load the root module

	// Create its associated structure
	mod_root = module_alloc(ELF_DIRECTORY ROOT_NAME);

	if (mod_root == NULL) {
		return -1;
	}

	res = module_load_shallow(mod_root);

	if (res != 0) {
		return res;
	}

	return 0;
}

int modules_com32_load(char *name) {
	char full_name[MODULE_NAME_SIZE];
	int res;

	strcpy(full_name, ELF_DIRECTORY);
	strcat(full_name, name);

	struct elf_module *module = module_alloc(full_name);

	res = module_load(module);

	if (res != 0)
		return res;

	if (*(module->init_func) != NULL) {
		res = (*(module->init_func))();
		INFO_PRINT("Initialization function returned: %d\n", res);
	} else {
		INFO_PRINT("No initialization function present.\n");
	}

	return res;
}

void modules_com32_unload(char *name) {
	char full_name[MODULE_NAME_SIZE];

	strcpy(full_name, ELF_DIRECTORY);
	strcat(full_name, name);

	struct elf_module *module = module_find(full_name);

	if (module != NULL) {
		if (*(module->exit_func) != NULL) {
			(*(module->exit_func))();
		}
		module_unload(module);
	} else {
		INFO_PRINT("Module %s is not loaded\n", full_name);
	}
}

void print_usage() {
	printf("Usage: test_com32 module ...\n");
	printf("Where:\n");
	printf("\tmodule\tThe name of an ELF module to load, eg. hello.dyn\n");
	printf("\n");
}

void modules_com32_finalize() {
	modules_term();
}

int main(int argc, char **argv) {
	int res, i;

	// Open a standard r/w console
	openconsole(&dev_stdcon_r, &dev_stdcon_w);

	argc--;
	argv++;

	if (argc == 0) {
		print_usage();
		return 1;
	}

	INFO_PRINT("Setting up the module subsystem...\n");

	// Initializing the module subsystem
	res = modules_com32_setup();

	INFO_PRINT("Loading all the specified modules...\n");

	// Load the modules...
	for (i = 0; i < argc; i++) {
		modules_com32_load(argv[i]);
	}

	INFO_PRINT("Unloading all the specified modules...\n");

	// ...then unload them
	for (i = argc-1; i >= 0; i--) {
		modules_com32_unload(argv[i]);
	}

	modules_com32_finalize();

	return 0;
}
