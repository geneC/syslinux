# $Id$
#
# memcpy.S
#
# Simple 16-bit memcpy() implementation
#

	.text
	.code16
	.globl memcpy
	.type memcpy, @function
memcpy:
	cld
	pushl %edi
	pushl %esi
	movw 12(%esp),%di
	movw 16(%esp),%si
	movw 20(%esp),%cx
	rep ; movsb
	popl %esi
	popl %edi
	ret

	.size memcpy,.-memcpy
