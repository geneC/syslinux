# $Id$
#
# memset.S
#
# Minimal 16-bit memset() implementation
#

	.text
	.code16
	.globl memset
	.type memset, @function
memset:
	cld
	pushl %edi
	movw 8(%esp),%di
	movb 12(%esp),%al
	movw 16(%esp),%cx
	rep ; stosb
	popl %edi
	ret

	.size memset,.-memset
