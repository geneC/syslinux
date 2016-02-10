/* -----------------------------------------------------------------------
 *   
 *   Copyright 2008-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2016 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * Linker script for the SYSLINUX core when built for i386-bios
 */

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
EXTERN(_start)
ENTRY(_start)

STACK32_LEN = 65536;

SECTIONS
{
	/* Prefix structure for the compression program */
	. = 0;
	HIDDEN(__module_start = ABSOLUTE(.));
	.prefix : {
		*(.prefix)
	}

	/* "Early" sections (before the load) */
	. = 0x1000;

	.earlybss (NOLOAD) : {
		HIDDEN(__earlybss_start = .);
		*(.earlybss)
		HIDDEN(__earlybss_end = .);
	}
	HIDDEN(__earlybss_len = ABSOLUTE(__earlybss_end) - ABSOLUTE(__earlybss_start));
	HIDDEN(__earlybss_dwords = (__earlybss_len + 3) >> 2);

	. = ALIGN(4);
	.bss16 (NOLOAD) : {
		HIDDEN(__bss16_start = .);
		*(.bss16)
		HIDDEN(__bss16_end = .);
	}
	HIDDEN(__bss16_len = ABSOLUTE(__bss16_end) - ABSOLUTE(__bss16_start));
	HIDDEN(__bss16_dwords = (__bss16_len + 3) >> 2);

	. = ALIGN(4);
 	.config : AT (__config_lma) {
		HIDDEN(__config_start = .);
		*(.config)
		HIDDEN(__config_end = .);
	}
	HIDDEN(__config_len = ABSOLUTE(__config_end) - ABSOLUTE(__config_start));
	HIDDEN(__config_dwords = (__config_len + 3) >> 2);

	/* Generated and/or copied code */

	. = ALIGN(128);		/* Minimum separation from mutable data */
 	.replacestub : AT (__replacestub_lma) {
		HIDDEN(__replacestub_start = .);
		*(.replacestub)
		HIDDEN(__replacestub_end = .);
	}
	HIDDEN(__replacestub_len = ABSOLUTE(__replacestub_end) - ABSOLUTE(__replacestub_start));
	HIDDEN(__replacestub_dwords = (__replacestub_len + 3) >> 2);

	. = ALIGN(16);
	HIDDEN(__gentextnr_lma = .);
	.gentextnr : AT(__gentextnr_lma) {
		HIDDEN(__gentextnr_start = .);
		*(.gentextnr)
		HIDDEN(__gentextnr_end = .);
	}
	HIDDEN(__gentextnr_len = ABSOLUTE(__gentextnr_end) - ABSOLUTE(__gentextnr_start));
	HIDDEN(__gentextnr_dwords = (__gentextnr_len + 3) >> 2);

	. = STACK_BASE;
	.stack16 : AT(STACK_BASE) {
		HIDDEN(__stack16_start = .);
		. += STACK_LEN;
		HIDDEN(__stack16_end = .);
	}
	HIDDEN(__stack16_len = ABSOLUTE(__stack16_end) - ABSOLUTE(__stack16_start));
	HIDDEN(__stack16_dwords = (__stack16_len + 3) >> 2);

	/* Initialized sections */

	. = 0x7c00;
	.init : {
		FILL(0x90909090)
		HIDDEN(__init_start = .);
		*(.init)
		HIDDEN(__init_end = .);
	}
	HIDDEN(__init_len = ABSOLUTE(__init_end) - ABSOLUTE(__init_start));
	HIDDEN(__init_dwords = (__init_len + 3) >> 2);

	. = ALIGN(4);
	.text16 : {
		FILL(0x90909090)
		HIDDEN(__text16_start = .);
		*(.text16)
		HIDDEN(__text16_end = .);
	}
	HIDDEN(__text16_len = ABSOLUTE(__text16_end) - ABSOLUTE(__text16_start));
	HIDDEN(__text16_dwords = (__text16_len + 3) >> 2);

	/*
	 * .textnr is used for 32-bit code that is used on the code
	 * path to initialize the .text segment
	 */
	. = ALIGN(16);
	.textnr : {
		FILL(0x90909090)
		HIDDEN(__textnr_start = .);
		*(.textnr)
		HIDDEN(__textnr_end = .);
	}
	HIDDEN(__textnr_len = ABSOLUTE(__textnr_end) - ABSOLUTE(__textnr_start));
	HIDDEN(__textnr_dwords = (__textnr_len + 3) >> 2);

	. = ALIGN(16);
	HIDDEN(__bcopyxx_start = .);

	.bcopyxx.text : {
		FILL(0x90909090)
		HIDDEN(__bcopyxx_text_start = .);
		*(.bcopyxx.text)
		HIDDEN(__bcopyxx_text_end = .);
	}
	HIDDEN(__bcopyxx_text_len = ABSOLUTE(__bcopyxx_text_end) - ABSOLUTE(__bcopyxx_text_start));
	HIDDEN(__bcopyxx_text_dwords = (__bcopyxx_text_len + 3) >> 2);

	.bcopyxx.data : {
		HIDDEN(__bcopyxx_data_start = .);
		*(.bcopyxx.text)
		HIDDEN(__bcopyxx_data_end = .);
	}
	HIDDEN(__bcopyxx_data_len = ABSOLUTE(__bcopyxx_data_end) - ABSOLUTE(__bcopyxx_data_start));
	HIDDEN(__bcopyxx_data_dwords = (__bcopyxx_data_len + 3) >> 2);

	HIDDEN(__bcopyxx_end = .);
	HIDDEN(__bcopyxx_len = ABSOLUTE(__bcopyxx_end) - ABSOLUTE(__bcopyxx_start));
	HIDDEN(__bcopyxx_dwords = (__bcopyxx_len + 3) >> 2);

	. = ALIGN(4);
	.data16 : {
	      HIDDEN(__data16_start = .);
	      *(.data16)
	      HIDDEN(__data16_end = .);
	}
	HIDDEN(__data16_len = ABSOLUTE(__data16_end) - ABSOLUTE(__data16_start));
	HIDDEN(__data16_dwords = (__data16_len + 3) >> 2);

	. = ALIGN(4);
	HIDDEN(__config_lma = ABSOLUTE(.));
	. += SIZEOF(.config);

	. = ALIGN(4);
	HIDDEN(__replacestub_lma = ABSOLUTE(.));
	. += SIZEOF(.replacestub);

	/* The 32-bit code loads above the non-progbits sections */

	. = ALIGN(16);
	HIDDEN(__pm_code_lma = ABSOLUTE(.));

	HIDDEN(__high_clear_start = .);

	. = ALIGN(512);
	.adv (NOLOAD) : {
		HIDDEN(__adv_start = .);
		*(.adv)
		HIDDEN(__adv_end = .);
	}
	HIDDEN(__adv_len = ABSOLUTE(__adv_end) - ABSOLUTE(__adv_start));
	HIDDEN(__adv_dwords = (__adv_len + 3) >> 2);

	/* Late uninitialized sections */

	. = ALIGN(4);
	.uibss (NOLOAD) : {
		HIDDEN(__uibss_start = .);
		*(.uibss)
		HIDDEN(__uibss_end = .);
	}
	HIDDEN(__uibss_len = ABSOLUTE(__uibss_end) - ABSOLUTE(__uibss_start));
	HIDDEN(__uibss_dwords = (__uibss_len + 3) >> 2);

	HIDDEN(_end16 = .);
	HIDDEN(__assert_end16 = ASSERT(_end16 <= 0x10000, "64K overflow"));

	/*
	 * Special 16-bit segments
	 */
	. = ALIGN(65536);
	.xfer_buf (NOLOAD) : {
		*(.xfer_buf)
	}
	HIDDEN(xfer_buf_seg = core_xfer_buf >> 4);

	/*
	 * The auxilliary data segment is used by the 16-bit code
	 * for items that don't need to live in the bottom 64K.
	 */

	. = ALIGN(16);
	.auxseg (NOLOAD) : {
		HIDDEN(__auxseg_start = .);
		*(.auxseg)
		HIDDEN(__auxseg_end = .);
	}
	HIDDEN(__auxseg_len = ABSOLUTE(__auxseg_end) - ABSOLUTE(__auxseg_start));
	HIDDEN(__auxseg_dwords = (__auxseg_len + 3) >> 2);
	HIDDEN(aux_seg = __auxseg_start >> 4);

	/*
	 * Used to allocate lowmem buffers from 32-bit code
	 */
	.lowmem (NOLOAD) : {
		HIDDEN(__lowmem_start = .);
		*(.lowmem)
		HIDDEN(__lowmem_end = .);
	}
	HIDDEN(__lowmem_len = ABSOLUTE(__lowmem_end) - ABSOLUTE(__lowmem_start));
	HIDDEN(__lowmem_dwords = (__lowmem_len + 3) >> 2);

	HIDDEN(__high_clear_end = .);

	HIDDEN(__high_clear_len = ABSOLUTE(__high_clear_end) - ABSOLUTE(__high_clear_start));
	HIDDEN(__high_clear_dwords = (__high_clear_len + 3) >> 2);

	/* Start of the lowmem heap */
	. = ALIGN(16);
	HIDDEN(__lowmem_heap = .);

	/*
	 * 32-bit code.  This is a hack for the moment due to the
	 * real-mode segments also allocated.
	 */

	. = 0x100000;

	HIDDEN(__pm_code_start = .);
	HIDDEN(__vma_to_lma = ABSOLUTE(__pm_code_lma - __pm_code_start));

	.text : AT(ADDR(.text) + __vma_to_lma) {
		FILL(0x90909090)
		HIDDEN(__text_start = .);
		*(.text)
		*(.text.*)
		HIDDEN(__text_end = .);
	}

	.rodata : AT(ADDR(.rodata) + __vma_to_lma) {
		HIDDEN(__rodata_start = .);
		*(.rodata)
		*(.rodata.*)
		HIDDEN(__rodata_end = .);
	}

	.ctors : AT(ADDR(.ctors) + __vma_to_lma) {
		HIDDEN(__ctors_start = .);
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.ctors))
		HIDDEN(__ctors_end = .);
	}

	.dtors : AT(ADDR(.dtors) + __vma_to_lma) {
		HIDDEN(__dtors_start = .);
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		HIDDEN(__dtors_end = .);
	}

	.dynsym : AT(ADDR(.dynsym) + __vma_to_lma) {
		HIDDEN(__dynsym_start = .);
		KEEP (*(.dynsym))
		HIDDEN(__dynsym_end = .);
	}
	HIDDEN(__dynsym_len = __dynsym_end - __dynsym_start);

	.dynstr : AT(ADDR(.dynstr) + __vma_to_lma) {
		HIDDEN(__dynstr_start = .);
		KEEP (*(.dynstr))
		HIDDEN(__dynstr_end = .);
	}
	HIDDEN(__dynstr_len = __dynstr_end - __dynstr_start);

	.gnu.hash : AT(ADDR(.gnu.hash) + __vma_to_lma) {
		HIDDEN(__gnu_hash_start = .);
		KEEP (*(.gnu.hash))
		HIDDEN(__gnu_hash_end = .);
	}


	.dynlink : AT(ADDR(.dynlink) + __vma_to_lma) {
		HIDDEN(__dynlink_start = .);
		KEEP (*(.dynlink))
		HIDDEN(__dynlink_end = .);
	}

	.got : AT(ADDR(.got) + __vma_to_lma) {
		HIDDEN(__got_start = .);
		KEEP (*(.got))
		KEEP (*(.got.plt))
		HIDDEN(__got_end = .);
	}

	.dynamic : AT(ADDR(.dynamic) + __vma_to_lma) {
		HIDDEN(__dynamic_start = .);
		KEEP (*(.dynamic))
		HIDDEN(__dynamic_end = .);
	}

	.data : AT(ADDR(.data) + __vma_to_lma) {
		HIDDEN(__data_start = .);
		*(.data)
		*(.data.*)
		HIDDEN(__data_end = .);
	}

	HIDDEN(__pm_code_end = .);
	HIDDEN(__pm_code_len = ABSOLUTE(__pm_code_end) - ABSOLUTE(__pm_code_start));
	HIDDEN(__pm_code_dwords = (__pm_code_len + 3) >> 2);

	. = ALIGN(128);

	HIDDEN(__bss_vma = .);
	HIDDEN(__bss_lma = ABSOLUTE(.));		/* Dummy */
	.bss (NOLOAD) : AT (__bss_lma) {
		HIDDEN(__bss_start = .);
		*(.bss)
		*(.bss.*)
		*(COMMON)
		HIDDEN(__bss_end = .);
	}
	HIDDEN(__bss_len = ABSOLUTE(__bss_end) - ABSOLUTE(__bss_start));
	HIDDEN(__bss_dwords = (__bss_len + 3) >> 2);

	/* Very large objects which don't need to be zeroed */

	HIDDEN(__hugebss_vma = .);
	HIDDEN(__hugebss_lma = ABSOLUTE(.));		/* Dummy */
	.hugebss (NOLOAD) : AT (__hugebss_lma) {
		HIDDEN(__hugebss_start = .);
		*(.hugebss)
		*(.hugebss.*)
		HIDDEN(__hugebss_end = .);
	}
	HIDDEN(__hugebss_len = ABSOLUTE(__hugebss_end) - ABSOLUTE(__hugebss_start));
	HIDDEN(__hugebss_dwords = (__hugebss_len + 3) >> 2);


	/* XXX: This stack should be unified with the COM32 stack */
	HIDDEN(__stack_vma = .);
	HIDDEN(__stack_lma = ABSOLUTE(.));		/* Dummy */
	.stack (NOLOAD) : AT(__stack_lma) {
		HIDDEN(__stack_start = .);
		*(.stack)
		HIDDEN(__stack_end = .);
	}
	HIDDEN(__stack_len = ABSOLUTE(__stack_end) - ABSOLUTE(__stack_start));
	HIDDEN(__stack_dwords = (__stack_len + 3) >> 2);

	HIDDEN(_end = .);

	/* COM32R and kernels are loaded after our own PM code */
	. = ALIGN(65536);
	HIDDEN(free_high_memory = .);

	/* Stuff we don't need... */
	/DISCARD/ : {
		*(.eh_frame)
		*(.interp)
	}
}
