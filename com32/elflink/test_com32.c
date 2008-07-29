#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include "elf_module.h"

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
		printf("Initialization function returned: %d\n", res);
	} else {
		printf("No initialization function present.\n");
	}

	return res;
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

	// Initializing the module subsystem
	res = modules_com32_setup();

	for (i = 0; i < argc; i++) {
		modules_com32_load(argv[i]);
	}

	modules_com32_finalize();

	return 0;
}
