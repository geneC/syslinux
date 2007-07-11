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

	.globl	bootsec
stack		= 0x7c00
driveno		= (stack-6)
sectors		= (stack-8)
secpercyl	= (stack-12)

BIOS_page = 0x462

	/* gas/ld has issues with doing this as absolute addresses... */
	.section ".bootsec", "a", @nobits
	.globl	bootsec
bootsec:
	.space	512

	.text
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
	shrw	%cx		/* Bit 0 = fixed disk subset */
	jnc	1f

	/* We have EBIOS; patch in a jump to read_sector_ebios */
	movw	$0xeb+((read_sector_ebios-read_sector_cbios-2)<< 8), (read_sector_cbios)

1:
	popw	%dx

	/* Get (C)HS geometry */
	movb	$0x08, %ah
	int	$0x13
	andw	$0x3f, %cx	/* Sector count */
	pushw	%cx		/* Save sectors on the stack */
	xorw	%ax, %ax
	pushw	%ax		/* High word of sectors/cylinder */
	movb	%dh, %al	/* dh = number of heads */
	incw	%ax		/* From 0-based to count */
	mulw	%cx		/* Heads*sectors -> sectors per cylinder */
	pushw	%ax		/* Save sectors/cylinder on the stack */

	xorl	%eax, %eax	/* Base */
	cdq			/* Root (%edx <- 0) */
	call	scan_partition_table

	/* If we get here, we have no OS */
	jmp	missing_os

/*
 * read_sector: read a single sector pointed to by %eax to 0x7c00.
 * CF is set on error.  All registers saved.
 */
read_sector:
	pushal
read_sector_cbios:
	xorl	%edx, %edx
	divl	(secpercyl)
	rorb	%ah
	rorb	%ah
	movb	%ah, %cl
	movb	%al, %ch
	movw	%dx, %ax
	divb	(sectors)
	movb	%al, %dh
	incb	%ah
	orb	%ah, %cl
	movw	$bootsec, %bx
	movw	$0x0201, %ax
	jmp	read_common
read_sector_ebios:
	movw	$dapa, %si
	movl	%eax, 8(%si)
	movb	$0x42, %ah
read_common:
	movb	(driveno), %dl
	int	$0x13
	popal
	ret

/*
 * read_partition_table:
 *	Read a partition table (pointed to by %eax), and copy
 *	the partition table into the ptab buffer.
 *	Clobbers %si, %di, and %cx, other registers preserved.
 */
ptab	= _start+446

read_partition_table:
	call	read_sector
	jc	20f
	movw	$bootsec+446, %si
	movw	$ptab, %di
	movw	$(16*4/2), %cx
	rep ; movsw
20:
	ret

/*
 * scan_partition_table:
 *	Scan a partition table currently loaded in the partition table
 *	area.  Preserve all registers.
 *
 *      On entry:
 *	  %eax - base (location of this partition table)
 *	  %edx - root (offset from MBR, or 0 for MBR)
 *
 *      These get pushed into stack slots:
 *        28(%bp) - %eax - base
 *	  20(%bp) - %edx - root
 */

scan_partition_table:
	pushal
	movw	%sp, %bp

	/* Search for active partitions */
	movw	$ptab, %bx
	movw	$4, %cx
	xorw	%ax, %ax
	push	%bx
	push	%cx
5:
	testb	$0x80, (%bx)
	jz	6f
	incw	%ax
	movw	%bx, %si
6:
	addw	$16, %bx
	loopw	5b

	decw	%ax		/* Number of active partitions found */
	jz	boot
	jns	too_many_active

	/* No active partitions found, look for extended partitions */
	popw	%bx		/* %bx <- ptab */
	popw	%cx		/* %cx <- 4    */
7:
	movb	4(%bx), %al
	cmpb	$0x0f, %al	/* 0x0f = Win9x extended */
	je	8f
	andb	$~0x80, %al	/* 0x85 = Linux extended */
	cmpb	$0x05, %al	/* 0x05 = MS-DOS extended */
	jne	9f

	/* It is an extended partition.  Read the extended partition and
	   try to scan it.  If the scan returns, re-load the current
	   partition table and resume scan. */
8:
	movl	8(%bx), %eax		/* Partition table offset */
	movl	20(%bp), %edx		/* "Root" */
	addl	%edx, %eax		/* Compute location of new ptab */
	andl	%edx, %edx		/* Is this the MBR? */
	jnz	10f
	movl	%eax, %edx		/* Offset -> root if this was MBR */
10:
	call	read_partition_table
	jc	11f
	call	scan_partition_table
11:
	/* This returned, so we need to reload the current partition table */
	movl	28(%bp), %eax		/* "Base" */
	call	read_partition_table

	/* fall through */
9:
	/* Not an extended partition */
	addw	$16, %bx
	loopw	7b

	/* Nothing found, return */
	popal
	ret

/*
 * boot: invoke the actual bootstrap. (%si) points to the partition
 *	 table entry, and 28(%bp) has the partition table base.
 */
boot:
	movl	8(%si), %eax
	addl	28(%bp), %eax
	movl	%eax, 8(%si)	/* Adjust in-memory partition table entry */
	call	read_sector
	jc	disk_error
	cmpw	$0xaa55, (bootsec+510)
	jne	missing_os		/* Not a valid boot sector */
	movw	$driveno, %sp
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
	.ascii	"Operating system load error."
	.byte	0
too_many_active_msg:
	.ascii	"Multiple active partitions."
	.byte	0

#ifndef NO_ALIGN
	.balign	4
#endif
dapa:
	.short	16		/* Size of packet */
	.short	1		/* Sector count */
	.short	0x7c00		/* Buffer offset */
	.short	0		/* Buffer segment */
	.long	0		/* LSW of LBA */
	.long	0		/* MSW of LBA */
