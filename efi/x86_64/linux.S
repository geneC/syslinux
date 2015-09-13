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

#define CR0_PG_BIT    31
#define CR4_PAE_BIT   5
#define MSR_EFER      0xc0000080

	.globl kernel_jump
	.type kernel_jump,@function
	.code64
kernel_jump:
	cli

	/* save the content of rsi (boot_param argument of kernel_jump function) */
	mov %rsi, %rbx

	call base_address
base_address:
	pop %rsi

	/* need to perform a long jump to update cs */

	/* load absolute address of pm_code in jmp_address location */
	lea (pm_code - base_address)(%rsi, 1), %rax
	mov %eax, (jmp_address - base_address)(%rsi, 1)

	ljmp *(jmp_address - base_address)(%rsi, 1)

jmp_address:
	.long 0    /* address */
	.word 0x10 /* segment */

	.code32
pm_code:

	/* cs segment has been updated, now update the rest */
	mov $0x18, %eax
	mov %eax, %ds
	mov %eax, %es
	mov %eax, %fs
	mov %eax, %gs
	mov %eax, %ss

	/* disable paging. */
	mov %cr0, %eax
	btr $CR0_PG_BIT, %eax /* PG in CR0 */
	mov %eax, %cr0

	/* disable long mode. */
	mov $MSR_EFER, %ecx
	rdmsr
	btr $8, %eax
	wrmsr

	/* kernel jump */
	mov %ebx, %esi
	jmp *%edi

	.code64
	.align 4
	.globl efi_handover_32
	.type  efi_handover_32,@function
efi_handover_32:
	movl	$38, errno(%rip)	/* ENOSYS */
	ret

	.globl efi_handover_64
	.globl efi_handover
	.type  efi_handover_64,@function
	.type  efi_handover,@function
efi_handover_64:
efi_handover:
	add	$512, %rcx
	cli
	jmp	*%rcx
