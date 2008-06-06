#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "elf.h"

void print_usage() {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\telftest objfile\n");
}

void print_elf_info(const char *file_name) {
	int elf_fd = open(file_name, O_RDONLY);
	void *elf_addr = NULL;
	Elf32_Ehdr *elf_header;
	struct stat elf_stat;
	int i;
	
	if (elf_fd < 0) {
		perror("Could not open object file");
		goto error;
	}
	
	if (fstat(elf_fd, &elf_stat) < 0) {
		perror("Could not get file information");
		goto error;
	}
	
	elf_addr = mmap(NULL, elf_stat.st_size, PROT_READ, MAP_PRIVATE, elf_fd, 0);
	
	if (elf_addr == NULL) {
		perror("Could not map the file into memory");
		goto error;
	}
	
	elf_header = (Elf32_Ehdr*)elf_addr;
	
	printf("Identification:\t");
	for (i=0; i < EI_NIDENT; i++) {
		printf("%d ", elf_header->e_ident[i]);
	}
	printf("\n");
	printf("Type:\t\t%u\n", elf_header->e_type);
	printf("Machine:\t%u\n", elf_header->e_machine);
	printf("Version:\t%lu\n", elf_header->e_version);
	printf("Entry:\t\t0x%08lx\n", elf_header->e_entry);
	printf("PHT Offset:\t0x%08lx\n", elf_header->e_phoff);
	printf("SHT Offset:\t0x%08lx\n", elf_header->e_shoff);
	printf("Flags:\t\t%lu\n", elf_header->e_flags);
	printf("Header size:\t%u (Structure size: %u)\n", elf_header->e_ehsize,
			sizeof(Elf32_Ehdr));
	
	
	munmap(elf_addr, elf_stat.st_size);
	close(elf_fd);
	
	return;
	
error:
	if (elf_addr != NULL)
		munmap(elf_addr, elf_stat.st_size);
	
	if (elf_fd >= 0)
		close(elf_fd);
	exit(2);
}

int main(int argc, char **argv) {
	const char *file_name = NULL;
	
	// Skip program name
	argc--;
	argv++;
	
	if (argc != 1) {
		print_usage();
		return 1;
	}
	
	file_name = argv[0];
	
	print_elf_info(file_name);
	
	return 0;
}
