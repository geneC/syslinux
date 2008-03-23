#
# mempcpy.S
#

	.text
	.globl	mempcpy
	.type	mempcpy, @function
mempcpy:
	pushl	%esi
	pushl	%edi
#ifdef REGPARM
	movl	%edx, %esi
#else
	movl	12(%esp), %eax
	movl	16(%esp), %esi
	movl	20(%esp), %ecx
#endif
	movl	%eax, %edi
	movl	%ecx, %edx

	shrl	$2, %ecx
	rep ; movsl

	jnc	1f		# The shrl had carry out if odd word count
	movsw
1:
	testb	$1, %dl
	jz	2f
	movsb
2:
	movl	%edi, %eax
	popl	%edi
	popl	%esi
	ret

	.size	mempcpy, .-mempcpy
