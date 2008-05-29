; -----------------------------------------------------------------------
;
;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; layout.inc
;
; Memory layout of segments
;

		; Default to 16-bit code
		bits 16

; Memory below 0800h is reserved for the BIOS and the MBR.
BSS_START	equ 0800h

; Text starts at the load address of 07C00h.
TEXT_START	equ 7C00h

; The secondary BSS section, above the text; we really wish we could
; just make it follow .bcopy32 or hang off the end,
; but it doesn't seem to work that way.
LATEBSS_START	equ 0B800h

%ifdef MAP
		[map all MAP]
%endif

;
; The various sections and their relationship
;
		; Use .earlybss for things that MUST be in low memory.
		section .earlybss	nobits start=BSS_START
		section .bcopy32	exec nowrite progbits align=4
		section .config		write progbits align=4
		section .config.end	write nobits   align=4

		; Use .bss for things that doesn't have to be in low memory;
		; with .bss1 and .bss2 to offload.  .earlybss should be used
		; for things that absolutely have to be below 0x7c00.
		section .bss		write nobits align=16

		; Warning here: RBFG build 22 randomly overwrites
		; memory location [0x5680,0x576c), possibly more.  It
		; seems that it gets confused and screws up the
		; pointer to its own internal packet buffer and starts
		; writing a received ARP packet into low memory.
%if IS_PXELINUX
		section .rbfg		write nobits
RBFG_brainfuck:	resb 2048		; Bigger than an Ethernet packet...
%endif

		; For section following .rbfg
%if IS_PXELINUX
		section .bss2		write nobits align=16
%else
		section .bss2		write nobits align=16
%endif

		section .text		exec write progbits align=16
		section .data		write progbits align=16

		section .adv		write nobits align=512

		; .uibss contains bss data which is guaranteed to be
		; safe to clobber during the loading of the image.  This
		; is because while loading the primary image we will clobber
		; the spillover from the last fractional sector load.
		section .uibss		write nobits align=16

		; Normal bss...
		section .bss1		write nobits align=16

		; Symbols from linker script
		extern Stack
		extern section..bcopy32.start
		extern section..config.start
		extern section..config.vstart
		extern section..config.end.start

		global _start

		section .text
