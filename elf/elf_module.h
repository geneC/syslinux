#ifndef ELF_MODULE_H_
#define ELF_MODULE_H_

#include <stdio.h>
#include <elf.h>
#include <stdint.h>
#include "linux_list.h"

#define MODULE_NAME_SIZE		64

#define MODULE_ELF_CLASS		ELFCLASS32
#define MODULE_ELF_CLASS_SIZE	32
#define MODULE_ELF_DATA			ELFDATA2LSB
#define MODULE_ELF_VERSION		EV_CURRENT
#define MODULE_ELF_TYPE			ET_DYN
#define MODULE_ELF_MACHINE		EM_386


typedef int (*module_init_func)();
typedef void (*module_exit_func)();

// Structure encapsulating a module loaded in memory
struct elf_module {
	char				name[MODULE_NAME_SIZE]; 		// The module name
	
	struct list_head	required;		// Head of the required modules list
	struct list_head	dependants;		// Head of module dependants list
	struct list_head	list;		// The list entry in the module list
	
	module_init_func	init_func;	// The initialization entry point
	module_exit_func	exit_func;	// The module finalization code
	
	
	void				*module_addr; // The module location in the memory
	Elf32_Addr			base_addr;	// The base address of the module
	Elf32_Word			module_size; // The module size in memory
	
	Elf32_Word			*hash_table;	// The symbol hash table
	Elf32_Word			*ghash_table;	// The GNU style hash table
	char				*str_table;		// The string table
	void 				*sym_table;		// The symbol table
	void				*got;			// The Global Offset Table
	Elf32_Dyn			*dyn_table;		// Dynamic loading information table
	
	Elf32_Word			strtable_size;	// The size of the string table
	Elf32_Word			syment_size;	// The size of a symbol entry
	
	
	// Transient - Data available while the module is loading
	FILE				*_file;	// The file object of the open file
	Elf32_Off			_cr_offset; // The current offset in the open file
};

// Structure encapsulating a module dependency need
struct module_dep {
	struct list_head	list;		// The list entry in the dependency list
	
	struct elf_module	*module;
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

extern struct elf_module *module_find(const char *name);

extern Elf32_Sym *module_find_symbol(const char *name, struct elf_module *module);
extern Elf32_Sym *global_find_symbol(const char *name, struct elf_module **module);

static inline void *module_get_absolute(Elf32_Addr addr, struct elf_module *module) {
	return (void*)(module->base_addr + addr);
}

#endif /*ELF_MODULE_H_*/
