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
	fprintf(stderr, "\telftest objfile ...\n");
}

void test_hello() {
	int i;
	
	struct elf_module *module;
	Elf32_Sym *symbol;
	
	symbol = global_find_symbol("undef_func", &module);
	
	void (*undef_func)(int) = module_get_absolute(symbol->st_value, module);
	
	symbol = global_find_symbol("test_func", &module);
	
	int (*test_func)(void) = module_get_absolute(symbol->st_value, module); 
	
	undef_func(0);
	
	for (i=0; i < 10; i++) {
		printf("%d\n", test_func());
	}
}

int main(int argc, char **argv) {
	int res;
	int i;
	struct elf_module *module;
	const char *module_name = NULL;
	
	// Skip program name
	argc--;
	argv++;
	
	if (argc < 1) {
		print_usage();
		return 1;
	}
	
	res = modules_init();
		
	if (res < 0) {
		fprintf(stderr, "Could not initialize module subsystem\n");
		exit(1);
	}
	
	for (i=0; i < argc; i++){
		module_name = argv[i];
		
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
		
	}
	
	test_hello();
	
	for (i=argc-1; i >= 0; i--) {
		module_name = argv[i];
		module = module_find(module_name);
		
		res = module_unload(module);
		
		if (res < 0) {
			fprintf(stderr, "Could not unload the module\n");
			goto error;
		}
	}
	
	modules_term();
	
	return 0;
	
error:
	modules_term();
	
	return 1;
}
