#
# Simple stub to get us to the right point in the 32-bit code;
# this module must be linked first
#
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
