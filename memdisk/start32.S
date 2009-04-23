/* -----------------------------------------------------------------------
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
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
 * Simple stub to get us to the right point in the 32-bit code;
 * this module must be linked first
 */
	.section ".init", "ax"
	.globl _start
_start:
	/* Zero the bss */
	cld
	movl	$__bss_start, %edi
	movl	$__bss_end, %ecx
	subl	%edi, %ecx
	xorl	%eax, %eax
	shrl	$2, %ecx
	rep ; stosl
	jmp setup
