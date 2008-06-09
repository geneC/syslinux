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

static int check_header(Elf32_Ehdr *elf_hdr) {
	
	// Check the header magic
	if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		elf_hdr->e_ident[EI_MAG3] != ELFMAG3) {
		
		fprintf(stderr, "Invalid ELF magic\n");
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
	
	if (elf_hdr->e_ident[EI_VERSION] != MODULE_ELF_VERSION) {
		fprintf(stderr, "Invalid ELF file version\n");
		return -1;
	}
	
	return 0;
}

// Loads the module into the system
int module_load(struct elf_module *module) {
	int res;
	Elf32_Ehdr *elf_hdr;
	
	INIT_LIST_HEAD(&module->list);
	INIT_LIST_HEAD(&module->deps);
	
	res = load_image(module);
	
	if (res < 0) {
		return res;
	}
	
	elf_hdr = (Elf32_Ehdr*)module->file_image;
	
	res = check_header(elf_hdr);
	
	if (res < 0) {
		goto error;
	}
	
	print_elf_ehdr(elf_hdr);
	
	res = unload_image(module);
	
	if (res < 0) {
		return res;
	}
	
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
