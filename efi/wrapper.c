/*
 * Copyright (C) 2011 Intel Corporation; author Matt Fleming
 *
 * Wrap the ELF shared library in a PE32 suit.
 *
 * Syslinux plays some games with the ELF sections that are not easily
 * converted to a PE32 executable. For instance, Syslinux requires
 * that a symbol hash table be present (GNU hash or SysV) so that
 * symbols in ELF modules can be resolved at runtime but the EFI
 * firmware loader doesn't like that and refuses to load the file.
 *
 * We pretend that we have an EFI executable with a single .text
 * section so that the EFI loader will load it and jump to the entry
 * point. Once the Syslinux ELF shared object has control we can do
 * whatever we want.
 */
#include <linux/elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wrapper.h"

/*
 * 'so_size' is the file size of the ELF shared object.
 */
static void write_header(FILE *f, __uint32_t entry, __uint32_t so_size)
{
	struct optional_hdr o_hdr;
	struct section t_sec, r_sec;
	struct extra_hdr e_hdr;
	struct coff_hdr c_hdr;
	struct header hdr;
	struct coff_reloc c_rel;
	__uint32_t total_sz = so_size;
	__uint32_t dummy = 0;
	__uint32_t hdr_sz;
	__uint32_t reloc_start, reloc_end;

	hdr_sz = sizeof(o_hdr) + sizeof(t_sec) + sizeof(e_hdr) +
		sizeof(r_sec) + sizeof(c_hdr) + sizeof(hdr) + sizeof(c_rel)
		+ sizeof(dummy);
	total_sz += hdr_sz;
	entry += hdr_sz;

	memset(&hdr, 0, sizeof(hdr));
	hdr.msdos_signature = MSDOS_SIGNATURE;
	hdr.pe_hdr = OFFSETOF(struct header, pe_signature);
	hdr.pe_signature = PE_SIGNATURE;
	fwrite(&hdr, sizeof(hdr), 1, f);

	memset(&c_hdr, 0, sizeof(c_hdr));
	c_hdr.arch = IMAGE_FILE_MACHINE_I386;
	c_hdr.nr_sections = 2;
	c_hdr.nr_syms = 1;
	c_hdr.optional_hdr_sz = sizeof(o_hdr) + sizeof(e_hdr);
	c_hdr.characteristics = IMAGE_FILE_32BIT_MACHINE |
		IMAGE_FILE_DEBUG_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE |
		IMAGE_FILE_LINE_NUMBERS_STRIPPED;
	fwrite(&c_hdr, sizeof(c_hdr), 1, f);

	memset(&o_hdr, 0, sizeof(o_hdr));
	o_hdr.format = PE32_FORMAT;
	o_hdr.major_linker_version = 0x02;
	o_hdr.minor_linker_version = 0x14;
	o_hdr.code_sz = total_sz;
	o_hdr.entry_point = entry;
	fwrite(&o_hdr, sizeof(o_hdr), 1, f);

	memset(&e_hdr, 0, sizeof(e_hdr));
	e_hdr.section_align = 4096;
	e_hdr.file_align = 512;
	e_hdr.image_sz = total_sz;
	e_hdr.headers_sz = 512;
	e_hdr.subsystem = IMAGE_SUBSYSTEM_EFI_APPLICATION;
	e_hdr.rva_and_sizes_nr = 1;
	fwrite(&e_hdr, sizeof(e_hdr), 1, f);

	memset(&t_sec, 0, sizeof(t_sec));
	strcpy((char *)t_sec.name, ".text");
	t_sec.virtual_sz = total_sz;
	t_sec.raw_data_sz = total_sz;
	t_sec.characteristics = IMAGE_SCN_CNT_CODE |
		IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_EXECUTE |
		IMAGE_SCN_MEM_READ;
	fwrite(&t_sec, sizeof(t_sec), 1, f);

	/*
	 * Write our dummy relocation and reloc section.
	 */
	memset(&r_sec, 0, sizeof(r_sec));
	strcpy((char *)r_sec.name, ".reloc");
	r_sec.virtual_sz = sizeof(c_rel);
	r_sec.virtual_address = ftell(f) + sizeof(r_sec);
	r_sec.raw_data_sz = r_sec.virtual_sz;
	r_sec.raw_data = r_sec.virtual_address;
	r_sec.characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
		IMAGE_SCN_ALIGN_1BYTES | IMAGE_SCN_MEM_DISCARDABLE |
		IMAGE_SCN_MEM_READ;
	fwrite(&r_sec, sizeof(r_sec), 1, f);

	memset(&c_rel, 0, sizeof(c_rel));
	c_rel.virtual_address = ftell(f) + sizeof(c_rel);
	c_rel.symtab_index = 10;
	fwrite(&c_rel, sizeof(c_rel), 1, f);
	fwrite(&dummy, sizeof(dummy), 1, f);

}

static void usage(char *progname)
{
	fprintf(stderr,	"usage: %s <ELF shared object> <output file>\n",
		progname);
}

int main(int argc, char **argv)
{
	struct stat st;
	Elf32_Ehdr e_hdr;
	FILE *f_in, *f_out;
	void *buf;
	size_t rv;

	if (argc < 3) {
		usage(argv[0]);
		exit(0);
	}

	f_in = fopen(argv[1], "r");
	if (!f_in) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	if (stat(argv[1], &st) != 0) {
		perror("stat");
		exit(EXIT_FAILURE);
	}

	f_out = fopen(argv[2], "w");
	if (!f_out) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	/*
	 * Parse the ELF header and find the entry point.
	 */
	fread((void *)&e_hdr, sizeof(e_hdr), 1, f_in);
	if (e_hdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    e_hdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    e_hdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    e_hdr.e_ident[EI_MAG3] != ELFMAG3) {
		fprintf(stderr, "Input file not ELF shared object\n");
		exit(EXIT_FAILURE);
	}

	/* We only support 32-bit for now.. */
	if (e_hdr.e_ident[EI_CLASS] != ELFCLASS32) {
		fprintf(stderr, "Input file not 32-bit ELF shared object\n");
		exit(EXIT_FAILURE);
	}

	buf = malloc(st.st_size);
	if (!buf) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	write_header(f_out, e_hdr.e_entry, st.st_size);

	/* Write out the entire ELF shared object */
	rewind(f_in);
	rv = fread(buf, st.st_size, 1, f_in);
	if (!rv && ferror(f_in)) {
		fprintf(stderr, "Failed to read all bytes from input\n");
		exit(EXIT_FAILURE);
	}

	fwrite(buf, st.st_size, rv, f_out);
	return 0;
}
