/*
 * Linker script for the SYSLINUX core
 */

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
EXTERN(_start)
ENTRY(_start)

STACK_LEN = 4096;

SECTIONS
{
	/* "Early" sections (before the load) */
	. = 0x0800;
	
	.earlybss : {
		__earlybss_start = .;
		*(.earlybss)
		__earlybss_len = . - __earlybss_start;
	}

	.bcopy32 : AT (__bcopy32_lma) {
		FILL(0x90909090)
		__bcopy32_start = .;
		*(.bcopy32)
		__bcopy32_len = . - __bcopy32_start;
	}

	.config : AT (__config_lma) {
		__config_start = .;
		*(.config)
		__config_len = . - __config_start;
	}

	.bss : AT(__bss_start) {
		__bss_start = .;
		*(.bss)
		*(.bss2)
		__bss_len = . - __bss_start;
	}

	/* Stack */

	. = 0x7c00 - STACK_LEN;
	.stack : {
		__stack_start = .;
		. += STACK_LEN;
		__stack_len = . - __stack_start;
	}

	/* Initialized sections */

	. = 0x7c00;
	.text : {
		FILL(0x90909090)
		__text_start = .;
		*(.text)
		__text_len = . - __text_start;
	}

	. = ALIGN(4);
	__bcopy32_lma = .;
	. += SIZEOF(.bcopy32);

	. = ALIGN(4);
	.data : {
	      __data_start = .;
	      *(.data)
	      __data_len = . - __data_start;
	}

	. = ALIGN(4);
	__config_lma = .;
	. += SIZEOF(.config);

	/* ADV, must be the last intialized section */

	. = ALIGN(512);
	.adv : {
		__adv_start = .;
		*(.adv)
		__adv_len = . - __adv_start;
	}

	/* Late uninitialized sections */

	.uibss : {
		__uibss_start = .;
		*(.uibss)
		__uibss_len = . - __uibss_start;
	}

	.bss1 : {
		__bss1_start = .;
		*(.bss1)
		__bss1_len = . - __bss1_start;
	}

	Stack			  = __stack_start;
	section..bcopy32.start    = __bcopy32_lma;
	section..config.start     = __config_lma;
	section..config.vstart    = __config_start;
	section..config.end.start = __config_start + SIZEOF(.config);
}
