#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "elf_module.h"

void print_usage() {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\telftest objfile\n");
}

int main(int argc, char **argv) {
	int res;
	struct elf_module *module;
	const char *module_name = NULL;
	
	// Skip program name
	argc--;
	argv++;
	
	if (argc != 1) {
		print_usage();
		return 1;
	}
	
	module_name = argv[0];
	
	res = modules_init();
	
	if (res < 0) {
		fprintf(stderr, "Could not initialize module subsystem\n");
		exit(1);
	}
	
	module = module_alloc(module_name);
	
	if (module == NULL) {
		fprintf(stderr, "Could not allocate the module\n");
		goto error;
	}
	
	res = module_load(module);
	
	if (res < 0) {
		fprintf(stderr, "Could not load the module\n");
		goto error;
	}
	
	module_unload(module);
	
	modules_term();
	
	return 0;
	
error:
	modules_term();
	
	return 1;
}
