/*
 * common.c
 *
 *  Created on: Aug 11, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fs.h>

#include <linux/list.h>
#include <sys/module.h>

#include "elfutils.h"
#include "common.h"

/**
 * The one and only list of loaded modules
 */
LIST_HEAD(modules_head);

// User-space debugging routines
#ifdef ELF_DEBUG
void print_elf_ehdr(Elf32_Ehdr *ehdr) {
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
	//fprintf(stderr, "Flags:\t\t%u\n", ehdr->e_flags);
	//fprintf(stderr, "Header size:\t%u (Structure size: %u)\n", ehdr->e_ehsize,sizeof(Elf32_Ehdr));
	fprintf(stderr, "phnum: %d shnum: %d\n", ehdr->e_phnum,
		ehdr->e_shnum);
}

void print_elf_symbols(struct elf_module *module) {
	unsigned int i;
	Elf32_Sym *crt_sym;

	for (i = 1; i < module->symtable_size; i++)
	{
		crt_sym = (Elf32_Sym*)(module->sym_table + i*module->syment_size);

		fprintf(stderr,"%s %d\n", module->str_table + crt_sym->st_name, crt_sym->st_value);

	}
}
#endif //ELF_DEBUG

static FILE *findpath(char *name)
{
	char path[FILENAME_MAX];
	FILE *f;
	char *p, *n;
	int i;

	f = fopen(name, "rb"); /* for full path */
	if (f)
		return f;

	p = PATH;
again:
	i = 0;
	while (*p && *p != ':' && i < FILENAME_MAX) {
		path[i++] = *p++;
	}

	if (*p == ':')
		p++;

	n = name;
	while (*n && i < FILENAME_MAX)
		path[i++] = *n++;
	path[i] = '\0';

	f = fopen(path, "rb");
	if (f)
		return f;

	if (p >= PATH && p < PATH + strlen(PATH))
		goto again;

	return NULL;
}

/*
 * Image files manipulation routines
 */

int image_load(struct elf_module *module)
{
	module->u.l._file = findpath(module->name);

	if (module->u.l._file == NULL) {
		DBG_PRINT("Could not open object file '%s'\n", module->name);
		goto error;
	}

	module->u.l._cr_offset = 0;

	return 0;

error:
	if (module->u.l._file != NULL) {
		fclose(module->u.l._file);
		module->u.l._file = NULL;
	}

	return -1;
}


int image_unload(struct elf_module *module) {
	if (module->u.l._file != NULL) {
		fclose(module->u.l._file);
		module->u.l._file = NULL;

	}
	module->u.l._cr_offset = 0;

	return 0;
}

int image_read(void *buff, size_t size, struct elf_module *module) {
	size_t result = fread(buff, size, 1, module->u.l._file);

	if (result < 1)
		return -1;

	module->u.l._cr_offset += size;
	return 0;
}

int image_skip(size_t size, struct elf_module *module) {
	void *skip_buff = NULL;
	size_t result;

	if (size == 0)
		return 0;

	skip_buff = malloc(size);
	result = fread(skip_buff, size, 1, module->u.l._file);
	free(skip_buff);

	if (result < 1)
		return -1;

	module->u.l._cr_offset += size;
	return 0;
}

int image_seek(Elf32_Off offset, struct elf_module *module) {
	if (offset < module->u.l._cr_offset) // Cannot seek backwards
		return -1;

	return image_skip(offset - module->u.l._cr_offset, module);
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

struct module_dep *module_dep_alloc(struct elf_module *module) {
	struct module_dep *result = malloc(sizeof(struct module_dep));

	INIT_LIST_HEAD (&result->list);

	result->module = module;

	return result;
}

struct elf_module *module_find(const char *name) {
	struct elf_module *cr_module;

	for_each_module(cr_module) {
		if (strcmp(cr_module->name, name) == 0)
			return cr_module;
	}

	return NULL;
}


// Performs verifications on ELF header to assure that the open file is a
// valid SYSLINUX ELF module.
int check_header_common(Elf32_Ehdr *elf_hdr) {
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


int enforce_dependency(struct elf_module *req, struct elf_module *dep) {
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

int clear_dependency(struct elf_module *req, struct elf_module *dep) {
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

int check_symbols(struct elf_module *module)
{
	unsigned int i;
	Elf32_Sym *crt_sym = NULL, *ref_sym = NULL;
	char *crt_name;
	struct elf_module *crt_module;

	int strong_count;
	int weak_count;

	for(i = 1; i < module->symtable_size; i++)
	{
		crt_sym = symbol_get_entry(module, i);
		crt_name = module->str_table + crt_sym->st_name;

		strong_count = 0;
		weak_count = 0;

		for_each_module(crt_module)
		{
			ref_sym = module_find_symbol(crt_name, crt_module);

			// If we found a definition for our symbol...
			if (ref_sym != NULL && ref_sym->st_shndx != SHN_UNDEF)
			{
				switch (ELF32_ST_BIND(ref_sym->st_info))
				{
					case STB_GLOBAL:
						strong_count++;
						break;
					case STB_WEAK:
						weak_count++;
						break;
				}
			}
		}

		if (crt_sym->st_shndx == SHN_UNDEF)
		{
			// We have an undefined symbol
			if (strong_count == 0 && weak_count == 0)
			{
				DBG_PRINT("Symbol %s is undefined\n", crt_name);
				printf("Undef symbol FAIL: %s\n",crt_name);
				return -1;
			}
		}
		else
		{
			if (strong_count > 0 && ELF32_ST_BIND(ref_sym->st_info) == STB_GLOBAL)
			{
				// It's not an error - at relocation, the most recent symbol
				// will be considered
				DBG_PRINT("Info: Symbol %s is defined more than once\n", crt_name);
			}
		}
		//printf("symbol %s laoded from %d\n",crt_name,crt_sym->st_value);
	}

	return 0;
}

int module_unloadable(struct elf_module *module) {
	if (!list_empty(&module->dependants))
		return 0;

	return 1;
}


// Unloads the module from the system and releases all the associated memory
int _module_unload(struct elf_module *module) {
	struct module_dep *crt_dep, *tmp;
	// Make sure nobody needs us
	if (!module_unloadable(module)) {
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
	if (module->module_addr != NULL) {
		elf_free(module->module_addr);

		DBG_PRINT("%s MODULE %s UNLOADED\n", module->shallow ? "SHALLOW" : "",
				module->name);
	}
	// Release the module structure
	free(module);

	return 0;
}

int module_unload(struct elf_module *module) {
	module_ctor_t *dtor;

	for (dtor = module->dtors; *dtor; dtor++)
		(*dtor) ();

	return _module_unload(module);
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
		crt_sym = symbol_get_entry(module, crt_index);

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
					Elf32_Sym *crt_sym = symbol_get_entry(module, (hasharr - gnu_chain_zero));

					if (strcmp(name, module->str_table + crt_sym->st_name) == 0) {
						return crt_sym;
					}
				}
			} while ((*hasharr++ & 1u) == 0);
		}
	}

	return NULL;
}

static Elf32_Sym *module_find_symbol_iterate(const char *name,struct elf_module *module)
{

	unsigned int i;
	Elf32_Sym *crt_sym;

	for (i=1; i < module->symtable_size; i++)
	{
		crt_sym = symbol_get_entry(module, i);
		if (strcmp(name, module->str_table + crt_sym->st_name) == 0)
		{
			return crt_sym;
		}
	}

	return NULL;
}

Elf32_Sym *module_find_symbol(const char *name, struct elf_module *module) {
	Elf32_Sym *result = NULL;

	if (module->ghash_table != NULL)
		result = module_find_symbol_gnu(name, module);

	if (result == NULL)
	{
		if (module->hash_table != NULL)
		{
			//printf("Attempting SYSV Symbol search\n");
			result = module_find_symbol_sysv(name, module);
		}
		else
		{
			//printf("Attempting Iterative Symbol search\n");
			result = module_find_symbol_iterate(name, module);
		}
	}

	return result;
}

Elf32_Sym *global_find_symbol(const char *name, struct elf_module **module) {
	struct elf_module *crt_module;
	Elf32_Sym *crt_sym = NULL;
	Elf32_Sym *result = NULL;

	for_each_module(crt_module) {
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
