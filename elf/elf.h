// License would go here

#ifndef ELF_H_
#define ELF_H_


////////////////////////////////////////////////////////////////////////////////
// GENERAL ELF FORMAT DECLARATIONS

// ELF data types
typedef unsigned long 	Elf32_Addr;
typedef unsigned short 	Elf32_Half;
typedef unsigned long 	Elf32_Off;
typedef long			Elf32_Sword;
typedef unsigned long	Elf32_Word;


////////////////////////////////////////////////////////////////////////////////
// ELF HEADER DECLARATIONS 

// Relevant indices in the identification section
enum e_ident_indices_enum {
	// Magic indices
	EI_MAG0		= 0,
	EI_MAG1		= 1,
	EI_MAG2		= 2,
	EI_MAG3		= 3,

	EI_CLASS	= 4,
	EI_DATA		= 5,
	EI_VERSION	= 6,
	EI_PAD		= 7,
	EI_NIDENT  	= 16 // Total size of the identification information
};

enum ei_class_enum {
	ELFCLASSNONE	= 0,
	ELFCLASS32		= 1,
	ELFCLASS64		= 2
};

enum ei_data_enum {
	ELFDATANONE		= 0,
	ELFDATA2LSB		= 1,
	ELFDATA2MSB		= 2
};


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
	unsigned char 	e_ident[EI_NIDENT]; // Identification
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
	Elf32_Half		e_shstrndx;			// Section name string table index
} Elf32_Ehdr;


////////////////////////////////////////////////////////////////////////////////
// ELF SECTION DECLARATIONS

// Special section indices
enum sh_special_enum {
	SHN_UNDEF		= 0, 		// Undefined
	SHN_LORESERVE	= 0xFF00, 	// Lower index of reserved range
	SHN_LOPROC		= 0xFF00, 	// Processor specific semantics interval
	SHN_HIPROC		= 0xFF1F,	// *
	SHN_ABS			= 0xFFF1,	// Absolute values for the reference
	SHN_COMMON		= 0xFFF2,	// Common symbols
	SHN_HIRESERVE	= 0xFFFF 	// Upper index of reserved range 
};

// Section type
enum sh_type_enum {
	SHT_NULL		= 0,			// Inactive
	SHT_PROGBITS	= 1,			// Program specific information
	SHT_SYMTAB		= 2,			// Symbol table
	SHT_STRTAB		= 3,			// String table
	SHT_RELA		= 4,			// Relocation entries with explicit addends
	SHT_HASH		= 5,			// Symbol hash table
	SHT_DYNAMIC		= 6,			// Dynamic linking information
	SHT_NOTE		= 7,			// Marking information
	SHT_NOBITS		= 8,			// No space occupied
	SHT_REL			= 9,			// Relocation entries w/o explicit addends
	SHT_SHLIB		= 10,			// Reserved
	SHT_DYNSYM		= 11,			// Symbol table
	SHT_LOPROC		= 0x70000000,	// Processor specific semantics
	SHT_HIPROC		= 0x7FFFFFFF,	// *
	SHT_LOUSER		= 0x80000000,	// Application programs
	SHT_HIUSER		= 0xFFFFFFFF	// *
};

// Section flags
enum sh_flags_enum {
	SHF_WRITE		= 0x1,			// Writable data
	SHF_ALLOC		= 0x2,			// Section occupies memory during execution
	SHF_EXECINSTR	= 0x4,			// Executable code
	SHF_MASKPROC	= 0xF0000000	// Processor specific semantics
};

// The section header
typedef struct {
	Elf32_Word		sh_name; 		// Index into section header string table
	Elf32_Word		sh_type; 		// Section type and semantic
	Elf32_Word		sh_flags;		// Section flags
	Elf32_Addr		sh_addr;		// Address of the memory mapping (or 0)
	Elf32_Off		sh_offset;		// Section location in the file
	Elf32_Word		sh_size;		// Section size in the file
	Elf32_Word		sh_link;		// Section header table index link 
	Elf32_Word		sh_info;		// Extra information
	Elf32_Word		sh_addralign;	// Alignment constraint
	Elf32_Word		sh_entsize;		// Entry size for table-like sections
} Elf32_Shdr;


////////////////////////////////////////////////////////////////////////////////
// SYMBOL TABLE DECLARATIONS

// Special symbol indices
enum st_special_enum {
	STN_UNDEF		= 0 // Undefined symbol index
};

// Macros for manipulating a symbol table entry information field
#define ELF32_ST_BIND(i)		((i) >> 4)
#define ELF32_ST_TYPE(i)		((i) & 0xF)
#define ELF32_ST_INFO(b,t)		((b) << 4) + ((t) & 0xF))

enum st_bind_enum {
	STB_LOCAL		= 0,
	STB_GLOBAL		= 1,
	STB_WEAK		= 2,
	STB_LOPROC		= 13,
	STB_HIPROC		= 15
};

// Symbol table entry
typedef struct {
	Elf32_Word		st_name;	// Index in the name table
	Elf32_Addr		st_value;	// Value associated with the symbol
	Elf32_Word		st_size;	// Symbol data size
	unsigned char	st_info;	// Type and binding attrs.
	unsigned char	st_other;	// Reserved (zero)
	Elf32_Half		st_shndx;	// Relevant section header table index
} Elf32_Sym;

#endif /*ELF_H_*/
