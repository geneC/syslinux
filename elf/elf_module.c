#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>

#ifdef ELF_USERSPACE_TEST

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#endif //ELF_USERSPACE_TEST

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


// User-space debugging routines
#ifdef ELF_USERSPACE_TEST
static void print_elf_ehdr(Elf32_Ehdr *ehdr) {
	int i;
	
	printf("Identification:\t");
	for (i=0; i < EI_NIDENT; i++) {
		printf("%d ", ehdr->e_ident[i]);
	}
	printf("\n");
	printf("Type:\t\t%u\n", ehdr->e_type);
	printf("Machine:\t%u\n", ehdr->e_machine);
	printf("Version:\t%u\n", ehdr->e_version);
	printf("Entry:\t\t0x%08x\n", ehdr->e_entry);
	printf("PHT Offset:\t0x%08x\n", ehdr->e_phoff);
	printf("SHT Offset:\t0x%08x\n", ehdr->e_shoff);
	printf("Flags:\t\t%u\n", ehdr->e_flags);
	printf("Header size:\t%u (Structure size: %u)\n", ehdr->e_ehsize,
			sizeof(Elf32_Ehdr));
}

static void print_elf_symbols(struct elf_module *module) {
	Elf32_Word *bkt = module->hash_table + 2;
	Elf32_Word *chn = module->hash_table + 2 + module->hash_table[0];
	Elf32_Word i, crt_index;
	Elf32_Sym *crt_sym;
	
	printf("Bucket count: %d \n", module->hash_table[0]);
	printf("Chain count: %d (Non GNU-Hash: %d)\n", module->hash_table[1],
			module->ghash_table[1]);
	
	for (i = 0; i < module->hash_table[0]; i++) {
		printf("Bucket %d:\n", i);
		crt_index = bkt[i];
		
		while (crt_index != STN_UNDEF) {
			crt_sym = (Elf32_Sym*)(module->sym_table + crt_index*module->syment_size);
			
			printf("%s\n", module->str_table + crt_sym->st_name);
			crt_index = chn[crt_index];
		}
	}
}
#endif //ELF_USERSPACE_TEST

#ifdef ELF_USERSPACE_TEST
static int load_image(struct elf_module *module) {
	char file_name[MODULE_NAME_SIZE+3]; // Include the extension
	struct stat elf_stat;
	
	strcpy(file_name, module->name);
	strcat(file_name, ".so");
	
	module->_file_fd = open(file_name, O_RDONLY);
	
	if (module->_file_fd < 0) {
		perror("Could not open object file");
		goto error;
	}
	
	if (fstat(module->_file_fd, &elf_stat) < 0) {
		perror("Could not get file information");
		goto error;
	}
	
	module->_file_size = elf_stat.st_size;
	
	module->_file_image = mmap(NULL, module->_file_size, PROT_READ, MAP_PRIVATE, 
			module->_file_fd, 0);
		
	if (module->_file_image == NULL) {
		perror("Could not map the file into memory");
		goto error;
	}
	
	return 0;
	
error:
	if (module->_file_image != NULL) {
		munmap(module->_file_image, module->_file_size);
		module->_file_image = NULL;
	}
	
	if (module->_file_fd > 0) {
		close(module->_file_fd);
		module->_file_fd = 0;
	}
	return -1;
}


static int unload_image(struct elf_module *module) {
	munmap(module->_file_image, module->_file_size);
	module->_file_image = NULL;
	
	close(module->_file_fd);
	module->_file_fd = 0;
	
	return 0;
}


#else
static int load_image(struct elf_module *module) {
	// TODO: Implement SYSLINUX specific code here
	return 0;
}

static int unload_image(struct elf_module *module) {
	// TODO: Implement SYSLINUX specific code here
	return 0;
}
#endif //ELF_USERSPACE_TEST


// Initialization of the module subsystem
int modules_init() {
	return 0;
}

// Termination of the module subsystem
void modules_term() {
	
}

// Allocates the structure for a new module
struct elf_module *module_alloc(const char *name) {
	struct elf_module *result = malloc(sizeof(struct elf_module));
	
	memset(result, 0, sizeof(struct elf_module));
	
	strncpy(result->name, name, MODULE_NAME_SIZE);
	
	return result;
}

// Performs verifications on ELF header to assure that the open file is a
// valid SYSLINUX ELF module.
static int check_header(struct elf_module *module) {
	Elf32_Ehdr *elf_hdr = elf_get_header(module->_file_image);
	
	// Check the header magic
	if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		elf_hdr->e_ident[EI_MAG3] != ELFMAG3) {
		
		fprintf(stderr, "The file is not an ELF object\n");
		return -1;
	}
	
	if (elf_hdr->e_ident[EI_CLASS] != MODULE_ELF_CLASS) {
		fprintf(stderr, "Invalid ELF class code\n");
		return -1;
	}
	
	if (elf_hdr->e_ident[EI_DATA] != MODULE_ELF_DATA) {
		fprintf(stderr, "Invalid ELF data encoding\n");
		return -1;
	}
	
	if (elf_hdr->e_ident[EI_VERSION] != MODULE_ELF_VERSION ||
			elf_hdr->e_version != MODULE_ELF_VERSION) {
		fprintf(stderr, "Invalid ELF file version\n");
		return -1;
	}
	
	if (elf_hdr->e_type != MODULE_ELF_TYPE) {
		fprintf(stderr, "The ELF file must be a shared object\n");
		return -1;
	}
	
	
	if (elf_hdr->e_machine != MODULE_ELF_MACHINE) {
		fprintf(stderr, "Invalid ELF architecture\n");
		return -1;
	}
	
	if (elf_hdr->e_phoff == 0x00000000) {
		fprintf(stderr, "PHT missing\n");
		return -1;
	}
	
	return 0;
}

static int load_segments(struct elf_module *module) {
	int i;
	Elf32_Ehdr *elf_hdr = elf_get_header(module->_file_image);
	Elf32_Phdr *cr_pht;
	
	Elf32_Addr min_addr  = 0x00000000; // Min. ELF vaddr
	Elf32_Addr max_addr  = 0x00000000; // Max. ELF vaddr
	Elf32_Word max_align = sizeof(void*); // Min. align of posix_memalign()
	Elf32_Addr min_alloc, max_alloc;   // Min. and max. aligned allocables 
	
	
	// Compute the memory needings of the module
	for (i=0; i < elf_hdr->e_phnum; i++) {
		cr_pht = elf_get_ph(module->_file_image, i);
		
		if (cr_pht->p_type == PT_LOAD) {
			if (i == 0) {
				min_addr = cr_pht->p_vaddr;
			} else {
				min_addr = MIN(min_addr, cr_pht->p_vaddr);
			}
			
			max_addr = MAX(max_addr, cr_pht->p_vaddr + cr_pht->p_memsz);
			max_align = MAX(max_align, cr_pht->p_align);
		}
	}
	
	if (max_addr - min_addr == 0) {
		// No loadable segments
		fprintf(stderr, "No loadable segments found\n");
		return -1;
	}
	
	// The minimum address that should be allocated
	min_alloc = min_addr - (min_addr % max_align);
	
	// The maximum address that should be allocated
	max_alloc = max_addr - (max_addr % max_align);
	if (max_addr % max_align > 0)
		max_alloc += max_align;
	
	
	if (posix_memalign(&module->module_addr, 
			max_align, 
			max_alloc-min_alloc) != 0) {
		
		fprintf(stderr, "Could not allocate segments\n");
		return -1;
	}
	
	module->base_addr = (Elf32_Addr)(module->module_addr) - min_alloc;
	module->module_size = max_alloc - min_alloc;
	
	// Zero-initialize the memory
	memset(module->module_addr, 0, module->module_size);
	
	for (i = 0; i < elf_hdr->e_phnum; i++) {
		cr_pht = elf_get_ph(module->_file_image, i);
		
		if (cr_pht->p_type == PT_LOAD) {
			// Copy the segment at its destination
			memcpy(module_get_absolute(cr_pht->p_vaddr, module),
					module->_file_image + cr_pht->p_offset,
					cr_pht->p_filesz);
			
			printf("Loadable segment of size 0x%08x copied from vaddr 0x%08x at 0x%08x\n",
					cr_pht->p_filesz,
					cr_pht->p_vaddr,
					(Elf32_Addr)module_get_absolute(cr_pht->p_vaddr, module));
		}
	}
	
	printf("Base address: 0x%08x, aligned at 0x%08x\n", module->base_addr,
			max_align);
	printf("Module size: 0x%08x\n", module->module_size);
	
	return 0;
}

static int prepare_dynlinking(struct elf_module *module) {
	int i;
	Elf32_Ehdr *elf_hdr = elf_get_header(module->_file_image);
	Elf32_Phdr *dyn_ph; // The program header for the dynamic section
	
	Elf32_Dyn  *dyn_entry; // The table of dynamic linking information
	
	for (i=0; i < elf_hdr->e_phnum; i++) {
		dyn_ph = elf_get_ph(module->_file_image, i);
		
		if (dyn_ph->p_type == PT_DYNAMIC)
			break;
		else
			dyn_ph = NULL;
	}
	
	if (dyn_ph == NULL) {
		fprintf(stderr, "Dynamic relocation information not found\n");
		return -1;
	}
	
	module->_dyn_info = (Elf32_Dyn*)(module->_file_image + dyn_ph->p_offset); 
	
	dyn_entry = module->_dyn_info;
	
	while (dyn_entry->d_tag != DT_NULL) {
		switch (dyn_entry->d_tag) {
		case DT_NEEDED:
			// TODO: Manage dependencies here
			break;
		case DT_HASH:
			module->hash_table = 
				(Elf32_Word*)module_get_absolute(dyn_entry->d_un.d_ptr, module);
			break;
		case DT_GNU_HASH:	// TODO: Add support for this one, too (50% faster)
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
			fprintf(stderr, "Cannot perform relocation for symbol %s\n",
					module->str_table + sym_ref->st_name);
			return 0;
		}
		
		// Compute the absolute symbol virtual address
		sym_addr = (Elf32_Addr)module_get_absolute(sym_def->st_value, sym_module);
		
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
		fprintf(stderr, "Relocation type %d not supported\n", type);
		return -1;
	}
	
	return 0;
}

static int resolve_symbols(struct elf_module *module) {
	Elf32_Dyn  *dyn_entry = module->_dyn_info;
	int i, res;
	
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
				fprintf(stderr, "Unsupported PLT relocation\n");
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
	int i;
	Elf32_Sym *crt_sym, *ref_sym;
	char *crt_name;
	struct elf_module *crt_module;
	
	int strong_count;
	int weak_count;
	
	// The chain count gives the number of symbols
	for (i = 1; i < module->hash_table[1]; i++) {
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
				fprintf(stderr, "Symbol %s is undefined\n", crt_name);
				//return -1;
			}
		} else {
			if (strong_count > 0 && ELF32_ST_BIND(ref_sym->st_info) == STB_GLOBAL) {
				fprintf(stderr, "Warning: Symbol %s is defined more than once\n", crt_name);
				//return -1;
			}
		}
	}
	
	return 0;
}

// Loads the module into the system
int module_load(struct elf_module *module) {
	int res;
	Elf32_Ehdr *elf_hdr;
	
	INIT_LIST_HEAD(&module->list);
	INIT_LIST_HEAD(&module->deps);
	
	
	// Get a mapping/copy of the ELF file in memory
	res = load_image(module);
	
	if (res < 0) {
		return res;
	}
	
	// Checking the header signature and members
	CHECKED(res, check_header(module), error);
	
	// Obtain the ELF header
	elf_hdr = elf_get_header(module->_file_image);

	// DEBUG
	print_elf_ehdr(elf_hdr);
	
	// Load the segments in the memory
	CHECKED(res, load_segments(module), error);
	// Obtain dynamic linking information
	CHECKED(res, prepare_dynlinking(module), error);
	
	// DEBUG
	print_elf_symbols(module);
	
	// Check the symbols for duplicates / missing definitions
	CHECKED(res, check_symbols(module), error);
	
	// Add the module at the beginning of the module list
	list_add(&module->list, &modules);
	
	// Perform the relocations
	CHECKED(res, resolve_symbols(module), error);
	
	// The file image is no longer needed
	CHECKED(res, unload_image(module), error);
	
	
	return 0;
	
error:
	unload_image(module);
	
	return res;
}

// Unloads the module from the system and releases all the associated memory
int module_unload(struct elf_module *module) {
	
	free(module);
	
	return 0;
}


Elf32_Sym *module_find_symbol(const char *name, struct elf_module *module) {
	unsigned long h = elf_hash((const unsigned char*)name);
	
	Elf32_Word *bkt = module->hash_table + 2;
	Elf32_Word *chn = module->hash_table + 2 + module->hash_table[0];
	
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
