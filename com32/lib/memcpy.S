#
# memcpy.S
#

	.text
	.globl	memcpy
	.type	memcpy, @function
memcpy:
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
	cld
	rep ; movsl

	jnc	1f		# The shrl had carry out if odd word count
	movsw
1:
	testb	$1, %dl
	jz	2f
	movsb
2:
	popl	%edi
	popl	%esi
	ret

	.size	memcpy, .-memcpy
