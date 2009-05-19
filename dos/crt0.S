	.code16

#ifndef REGPARM
# error "This file assumes -mregparm=3 -DREGPARM=3"
#endif

	.section ".text","ax"
	.globl _start
	.type _start,@function
_start:
	# Align the stack and make sure the high half is zero
	andl $0xfff8,%esp

	# DS, ES points to the PSP at this point
	pushw %es		# Save PSP pointer
	movw %cs,%ax
	movw %ax,%ds
	movw %ax,%es

	# Clear the .bss
	cld
	xorl %eax,%eax
	movw $__bss_start,%di
	movw $__bss_end+3,%cx
	subw %di,%cx
	shrw $2,%cx
	rep ; stosl

	# Copy the command line into our own segment
	popw %fs		# FS -> PSP
	movw $_cmdline,%di
	movzbw %fs:0x80,%cx
	movw $0x81,%si
	fs ; rep ; movsb
	# Already zero-terminated since we're writing into clean bss

	# Compute argc and argv (assumes REGPARM)
	movl $_cmdline,%edx
	pushl %eax		# Make space for argv
	movl %esp,%eax
	calll __parse_argv
	pushl %eax		# argc

	# Initialize malloc
	calll __init_memory_arena

	# Now call main... (NOTE: gcc forces main to be regparm 0)
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
	.balign 4
_cmdline:
	.space 128
	.size _cmdline,.-_cmdline
