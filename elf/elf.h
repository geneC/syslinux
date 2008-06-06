// License would go here

#ifndef ELF_H_
#define ELF_H_

// ELF data types
typedef unsigned long 	Elf32_Addr;
typedef unsigned short 	Elf32_Half;
typedef unsigned long 	Elf32_Off;
typedef long			Elf32_Sword;
typedef unsigned long	Elf32_Word;

// Size of the magic section
#define EI_NIDENT  16

// Object file types
enum e_type_enum {
	ET_NONE 	= 0, // No type
	ET_REL		= 1, // Relocatable
	ET_EXEC		= 2, // Executable
	ET_DYN		= 3, // Shared object
	ET_CORE		= 4, // Core
};

// Machine types
enum e_machine_enum {
	EM_NONE			= 0, // No machine
	EM_M32			= 1, // AT&T WE 32100
	EM_SPARC		= 2, // SPARC
	EM_386			= 3, // Intel 80386
	EM_68K			= 4, // Motorola 68000
	EM_88K			= 5, // Motorola 88000
	EM_860 			= 7, // Intel 80860
	EM_MIPS			= 8, // MIPS RS3000 Big Endian
	EM_MIPS_RS4_BE  = 10 // MIPS RS4000 Big Endian
};

// Object version
enum e_version_enum {
	EV_NONE			= 0, 	// Invalid version
	EV_CURRENT		= 1		// Current version
};

// The ELF header
typedef struct {
	unsigned char 	e_ident[EI_NIDENT]; // Magic
	Elf32_Half		e_type; 			// Object file type
	Elf32_Half		e_machine; 			// Machine type
	Elf32_Word		e_version;			// Object file version
	Elf32_Addr		e_entry;			// Program entry point
	Elf32_Off		e_phoff;			// Program Header Table (PHT) offset
	Elf32_Off		e_shoff;			// Section Header Table (SHT) offset
	Elf32_Word		e_flags;			// Processor specific flags
	Elf32_Half		e_ehsize;			// ELF header size
	Elf32_Half		e_phentsize;		// Size of an entry in PHT
	Elf32_Half		e_phnum;			// Number of entries in PHT
	Elf32_Half		e_shentsize;		// Size of an header in SHT
	Elf32_Half		e_shnum;			// Number of entries in SHT
	Elf32_Half		shstrndx;			// Section name string table index
} Elf32_Ehdr;



#endif /*ELF_H_*/
