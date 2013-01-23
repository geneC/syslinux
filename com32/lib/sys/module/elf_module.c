/*
 * elf_module.c
 *
 *  Created on: Aug 11, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>
#include <dprintf.h>
#include <core.h>

#include <linux/list.h>
#include <sys/module.h>
#include <sys/exec.h>

#include "elfutils.h"
#include "common.h"

static int check_header(Elf32_Ehdr *elf_hdr) {
	int res;

	res = check_header_common(elf_hdr);

	if (res != 0)
		return res;

	if (elf_hdr->e_type != MODULE_ELF_TYPE) {
		DBG_PRINT("The ELF file must be a shared object\n");
		return -1;
	}

	if (elf_hdr->e_phoff == 0x00000000) {
		DBG_PRINT("PHT missing\n");
		return -1;
	}

	return 0;
}

/*
 *
 * The implementation assumes that the loadable segments are present
 * in the PHT sorted by their offsets, so that only forward seeks would
 * be necessary.
 */
static int load_segments(struct elf_module *module, Elf32_Ehdr *elf_hdr) {
	int i;
	int res = 0;
	char *pht = NULL;
	char *sht = NULL;
	Elf32_Phdr *cr_pht;
	Elf32_Shdr *cr_sht;

	Elf32_Addr min_addr  = 0x00000000; // Min. ELF vaddr
	Elf32_Addr max_addr  = 0x00000000; // Max. ELF vaddr
	Elf32_Word max_align = sizeof(void*); // Min. align of posix_memalign()
	Elf32_Addr min_alloc, max_alloc;   // Min. and max. aligned allocables

	Elf32_Addr dyn_addr = 0x00000000;

	// Get to the PHT
	image_seek(elf_hdr->e_phoff, module);

	// Load the PHT
	pht = malloc(elf_hdr->e_phnum * elf_hdr->e_phentsize);
	if (!pht)
		return -1;

	image_read(pht, elf_hdr->e_phnum * elf_hdr->e_phentsize, module);

	// Compute the memory needings of the module
	for (i=0; i < elf_hdr->e_phnum; i++) {
		cr_pht = (Elf32_Phdr*)(pht + i * elf_hdr->e_phentsize);

		switch (cr_pht->p_type) {
		case PT_LOAD:
			if (i == 0) {
				min_addr = cr_pht->p_vaddr;
			} else {
				min_addr = MIN(min_addr, cr_pht->p_vaddr);
			}

			max_addr = MAX(max_addr, cr_pht->p_vaddr + cr_pht->p_memsz);
			max_align = MAX(max_align, cr_pht->p_align);
			break;
		case PT_DYNAMIC:
			dyn_addr = cr_pht->p_vaddr;
			break;
		default:
			// Unsupported - ignore
			break;
		}
	}

	if (max_addr - min_addr == 0) {
		// No loadable segments
		DBG_PRINT("No loadable segments found\n");
		goto out;
	}

	if (dyn_addr == 0) {
		DBG_PRINT("No dynamic information segment found\n");
		goto out;
	}

	// The minimum address that should be allocated
	min_alloc = min_addr - (min_addr % max_align);

	// The maximum address that should be allocated
	max_alloc = max_addr - (max_addr % max_align);
	if (max_addr % max_align > 0)
		max_alloc += max_align;


	if (elf_malloc(&module->module_addr,
			max_align,
			max_alloc-min_alloc) != 0) {

		DBG_PRINT("Could not allocate segments\n");
		goto out;
	}

	module->base_addr = (Elf32_Addr)(module->module_addr) - min_alloc;
	module->module_size = max_alloc - min_alloc;

	// Zero-initialize the memory
	memset(module->module_addr, 0, module->module_size);

	for (i = 0; i < elf_hdr->e_phnum; i++) {
		cr_pht = (Elf32_Phdr*)(pht + i * elf_hdr->e_phentsize);

		if (cr_pht->p_type == PT_LOAD) {
			// Copy the segment at its destination
			if (cr_pht->p_offset < module->u.l._cr_offset) {
				// The segment contains data before the current offset
				// It can be discarded without worry - it would contain only
				// headers
				Elf32_Off aux_off = module->u.l._cr_offset - cr_pht->p_offset;

				if (image_read((char *)module_get_absolute(cr_pht->p_vaddr, module) + aux_off,
					       cr_pht->p_filesz - aux_off, module) < 0) {
					res = -1;
					goto out;
				}
			} else {
				if (image_seek(cr_pht->p_offset, module) < 0) {
					res = -1;
					goto out;
				}

				if (image_read(module_get_absolute(cr_pht->p_vaddr, module),
						cr_pht->p_filesz, module) < 0) {
					res = -1;
					goto out;
				}
			}

			/*
			DBG_PRINT("Loadable segment of size 0x%08x copied from vaddr 0x%08x at 0x%08x\n",
					cr_pht->p_filesz,
					cr_pht->p_vaddr,
					(Elf32_Addr)module_get_absolute(cr_pht->p_vaddr, module));
			*/
		}
	}

	// Get to the SHT
	image_seek(elf_hdr->e_shoff, module);

	// Load the SHT
	sht = malloc(elf_hdr->e_shnum * elf_hdr->e_shentsize);
	if (!sht) {
		res = -1;
		goto out;
	}

	image_read(sht, elf_hdr->e_shnum * elf_hdr->e_shentsize, module);

	// Setup the symtable size
	for (i = 0; i < elf_hdr->e_shnum; i++) {
		cr_sht = (Elf32_Shdr*)(sht + i * elf_hdr->e_shentsize);

		if (cr_sht->sh_type == SHT_DYNSYM) {
			module->symtable_size = cr_sht->sh_size;
			break;
		}
	}

	free(sht);

	// Setup dynamic segment location
	module->dyn_table = module_get_absolute(dyn_addr, module);

	/*
	DBG_PRINT("Base address: 0x%08x, aligned at 0x%08x\n", module->base_addr,
			max_align);
	DBG_PRINT("Module size: 0x%08x\n", module->module_size);
	*/

out:
	// Free up allocated memory
	if (pht != NULL)
		free(pht);

	return res;
}

static int prepare_dynlinking(struct elf_module *module) {
	Elf32_Dyn  *dyn_entry = module->dyn_table;

	while (dyn_entry->d_tag != DT_NULL) {
		switch (dyn_entry->d_tag) {
		case DT_NEEDED:
			/*
			 * It's unlikely there'll be more than
			 * MAX_NR_DEPS DT_NEEDED entries but if there
			 * are then inform the user that we ran out of
			 * space.
			 */
			if (module->nr_needed < MAX_NR_DEPS)
				module->needed[module->nr_needed++] = dyn_entry->d_un.d_ptr;
			else {
				printf("Too many dependencies!\n");
				return -1;
			}
			break;
		case DT_HASH:
			module->hash_table =
				(Elf32_Word*)module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_GNU_HASH:
			module->ghash_table =
				(Elf32_Word*)module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_STRTAB:
			module->str_table =
				(char*)module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_SYMTAB:
			module->sym_table =
				module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_STRSZ:
			module->strtable_size = dyn_entry->d_un.d_val;
			break;
		case DT_SYMENT:
			module->syment_size = dyn_entry->d_un.d_val;
			break;
		case DT_PLTGOT: // The first entry in the GOT
			module->got = module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		}

		dyn_entry++;
	}

	return 0;
}

void undefined_symbol(void)
{
	printf("Error: An undefined symbol was referenced\n");
	kaboom();
}

static int perform_relocation(struct elf_module *module, Elf32_Rel *rel) {
	Elf32_Word *dest = module_get_absolute(rel->r_offset, module);

	// The symbol reference index
	Elf32_Word sym = ELF32_R_SYM(rel->r_info);
	unsigned char type = ELF32_R_TYPE(rel->r_info);

	// The symbol definition (if applicable)
	Elf32_Sym *sym_def = NULL;
	struct elf_module *sym_module = NULL;
	Elf32_Addr sym_addr = 0x0;

	if (sym > 0) {
		// Find out details about the symbol

		// The symbol reference
		Elf32_Sym *sym_ref = symbol_get_entry(module, sym);

		// The symbol definition
		sym_def =
			global_find_symbol(module->str_table + sym_ref->st_name,
					&sym_module);

		if (sym_def == NULL) {
			DBG_PRINT("Cannot perform relocation for symbol %s\n",
					module->str_table + sym_ref->st_name);

			if (ELF32_ST_BIND(sym_ref->st_info) != STB_WEAK)
				return -1;

			// This must be a derivative-specific
			// function. We're OK as long as we never
			// execute the function.
			sym_def = global_find_symbol("undefined_symbol", &sym_module);
		}

		// Compute the absolute symbol virtual address
		sym_addr = (Elf32_Addr)module_get_absolute(sym_def->st_value, sym_module);

		if (sym_module != module) {
			// Create a dependency
			enforce_dependency(sym_module, module);
		}
	}

	switch (type) {
	case R_386_NONE:
		// Do nothing
		break;
	case R_386_32:
		*dest += sym_addr;
		break;
	case R_386_PC32:
		*dest += sym_addr - (Elf32_Addr)dest;
		break;
	case R_386_COPY:
		if (sym_addr > 0) {
			memcpy((void*)dest, (void*)sym_addr, sym_def->st_size);
		}
		break;
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
		// Maybe TODO: Keep track of the GOT entries allocations
		*dest = sym_addr;
		break;
	case R_386_RELATIVE:
		*dest += module->base_addr;
		break;
	default:
		DBG_PRINT("Relocation type %d not supported\n", type);
		return -1;
	}

	return 0;
}

static int resolve_symbols(struct elf_module *module) {
	Elf32_Dyn  *dyn_entry = module->dyn_table;
	unsigned int i;
	int res;

	Elf32_Word plt_rel_size = 0;
	char *plt_rel = NULL;

	char *rel = NULL;
	Elf32_Word rel_size = 0;
	Elf32_Word rel_entry = 0;

	// The current relocation
	Elf32_Rel *crt_rel;

	while (dyn_entry->d_tag != DT_NULL) {
		switch(dyn_entry->d_tag) {

		// PLT relocation information
		case DT_PLTRELSZ:
			plt_rel_size = dyn_entry->d_un.d_val;
			break;
		case DT_PLTREL:
			if (dyn_entry->d_un.d_val != DT_REL) {
				DBG_PRINT("Unsupported PLT relocation\n");
				return -1;
			}
		case DT_JMPREL:
			plt_rel = module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;

		// Standard relocation information
		case DT_REL:
			rel = module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_RELSZ:
			rel_size = dyn_entry->d_un.d_val;
			break;
		case DT_RELENT:
			rel_entry = dyn_entry->d_un.d_val;
			break;

		// Module initialization and termination
		case DT_INIT:
			// TODO Implement initialization functions
			break;
		case DT_FINI:
			// TODO Implement finalization functions
			break;
		}

		dyn_entry++;
	}

	if (rel_size > 0) {
		// Process standard relocations
		for (i = 0; i < rel_size/rel_entry; i++) {
			crt_rel = (Elf32_Rel*)(rel + i*rel_entry);

			res = perform_relocation(module, crt_rel);

			if (res < 0)
				return res;
		}

	}

	if (plt_rel_size > 0) {
		// TODO: Permit this lazily
		// Process PLT relocations
		for (i = 0; i < plt_rel_size/sizeof(Elf32_Rel); i++) {
			crt_rel = (Elf32_Rel*)(plt_rel + i*sizeof(Elf32_Rel));

			res = perform_relocation(module, crt_rel);

			if (res < 0)
				return res;
		}
	}

	return 0;
}

static int extract_operations(struct elf_module *module) {
	Elf32_Sym *ctors_start, *ctors_end;
	Elf32_Sym *dtors_start, *dtors_end;
	module_ctor_t *ctors = NULL;
	module_ctor_t *dtors = NULL;

	ctors_start = module_find_symbol("__ctors_start", module);
	ctors_end = module_find_symbol("__ctors_end", module);

	if (ctors_start && ctors_end) {
		module_ctor_t *start, *end;
		int nr_ctors = 0;
		int i, size;

		start = module_get_absolute(ctors_start->st_value, module);
		end = module_get_absolute(ctors_end->st_value, module);

		nr_ctors = end - start;

		size = nr_ctors * sizeof(module_ctor_t);
		size += sizeof(module_ctor_t); /* NULL entry */

		ctors = malloc(size);
		if (!ctors) {
			printf("Unable to alloc memory for ctors\n");
			return -1;
		}

		memset(ctors, 0, size);
		for (i = 0; i < nr_ctors; i++)
			ctors[i] = start[i];

		module->ctors = ctors;
	}

	dtors_start = module_find_symbol("__dtors_start", module);
	dtors_end = module_find_symbol("__dtors_end", module);

	if (dtors_start && dtors_end) {
		module_ctor_t *start, *end;
		int nr_dtors = 0;
		int i, size;

		start = module_get_absolute(dtors_start->st_value, module);
		end = module_get_absolute(dtors_end->st_value, module);

		nr_dtors = end - start;

		size = nr_dtors * sizeof(module_ctor_t);
		size += sizeof(module_ctor_t); /* NULL entry */

		dtors = malloc(size);
		if (!dtors) {
			printf("Unable to alloc memory for dtors\n");
			free(ctors);
			return -1;
		}

		memset(dtors, 0, size);
		for (i = 0; i < nr_dtors; i++)
			dtors[i] = start[i];

		module->dtors = dtors;
	}

	return 0;
}

// Loads the module into the system
int module_load(struct elf_module *module) {
	int res;
	Elf32_Sym *main_sym;
	Elf32_Ehdr elf_hdr;
	module_ctor_t *ctor;
	struct elf_module *head = NULL;

	// Do not allow duplicate modules
	if (module_find(module->name) != NULL) {
		DBG_PRINT("Module %s is already loaded.\n", module->name);
		return EEXIST;
	}

	// Get a mapping/copy of the ELF file in memory
	res = image_load(module);

	if (res < 0) {
		return res;
	}

	// The module is a fully featured dynamic library
	module->shallow = 0;

	CHECKED(res, image_read(&elf_hdr, sizeof(Elf32_Ehdr), module), error);
	//printf("check... 1\n");
	
	//print_elf_ehdr(&elf_hdr);

	// Checking the header signature and members
	CHECKED(res, check_header(&elf_hdr), error);
	//printf("check... 2\n");

	// Load the segments in the memory
	CHECKED(res, load_segments(module, &elf_hdr), error);
	//printf("bleah... 3\n");
	// Obtain dynamic linking information
	CHECKED(res, prepare_dynlinking(module), error);
	//printf("check... 4\n");

	head = module_current();

	/* Find modules we need to load as dependencies */
	if (module->str_table) {
		int i;

		/*
		 * Note that we have to load the dependencies in
		 * reverse order.
		 */
		for (i = module->nr_needed - 1; i >= 0; i--) {
			char *dep, *p;
			char *argv[2] = { NULL, NULL };

			dep = module->str_table + module->needed[i];

			/* strip everything but the last component */
			if (!strlen(dep))
				continue;

			if (strchr(dep, '/')) {
				p = strrchr(dep, '/');
				p++;
			} else
				p = dep;

			argv[0] = p;
			res = spawn_load(p, 1, argv);
			if (res < 0) {
				printf("Failed to load %s\n", p);
				goto error;
			}
		}
	}

	// Check the symbols for duplicates / missing definitions
	CHECKED(res, check_symbols(module), error);
	//printf("check... 5\n");

	main_sym = module_find_symbol("main", module);
	if (main_sym)
		module->main_func =
			module_get_absolute(main_sym->st_value, module);

	//printf("check... 6\n");

	// Add the module at the beginning of the module list
	list_add(&module->list, &modules_head);

	// Perform the relocations
	resolve_symbols(module);

	// Obtain constructors and destructors
	CHECKED(res, extract_operations(module), error);

	//dprintf("module->symtable_size = %d\n", module->symtable_size);

	//print_elf_symbols(module);

	// The file image is no longer needed
	image_unload(module);

	/*
	DBG_PRINT("MODULE %s LOADED SUCCESSFULLY (main@%p, init@%p, exit@%p)\n",
			module->name,
			(module->main_func == NULL) ? NULL : *(module->main_func),
			(module->init_func == NULL) ? NULL : *(module->init_func),
			(module->exit_func == NULL) ? NULL : *(module->exit_func));
	*/

	for (ctor = module->ctors; ctor && *ctor; ctor++)
		(*ctor) ();

	return 0;

error:
	if (head)
		unload_modules_since(head->name);

	// Remove the module from the module list (if applicable)
	list_del_init(&module->list);

	if (module->module_addr != NULL) {
		elf_free(module->module_addr);
		module->module_addr = NULL;
	}

	image_unload(module);

	// Clear the execution part of the module buffer
	memset(&module->u, 0, sizeof module->u);

	return res;
}

