/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2013 Intel Corporation; author: Matt Fleming
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

	.globl kernel_jump
	.type  kernel_jump,@function
	.text
kernel_jump:
	cli
	movl	0x8(%esp), %esi
	movl	0x4(%esp), %ecx
	jmp	*%ecx
