		.section ".text","ax"
		.globl _start
_start:
		# This first instruction acts as COM32 magic number
		movl $0x21cd4cff,%eax
		leal 4(%esp),%esi
		movl $__com32,%edi
		mov $5,%ecx
		cld
		rep ; movsl
		jmp __start

		.section ".bss","a"
		.globl __com32
__com32:	.space 20
