/* -----------------------------------------------------------------------
 *
 *   Copyright 2003-2008 H. Peter Anvin - All Rights Reserved
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
 * COM32 start up code - must be linked first in the binary
 */

/* Number of arguments in our version of the entry structure */
#define COM32_ARGS 7

		.section ".init","ax"
		.globl _start
		.type _start, @function
_start:
		/* This first instruction acts as COM32 magic number */
		movl $0x21cd4cff,%eax

		/* Upwards string operations */
		cld

		/* Zero the .bss segment */
		xorl %eax,%eax
		movl $__bss_start,%edi		# Symbol provided by linker
		movl $_end+3,%ecx		# Symbol provided by linker
		subl %edi,%ecx
		shrl $2,%ecx
		rep ; stosl

		/* Copy COM32 invocation parameters */
		leal 4(%esp),%esi		# Argument list
		movl $__com32,%edi
		movl $(COM32_ARGS),%ecx
		movl %esp,-4(%edi)		# Save the initial stack ptr
		cmpl (%esi),%ecx
		jbe 1f
		movl (%esi),%ecx
1:		inc %ecx			# Copy the argument count, too
		rep ; movsl

		/* Parse the command line (assumes REGPARM) */
		movl __com32+4,%edx		# Command line
		pushl %edx			# Make space for argv
		movl %esp,%eax
		call __parse_argv
		pushl %eax			# Save argc

		/* Look for library initialization functions */
		movl $__ctors_start, %esi
2:
		cmpl $__ctors_end, %esi
		jae 3f
		call *(%esi)
		addl $4,%esi
		jmp 2b
/*
 * Actually run main.  This assumes REGPARM is used!!!!
 */
3:
		popl %eax			# argc
		popl %edx			# argv
		call main
		call *(__exit_handler)
		hlt
		.size _start, .-_start

		.bss
		.globl __entry_esp
__entry_esp:	.space 4
		.globl __com32
__com32:	.space 4*(COM32_ARGS+1)
