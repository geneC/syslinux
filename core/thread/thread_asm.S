	.globl	__switch_to
	.type	__switch_to, @function
__switch_to:
	movl	__current, %edx
	movl	%ebx,   (%edx)
	movl	%esp,  4(%edx)
	movl	%ebp,  8(%edx)
	movl	%esi, 12(%edx)
	movl	%edi, 16(%edx)

	movl	  (%eax), %ebx
	movl	 4(%eax), %esp
	movl	 8(%eax), %ebp
	movl	12(%eax), %esi
	movl	16(%eax), %edi
	movl	%eax, __current
	ret
	.size	__switch_to, .-__switch_to

	.globl	__start_thread
	.type	__start_thread, @function
__start_thread:
	movl	%edi, %eax		/* Thread function argument */

	pushl	$0			/* For gdb's benefit */
	movl	%esp, %ebp		/* For gdb's benefit */

	pushl	%ebx			/* Set up the flags/interrupt state */
	popfl

	call	*%esi			/* Run the desired function */
	jmp	__exit_thread		/* If we get here, kill the thread */
	.size	__start_thread, .-__start_thread
