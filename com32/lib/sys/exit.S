/*
 * Implementation of _exit() for com32 based on c32entry.S
 */
	.text

	.globl _Exit
	.type _Exit, @function
_Exit:
	/* Just fall through to _exit */
	.size _Exit, .-_Exit

	.globl _exit
	.type _exit, @function
_exit:
#ifdef REGPARM
	pushl %eax
#endif

	/* Run any destructors */
	movl $__dtors_start, %esi
2:
	cmpl $__dtors_end, %esi
	jae 1f
	call *(%esi)
	addl $4,%esi
	jmp 2b

1:
#ifdef REGPARM
	popl %eax
#else
	movl 4(%esp),%eax		# Exit code in %eax = return value
#endif
	movl (__entry_esp),%esp		# Return stack pointer to entry value
	ret				# Return to termination address
	.size _exit, .-_exit

	.data
__exit_handler:
	.globl __exit_handler
	.long _exit
