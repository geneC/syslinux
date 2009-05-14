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

	. = ALIGN(4);
	.bss16 : {
		__bss16_start = .;
		*(.bss16)
		__bss16_end = .;
	}
	__bss16_len = __bss16_end - __bss16_start;
	__bss16_dwords = (__bss16_len + 3) >> 2;

	. = ALIGN(4);
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
	.init : {
		FILL(0x90909090)
		__init_start = .;
		*(.init)
		__init_end = .;
	}
	__init_len = __init_end - __init_start;
	__init_dwords = (__init_len + 3) >> 2;

	.text16 : {
		FILL(0x90909090)
		__text16_start = .;
		*(.text16)
		__text16_end = .;
	}
	__text16_len = __text16_end - __text16_start;
	__text16_dwords = (__text16_len + 3) >> 2;

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
	.data16 : {
	      __data16_start = .;
	      *(.data16)
	      __data16_end = .;
	}
	__data16_len = __data16_end - __data16_start;
	__data16_dwords = (__data16_len + 3) >> 2;

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

	. = ALIGN(4);
	.uibss : {
		__uibss_start = .;
		*(.uibss)
		__uibss_end = .;
	}
	__uibss_len = __uibss_end - __uibss_start;
	__uibss_dwords = (__uibss_len + 3) >> 2;

	. = ASSERT(__uibss_end <= 0x10000, "64K overflow");

	. = 0x100000;
	.com32 : {
		*(.com32)
	}
}
