	.code16

	.section ".init","ax"
	.globl _start
	.type _start,@function
_start:
	# Align the stack and make sure the high half is zero
	andl $0xfff8,%esp
	
	# Clear the .bss
	cld
	xorl %eax,%eax
	movw $__bss_start,%di
	movw $_end+3,%cx
	subw %di,%cx
	shrw $2,%cx
	rep ; stosl

	# Compute argc and argv
	xorl %edx,%edx
	movzbw 80,%bx
	movb %dl,81(%bx)	# Zero-terminate string
	movb $81,%dl
	pushl %eax		# Make space for argv
	movl %esp,%eax
	calll __parse_argv
	pushl %eax		# argc

	# Initialize malloc
	calll __init_memory_arena

	# Now call main...
	popl %eax		# argc
	popl %edx		# argv
	calll main

	# Here %eax is the exit code, fall through into exit
	
	.size _start,.-_start

	.globl exit
	.type exit,@function
exit:
	# Exit code already in %eax
	movb $0x4c,%ah		# Terminate program
	int $0x21
1:	hlt
	jmp 1b
	.size exit,.-exit
	
	.section ".bss","aw"
argv:	.space 4*128
