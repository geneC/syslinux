#include <stdio.h>
#include <stdlib.h>
#include <console.h>

#include "elf_module.h"

#define KLIBC_NAME			"klibc.dyn"
#define ROOT_NAME			"_root_.dyn"

#define ELF_DIRECTORY		"/dyn/"

static struct elf_module    *mod_root;
static struct elf_module	*mod_klibc;

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

void modules_com32_finalize() {
	modules_term();
}

int main(int argc, char **argv) {
	int res;

	// Open a standard r/w console
	openconsole(&dev_stdcon_r, &dev_stdcon_w);

	// Initializing the module subsystem
	res = modules_com32_setup();

	// Load klibc
	mod_klibc = module_alloc(ELF_DIRECTORY KLIBC_NAME);
	module_load(mod_klibc);

	if (res != 0) {
		printf("ERROR: Could not fully initialize the module!\n");
		return res;
	}

	modules_com32_finalize();

	return 0;
}
