#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>

#include "linux_list.h"
#include "elf_module.h"
#include "elf_utils.h"

// Performs an operation and jumps to a given label if an error occurs
#define CHECKED(res, expr, error)		\
	do { 								\
		(res) = (expr);					\
		if ((res) < 0)					\
			goto error;					\
	} while (0)

#define MIN(x,y)	(((x) < (y)) ? (x) : (y))
#define MAX(x,y)	(((x) > (y)) ? (x) : (y))

// The list of loaded modules
static LIST_HEAD(modules);

#ifdef ELF_DEBUG
#define DBG_PRINT(fmt, args...)	fprintf(stderr, "[DBG] " fmt, ##args)
#else
#define DBG_PRINT(fmt, args...)	// Expand to nothing
#endif


// User-space debugging routines
#ifdef ELF_DEBUG
static void print_elf_ehdr(Elf32_Ehdr *ehdr) {
	int i;

	fprintf(stderr, "Identification:\t");
	for (i=0; i < EI_NIDENT; i++) {
		printf("%d ", ehdr->e_ident[i]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Type:\t\t%u\n", ehdr->e_type);
	fprintf(stderr, "Machine:\t%u\n", ehdr->e_machine);
	fprintf(stderr, "Version:\t%u\n", ehdr->e_version);
	fprintf(stderr, "Entry:\t\t0x%08x\n", ehdr->e_entry);
	fprintf(stderr, "PHT Offset:\t0x%08x\n", ehdr->e_phoff);
	fprintf(stderr, "SHT Offset:\t0x%08x\n", ehdr->e_shoff);
	fprintf(stderr, "Flags:\t\t%u\n", ehdr->e_flags);
	fprintf(stderr, "Header size:\t%u (Structure size: %u)\n", ehdr->e_ehsize,
			sizeof(Elf32_Ehdr));
}

static void print_elf_symbols(struct elf_module *module) {
	unsigned int i;
	Elf32_Sym *crt_sym;

	for (i = 1; i < module->symtable_size; i++) {
		crt_sym = (Elf32_Sym*)(module->sym_table + i*module->syment_size);

		fprintf(stderr, "%s\n", module->str_table + crt_sym->st_name);

	}
}
#endif //ELF_DEBUG

static int image_load(struct elf_module *module) {
	module->_file = fopen(module->name, "rb");

	if (module->_file == NULL) {
		DBG_PRINT("Could not open object file '%s'\n", module->name);
		goto error;
	}

	module->_cr_offset = 0;

	return 0;

error:
	if (module->_file != NULL) {
		fclose(module->_file);
		module->_file = NULL;
	}

	return -1;
}


static int image_unload(struct elf_module *module) {
	if (module->_file != NULL) {
		fclose(module->_file);
		module->_file = NULL;
	}
	module->_cr_offset = 0;

	return 0;
}

static int image_read(void *buff, size_t size, struct elf_module *module) {
	size_t result = fread(buff, size, 1, module->_file);

	if (result < 1)
		return -1;

	DBG_PRINT("[I/O] Read %u\n", size);
	module->_cr_offset += size;
	return 0;
}

static int image_skip(size_t size, struct elf_module *module) {
	void *skip_buff = NULL;
	size_t result;

	if (size == 0)
		return 0;

	skip_buff = malloc(size);
	result = fread(skip_buff, size, 1, module->_file);
	free(skip_buff);

	if (result < 1)
		return -1;

	DBG_PRINT("[I/O] Skipped %u\n", size);
	module->_cr_offset += size;
	return 0;
}

static int image_seek(Elf32_Off offset, struct elf_module *module) {
	if (offset < module->_cr_offset) // Cannot seek backwards
		return -1;

	return image_skip(offset - module->_cr_offset, module);
}


// Initialization of the module subsystem
int modules_init(void) {
	return 0;
}

// Termination of the module subsystem
void modules_term(void) {

}

// Allocates the structure for a new module
struct elf_module *module_alloc(const char *name) {
	struct elf_module *result = malloc(sizeof(struct elf_module));

	memset(result, 0, sizeof(struct elf_module));

	INIT_LIST_HEAD(&result->list);
	INIT_LIST_HEAD(&result->required);
	INIT_LIST_HEAD(&result->dependants);

	strncpy(result->name, name, MODULE_NAME_SIZE);

	return result;
}

static struct module_dep *module_dep_alloc(struct elf_module *module) {
	struct module_dep *result = malloc(sizeof(struct module_dep));

	INIT_LIST_HEAD (&result->list);

	result->module = module;

	return result;
}

struct elf_module *module_find(const char *name) {
	struct elf_module *cr_module;

	list_for_each_entry(cr_module, &modules, list) {
		if (strcmp(cr_module->name, name) == 0)
			return cr_module;
	}

	return NULL;
}

// Performs verifications on ELF header to assure that the open file is a
// valid SYSLINUX ELF module.
static int check_header_common(Elf32_Ehdr *elf_hdr) {
	// Check the header magic
	if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		elf_hdr->e_ident[EI_MAG3] != ELFMAG3) {

		DBG_PRINT("The file is not an ELF object\n");
		return -1;
	}

	if (elf_hdr->e_ident[EI_CLASS] != MODULE_ELF_CLASS) {
		DBG_PRINT("Invalid ELF class code\n");
		return -1;
	}

	if (elf_hdr->e_ident[EI_DATA] != MODULE_ELF_DATA) {
		DBG_PRINT("Invalid ELF data encoding\n");
		return -1;
	}

	if (elf_hdr->e_ident[EI_VERSION] != MODULE_ELF_VERSION ||
			elf_hdr->e_version != MODULE_ELF_VERSION) {
		DBG_PRINT("Invalid ELF file version\n");
		return -1;
	}

	if (elf_hdr->e_machine != MODULE_ELF_MACHINE) {
		DBG_PRINT("Invalid ELF architecture\n");
		return -1;
	}

	return 0;
}

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

static int check_header_shallow(Elf32_Ehdr *elf_hdr) {
	int res;

	res = check_header_common(elf_hdr);

	if (res != 0)
		return res;

	if (elf_hdr->e_shoff == 0x00000000) {
		DBG_PRINT("SHT missing\n");
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
	void *pht = NULL;
	Elf32_Phdr *cr_pht;

	Elf32_Addr min_addr  = 0x00000000; // Min. ELF vaddr
	Elf32_Addr max_addr  = 0x00000000; // Max. ELF vaddr
	Elf32_Word max_align = sizeof(void*); // Min. align of posix_memalign()
	Elf32_Addr min_alloc, max_alloc;   // Min. and max. aligned allocables

	Elf32_Addr dyn_addr = 0x00000000;

	// Get to the PHT
	image_seek(elf_hdr->e_phoff, module);

	// Load the PHT
	pht = malloc(elf_hdr->e_phnum * elf_hdr->e_phentsize);
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
			if (cr_pht->p_offset < module->_cr_offset) {
				// The segment contains data before the current offset
				// It can be discarded without worry - it would contain only
				// headers
				Elf32_Off aux_off = module->_cr_offset - cr_pht->p_offset;

				if (image_read(module_get_absolute(cr_pht->p_vaddr, module) + aux_off,
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

			DBG_PRINT("Loadable segment of size 0x%08x copied from vaddr 0x%08x at 0x%08x\n",
					cr_pht->p_filesz,
					cr_pht->p_vaddr,
					(Elf32_Addr)module_get_absolute(cr_pht->p_vaddr, module));
		}
	}

	// Setup dynamic segment location
	module->dyn_table = module_get_absolute(dyn_addr, module);

	DBG_PRINT("Base address: 0x%08x, aligned at 0x%08x\n", module->base_addr,
			max_align);
	DBG_PRINT("Module size: 0x%08x\n", module->module_size);

out:
	// Free up allocated memory
	if (pht != NULL)
		free(pht);

	return res;
}

static int load_shallow_sections(struct elf_module *module, Elf32_Ehdr *elf_hdr) {
	int i;
	int res = 0;
	void *sht = NULL;
	void *buffer = NULL;
	Elf32_Shdr *crt_sht;
	Elf32_Off buff_offset;

	Elf32_Off min_offset = 0xFFFFFFFF;
	Elf32_Off max_offset = 0x00000000;
	Elf32_Word max_align = 0x1;

	Elf32_Off sym_offset = 0xFFFFFFFF;
	Elf32_Off str_offset = 0xFFFFFFFF;


	char *sh_strtable;

	// We buffer the data up to the SHT
	buff_offset = module->_cr_offset;

	buffer = malloc(elf_hdr->e_shoff - buff_offset);
	// Get to the SHT
	image_read(buffer, elf_hdr->e_shoff - buff_offset, module);

	// Load the SHT
	sht = malloc(elf_hdr->e_shnum * elf_hdr->e_shentsize);
	image_read(sht, elf_hdr->e_shnum * elf_hdr->e_shentsize, module);

	// Get the string table of the section names
	crt_sht = (Elf32_Shdr*)(sht + elf_hdr->e_shstrndx * elf_hdr->e_shentsize);
	sh_strtable = (char*)(buffer + (crt_sht->sh_offset - buff_offset));

	for (i = 0; i < elf_hdr->e_shnum; i++) {
		crt_sht = (Elf32_Shdr*)(sht + i*elf_hdr->e_shentsize);

		if (strcmp(".symtab", sh_strtable + crt_sht->sh_name) == 0) {
			// We found the symbol table
			min_offset = MIN(min_offset, crt_sht->sh_offset);
			max_offset = MAX(max_offset, crt_sht->sh_offset + crt_sht->sh_size);
			max_align = MAX(max_align, crt_sht->sh_addralign);

			sym_offset = crt_sht->sh_offset;

			module->syment_size = crt_sht->sh_entsize;
			module->symtable_size = crt_sht->sh_size / crt_sht->sh_entsize;
		}
		if (strcmp(".strtab", sh_strtable + crt_sht->sh_name) == 0) {
			// We found the string table
			min_offset = MIN(min_offset, crt_sht->sh_offset);
			max_offset = MAX(max_offset, crt_sht->sh_offset + crt_sht->sh_size);
			max_align = MAX(max_align, crt_sht->sh_addralign);

			str_offset = crt_sht->sh_offset;

			module->strtable_size = crt_sht->sh_size;
		}
	}

	if (elf_malloc(&module->module_addr, max_align,
			max_offset - min_offset) != 0) {
		DBG_PRINT("Could not allocate sections\n");
		goto out;
	}

	// Copy the data
	image_seek(min_offset, module);
	image_read(module->module_addr, max_offset - min_offset, module);

	// Setup module information
	module->module_size = max_offset - min_offset;
	module->str_table = (char*)(module->module_addr + (str_offset - min_offset));
	module->sym_table = module->module_addr + (sym_offset - min_offset);

out:
	// Release the SHT
	if (sht != NULL)
		free(sht);

	// Release the buffer
	if (buffer != NULL)
		free(buffer);

	return res;
}

static int prepare_dynlinking(struct elf_module *module) {
	Elf32_Dyn  *dyn_entry = module->dyn_table;

	while (dyn_entry->d_tag != DT_NULL) {
		switch (dyn_entry->d_tag) {
		case DT_NEEDED:
			// TODO: Manage dependencies here
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

	// Now compute the number of symbols in the symbol table
	if (module->ghash_table != NULL) {
		module->symtable_size = module->ghash_table[1];
	} else {
		module->symtable_size = module->hash_table[1];
	}

	return 0;
}

static int enforce_dependency(struct elf_module *req, struct elf_module *dep) {
	struct module_dep *crt_dep;
	struct module_dep *new_dep;

	list_for_each_entry(crt_dep, &req->dependants, list) {
		if (crt_dep->module == dep) {
			// The dependency is already enforced
			return 0;
		}
	}

	new_dep = module_dep_alloc(req);
	list_add(&new_dep->list, &dep->required);

	new_dep = module_dep_alloc(dep);
	list_add(&new_dep->list, &req->dependants);

	return 0;
}

static int clear_dependency(struct elf_module *req, struct elf_module *dep) {
	struct module_dep *crt_dep = NULL;
	int found = 0;

	list_for_each_entry(crt_dep, &req->dependants, list) {
		if (crt_dep->module == dep) {
			found = 1;
			break;
		}
	}

	if (found) {
		list_del(&crt_dep->list);
		free(crt_dep);
	}

	found = 0;

	list_for_each_entry(crt_dep, &dep->required, list) {
		if (crt_dep->module == req) {
			found = 1;
			break;
		}
	}

	if (found) {
		list_del(&crt_dep->list);
		free(crt_dep);
	}

	return 0;
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
		Elf32_Sym *sym_ref =
			(Elf32_Sym*)(module->sym_table + sym * module->syment_size);

		// The symbol definition
		sym_def =
			global_find_symbol(module->str_table + sym_ref->st_name,
					&sym_module);

		if (sym_def == NULL) {
			// This should never happen
			DBG_PRINT("Cannot perform relocation for symbol %s\n",
					module->str_table + sym_ref->st_name);

			return -1;
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
	void *plt_rel = NULL;

	void *rel = NULL;
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

static int check_symbols(struct elf_module *module) {
	unsigned int i;
	Elf32_Sym *crt_sym = NULL, *ref_sym = NULL;
	char *crt_name;
	struct elf_module *crt_module;

	int strong_count;
	int weak_count;

	for (i = 1; i < module->symtable_size; i++) {
		crt_sym = (Elf32_Sym*)(module->sym_table + i * module->syment_size);
		crt_name = module->str_table + crt_sym->st_name;

		strong_count = 0;
		weak_count = 0;

		list_for_each_entry(crt_module, &modules, list) {
			ref_sym = module_find_symbol(crt_name, crt_module);

			// If we found a definition for our symbol...
			if (ref_sym != NULL && ref_sym->st_shndx != SHN_UNDEF) {
				switch (ELF32_ST_BIND(ref_sym->st_info)) {
				case STB_GLOBAL:
					strong_count++;
					break;
				case STB_WEAK:
					weak_count++;
					break;
				}
			}
		}

		if (crt_sym->st_shndx == SHN_UNDEF) {
			// We have an undefined symbol
			if (strong_count == 0 && weak_count == 0) {
				DBG_PRINT("Symbol %s is undefined\n", crt_name);
				return -1;
			}
		} else {
			if (strong_count > 0 && ELF32_ST_BIND(ref_sym->st_info) == STB_GLOBAL) {
				// It's not an error - at relocation, the most recent symbol
				// will be considered
				DBG_PRINT("Info: Symbol %s is defined more than once\n", crt_name);
			}
		}
	}

	return 0;
}

static int extract_operations(struct elf_module *module) {
	Elf32_Sym *init_sym = module_find_symbol(MODULE_ELF_INIT_PTR, module);
	Elf32_Sym *exit_sym = module_find_symbol(MODULE_ELF_EXIT_PTR, module);

	if (init_sym == NULL) {
		DBG_PRINT("Cannot find initialization routine.\n");
		return -1;
	}
	if (exit_sym == NULL) {
		DBG_PRINT("Cannot find exit routine.\n");
		return -1;
	}

	module->init_func = (module_init_func*)module_get_absolute(
								init_sym->st_value, module);

	module->exit_func = (module_exit_func*)module_get_absolute(
								exit_sym->st_value, module);

	return 0;
}

// Loads the module into the system
int module_load(struct elf_module *module) {
	int res;
	Elf32_Ehdr elf_hdr;

	// Get a mapping/copy of the ELF file in memory
	res = image_load(module);

	if (res < 0) {
		return res;
	}

	// The module is a fully featured dynamic library
	module->shallow = 0;

	CHECKED(res, image_read(&elf_hdr, sizeof(Elf32_Ehdr), module), error);

	// Checking the header signature and members
	CHECKED(res, check_header(&elf_hdr), error);

	// Load the segments in the memory
	CHECKED(res, load_segments(module, &elf_hdr), error);
	// Obtain dynamic linking information
	CHECKED(res, prepare_dynlinking(module), error);

	// Check the symbols for duplicates / missing definitions
	CHECKED(res, check_symbols(module), error);

	// Obtain constructors and destructors
	CHECKED(res, extract_operations(module), error);

	// Add the module at the beginning of the module list
	list_add(&module->list, &modules);

	// Perform the relocations
	resolve_symbols(module);



	// The file image is no longer needed
	image_unload(module);

	DBG_PRINT("MODULE %s LOADED SUCCESSFULLY (&init@0x%08X, &exit@0x%08X)\n",
			module->name, module->init_func, module->exit_func);

	return 0;

error:
	// Remove the module from the module list (if applicable)
	list_del_init(&module->list);

	if (module->module_addr != NULL) {
		elf_free(module->module_addr);
		module->module_addr = NULL;
	}

	image_unload(module);

	return res;
}

int module_load_shallow(struct elf_module *module) {
	int res;
	Elf32_Ehdr elf_hdr;

	res = image_load(module);

	if (res < 0)
		return res;

	module->shallow = 1;

	CHECKED(res, image_read(&elf_hdr, sizeof(Elf32_Ehdr), module), error);

	// Checking the header signature and members
	CHECKED(res, check_header_shallow(&elf_hdr), error);

	CHECKED(res, load_shallow_sections(module, &elf_hdr), error);

	// Check the symbols for duplicates / missing definitions
	CHECKED(res, check_symbols(module), error);

	// Add the module at the beginning of the module list
	list_add(&module->list, &modules);

	// The file image is no longer needed
	image_unload(module);

	DBG_PRINT("SHALLOW MODULE %s LOADED SUCCESSFULLY\n", module->name);

	return 0;

error:
	image_unload(module);

	return res;
}

// Unloads the module from the system and releases all the associated memory
int module_unload(struct elf_module *module) {
	struct module_dep *crt_dep, *tmp;
	// Make sure nobody needs us
	if (!list_empty(&module->dependants)) {
		DBG_PRINT("Module is required by other modules.\n");
		return -1;
	}

	// Remove any dependency information
	list_for_each_entry_safe(crt_dep, tmp, &module->required, list) {
		clear_dependency(crt_dep->module, module);
	}

	// Remove the module from the module list
	list_del_init(&module->list);

	// Release the loaded segments or sections
	elf_free(module->module_addr);
	// Release the module structure
	free(module);

	return 0;
}


static Elf32_Sym *module_find_symbol_sysv(const char *name, struct elf_module *module) {
	unsigned long h = elf_hash((const unsigned char*)name);
	Elf32_Word *cr_word = module->hash_table;

	Elf32_Word nbucket = *cr_word++;
	cr_word++; // Skip nchain

	Elf32_Word *bkt = cr_word;
	Elf32_Word *chn = cr_word + nbucket;

	Elf32_Word crt_index = bkt[h % module->hash_table[0]];
	Elf32_Sym *crt_sym;


	while (crt_index != STN_UNDEF) {
		crt_sym = (Elf32_Sym*)(module->sym_table + crt_index*module->syment_size);

		if (strcmp(name, module->str_table + crt_sym->st_name) == 0)
			return crt_sym;

		crt_index = chn[crt_index];
	}

	return NULL;
}

static Elf32_Sym *module_find_symbol_gnu(const char *name, struct elf_module *module) {
	unsigned long h = elf_gnu_hash((const unsigned char*)name);

	// Setup code (TODO: Optimize this by computing only once)
	Elf32_Word *cr_word = module->ghash_table;
	Elf32_Word nbucket = *cr_word++;
	Elf32_Word symbias = *cr_word++;
	Elf32_Word bitmask_nwords = *cr_word++;

	if ((bitmask_nwords & (bitmask_nwords - 1)) != 0) {
		DBG_PRINT("Invalid GNU Hash structure\n");
		return NULL;
	}

	Elf32_Word gnu_shift = *cr_word++;

	Elf32_Addr *gnu_bitmask = (Elf32_Addr*)cr_word;
	cr_word += MODULE_ELF_CLASS_SIZE / 32 * bitmask_nwords;

	Elf32_Word *gnu_buckets = cr_word;
	cr_word += nbucket;

	Elf32_Word *gnu_chain_zero = cr_word - symbias;

	// Computations
	Elf32_Word bitmask_word = gnu_bitmask[(h / MODULE_ELF_CLASS_SIZE) &
	                                       (bitmask_nwords - 1)];

	unsigned int hashbit1 = h & (MODULE_ELF_CLASS_SIZE - 1);
	unsigned int hashbit2 = (h >> gnu_shift) & (MODULE_ELF_CLASS_SIZE - 1);

	if ((bitmask_word >> hashbit1) & (bitmask_word >> hashbit2) & 1) {
		unsigned long rem;
		Elf32_Word bucket;

		rem = h % nbucket;

		bucket = gnu_buckets[rem];

		if (bucket != 0) {
			const Elf32_Word* hasharr = &gnu_chain_zero[bucket];

			do {
				if (((*hasharr ^ h ) >> 1) == 0) {
					Elf32_Sym *crt_sym = (Elf32_Sym*)(module->sym_table +
							(hasharr - gnu_chain_zero) * module->syment_size);

					if (strcmp(name, module->str_table + crt_sym->st_name) == 0) {
						return crt_sym;
					}
				}
			} while ((*hasharr++ & 1u) == 0);
		}
	}

	return NULL;
}

static Elf32_Sym *module_find_symbol_iterate(const char *name,
		struct elf_module *module) {

	unsigned int i;
	Elf32_Sym *crt_sym;

	for (i=1; i < module->symtable_size; i++) {
		crt_sym = (Elf32_Sym*)(module->sym_table + i*module->syment_size);

		if (strcmp(name, module->str_table + crt_sym->st_name) == 0) {
			return crt_sym;
		}
	}

	return NULL;
}

Elf32_Sym *module_find_symbol(const char *name, struct elf_module *module) {
	Elf32_Sym *result = NULL;

	if (module->ghash_table != NULL)
		result = module_find_symbol_gnu(name, module);

	if (result == NULL) {
		if (module->hash_table != NULL)
			result = module_find_symbol_sysv(name, module);
		else
			result = module_find_symbol_iterate(name, module);
	}

	return result;
}

Elf32_Sym *global_find_symbol(const char *name, struct elf_module **module) {
	struct elf_module *crt_module;
	Elf32_Sym *crt_sym = NULL;
	Elf32_Sym *result = NULL;

	list_for_each_entry(crt_module, &modules, list) {
		crt_sym = module_find_symbol(name, crt_module);

		if (crt_sym != NULL && crt_sym->st_shndx != SHN_UNDEF) {
			switch (ELF32_ST_BIND(crt_sym->st_info)) {
			case STB_GLOBAL:
				if (module != NULL) {
					*module = crt_module;
				}
				return crt_sym;
			case STB_WEAK:
				// Consider only the first weak symbol
				if (result == NULL) {
					if (module != NULL) {
						*module = crt_module;
					}
					result = crt_sym;
				}
				break;
			}
		}
	}

	return result;
}
