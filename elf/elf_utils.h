#ifndef ELF_UTILS_H_
#define ELF_UTILS_H_

#include <elf.h>

// Returns a pointer to the ELF header structure
static inline Elf32_Ehdr *elf_get_header(void *elf_image) {
	return (Elf32_Ehdr*)elf_image;
}

// Returns a pointer to the first entry in the Program Header Table
static inline Elf32_Phdr *elf_get_pht(void *elf_image) {
	Elf32_Ehdr *elf_hdr = elf_get_header(elf_image);
	
	return (Elf32_Phdr*)((Elf32_Off)elf_hdr + elf_hdr->e_phoff);
}

// Returns the element with the given index in the PTH
static inline Elf32_Phdr *elf_get_ph(void *elf_image, int index) {
	Elf32_Phdr *elf_pht = elf_get_pht(elf_image);
	Elf32_Ehdr *elf_hdr = elf_get_header(elf_image);
	
	return (Elf32_Phdr*)((Elf32_Off)elf_pht + index * elf_hdr->e_phentsize);
}

extern unsigned long elf_hash(const unsigned char *name);
extern unsigned long elf_gnu_hash(const unsigned char *name);

#endif /*ELF_UTILS_H_*/
