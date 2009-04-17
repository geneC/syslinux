/* -----------------------------------------------------------------------
 *   
 *   Copyright 2008-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

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
		__earlybss_end = .;
	}
	__earlybss_len = __earlybss_end - __earlybss_start;
	__earlybss_dwords = (__earlybss_len + 3) >> 2;

	.bss : {
		__bss_start = .;
		*(.bss)
		*(.bss2)
		__bss_end = .;
	}
	__bss_len = __bss_end - __bss_start;
	__bss_dwords = (__bss_len + 3) >> 2;

 	.config : AT (__config_lma) {
		__config_start = .;
		*(.config)
		__config_end = .;
	}
	__config_len = __config_end - __config_start;
	__config_dwords = (__config_len + 3) >> 2;

	. = ALIGN(16);
 	.replacestub : AT (__replacestub_lma) {
		__replacestub_start = .;
		*(.replacestub)
		__replacestub_end = .;
	}
	__replacestub_len = __replacestub_end - __replacestub_start;
	__replacestub_dwords = (__replacestub_len + 3) >> 2;

	/* Stack */

	STACK_BASE = 0x7c00 - STACK_LEN;
	. = STACK_BASE;
	.stack : AT(STACK_BASE) {
		__stack_start = .;
		. += STACK_LEN;
		__stack_end = .;
	}
	__stack_len = __stack_end - __stack_start;
	__stack_dwords = (__stack_len + 3) >> 2;

	/* Initialized sections */

	. = 0x7c00;
	.text : {
		FILL(0x90909090)
		__text_start = .;
		*(.text)
		__text_end = .;
	}
	__text_len = __text_end - __text_start;
	__text_dwords = (__text_len + 3) >> 2;

	. = ALIGN(16);
	.bcopyxx : {
		FILL(0x90909090)
		__bcopyxx_start = .;
		*(.bcopyxx)
		__bcopyxx_end = .;
	}
	__bcopyxx_len = __bcopyxx_end - __bcopyxx_start;
	__bcopyxx_dwords = (__bcopyxx_len + 3) >> 2;

	. = ALIGN(4);
	.data : {
	      __data_start = .;
	      *(.data)
	      __data_end = .;
	}
	__data_len = __data_end - __data_start;
	__data_dwords = (__data_len + 3) >> 2;

	. = ALIGN(4);
	__config_lma = .;
	. += SIZEOF(.config);

	. = ALIGN(4);
	__replacestub_lma = .;
	. += SIZEOF(.replacestub);

	/* ADV, must be the last intialized section */

	. = ALIGN(512);
	.adv : {
		__adv_start = .;
		*(.adv)
		__adv_end = .;
	}
	__adv_len = __adv_end - __adv_start;
	__adv_dwords = (__adv_len + 3) >> 2;

	/* Late uninitialized sections */

	.uibss : {
		__uibss_start = .;
		*(.uibss)
		__uibss_end = .;
	}
	__uibss_len = __uibss_end - __uibss_start;
	__uibss_dwords = (__uibss_len + 3) >> 2;

	.bss1 : {
		__bss1_start = .;
		*(.bss1)
		__bss1_end = .;
	}
	__bss1_len = __bss1_end - __bss1_start;
	__bss1_dwords = (__bss1_len + 3) >> 2;

	. = ASSERT(__bss1_end <= 0x10000, "64K overflow");

	. = 0x100000;
	.com32 : {
		*(.com32)
	}
}
