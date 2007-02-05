/* -----------------------------------------------------------------------
 *   
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
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

	.code16
	.text

bootsec = 0x7c00
stack	= bootsec
driveno	= (stack-4)
heads	= (stack-6)
sectors	= (stack-8)

BIOS_page = 0x462
	
	.globl	_start
_start:
	cli
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	$stack, %sp
	movw	%sp, %si
	pushw	%es		/* es:di -> $PnP header */
	pushw	%di
	pushw	%dx		/* dl -> drive number */
	movw	%ax, %es
	sti
	cld

	/* Copy down to 0:0x600 */
	movw	$_start, %di
	movw	$(512/2), %cx
	rep; movsw

	ljmpw	$0, $next

next:
	/* Check to see if we have EBIOS */
	pushw	%dx		/* drive number */
	movw	$0x4100, %ax
	movw	$0x55aa, %bx
	xorw	%cx, %cx
	xorb	%dh, %dh
	stc
	int	$0x13
	jc	1f
	cmpw	$0xaa55, %bx
	jne	1f
	testb	$0x01, %cl
	jz	1f

	/* We have EBIOS; patch in a jump to read_sector_ebios */
	movw	$0xeb+((read_sector_ebios-read_sector_cbios+2)<< 8), (read_sector_cbios)

1:
	popw	%dx

	/* Get (C)HS geometry */
	movb	$0x08, %ah
	int	$0x13
	xorw	%ax, %ax
	movb	%dh, %al	/* dh = number of heads */
	incw	%ax		/* From 0-based to count */
	pushw	%ax		/* Save heads on the stack */
	andw	$0x3f, %cx	/* Sector count */
	pushw	%cx		/* Save sectors on the stack */

	xorl	%eax, %eax
	pushl	%eax		/* Base */
	pushl	%eax		/* Root */
	call	scan_partition_table

	
	/* If we get here, we have no OS */
	jmp	missing_os

/*
 * read_sector: read a single sector pointed to by %eax to 0x7c00.
 * CF is set on error.  All registers clobbered.
 */
read_sector:
read_sector_cbios:
	movl	%eax, %edx
	shrl	$16, %edx
	divw	(sectors)
	incw	%dx
	movw	%dx, %cx
	xorw	%dx, %dx
	divw	(heads)
	/* dx = head, ax = cylinder */
	movb	%al, %ch
	shrw	$2, %ax
	shrw	%ax
	andb	$0xc0, %al
	orb	%al, %cl
	movb	%dl, %dh
	movw	$bootsec, %bx
	movw	$0x0201, %ax
	jmp	read_common
read_sector_ebios:
	movw	$dapa, %si
	movl	%eax, 8(%si)
	movb	$0x42, %ah
read_common:
	int	$0x13
	ret

/*
 * read_partition_table:
 *	Read a partition table (pointed to by %eax), and copy
 *	the partition table into the ptab buffer.
 *	Preserve registers.
 */
ptab	= _start+446

read_partition_table:
	pushal
	call	read_sector
	jc	20f
	movw	$bootsec+446, %si
	movw	$ptab, %di
	movw	$(16*4/2), %cx
	rep ; movsw
20:
	popal
	ret
			
/*
 * scan_partition_table:
 *	Scan a partition table currently loaded in the partition table
 *	area.  Preserve 16-bit registers.
 *
 *	On stack:
 *	  18(%bp) - root (offset from MBR, or 0 for MBR)
 *	  22(%bp) - base (location of this partition table)
 */
	
scan_partition_table:
	pusha
	movw	%sp, %bp

	/* Search for active partitions */	
	movw	$ptab, %di
	movw	$4, %cx
	xorw	%ax, %ax
5:
	testb	$0x80, (%di)
	jz 6f
	incw	%ax
	movw	%di, %si
6:
	addw	$16, %di
	loopw	5b

	cmpw	$1, %ax		/* Number of active partitions found */
	je	boot
	ja	too_many_active

	/* No active partitions found, look for extended partitions */
	movw	$ptab, %di
	movb	$4, %cl		/* cx == 0 here */
7:
	movb	4(%di), %al
	cmpb	$0x05, %al	/* MS-DOS extended */
	je	8f
	cmpb	$0x0f, %al	/* Win9x extended */
	je	8f
	cmpb	$0x85, %al	/* Linux extended */
	jne	9f
	
	/* It is an extended partition.  Read the extended partition and
	   try to scan it.  If the scan returns, re-load the current
	   partition table and resume scan. */
8:
	movl	8(%di), %eax		/* Partition table offset */
	movl	18(%bp), %edx		/* "Root" */
	addl	%edx, %eax		/* Compute location of new ptab */
	andl	%edx, %edx		/* Is this the MBR? */
	jnz	10f
	movl	%eax, %edx		/* Offset -> root if this was MBR */
10:
	pushl	%eax			/* Push new base */
	pushl	%edx			/* Push new root */
	call	read_partition_table
	jc 11f
	call	scan_partition_table
11:
	addw	$8, %sp
	/* This returned, so we need to reload the current partition table */
	movl	22(%bp), %eax
	call	read_partition_table

	/* fall through */		
9:
	/* Not an extended partition */
	addw	$16, %di
	loopw	7b

	/* Nothing found, return */
	popa
	ret

/*
 * boot: invoke the actual bootstrap. (%si) points to the partition
 *	 table entry, and 22(%bp) has the partition table base.
 */
boot:
	movl	8(%si), %eax
	addl	22(%bp), %eax
	movl	%eax, 8(%si)
	call	read_sector
	jc	disk_error
	cmpw	$0xaa55, (bootsec+0x510)
	je	missing_os		/* Not a valid boot sector */
	movw	$stack-6, %sp
	popw	%dx		/* dl -> drive number */
	popw	%di		/* es:di -> $PnP vector */
	popw	%es
	cli
	jmp	bootsec

/*
 * error messages
 */
missing_os:
	movw	$missing_os_msg, %si
	jmp	error
disk_error:
	movw	$disk_error_msg, %si
	jmp	error
too_many_active:
	movw	$too_many_active_msg, %si
	/* jmp error */
	
error:
2:
	lodsb
	andb	%al, %al
	jz	3f
	movb	$0x0e, %ah
	movb	(BIOS_page), %bh
	movb	$0x07, %bl
	int	$0x10
	jmp	2b
3:
	int	$0x18		/* Boot failure */
	jmp	.		/* Die */

missing_os_msg:
	.ascii	"Missing operating system."
	.byte	0
disk_error_msg:
	.ascii	"Failed to load operating system."
	.byte	0
too_many_active_msg:
	.ascii	"Multiple active partititons."
	.byte	0

	.balign	4
dapa:
	.short	16		/* Size of packet */
	.short	1		/* Sector count */
	.short	0x7c00		/* Buffer offset */
	.short	0		/* Buffer segment */
	.long	0		/* LSW of LBA */
	.long	0		/* MSW of LBA */
