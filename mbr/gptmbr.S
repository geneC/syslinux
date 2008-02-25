/* -----------------------------------------------------------------------
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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

/* Partition table header here */
phdr		= 0x7e00	/* Above the boot sector and stack */
/* Partition table sector here */
/* To handle > 32K we need to play segment tricks... */
psec		= _phdr + 512

/* BootGUID */
bootguid	= _start + 0x1a8
/* Where we put DS:SI */
dssi_out	= _start + 0x1be

BIOS_page	= 0x462

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
	movb	$0x41, %ah	/* %al == 0 already */
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

	/* We have EBIOS; patch in the following code at
	   read_sector_cbios: movb $0x42, %ah ;  jmp read_common */
	movl	$0xeb42b4+((read_common-read_sector_cbios-4) << 24), \
		(read_sector_cbios)

1:
	popw	%dx

	/* Get (C)HS geometry */
	movb	$0x08, %ah
	int	$0x13
	andw	$0x3f, %cx	/* Sector count */
	pushw	%cx		/* Save sectors on the stack */
	movzbw	%dh, %ax	/* dh = max head */
	incw	%ax		/* From 0-based max to count */
	mulw	%cx		/* Heads*sectors -> sectors per cylinder */

	/* Save sectors/cylinder on the stack */
	pushw	%dx		/* High word */
	pushw	%ax		/* Low word */

	/* Load partition table header */
	xorl	%eax,%eax
	xorl	%edx,%edx
	incl	%eax
	movw	$phdr,%bx
	call	read_sector

	/* Number of partition sectors */
	/* We assume the partition table is 32K or less, and that
	   the sector size is 512. */
	movw	(phdr+80),%cx		/* NumberOfPartitionEntries */
	movw	(phdr+84),%ax		/* SizeOfPartitionEntry */
	pushw	%ax
	pushw	%cx
	mulw	%cx
	shrw	$9,%ax
	xchgw	%ax,%cx
	incw	%cx

	/* Starting LBA of partition array */
	movl	(phdr+72),%eax
	movl	(phdr+76),%edx

	pushw	%bx
get_ptab:
	call	read_sector
	call	inc64
	loopw	get_ptab

	/* Find the boot partition */
	popw	%si			/* Partition table in memory */
	popw	%cx			/* NumberOfPartitionEntries */
	popw	%ax			/* SizeOfPartitionEntry */
find_part:
	pushw	%cx
	pushw	%si
	addw	$16,%si
	movw	$bootguid,%di
	movw	$8,%cx
	repe; cmpsw
	popw	%si
	popw	%cx
	je found_part
	addw	%ax,%si
	loopw	find_part

	call	error
	.ascii	"Boot partition not found\r\n"

found_part:
	movw	%ax,%cx		/* Set up %cx for rep movsb further down */
	
	movw	$dssi_out,%di
	pushw	%di
	
	/* 80 00 00 00 ee 00 00 00
	   - bootable partition, type EFI (EE), no CHS information */
	xorl	%eax,%eax
	movb	$0x80,%al
	stosl
	movb	$0xee,%al
	stosl
	movl	40(%si),%eax
	movl	48(%si),%edx
	call	saturate_stosl		/* Partition start */

	subl	32(%si),%eax
	sbbl	36(%si),%edx
	call	inc64
	call	saturate_stosl		/* Partition length */

	rep; movsb			/* GPT entry follows MBR entry */
	popw	%si

/*
 * boot: invoke the actual bootstrap. %ds:%si points to the
 * partition information in memory.
 */
boot:
	movl	(40+16)(%si),%eax
	movl	(48+16)(%si),%edx
	movw	$bootsec,%bx
	call	read_sector
	cmpw	$0xaa55, -2(%bx)
	jne	missing_os	/* Not a valid boot sector */
	movw	$driveno, %sp	/* driveno == bootsec-6 */
	popw	%dx		/* dl -> drive number */
	popw	%di		/* es:di -> $PnP vector */
	popw	%es
	cli
	jmpw	*%sp		/* %sp == bootsec */

missing_os:
	call	error
	.ascii	"Operating system not bootable\r\n"

saturate_stosl:
	pushl	%eax
	andl	%edx,%edx
	jz 1f
	orl	$-1,%eax
1:	stosl
	popl	%eax
	ret

inc64:
	addl	$1,%eax
	adcl	$0,%edx
	ret

/*
 * read_sector: read a single sector pointed to by %edx:%eax to
 * %es:%bx.  CF is set on error.  All registers saved.
 */
read_sector:
	pushal
	pushl	%edx	/* MSW of LBA */
	pushl	%eax	/* LSW of LBA */
	pushw	%es	/* Buffer segment */
	pushw	%bx	/* Buffer offset */
	pushw	$1	/* Sector count */
	pushw	$16	/* Size of packet */
	movw	%sp, %si

	/* This chunk is skipped if we have ebios */
	/* Do not clobber %es:%bx or %edx:%eax before this chunk! */
read_sector_cbios:
	divl	(secpercyl)
	shlb	$6, %ah
	movb	%ah, %cl
	movb	%al, %ch
	xchgw	%dx, %ax
	divb	(sectors)
	movb	%al, %dh
	orb	%ah, %cl
	incw	%cx	/* Sectors are 1-based */
	movw	$0x0201, %ax

read_common:
	movb	(driveno), %dl
	int	$0x13
	addw	$16, %sp	/* Drop DAPA */
	popal
	jc	disk_error
	addb	$2, %bh		/* bx += 512: point to the next buffer */
	ret

disk_error:
	call	error
	.ascii	"Disk error on boot\r\n"

/*
 * Print error messages.  This is invoked with "call", with the
 * error message at the return address.
 */
error:
	popw	%si
2:
	lodsb
	movb	$0x0e, %ah
	movb	(BIOS_page), %bh
	movb	$0x07, %bl
	int	$0x10		/* May destroy %bp */
	cmpb	$10, %al	/* Newline? */
	jne	2b

	int	$0x18		/* Boot failure */
die:
	hlt
	jmp	die
