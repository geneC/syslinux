#ifndef ELF_MODULE_H_
#define ELF_MODULE_H_

#include <elf.h>
#include <stdint.h>
#include "linux_list.h"

#define MODULE_NAME_SIZE		64

#define MODULE_ELF_CLASS		ELFCLASS32
#define MODULE_ELF_DATA			ELFDATA2LSB
#define MODULE_ELF_VERSION		EV_CURRENT
#define MODULE_ELF_TYPE			ET_DYN
#define MODULE_ELF_MACHINE		EM_386


typedef int (*module_init_func)();
typedef void (*module_exit_func)();

// Structure encapsulating a module loaded in memory
struct elf_module {
	char				name[MODULE_NAME_SIZE]; 		// The module name
	
	struct list_head	deps;		// Head of module dependency list
	struct list_head	list;		// The list entry in the module list
	
	module_init_func	init_func;	// The initialization entry point
	module_exit_func	exit_func;	// The module finalization code
	
	void				*file_image; // The image of the module file in memory
	uint32_t			file_size; // The size of the module file
	
	void				*module_addr; // The module location in the memory
	Elf32_Addr			base_addr;	// The base address of the module
	Elf32_Word			module_size; // The module size in memory
	
	// Information for modules loaded in user space
#ifdef ELF_USERSPACE_TEST
	int					file_fd;	// The file descriptor of the open file
#endif
};

// Structure encapsulating a module dependency need
struct module_dep {
	struct list_head	list;		// The list entry in the dependency list
	
	char				name[MODULE_NAME_SIZE];		// The name of the module
};

// Initialization of the module subsystem
extern int modules_init();
// Termination of the module subsystem
extern void modules_term();

// Allocates the structure for a new module
extern struct elf_module *module_alloc(const char *name);

// Loads the module into the system
extern int module_load(struct elf_module *module);

// Unloads the module from the system and releases all the associated memory
extern int module_unload(struct elf_module *module);

#endif /*ELF_MODULE_H_*/
