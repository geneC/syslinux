/*
 * exec.c
 *
 *  Created on: Aug 14, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#include <sys/module.h>
#include <sys/exec.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>



#define DBG_PRINT(fmt, args...)	fprintf(stderr, "[EXEC] " fmt, ##args)


static struct elf_module    *mod_root = NULL;

static char *module_get_fullname(const char *name) {
	static char name_buff[MODULE_NAME_SIZE];

	if (name == NULL)
		return NULL;

	strcpy(name_buff, EXEC_DIRECTORY);
	strcat(name_buff, name);

	return name_buff;
}

int exec_init(size_t root_addr) {
	int res;

	res = modules_init();

	if (res != 0)
		return res;

	// Load the root module
	mod_root = module_alloc(module_get_fullname(EXEC_ROOT_NAME));

	if (mod_root == NULL)
		return -1;

	res = module_load_shallow(mod_root, root_addr);

	if (res != 0) {
		mod_root = NULL;
		return res;
	}

	return 0;
}

int load_library(const char *name) {
	int res;
	struct elf_module *module = module_alloc(module_get_fullname(name));

	if (module == NULL)
		return -1;

	res = module_load(module);


	if (res != 0) {
		module_unload(module);
		return res;
	}

	if (module->main_func != NULL) {
		DBG_PRINT("Cannot load executable module as library.\n");
		module_unload(module);
		return -1;
	}

	if (module->init_func != NULL) {
		res = (*(module->init_func))();
		DBG_PRINT("Initialization function returned: %d\n", res);
	} else {
		DBG_PRINT("No initialization function present.\n");
	}

	if (res != 0) {
		module_unload(module);
		return res;
	}

	return 0;
}

int unload_library(const char *name) {
	int res;
	struct elf_module *module = module_find(module_get_fullname(name));

	if (module == NULL)
		return -1;

	if (!module_unloadable(module)) {
		return -1;
	}

	if (module->exit_func != NULL) {
		(*(module->exit_func))();
	}

	res = module_unload(module);

	return res;
}

int spawnv(const char *name, const char **argv) {
	int res, ret_val = 0;

	struct elf_module *module = module_alloc(module_get_fullname(name));

	if (module == NULL)
		return -1;

	res = module_load(module);

	if (res != 0) {
		module_unload(module);
		return res;
	}

	if (module->main_func != NULL) {
		const char **last_arg = argv;
		void *old_tag;
		while (*last_arg != NULL)
			last_arg++;

		// Setup the memory allocation context
		old_tag = __mem_get_tag_global();
		__mem_set_tag_global(module);

		// Execute the program
		ret_val = (*(module->main_func))(last_arg - argv, argv);

		// Clean up the allocation context
		__free_tagged(module);
		// Restore the allocation context
		__mem_set_tag_global(old_tag);
	} else {
		// We can't execute without a main function
		module_unload(module);
		return -1;
	}

	res = module_unload(module);

	if (res != 0) {
		return res;
	}

	return ((unsigned int)ret_val & 0xFF);
}

int spawnl(const char *name, const char *arg, ...) {
	/*
	 * NOTE: We assume the standard ABI specification for the i386
	 * architecture. This code may not work if used in other
	 * circumstances, including non-variadic functions, different
	 * architectures and calling conventions.
	 */

	return spawnv(name, &arg);
}


void exec_term() {
	modules_term();
}
