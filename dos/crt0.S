	.code16

	.section ".init","ax"
	.globl _start
	.type _start,@function
_start:
	# Clear the .bss
	cld
	xorl %eax,%eax
	movw $__bss_start,%di
	movw $_end+3,%cx
	subw %di,%cx
	shrw $2,%cx
	rep ; stosl

	# Compute argc and argv
	movzbw 0x80,%cx
	movl $0x81,%esi
	movw $argv,%di

	xorl %eax,%eax		# Dummy argv[0]
	stosl

1:
	# Skip any space or control character
	jcxz 2f
	lodsb
	decw %cx
	cmp $0x20,%al
	ja 3f
5:
	movb $0,-1(%si)
	jmp 1b

3:
	movl %esi,%eax
	stosl
4:
	# Skip this word
	jcxz 2f
	lodsb
	decw %cx
	cmp $0x20,%al
	ja 4b
	jmp 5b
2:
	xorl %eax,%eax		# Final null argv entry
	stosl	

	# Initialize malloc
	calll __init_memory_arena

	# Now call main...
	pushl $argv		# Push argv
	pushl %eax		# Dummy argc (we dont look at it)
	calll main
	
	pushl %eax

	.globl exit
exit:
	popl %eax
	movb $0x4c,%ah		# Terminate program
	int $0x21

	.section ".bss","aw"
argv:	.space 4*128
