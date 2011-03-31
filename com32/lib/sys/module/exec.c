/*
 * exec.c
 *
 *  Created on: Aug 14, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#include <sys/module.h>
#include <sys/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <setjmp.h>
#include <alloca.h>
#include <dprintf.h>

#define DBG_PRINT(fmt, args...) dprintf("[EXEC] " fmt, ##args)

static struct elf_module    *mod_root = NULL;
struct elf_module *__syslinux_current = NULL;

int exec_init(void)
{
	int res;

	res = modules_init();
	if (res != 0)
		return res;

	// Load the root module
	mod_root = module_alloc(EXEC_ROOT_NAME);

	if (mod_root == NULL)
		return -1;

	res = module_load_shallow(mod_root, 0);
	if (res != 0) {
		mod_root = NULL;
		return res;
	}

	return 0;
}

int get_module_type(struct elf_module *module)
{
	if(module->main_func) return EXEC_MODULE;
	else if(module->init_func) return LIB_MODULE;
	return UNKNOWN_MODULE;
}

int load_library(const char *name)
{
	int res;
	struct elf_module *module = module_alloc(name);

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
		if (res)
			DBG_PRINT("Initialization error! function returned: %d\n", res);
	} else {
		DBG_PRINT("No initialization function present.\n");
	}

	if (res != 0) {
		module_unload(module);
		return res;
	}

	return 0;
}

int unload_library(const char *name)
{
	int res;
	struct elf_module *module = module_find(name);

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

jmp_buf __process_exit_jmp;

#if 0
int spawnv(const char *name, const char **argv)
{
	int res, ret_val = 0;
	const char **arg;
	int argc;
	char **argp, **args;
	struct elf_module *previous;
	malloc_tag_t prev_mem_tag;

	struct elf_module *module = module_alloc(name);

	if (module == NULL)
		return -1;

	res = module_load(module);
	if (res != 0) {
		module_unload(module);
		return res;
	}

	if (module->main_func == NULL) {
		// We can't execute without a main function
		module_unload(module);
		return -1;
	}
	/*if (module->main_func != NULL) {
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
	}*/
	// Set up the process context
	previous = __syslinux_current;
	prev_mem_tag = __mem_get_tag_global();

	// Setup the new process context
	__syslinux_current = module;
	__mem_set_tag_global((malloc_tag_t)module);

	// Generate a new process copy of argv (on the stack)
	argc = 0;
	for (arg = argv; *arg; arg++)
		argc++;

	args = alloca((argc+1) * sizeof(char *));

	for (arg = argv, argp = args; *arg; arg++, argp++) {
		size_t l = strlen(*arg)+1;
		*argp = alloca(l);
		memcpy(*argp, *arg, l);
	}

	*args = NULL;

	// Execute the program
	ret_val = setjmp(module->u.x.process_exit);

	if (ret_val)
		ret_val--;		/* Valid range is 0-255 */
	else if (!module->main_func)
		ret_val = -1;
	else
		exit((*module->main_func)(argc, args)); /* Actually run! */

	// Clean up the allocation context
	__free_tagged(module);
	// Restore the allocation context
	__mem_set_tag_global(prev_mem_tag);
	// Restore the process context
	__syslinux_current = previous;

	res = module_unload(module);

	if (res != 0) {
		return res;
	}

	return ((unsigned int)ret_val & 0xFF);
}

int spawnl(const char *name, const char *arg, ...)
{
	/*
	 * NOTE: We assume the standard ABI specification for the i386
	 * architecture. This code may not work if used in other
	 * circumstances, including non-variadic functions, different
	 * architectures and calling conventions.
	 */
	return spawnv(name, &arg);
}
#endif

struct elf_module *cur_module;

int spawn_load(const char *name,const char **argv)
{
	int res, ret_val = 0;
	const char **arg;
	int argc;
	char **argp, **args;
	struct elf_module *previous;
	//malloc_tag_t prev_mem_tag;
	struct elf_module *module = module_alloc(name);
	struct elf_module *prev_module;

	int type;

	dprintf("enter: name = %s", name);

	if (module == NULL)
		return -1;

	if (!strcmp(cur_module->name, module->name)) {
		dprintf("We is running this module %s already!", module->name);

		/*
		 * If we're already running the module and it's of
		 * type EXEC_MODULE, then just return. We don't reload
		 * the module because that might cause us to re-run
		 * the init functions, which will cause us to run the
		 * MODULE_MAIN function, which will take control of
		 * this process.
		 *
		 * This can happen if some other EXEC_MODULE is
		 * resolving a symbol that is exported by the current
		 * EXEC_MODULE.
		 */
		if (get_module_type(module) == EXEC_MODULE)
			return 0;
	}

	res = module_load(module);
	if (res != 0) {
		module_unload(module);
		return res;
	}

	type = get_module_type(module);
	prev_module = cur_module;
	cur_module = module;

	dprintf("type = %d, prev = %s, cur = %s",
		type, prev_module->name, cur_module->name);

	if(type==LIB_MODULE)
	{
		if (module->init_func != NULL) {
			res = (*(module->init_func))();
			DBG_PRINT("Initialization function returned: %d\n", res);
		} else {
			DBG_PRINT("No initialization function present.\n");
		}

		if (res != 0) {
			cur_module = prev_module;
			module_unload(module);
			return res;
		}
		return 0;
	}
	else if(type==EXEC_MODULE)
	{
		previous = __syslinux_current;
		//prev_mem_tag = __mem_get_tag_global();

		// Setup the new process context
		__syslinux_current = module;
		//__mem_set_tag_global((malloc_tag_t)module);

		// Generate a new process copy of argv (on the stack)
		argc = 0;
		for (arg = argv; *arg; arg++)
			argc++;

		args = alloca((argc+1) * sizeof(char *));

		for (arg = argv, argp = args; *arg; arg++, argp++) {
			size_t l = strlen(*arg)+1;
			*argp = alloca(l);
			memcpy(*argp, *arg, l);
		}

		*args = NULL;

		// Execute the program
		ret_val = setjmp(module->u.x.process_exit);

		if (ret_val)
			ret_val--;		/* Valid range is 0-255 */
		else if (!module->main_func)
			ret_val = -1;
		else
			exit((*module->main_func)(argc, args)); /* Actually run! */


		// Clean up the allocation context
		//__free_tagged(module);
		// Restore the allocation context
		//__mem_set_tag_global(prev_mem_tag);
		// Restore the process context
		__syslinux_current = previous;

		cur_module = prev_module;
		res = module_unload(module);

		if (res != 0) {
			return res;
		}

		return ((unsigned int)ret_val & 0xFF);
	}
	/*
	module_unload(module);
	return -1;
	*/
}

/*
 * Avoid circular dependencies.
 *
 * It's possible that someone messed up the modules.dep file and that
 * it includes circular dependencies, so we need to take steps here to
 * avoid looping in module_load_dependencies() forever.
 *
 * We build a singly-linked list of modules that are in the middle of
 * being loaded. When they have completed loading their entry is
 * removed from this list in LIFO order (new entries are always added
 * to the head of the list).
 */
struct loading_dep {
	const char *name;
	struct module_dep *next;
};
static struct loading_dep *loading_deps;

/*
 * Remember that because we insert elements in a LIFO order we need to
 * start from the end of the list and work towards the front so that
 * we print the modules in the order in which we tried to load them.
 *
 * Yay for recursive function calls.
 */
static void print_loading_dep(struct loading_dep *dep)
{
	if (dep) {
		print_loading_dep(dep->next);
		printf("\t\t\"%s\"\n", dep->name);
	}
}

int module_load_dependencies(const char *name,const char *dep_file)
{
	FILE *d_file=fopen(dep_file,"r");
	char line[2048],aux[2048],temp_name[MODULE_NAME_SIZE],slbz[24];
	int i=0,j=0,res=0;
	struct loading_dep *dep;

	if(d_file==NULL)
	{
		DBG_PRINT("Could not open object file '%s'\n",dep_file);
		return -1;
	}

	/*
	 * Are we already in the middle of loading this module's
	 * dependencies?
	 */
	for (dep = loading_deps; dep; dep = dep->next) {
		if (!strcasecmp(dep->name, name))
			break;	/* found */
	}

	if (dep) {
		struct loading_dep *last, *prev;

		/* Dup! */
		printf("\t\tCircular depedency detected when loading "
		       "modules!\n");
		printf("\t\tModules dependency chain looks like this,\n\n");
		print_loading_dep(loading_deps);
		printf("\n\t\t... and we tried to load \"%s\" again\n", name);

		return -1;
	} else {
		dep = malloc(sizeof(*dep));
		if (!dep) {
			printf("Failed to alloc memory for loading_dep\n");
			return -1;
		}
		
		dep->name = name;
		dep->next = loading_deps;

		/* Insert at the head of the list */
		loading_deps = dep;
	}

	/* Note from feng:
	 * new modues.dep has line like this:
	 *	a.c32: b.32 c.c32 d.c32
	 * with desktop glibc
	 *	sscanf(line,"%[^:]: %[^\n]", temp_name, aux);
	 * works, which doesn't work here
	 */
	memset(temp_name, 0, sizeof(temp_name));
	memset(aux, 0, sizeof(aux));
	while (1) {
		if(fgets(line,2048,d_file)==NULL)
			break;

		//sscanf(line,"%s %[^\t\n]s",temp_name,aux);
		//sscanf(line,"%[^:]: %[^\n]", temp_name, aux);
		//sscanf(line,"%[^:]: %[^\n]\n", temp_name, aux);

		sscanf(line,"%[^:]:", temp_name);
		if (!strncmp(name, temp_name, strlen(name))) {
			/* The next 2 chars should be ':' and ' ' */
			i = strlen(temp_name);
			if (line[i] != ':' || line[i+1] != ' ')
				break;

			i +=2;
			j = 0;
			while (line[i] != '\n')
				aux[j++] = line[i++];
			aux[j] = '\0';
			//dprintf("found dependency: temp_name = %s, aux = %s, name = %s", temp_name, aux, name);
			break;
		}
	}
	fclose(d_file);

	/* Reuse temp_name for dependent module name buffer */
	memset(temp_name, 0, sizeof(temp_name));
	i = 0;
	while (aux[i]) {
		sscanf(aux + i, "%s", temp_name);
		//dprintf("load module: %s", temp_name);
		i += strlen(temp_name);
		i++;	/* skip a space */

		if (strlen(temp_name)) {
			char *argv[2] = { NULL, NULL };
			int ret;

			ret = module_load_dependencies(temp_name,
						       MODULES_DEP);
			if (!ret) {
				if (spawn_load(temp_name, argv) < 0)
					continue;
			}
		}
	}

	/* Remove our entry from the head of loading_deps */
	loading_deps = loading_deps->next;
	free(dep);

	return 0;
}

void exec_term(void)
{
	modules_term();
}
