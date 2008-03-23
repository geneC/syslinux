/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 rPath, Inc. - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * memmove.S
 *
 * Reasonably efficient memmove
 */

	.globl	memmove
	.type	memmove,@function
	.text
memmove:
	pushl	%esi
	pushl	%edi

	movl	%eax,%edi
	movl	%edx,%esi

	cmpl	%edi,%esi
	jb	1f

	/* source >= dest, forwards move */
	movl	%ecx,%edx
	shrl	$2,%ecx
	andl	$3,%edx

	rep; movsl
	movl	%edx,%ecx
	rep; movsb

	jmp	2f
1:
	/* source < dest, backwards move */
	leal	-4(%ecx,%esi),%esi
	leal	-4(%ecx,%edi),%edi

	movl	%ecx,%edx
	shrl	$2,%ecx
	andl	$3,%edx

	std
	rep; movsl
	movl	%edx,%ecx
	addl	$3,%esi
	addl	$3,%edi
	rep; movsb
	cld

2:
	popl	%edi
	popl	%esi
	ret

	.size	memmove, .-memmove
