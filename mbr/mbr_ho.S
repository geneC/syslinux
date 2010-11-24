/* -----------------------------------------------------------------------
 *
 *   Copyright 2010 Gene Cumm
 *
 *   Portions from mbr.S:
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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
 * mbr_ho.S: MBR-like codeblock to display handoff data
 *
 * Displays the values of DL, DS, SI, the contents of [DS:SI] (16 bytes),
 * the values of ES, DI then the contents of [ES:DI] (4 bytes) then reports
 * a boot failure.
 */

#define DEBUG_MARKER1
#define DEBUG_START
#define DEBUG_LOADE
#define DEBUG_PNP

#ifdef DEBUG_MARKER1
	.macro ASCII_MARKER1 s:vararg
	.ascii \s
	.endm
#else	/* DEBUG_MARKER1 */
	.macro ASCII_MARKER1 s
	.endm
#endif	/* DEBUG_MARKER1 */

#ifdef DEBUG_LOADE
	.macro LOADE r:req, t:req
	movw	(e_\r), %\t
	.endm
#else	/* DEBUG_LOADE */
	.macro LOADE r:req, t:req
	popw %\t
	.endm
#endif	/* DEBUG_LOADE */

	.code16
	.text

entry		= 0x7c00
pnps		= (entry)
pnpi		= (pnps-2)
stack		= (pnpi-2)
e_di		= (stack-2)
e_es		= (stack-4)
e_si		= (stack-6)
e_ds		= (stack-8)

BIOS_page	= 0x462

	.globl	_start
_start:
	cli
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$stack, %sp
	sti
	cld
	pushw	%di		/* es:di -> $PnP header */
	pushw	%es
	pushw	%si
	pushw	%ds
	push	%cs
	pop	%ds

#ifdef DEBUG_START
	pushw	%dx
	call	crlf
	movw	$(_start),%dx	/* 0x0600 mbr.ld .text address */
	call	wrhexw
	call	crlf
//	movw	$(entry - _start),%dx	/* 0x7C00 */
	call	caddr
caddr:	popw	%ax
	sub	$(caddr - _start), %ax
	movw	%ax, %dx	/* Same size as pushw %ax;popw %dx */
	call	wrhexw
	call	crlf
	popw	%dx
#endif	/* DEBUG_START */

	/* write DL */
pr_dl:	call	wrstr
	.ascii	"DL: \0"
	call	wrhexb
	/* DS */
pr_ds:	call	wrstr
	.ascii	"  DS: \0"
//	popw	%dx
	LOADE	ds, dx
	pushw	%dx
	popw	%es
	call	wrhexw
	/* SI */
pr_si:	call	wrstr
	.ascii	"  SI: \0"
//	popw	%dx
	LOADE	si, dx
	pushw	%dx
	popw	%di
	call	wrhexw
	call	crlf
	/* DS:SI */
	movw	$16, %cx
	call	wrhexbses
	call	crlf

	/* ES */
pr_es:	call	wrstr
	.ascii	"ES: \0"
//	popw	%dx
	LOADE	es, dx
	pushw	%dx
	popw	%es
	call	wrhexw
pr_di:	call	wrstr
	.ascii	"  DI: \0"
//	popw	%dx
	LOADE	di, dx
	pushw	%dx
	popw	%di
	call	wrhexw
	call	crlf
	/* ES:DI */	/* %es:0(%di) */
	movw	$4, %cx
	call	wrhexbses
	call	crlf

	/* $PnP Scan */
	movw	$0xf000, %dx
	pushw	%dx
	popw	%es
	movw	$0, %si
	movw	$0x1000, %cx
	/* 0x506E5024 */
	movw	$0x5024, %dx
	movw	$0x506E, %bx
ch_pnp:	es lodsw
	cmp	%dx, %ax
	jne	ch_pnp_l
	es lodsw
	cmp	%bx, %ax
	je	pr_pnp
ch_pnp_l:
	addw	$14, %si
	andw	$0xFFF0, %si
	loopw	ch_pnp
	jmp	end
pr_pnp:
	call	wrstr
	.ascii	"$PnP-\0"
	movw	%es, %dx
	call	wrhexw
	movb	$':, %al
	call	wrchr
	movw	%si, %dx
	andw	$0xFFF0, %dx
	call	wrhexw
	call	crlf

end:
	jmp	bootfail

	ASCII_MARKER1	"wc"
wrchr:
	movb	$0x0e, %ah
	movb	(BIOS_page), %bh
	movb	$0x07, %bl
	int	$0x10		/* May destroy %bp */
	ret

	ASCII_MARKER1	"ws"
wrstr:
	pop	%si
wrstr_l:
	lodsb
	cmpb	$0, %al
	je	wrstr_d
	call	wrchr
	jmp	wrstr_l
wrstr_d:
	push	%si
	ret

crlf:
	call wrstr
	.ascii "\r\n\0"
	ret

	ASCII_MARKER1	"hx"
wrhexn:
	and	$0x0F, %al
	cmpb	$10, %al
	jae	.alph
	addb	$'0, %al
	jmp	.wc
.alph:
	addb	$('A - 10), %al
.wc:
	call wrchr
	ret

wrhexb:
	movb	%dl, %al
	pushw	%ax
	rorw	$4, %ax
	call	wrhexn
	popw	%ax
	call	wrhexn
	ret

wrhexw:
	rorw	$8, %dx
	call wrhexb
	rorw	$8, %dx
	call wrhexb
	ret

	ASCII_MARKER1	"HE"
wrhexbses:
	pushw	%di
	popw	%si
wrhexbses_l:
	movb	$' , %al
	call	wrchr
	es lodsb
	movw	%ax, %dx
	call	wrhexb
	loop	wrhexbses_l
	ret

data:
	ASCII_MARKER1	"bf"
bootfail:
	int	$0x18		/* Boot failure */
die:
	hlt
	jmp	die

zerob:		/* Begin zeroing block to fill to desired length */
		/* 420 bytes for FAT32 */
zeroln		= (420 - (zerob - _start))
	.=.+zeroln	/* prevents overflow */
zeroe:
