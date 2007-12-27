; -----------------------------------------------------------------------
;
;   Copyright 1994-2007 H. Peter Anvin - All Rights Reserved
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


; Memory below 0800h is reserved for the BIOS and the MBR.
BSS_START	equ 0800h

; Text starts at the load address of 07C00h.
TEXT_START	equ 7C00h

; The secondary BSS section, above the text; we really wish we could
; just make it follow .bcopy32 or hang off the end,
; but it doesn't seem to work that way.
LATEBSS_START	equ 0B800h

; Reserve memory for the stack.  This causes checkov to abort the
; compile if we violate this space.
STACK_SIZE	equ 4096
STACK_START	equ TEXT_START-STACK_SIZE

%ifdef MAP
		[map all MAP]
%endif

;
; The various sections and their relationship
;
		org TEXT_START

		; Use .earlybss for things that MUST be in low memory.
		section .earlybss	nobits start=BSS_START
		section .bcopy32	align=4 valign=16 follows=.data vfollows=.earlybss
		section .config		align=4 valign=16 follows=.bcopy32 vfollows=.bcopy32
		section .config.end	nobits valign=4 vfollows=.config

		; Use .bss1 for things that doesn't have to be in low memory,
		; but are in order to offload .bss
		section .bss1		nobits valign=16 vfollows=.config.end

		section .text		start=TEXT_START
		; NASM BUG: .data always follows .text; can't override
		section .data		align=16 ; follows=.text

		; This empty section works around a NASM bug with regards
		; to follows= and nobits sections following a section which
		; has VMA != LMA.
		section .advpad		progbits align=512 follows=.config

		section .adv		nobits align=512 follows=.advpad

		; .uibss contains bss data which is guaranteed to be
		; safe to clobber during the loading of the image.  This
		; is because while loading the primary image we will clobber
		; the spillover from the last fractional sector load.
		section .uibss		nobits align=16 follows=.adv

		; Normal bss...
		section .bss		nobits align=16 follows=.uibss

		; Reserve space for stack
		section .stack		nobits align=16 start=STACK_START
Stack		resb	STACK_SIZE
