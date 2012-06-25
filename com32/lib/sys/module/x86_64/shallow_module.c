/*
 * shallow_module.c
 *
 *  Created on: Aug 11, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */


#include <string.h>
#include <sys/module.h>

#include "common.h"
#include "elfutils.h"


static int check_header_shallow(Elf64_Ehdr *elf_hdr) {
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

static int load_shallow_sections(struct elf_module *module, Elf64_Ehdr *elf_hdr) {
	int i;
	int res = 0;
	void *sht = NULL;
	void *buffer = NULL;
	Elf64_Shdr *crt_sht;
	Elf64_Off buff_offset;

	Elf64_Off min_offset = 0xFFFFFFFFFFFFFFFF;
	Elf64_Off max_offset = 0x0000000000000000;
	Elf64_Word max_align = 0x1;

	Elf64_Off sym_offset = 0xFFFFFFFFFFFFFFFF;
	Elf64_Off str_offset = 0xFFFFFFFFFFFFFFFF;


	char *sh_strtable;

	// We buffer the data up to the SHT
	buff_offset = module->u.l._cr_offset;

	buffer = malloc(elf_hdr->e_shoff - buff_offset);
	// Get to the SHT
	image_read(buffer, elf_hdr->e_shoff - buff_offset, module);

	// Load the SHT
	sht = malloc(elf_hdr->e_shnum * elf_hdr->e_shentsize);
	image_read(sht, elf_hdr->e_shnum * elf_hdr->e_shentsize, module);

	// Get the string table of the section names
	crt_sht = (Elf64_Shdr*)(sht + elf_hdr->e_shstrndx * elf_hdr->e_shentsize);
	sh_strtable = (char*)(buffer + (crt_sht->sh_offset - buff_offset));

	for (i = 0; i < elf_hdr->e_shnum; i++) {
		crt_sht = (Elf64_Shdr*)(sht + i*elf_hdr->e_shentsize);

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


int module_load_shallow(struct elf_module *module, Elf64_Addr base_addr) {
	int res;
	Elf64_Ehdr elf_hdr;

	// Do not allow duplicate modules
	if (module_find(module->name) != NULL) {
		DBG_PRINT("Module already loaded.\n");
		return -1;
	}

	res = image_load(module);

	if (res < 0)
		return res;

	module->shallow = 1;

	CHECKED(res, image_read(&elf_hdr, sizeof(Elf64_Ehdr), module), error);

	// Checking the header signature and members
	CHECKED(res, check_header_shallow(&elf_hdr), error);

	CHECKED(res, load_shallow_sections(module, &elf_hdr), error);
	module->base_addr = base_addr;

	// Check the symbols for duplicates / missing definitions
	CHECKED(res, check_symbols(module), error);

	// Add the module at the beginning of the module list
	list_add(&module->list, &modules_head);

	// The file image is no longer needed
	image_unload(module);

	DBG_PRINT("SHALLOW MODULE %s LOADED SUCCESSFULLY\n", module->name);

	return 0;

error:
	image_unload(module);

	return res;
}
