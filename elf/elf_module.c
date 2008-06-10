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
#endif //ELF_USERSPACE_TEST

#ifdef ELF_USERSPACE_TEST
static int load_image(struct elf_module *module) {
	char file_name[MODULE_NAME_SIZE+3]; // Include the extension
	struct stat elf_stat;
	
	strcpy(file_name, module->name);
	strcat(file_name, ".so");
	
	module->file_fd = open(file_name, O_RDONLY);
	
	if (module->file_fd < 0) {
		perror("Could not open object file");
		goto error;
	}
	
	if (fstat(module->file_fd, &elf_stat) < 0) {
		perror("Could not get file information");
		goto error;
	}
	
	module->file_size = elf_stat.st_size;
	
	module->file_image = mmap(NULL, module->file_size, PROT_READ, MAP_PRIVATE, 
			module->file_fd, 0);
		
	if (module->file_image == NULL) {
		perror("Could not map the file into memory");
		goto error;
	}
	
	return 0;
	
error:
	if (module->file_image != NULL) {
		munmap(module->file_image, module->file_size);
		module->file_image = NULL;
	}
	
	if (module->file_fd > 0) {
		close(module->file_fd);
		module->file_fd = 0;
	}
	return -1;
}


static int unload_image(struct elf_module *module) {
	munmap(module->file_image, module->file_size);
	module->file_image = NULL;
	
	close(module->file_fd);
	module->file_fd = 0;
	
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
	Elf32_Ehdr *elf_hdr = elf_get_header(module->file_image);
	
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
	Elf32_Ehdr *elf_hdr = elf_get_header(module->file_image);
	Elf32_Phdr *cr_pht;
	
	Elf32_Addr min_addr  = 0x00000000; // Min. ELF vaddr
	Elf32_Addr max_addr  = 0x00000000; // Max. ELF vaddr
	Elf32_Word max_align = sizeof(void*); // Min. align of posix_memalign()
	Elf32_Addr min_alloc, max_alloc;   // Min. and max. aligned allocables 
	
	
	// Compute the memory needings of the module
	for (i=0; i < elf_hdr->e_phnum; i++) {
		cr_pht = elf_get_ph(module->file_image, i);
		
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
		cr_pht = elf_get_ph(module->file_image, i);
		
		if (cr_pht->p_type == PT_LOAD) {
			// Copy the segment at its destination
			memcpy((void*)(module->base_addr + cr_pht->p_vaddr),
					module->file_image + cr_pht->p_offset,
					cr_pht->p_filesz);
			
			printf("Loadable segment of size 0x%08x copied from vaddr 0x%08x at 0x%08x\n",
					cr_pht->p_filesz,
					cr_pht->p_vaddr,
					module->base_addr + cr_pht->p_vaddr);
		}
	}
	
	printf("Base address: 0x%08x, aligned at 0x%08x\n", module->base_addr,
			max_align);
	printf("Module size: 0x%08x\n", module->module_size);
	
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
	elf_hdr = elf_get_header(module->file_image);

	// DEBUG
	print_elf_ehdr(elf_hdr);
	
	CHECKED(res, load_segments(module), error);
	
	// The file image is no longer neededchar
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
