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
 * the values of ES, DI, the contents of [ES:DI] (4 bytes), scans memory for
 * $PnP then reports a boot failure.
 *
 * This should (hopefully) be only 8086 code
 */

// #define DEBUG_MARKER1	/* Insert markers in binary */
// #define DEBUG_START	/* Print entry addresses at start */
// #define DEBUG_LOADE	/* movw versus pop */
#define DEBUG_PNP	/* Scan for $PnP and show address */
#define DEBUG_PAK	/* Press Any Key before boot fail */

#ifdef DEBUG_MARKER1
	.macro ASCII_MARKER1 s:vararg
	.ascii	\s
	.endm
#else	/* DEBUG_MARKER1 */
	.macro ASCII_MARKER1 s:vararg
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
stack		= (entry)
e0_ax		= (stack-2)
e0_cx		= (stack-4)
e0_dx		= (stack-6)
e0_bx		= (stack-8)
e0_sp		= (stack-10)
e0_bp		= (stack-12)
e0_si		= (stack-14)
e0_di		= (stack-16)
e0_bot		= (stack-16)
e_di		= (e0_bot-2)	/* Original register values from entry point */
e_es		= (e0_bot-4)
e_si		= (e0_bot-6)
e_ds		= (e0_bot-8)

BIOS_page	= 0x462

	.globl	_start
_start:
	cli
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$stack, %sp
	pushaw
	pushw	%di		/* es:di -> $PnP header */
	pushw	%es
	pushw	%si
	pushw	%ds
	sti
	cld
	pushw	%cs
	popw	%ds

#ifdef DEBUG_START
	pushw	%dx
	call	crlf
	movw	$(_start),%dx	/* 0x0600 mbr.ld .text address */
	call	wrhexw
	call	crlf
	call	caddr
caddr:
	popw	%dx
	subw	$(caddr - _start), %dx
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
	LOADE	ds, dx
	pushw	%dx
	popw	%es
	call	wrhexw
	/* SI */
pr_si:	call	wrstr
	.ascii	"  SI: \0"
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
	LOADE	es, dx
	pushw	%dx
	popw	%es
	call	wrhexw
pr_di:	call	wrstr
	.ascii	"  DI: \0"
	LOADE	di, dx
	pushw	%dx
	popw	%di
	call	wrhexw
	call	crlf
	/* ES:DI */	/* %es:0(%di) */
	movw	$4, %cx
	call	wrhexbses

#ifdef DEBUG_PNP
	subw	$4, %si
	es lodsw
	cmpw	$0x5024, %ax
	jne	scn_pnp
	es lodsw
	cmpw	$0x506E, %ax
	jne	scn_pnp
	call	wrstr
	.ascii	" =$PnP\0"
scn_pnp:
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
	cmpw	%dx, %ax
	jne	ch_pnp_l
	es lodsw
	cmpw	%bx, %ax
	je	pr_pnp
ch_pnp_l:
	addw	$14, %si
	andw	$0xFFF0, %si
	loopw	ch_pnp
	jmp	end
pr_pnp:
	pushw	%si
	call	wrstr
	.ascii	"$PnP-\0"
	movw	%es, %dx
	call	wrhexw
	movb	$':, %al
	call	wrchr
	popw	%dx
	andw	$0xFFF0, %dx
	call	wrhexw
#endif	/* DEBUG_PNP */

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
	call	wrstr
	.ascii	"\r\n\0"
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
#ifdef DEBUG_PAK
	call wrstr
	.ascii	"\r\n\r\nPress any key\r\n\0"
	xor	%ax, %ax
	int	$0x16
#endif
	int	$0x18		/* Boot failure */
die:
	hlt
	jmp	die

zerob:		/* Begin zeroing block to fill to desired length */
		/* 420 bytes for FAT32 */
zeroln		= (420 - (zerob - _start))
	.=.+zeroln	/* prevents overflow */
zeroe:
